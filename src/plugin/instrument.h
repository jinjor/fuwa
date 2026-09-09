#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

// 上位層が音源に対して知っていることの全て。
// VST3 の型はこのヘッダには現れない。docs/architecture.md の「plugin」を参照。
namespace fuwa::plugin {

// 1 ブロックの中の相対位置で表したノートの出来事。
struct NoteEvent {
  std::int32_t sampleOffset;
  std::int16_t pitch;  // 0-127
  float velocity;      // 0.0-1.0
  bool on;
};

class Instrument {
 public:
  virtual ~Instrument() = default;

  Instrument(const Instrument&) = delete;
  Instrument& operator=(const Instrument&) = delete;

  // レンダリング前に一度呼ぶ。以降 maxBlockSize を超えるブロックは渡さない。
  virtual bool prepare(double sampleRate, std::int32_t maxBlockSize, std::string& error) = 0;

  // events は sampleOffset の昇順であること。
  // out は channelCount 本のバッファで、それぞれ frameCount 個分の書き込み先。
  virtual void render(const NoteEvent* events, std::size_t eventCount, float* const* out,
                      std::int32_t channelCount, std::int32_t frameCount) = 0;

  virtual std::int32_t outputChannelCount() const = 0;
  virtual const std::string& name() const = 0;

 protected:
  Instrument() = default;
};

}  // namespace fuwa::plugin
