#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "api/protocol.h"
#include "engine/studio.h"
#include "model/project.h"

// fuwa が受け取れる操作の全て。argv を解釈するのはここだけで、CLI もサーバも
// 中身を知らない。UI が付いても同じここを通る（特権的な経路を作らない）。
namespace fuwa::api {

class Commands {
 public:
  Commands(model::Project& project, engine::Studio& studio);

  Response run(const std::vector<std::string>& args);

  // quit を受けたら true。サーバはこれを見て店じまいする。
  bool quitRequested() const { return quit_; }

 private:
  Response help() const;
  Response status() const;
  Response tracks() const;
  Response plugins(const std::vector<std::string>& args);
  Response instrument(const std::vector<std::string>& args);
  Response notes(const std::vector<std::string>& args);
  Response play(const std::vector<std::string>& args);

  // 名前か id でトラックを引く。見つからなければ nullptr を返し、error を埋める。
  const model::Track* resolveTrack(std::string_view text, std::string& error) const;

  model::Project& project_;
  engine::Studio& studio_;
  bool quit_ = false;
};

}  // namespace fuwa::api
