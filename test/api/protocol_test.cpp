#include "api/protocol.h"

#include <cstdio>

#include "check.h"

// 線の上の形式。argv に入りうるものが往復して壊れないことを確かめる。
namespace {

void roundTrip(const std::vector<std::string>& args) {
  const std::string line = fuwa::api::encodeRequest(args);
  CHECK(line.find('\n') == std::string::npos);  // 1 行 1 メッセージが成り立つこと

  std::vector<std::string> back;
  std::string error;
  CHECK(fuwa::api::decodeRequest(line, back, error));
  CHECK(error.empty());
  CHECK(back == args);
}

}  // namespace

int main() {
  roundTrip({});
  roundTrip({"tracks"});
  roundTrip({"notes", "set", "main", "17-20", "/tmp/a b.mid"});

  // argv に入りうる厄介なもの。改行が入っても行は割れない。
  roundTrip({"prompt", "line one\nline two"});
  roundTrip({"prompt", "\"quoted\" and \\backslash\\"});
  roundTrip({"prompt", "タブ\tと制御文字\x01"});
  roundTrip({"prompt", "", "after an empty one"});
  roundTrip({"prompt", "絵文字 🎹 も通る"});

  // 読めないもの。
  {
    std::vector<std::string> args;
    std::string error;
    CHECK(!fuwa::api::decodeRequest("", args, error));
    CHECK(!fuwa::api::decodeRequest("not json", args, error));
    CHECK(!fuwa::api::decodeRequest("{\"a\":1}", args, error));      // 配列でない
    CHECK(!fuwa::api::decodeRequest("[\"play\",17]", args, error));  // 文字列でない要素
    CHECK(args.empty());
    CHECK(!error.empty());
  }

  // 返事。
  {
    const fuwa::api::Response sent{3, "something\nwith \"quotes\""};
    const std::string line = fuwa::api::encodeResponse(sent);
    CHECK(line.find('\n') == std::string::npos);

    fuwa::api::Response back;
    std::string error;
    CHECK(fuwa::api::decodeResponse(line, back, error));
    CHECK(back.code == sent.code);
    CHECK(back.text == sent.text);

    CHECK(fuwa::api::decodeResponse(fuwa::api::encodeResponse({-1, ""}), back, error));
    CHECK(back.code == -1);
    CHECK(back.text.empty());

    CHECK(!fuwa::api::decodeResponse("{\"text\":\"no code\"}", back, error));
    CHECK(!fuwa::api::decodeResponse("[]", back, error));
    CHECK(!fuwa::api::decodeResponse("{\"code\":\"zero\"}", back, error));
  }

  std::printf("protocol ok\n");
  return 0;
}
