#include "engine/player.h"

#include <cmath>
#include <cstdio>
#include <numbers>

#include "check.h"

namespace {

// 鳴っているノートの数だけ振幅が増える偽の音源。
// プラグインを読み込まずにスケジューリングだけを確かめられる。
class CountingInstrument final : public fuwa::plugin::Instrument {
 public:
  bool prepare(double, std::int32_t, std::string&) override { return true; }

  void render(const fuwa::plugin::NoteEvent* events, std::size_t eventCount, float* const* out,
              std::int32_t channelCount, std::int32_t frameCount) override {
    std::size_t next = 0;
    for (std::int32_t frame = 0; frame < frameCount; ++frame) {
      while (next < eventCount && events[next].sampleOffset <= frame) {
        sounding_ += events[next].on ? 1 : -1;
        CHECK(sounding_ >= 0);
        ++next;
      }
      for (std::int32_t ch = 0; ch < channelCount; ++ch) {
        out[ch][frame] = static_cast<float>(sounding_);
      }
    }
    CHECK(next == eventCount);
    totalFrames_ += frameCount;
  }

  std::int32_t outputChannelCount() const override { return 2; }
  const std::string& name() const override { return name_; }

  int sounding() const { return sounding_; }
  std::int64_t totalFrames() const { return totalFrames_; }

 private:
  std::string name_ = "counting";
  int sounding_ = 0;
  std::int64_t totalFrames_ = 0;
};

}  // namespace

int main() {
  CountingInstrument instrument;

  fuwa::engine::Schedule schedule;
  schedule.bpm = 120.0;  // 1 拍 = 0.5 秒
  schedule.notes.push_back({0.0, 1.0, 60, 1.0f});
  schedule.notes.push_back({1.0, 1.0, 64, 1.0f});

  fuwa::engine::RenderSettings settings;
  settings.sampleRate = 48000.0;
  settings.blockSize = 512;
  settings.tailSeconds = 0.5;

  fuwa::audio::BufferSink sink;
  fuwa::engine::renderAll(instrument, schedule, settings, sink);

  // 2 拍 = 1 秒、それに余韻 0.5 秒。ブロック境界の丸めがあるので幅を持たせる。
  CHECK(sink.frameCount() >= 48000 + 24000 - 512);
  CHECK(sink.frameCount() <= 48000 + 24000 + 512);
  CHECK(sink.channels().size() == 2);

  // 全てのノートが閉じていること
  CHECK(instrument.sounding() == 0);

  const auto& left = sink.channels()[0];

  // 0.25 秒地点では 1 音鳴っている
  CHECK(std::abs(left[12000] - 1.0f) < 1e-6f);
  // 0.75 秒地点でも 1 音（最初のノートは 0.5 秒で切れ、次が鳴っている）
  CHECK(std::abs(left[36000] - 1.0f) < 1e-6f);
  // 余韻の部分は無音
  CHECK(std::abs(left[static_cast<std::size_t>(sink.frameCount()) - 1]) < 1e-6f);

  // ノートが無ければ余韻だけ
  fuwa::audio::BufferSink empty;
  CountingInstrument silent;
  fuwa::engine::renderAll(silent, {}, settings, empty);
  CHECK(empty.frameCount() > 0);

  std::printf("ok\n");
  return 0;
}
