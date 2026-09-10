#pragma once

#include <cstdint>

// 曲のデータ。再生の都合もプラグインの都合も知らない。
namespace fuwa::model {

struct Note {
  double startBeat;  // 曲頭からの拍数（四分音符が 1）
  double lengthBeats;
  std::int16_t pitch;  // 0-127
  float velocity;      // 0.0-1.0
};

// 守るべき不変条件。これを満たさないノートはプロジェクトに入れない。
inline bool valid(const Note& note) {
  return note.startBeat >= 0.0 && note.lengthBeats > 0.0 && note.pitch >= 0 && note.pitch <= 127 &&
         note.velocity >= 0.0f && note.velocity <= 1.0f;
}

}  // namespace fuwa::model
