#include "audio/wav.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>

namespace fuwa::audio {
namespace {

// RIFF はリトルエンディアン固定。
void putU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
  out.push_back(static_cast<std::uint8_t>(v));
  out.push_back(static_cast<std::uint8_t>(v >> 8));
  out.push_back(static_cast<std::uint8_t>(v >> 16));
  out.push_back(static_cast<std::uint8_t>(v >> 24));
}

void putU16(std::vector<std::uint8_t>& out, std::uint16_t v) {
  out.push_back(static_cast<std::uint8_t>(v));
  out.push_back(static_cast<std::uint8_t>(v >> 8));
}

void putTag(std::vector<std::uint8_t>& out, const char (&tag)[5]) {
  out.insert(out.end(), tag, tag + 4);
}

std::int16_t toPcm16(float sample) {
  const float clamped = std::clamp(sample, -1.0f, 1.0f);
  return static_cast<std::int16_t>(std::lround(clamped * 32767.0f));
}

}  // namespace

bool writeWav(const std::filesystem::path& path, const std::vector<std::vector<float>>& channels,
              double sampleRate, std::string& error) {
  if (channels.empty()) {
    error = "チャンネルがない";
    return false;
  }

  const auto channelCount = static_cast<std::uint16_t>(channels.size());
  const auto frameCount = static_cast<std::uint32_t>(channels.front().size());
  const std::uint16_t bytesPerSample = 2;
  const std::uint32_t dataBytes = frameCount * channelCount * bytesPerSample;

  std::vector<std::uint8_t> out;
  out.reserve(44 + dataBytes);

  putTag(out, "RIFF");
  putU32(out, 36 + dataBytes);
  putTag(out, "WAVE");

  putTag(out, "fmt ");
  putU32(out, 16);                                          // PCM のチャンクサイズ
  putU16(out, 1);                                           // PCM
  putU16(out, channelCount);
  putU32(out, static_cast<std::uint32_t>(sampleRate));
  putU32(out, static_cast<std::uint32_t>(sampleRate) * channelCount * bytesPerSample);
  putU16(out, static_cast<std::uint16_t>(channelCount * bytesPerSample));
  putU16(out, bytesPerSample * 8);

  putTag(out, "data");
  putU32(out, dataBytes);

  for (std::uint32_t frame = 0; frame < frameCount; ++frame) {
    for (std::uint16_t ch = 0; ch < channelCount; ++ch) {
      const auto& source = channels[ch];
      const float sample = frame < source.size() ? source[frame] : 0.0f;
      const std::int16_t pcm = toPcm16(sample);
      out.push_back(static_cast<std::uint8_t>(pcm & 0xff));
      out.push_back(static_cast<std::uint8_t>((pcm >> 8) & 0xff));
    }
  }

  std::ofstream file(path, std::ios::binary);
  if (!file) {
    error = "書き込めない: " + path.string();
    return false;
  }
  file.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
  if (!file) {
    error = "書き込みに失敗: " + path.string();
    return false;
  }
  return true;
}

}  // namespace fuwa::audio
