#pragma once

#include <filesystem>
#include <functional>
#include <string>

#include "api/command.h"

namespace fuwa::api {

// Unix domain socket で 1 行 1 リクエストを受ける。形式は api/protocol.h。
//
// 一度に相手をするのは 1 接続。エージェントも UI も待たせて困る速さではないし、
// 曲の状態を複数のスレッドから触らせないほうが安い。
// 再生はデバイスが自前のスレッドで回すので、play はすぐ戻る。
//
// quit を受けるか、SIGINT / SIGTERM が来るまで戻らない。
// onReady は待ち受けが立った後に一度だけ呼ばれる。立つ前に失敗したら呼ばれない。
bool serve(const std::filesystem::path& socketPath, Commands& commands,
           const std::function<void()>& onReady, std::string& error);

}  // namespace fuwa::api
