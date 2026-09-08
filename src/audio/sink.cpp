#include "audio/sink.h"

namespace fuwa::audio {

void BufferSink::write(const float* const* channels, std::int32_t channelCount,
                       std::int32_t frameCount) {
  if (channels_.size() < static_cast<std::size_t>(channelCount)) {
    channels_.resize(static_cast<std::size_t>(channelCount));
  }
  for (std::int32_t ch = 0; ch < channelCount; ++ch) {
    auto& dst = channels_[static_cast<std::size_t>(ch)];
    dst.insert(dst.end(), channels[ch], channels[ch] + frameCount);
  }
}

std::int32_t BufferSink::frameCount() const {
  return channels_.empty() ? 0 : static_cast<std::int32_t>(channels_.front().size());
}

}  // namespace fuwa::audio
