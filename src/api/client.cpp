#include "api/client.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

#include "api/socket.h"

namespace fuwa::api {
namespace {

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

}  // namespace

bool send(const std::filesystem::path& socketPath, const std::vector<std::string>& args,
          Response& response, std::string& error) {
  if (!socketPathFits(socketPath, error)) {
    return false;
  }

  const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    error = "cannot create the socket";
    return false;
  }

  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  std::snprintf(address.sun_path, sizeof(address.sun_path), "%s", socketPath.c_str());

  if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    // 繋げない理由を確かめずに「起動していない」と言わないこと。サンドボックスの中から
    // 呼ばれると弾かれるが、そのときサーバーは動いている。区別しないと、呼んだ側が
    // 動いているサーバーを止めて立て直そうとする。
    const int reason = errno;
    close(fd);
    if (reason == ENOENT || reason == ECONNREFUSED) {
      error = "fuwa is not running; start it with `fuwa serve`";
    } else if (reason == EACCES || reason == EPERM) {
      error = "cannot reach fuwa at " + socketPath.string() + ": " + std::strerror(reason) +
              "\nfuwa may well be running. this looks like a sandbox or a permission problem,"
              " not a stopped server";
    } else {
      error = "cannot reach fuwa at " + socketPath.string() + ": " + std::strerror(reason);
    }
    return false;
  }

  if (!writeAll(fd, encodeRequest(args) + "\n")) {
    error = "cannot send the command";
    close(fd);
    return false;
  }
  // これ以上送らないと伝える。サーバはここで読み終わる。
  shutdown(fd, SHUT_WR);

  std::string received;
  char chunk[4096];
  for (;;) {
    const ssize_t got = read(fd, chunk, sizeof(chunk));
    if (got < 0 && errno == EINTR) {
      continue;
    }
    if (got <= 0) {
      break;
    }
    received.append(chunk, static_cast<std::size_t>(got));
    if (received.find('\n') != std::string::npos) {
      break;
    }
  }
  close(fd);

  if (received.empty()) {
    error = "fuwa closed the connection without answering";
    return false;
  }
  const std::size_t newline = received.find('\n');
  return decodeResponse(received.substr(0, newline), response, error);
}

}  // namespace fuwa::api
