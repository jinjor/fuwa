#include "plugin/vst3.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "audio/sink.h"
#include "check.h"
#include "engine/player.h"

// SDK 同梱の mda を相手にホストが本当に音を出せているかを確かめる。
// mda はライセンスも外部依存も要らないので、どの環境でも同じ結果になる。
namespace {

float peakOf(const std::vector<std::vector<float>>& channels) {
  float peak = 0.0f;
  for (const auto& channel : channels) {
    for (float sample : channel) {
      peak = std::max(peak, std::abs(sample));
    }
  }
  return peak;
}

float render(const std::filesystem::path& bundle, const std::string& className,
             const fuwa::engine::Schedule& schedule) {
  std::string error;
  auto instrument = fuwa::plugin::vst3::load(bundle, className, error);
  CHECK(instrument != nullptr);
  CHECK(error.empty());

  fuwa::engine::RenderSettings settings;
  settings.tailSeconds = 0.25;
  CHECK(instrument->prepare(settings.sampleRate, settings.blockSize, error));
  CHECK(instrument->outputChannelCount() > 0);

  fuwa::audio::BufferSink sink;
  fuwa::engine::renderAll(*instrument, schedule, settings, sink);
  CHECK(sink.frameCount() > 0);
  return peakOf(sink.channels());
}

}  // namespace

int main(int argc, char** argv) {
  CHECK(argc >= 2);
  const std::filesystem::path bundle = argv[1];

  std::string error;
  const auto classes = fuwa::plugin::vst3::listClasses(bundle, error);
  CHECK(error.empty());
  CHECK(classes.size() >= 4);

  const auto instruments = std::count_if(classes.begin(), classes.end(),
                                         [](const auto& info) { return info.isInstrument; });
  CHECK(instruments >= 4);

  fuwa::engine::Schedule schedule;
  schedule.tempo = fuwa::model::Tempo(120.0);
  schedule.notes.push_back({0.0, 1.0, 60, 0.9f});

  // ノートを与えれば鳴る
  const float sounded = render(bundle, "mda JX10", schedule);
  CHECK(sounded > 0.01f);

  // ノートが無ければ鳴らない。鳴っていると「常に音が出ている」だけの
  // テストになってしまうので、こちらも確かめる。
  const float silent = render(bundle, "mda JX10", {});
  CHECK(silent < 1e-4f);

  // 名前を指定しなければインストゥルメントが選ばれる（エフェクトではなく）
  auto chosen = fuwa::plugin::vst3::load(bundle, "", error);
  CHECK(chosen != nullptr);
  CHECK(chosen->name().rfind("mda", 0) == 0);

  // 存在しない名前は失敗する
  auto missing = fuwa::plugin::vst3::load(bundle, "存在しない音源", error);
  CHECK(missing == nullptr);
  CHECK(!error.empty());

  std::printf("ok\n");
  return 0;
}
