#include "audio/level.h"

#include <cmath>
#include <cstddef>

namespace fuwa::audio {

Level measureLevel(const std::vector<std::vector<float>>& channels) {
  Level level;
  double sum = 0.0;
  std::size_t count = 0;
  for (const auto& channel : channels) {
    for (float sample : channel) {
      level.peak = std::max(level.peak, std::abs(sample));
      sum += static_cast<double>(sample) * sample;
      ++count;
    }
  }
  level.rms = count > 0 ? std::sqrt(sum / static_cast<double>(count)) : 0.0;
  return level;
}

}  // namespace fuwa::audio
