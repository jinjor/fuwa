#include "api/socket.h"

#include <sys/un.h>

#include <cstdlib>

namespace fuwa::api {

std::filesystem::path defaultSocketPath() {
  const char* fromEnv = std::getenv("FUWA_SOCKET");
  if (fromEnv != nullptr && *fromEnv != '\0') {
    return fromEnv;
  }
  const char* home = std::getenv("HOME");
  return std::filesystem::path(home != nullptr ? home : ".") / ".fuwa" / "fuwa.sock";
}

bool socketPathFits(const std::filesystem::path& path, std::string& error) {
  const sockaddr_un address{};
  if (path.string().size() >= sizeof(address.sun_path)) {
    error = "the socket path is too long: " + path.string();
    return false;
  }
  return true;
}

}  // namespace fuwa::api
