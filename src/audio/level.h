#pragma once

#include <vector>

namespace fuwa::audio {

// 信号を測った結果。指定した値ではなく、出てきた値。
// 指定する側（音量やパン）はモデルの住人で、こちらは違う。
struct Level {
  float peak = 0.0f;
  double rms = 0.0;
};

Level measureLevel(const std::vector<std::vector<float>>& channels);

}  // namespace fuwa::audio
