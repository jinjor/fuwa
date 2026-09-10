#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "api/protocol.h"

namespace fuwa::api {

// argv をそのままサーバへ渡し、返事をもらう。
// ここもコマンドの意味を知らない。知っているのは api/command だけ。
bool send(const std::filesystem::path& socketPath, const std::vector<std::string>& args,
          Response& response, std::string& error);

}  // namespace fuwa::api
