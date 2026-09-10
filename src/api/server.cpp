#include "api/server.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <vector>

#include "api/protocol.h"
#include "api/socket.h"

namespace fuwa::api {
namespace {

// 溜め込む 1 リクエストの上限。改行が来ないまま流し込まれ続けても膨らまないように。
constexpr std::size_t kMaxRequestBytes = 1u << 20;

// シグナルハンドラから触ってよいのはこれだけ。
volatile std::sig_atomic_t gStopped = 0;

extern "C" void onSignal(int) { gStopped = 1; }

// SA_RESTART を付けない。accept が EINTR で戻ってこないと、止まれない。
void catchSignal(int number) {
  struct sigaction action {};
  action.sa_handler = onSignal;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;
  sigaction(number, &action, nullptr);
}

void fillAddress(sockaddr_un& address, const std::filesystem::path& path) {
  address.sun_family = AF_UNIX;
  std::snprintf(address.sun_path, sizeof(address.sun_path), "%s", path.c_str());
}

// 残っているソケットの向こうに誰かいるか。繋がれば動いている。
bool alreadyRunning(const std::filesystem::path& path) {
  const int probe = socket(AF_UNIX, SOCK_STREAM, 0);
  if (probe < 0) {
    return false;
  }
  sockaddr_un address{};
  fillAddress(address, path);
  const bool connected =
      connect(probe, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
  close(probe);
  return connected;
}

bool writeAll(int fd, const std::string& data) {
  std::size_t written = 0;
  while (written < data.size()) {
    const ssize_t sent = write(fd, data.data() + written, data.size() - written);
    if (sent <= 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    written += static_cast<std::size_t>(sent);
  }
  return true;
}

bool respond(int fd, const std::string& line, Commands& commands) {
  std::vector<std::string> args;
  std::string error;
  Response response;
  if (decodeRequest(line, args, error)) {
    response = commands.run(args);
  } else {
    response = {2, error};
  }
  return writeAll(fd, encodeResponse(response) + "\n");
}

void serveConnection(int fd, Commands& commands) {
  std::string buffer;
  char chunk[4096];

  for (;;) {
    const ssize_t got = read(fd, chunk, sizeof(chunk));
    if (got < 0 && errno == EINTR) {
      continue;
    }
    if (got <= 0) {
      return;
    }
    buffer.append(chunk, static_cast<std::size_t>(got));

    for (std::size_t newline = buffer.find('\n'); newline != std::string::npos;
         newline = buffer.find('\n')) {
      const std::string line = buffer.substr(0, newline);
      buffer.erase(0, newline + 1);
      if (!respond(fd, line, commands) || commands.quitRequested()) {
        return;
      }
    }

    if (buffer.size() > kMaxRequestBytes) {
      return;
    }
  }
}

}  // namespace

bool serve(const std::filesystem::path& socketPath, Commands& commands,
           const std::function<void()>& onReady, std::string& error) {
  if (!socketPathFits(socketPath, error)) {
    return false;
  }
  if (std::filesystem::exists(socketPath)) {
    if (alreadyRunning(socketPath)) {
      error = "fuwa is already running on " + socketPath.string();
      return false;
    }
    // 前のプロセスが消し残したもの。
    std::filesystem::remove(socketPath);
  }

  std::error_code ignored;
  std::filesystem::create_directories(socketPath.parent_path(), ignored);

  const int listener = socket(AF_UNIX, SOCK_STREAM, 0);
  if (listener < 0) {
    error = "cannot create the socket";
    return false;
  }

  sockaddr_un address{};
  fillAddress(address, socketPath);
  if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    error = "cannot bind " + socketPath.string() + ": " + std::strerror(errno);
    close(listener);
    return false;
  }
  if (listen(listener, 4) != 0) {
    error = "cannot listen on " + socketPath.string();
    close(listener);
    std::filesystem::remove(socketPath);
    return false;
  }

  // 相手が先に切ったソケットへの書き込みで死なないようにする。
  std::signal(SIGPIPE, SIG_IGN);
  catchSignal(SIGINT);
  catchSignal(SIGTERM);

  if (onReady) {
    onReady();
  }

  while (gStopped == 0 && !commands.quitRequested()) {
    const int client = accept(listener, nullptr, nullptr);
    if (client < 0) {
      if (errno == EINTR) {
        continue;
      }
      error = "cannot accept a connection: " + std::string(std::strerror(errno));
      close(listener);
      std::filesystem::remove(socketPath);
      return false;
    }
    serveConnection(client, commands);
    close(client);
  }

  close(listener);
  std::filesystem::remove(socketPath);
  return true;
}

}  // namespace fuwa::api
