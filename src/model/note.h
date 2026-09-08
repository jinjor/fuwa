#pragma once

#include <cstdint>

// 曲のデータ。再生の都合もプラグインの都合も知らない。
namespace fuwa::model {

struct Note {
  double startBeat;
  double lengthBeats;
  std::int16_t pitch;  // 0-127
  float velocity;      // 0.0-1.0
};

}  // namespace fuwa::model
