#include <cstdio>
#include <string>
#include <vector>

#include "api/client.h"
#include "api/command.h"
#include "api/server.h"
#include "api/socket.h"
#include "engine/studio.h"
#include "model/project.h"

// fuwa の入り口。
//
//   fuwa serve      サーバを建てて操作を待つ
//   fuwa <なにか>   それをそのままサーバへ渡し、返事を出す
//
// CLI はコマンドを解釈しない。argv を運ぶだけで、意味を知っているのはサーバ側の
// api/command だけ。ここで見ているのは「どちらの役をやるか」であって、コマンドではない。
namespace {

int serve() {
  fuwa::model::Project project;
  fuwa::engine::Studio studio;
  fuwa::api::Commands commands(project, studio);

  const std::filesystem::path socketPath = fuwa::api::defaultSocketPath();
  const auto announce = [&socketPath] {
    std::printf("listening on %s\n", socketPath.string().c_str());
    std::fflush(stdout);
  };

  std::string error;
  if (!fuwa::api::serve(socketPath, commands, announce, error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }
  return 0;
}

int forward(const std::vector<std::string>& args) {
  fuwa::api::Response response;
  std::string error;
  if (!fuwa::api::send(fuwa::api::defaultSocketPath(), args, response, error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }

  std::string text = response.text;
  if (!text.empty() && text.back() != '\n') {
    text.push_back('\n');
  }
  std::fputs(text.c_str(), response.code == 0 ? stdout : stderr);
  return response.code;
}

}  // namespace

int main(int argc, char** argv) {
  const std::vector<std::string> args(argv + 1, argv + argc);
  if (args.size() == 1 && args[0] == "serve") {
    return serve();
  }
  return forward(args);
}
