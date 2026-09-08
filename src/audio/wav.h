#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fuwa::audio {

// 16bit PCM の WAV を書く。channels は [チャンネル][フレーム]。
bool writeWav(const std::filesystem::path& path, const std::vector<std::vector<float>>& channels,
              double sampleRate, std::string& error);

}  // namespace fuwa::audio
