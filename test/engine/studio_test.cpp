#include "engine/studio.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "check.h"

// ノートを置いてから音が出るまでの一続きを、実在の音源（SDK 同梱の mda）で確かめる。
// IPC を通さないので、鳴らない原因が音の側かサーバの側かを切り分けられる。
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

fuwa::model::Range bars(const fuwa::model::Project& project, int first, int last) {
  fuwa::model::Range range;
  range.track = project.tracks().front().id;
  range.firstBar = first;
  range.lastBar = last;
  return range;
}

}  // namespace

int main(int argc, char** argv) {
  CHECK(argc >= 2);
  const std::filesystem::path bundle = argv[1];

  fuwa::model::Project project;
  fuwa::engine::Studio studio;
  std::string error;

  // 音源を差す前は鳴らせない。
  {
    fuwa::audio::BufferSink sink;
    CHECK(!studio.render(project, 1, sink, error));
    CHECK(!error.empty());
  }

  const std::vector<std::string> names = studio.listInstruments(bundle, error);
  CHECK(error.empty());
  CHECK(!names.empty());
  CHECK(studio.setInstrument(project.tracks().front().id, bundle, names.front(), error));
  CHECK(studio.instrumentName(project.tracks().front().id) == names.front());

  // ノートが無ければ鳴らすものが無い。
  {
    fuwa::audio::BufferSink sink;
    CHECK(!studio.render(project, 1, sink, error));
  }

  CHECK(project.replaceRange(bars(project, 5, 6), {{0.0, 1.0, 60, 0.9f}, {2.0, 1.0, 67, 0.9f}}).ok);

  // 5 小節目から鳴らす。頭のノートが最初から鳴るので、すぐに音が立つ。
  float peakFromFive = 0.0f;
  {
    fuwa::audio::BufferSink sink;
    CHECK(studio.render(project, 5, sink, error));
    CHECK(sink.frameCount() > 0);
    peakFromFive = peakOf(sink.channels());
    CHECK(peakFromFive > 0.0f);
  }

  // 1 小節目から鳴らすと、頭に 4 小節ぶんの無音が付く。
  {
    fuwa::audio::BufferSink sink;
    CHECK(studio.render(project, 1, sink, error));
    // 120 BPM の 4/4 で 4 小節は 8 秒。48000Hz なので 8 秒ぶん以上長い。
    CHECK(sink.frameCount() > 8 * 48000);
    CHECK(std::abs(peakOf(sink.channels()) - peakFromFive) < 0.01f);
  }

  // 7 小節目から鳴らすものは無い。
  {
    fuwa::audio::BufferSink sink;
    CHECK(!studio.render(project, 7, sink, error));
  }

  std::printf("studio ok\n");
  return 0;
}
