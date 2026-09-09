#include "audio/wav.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

#include "check.h"

int main() {
  const std::filesystem::path path = std::filesystem::temp_directory_path() / "fuwa_wav_test.wav";
  std::filesystem::remove(path);

  // 2ch × 100 フレーム。左を +1.0、右を -1.0 で埋める。
  std::vector<std::vector<float>> channels(2, std::vector<float>(100, 0.0f));
  for (std::size_t i = 0; i < 100; ++i) {
    channels[0][i] = 1.0f;
    channels[1][i] = -1.0f;
  }

  std::string error;
  CHECK(fuwa::audio::writeWav(path, channels, 48000.0, error));
  CHECK(error.empty());

  std::ifstream file(path, std::ios::binary);
  CHECK(file);
  std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  // ヘッダ 44 バイト + 100 フレーム × 2ch × 2byte
  CHECK(bytes.size() == 44 + 100 * 2 * 2);
  CHECK(std::string(bytes.begin(), bytes.begin() + 4) == "RIFF");
  CHECK(std::string(bytes.begin() + 8, bytes.begin() + 12) == "WAVE");
  CHECK(std::string(bytes.begin() + 36, bytes.begin() + 40) == "data");

  // 最初のフレーム: 左 +32767、右 -32767
  const auto left = static_cast<std::int16_t>((static_cast<unsigned char>(bytes[45]) << 8) |
                                              static_cast<unsigned char>(bytes[44]));
  const auto right = static_cast<std::int16_t>((static_cast<unsigned char>(bytes[47]) << 8) |
                                               static_cast<unsigned char>(bytes[46]));
  CHECK(left == 32767);
  CHECK(right == -32767);

  // クリップされること
  channels[0][0] = 4.0f;
  CHECK(fuwa::audio::writeWav(path, channels, 48000.0, error));

  std::filesystem::remove(path);
  std::printf("ok\n");
  return 0;
}
