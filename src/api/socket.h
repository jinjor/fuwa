#pragma once

#include <filesystem>
#include <string>

// Unix domain socket の置き場所。サーバも CLI も同じ答えを見る必要がある。
namespace fuwa::api {

// FUWA_SOCKET があればそれ。無ければ ~/.fuwa/fuwa.sock。
std::filesystem::path defaultSocketPath();

// Unix domain socket のアドレスに入るパスは 100 バイト強しかない。
// 繋いでから謎の失敗をするより、先に弾いて理由を言う。
bool socketPathFits(const std::filesystem::path& path, std::string& error);

}  // namespace fuwa::api
