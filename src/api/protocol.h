#pragma once

#include <string>
#include <string_view>
#include <vector>

// 線の上でやりとりする形式。
//
//   リクエスト  ["notes","set","main","17-20","phrase.mid"]
//   レスポンス  {"code":0,"text":"replaced 4 bars"}
//
// リクエストは argv をそのまま入れた JSON の配列。CLI はコマンドの意味を知らず、
// 受け取った引数を運ぶだけ。解釈するのは受け取った側の 1 箇所（api/command）。
//
// どちらも 1 行 1 メッセージ。JSON は文字列の中に生の改行を許さない（必ずエスケープ
// される）ので、改行で区切るだけで曖昧さなく分けられる。長さを前置する必要はない。
namespace fuwa::api {

struct Response {
  int code = 0;
  std::string text;
};

std::string encodeRequest(const std::vector<std::string>& args);
bool decodeRequest(std::string_view line, std::vector<std::string>& args, std::string& error);

std::string encodeResponse(const Response& response);
bool decodeResponse(std::string_view line, Response& response, std::string& error);

}  // namespace fuwa::api
