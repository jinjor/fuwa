#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace fuwa::audio {

// 既定の出力デバイス。あらかじめ用意した音を流す。start() はすぐ戻る。
//
// プラグインをリアルタイムスレッドで回すのはまだ先。リアルタイムコールバックの中で
// やるのは生のポインタからの書き写しだけなので、確保もロックも起きない。
class Device {
 public:
  Device();
  ~Device();

  Device(const Device&) = delete;
  Device& operator=(const Device&) = delete;

  // channels は channelCount 本のポインタの配列。配列そのものも、指している先も、
  // stop() が戻るまで生かしておくこと。鳴っている最中に呼べば鳴らし直す。
  bool start(const float* const* channels, std::int32_t channelCount, std::int64_t frameCount,
             double sampleRate, std::string& error);

  // 戻った時点でコールバックは止まっている。鳴っていなければ何もしない。
  void stop();

  // 最後まで鳴り切ったら false になる。
  bool playing() const;

  // 再生済みのフレーム数。
  std::int64_t position() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace fuwa::audio
