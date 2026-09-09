#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "audio/device.h"
#include "audio/sink.h"
#include "audio/wav.h"
#include "engine/player.h"
#include "model/note.h"
#include "plugin/vst3.h"

// Step 0 の動作確認用。VST3 の音源を読み込み、決め打ちのノートを鳴らして WAV に書く。
// 耳で確かめられないので、書き出しと同時にピークと実効値を報告する。
namespace {

fuwa::engine::Schedule demoSchedule() {
  fuwa::engine::Schedule schedule;
  schedule.bpm = 120.0;
  const std::int16_t pitches[] = {60, 62, 64, 65, 67};
  double beat = 0.0;
  for (std::int16_t pitch : pitches) {
    schedule.notes.push_back({beat, 0.5, pitch, 0.8f});
    beat += 0.5;
  }
  return schedule;
}

struct Level {
  float peak = 0.0f;
  double rms = 0.0;
};

Level measure(const std::vector<std::vector<float>>& channels) {
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

int listClasses(const std::filesystem::path& path) {
  std::string error;
  const auto classes = fuwa::plugin::vst3::listClasses(path, error);
  if (classes.empty()) {
    std::fprintf(stderr, "クラスを読めない: %s\n", error.c_str());
    return 1;
  }
  for (const auto& info : classes) {
    std::printf("%-32s %-40s %s\n", info.name.c_str(), info.subCategories.c_str(),
                info.isInstrument ? "instrument" : "effect");
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr,
                 "usage:\n"
                 "  %s [--play] <plugin.vst3> [out.wav] [class-name]\n"
                 "  %s --list <plugin.vst3>\n",
                 argv[0], argv[0]);
    return 2;
  }

  if (std::string(argv[1]) == "--list") {
    if (argc < 3) {
      std::fprintf(stderr, "--list にはプラグインのパスが要る\n");
      return 2;
    }
    return listClasses(argv[2]);
  }

  int arg = 1;
  bool playAfter = false;
  if (std::string(argv[arg]) == "--play") {
    playAfter = true;
    ++arg;
  }
  if (arg >= argc) {
    std::fprintf(stderr, "プラグインのパスが要る\n");
    return 2;
  }

  const std::filesystem::path pluginPath = argv[arg];
  const std::filesystem::path outPath = arg + 1 < argc ? argv[arg + 1] : "out.wav";
  const std::string className = arg + 2 < argc ? argv[arg + 2] : "";

  std::string error;
  auto instrument = fuwa::plugin::vst3::load(pluginPath, className, error);
  if (!instrument) {
    std::fprintf(stderr, "読み込めない: %s\n", error.c_str());
    return 1;
  }

  const fuwa::engine::RenderSettings settings;
  if (!instrument->prepare(settings.sampleRate, settings.blockSize, error)) {
    std::fprintf(stderr, "準備できない: %s\n", error.c_str());
    return 1;
  }

  fuwa::audio::BufferSink sink;
  fuwa::engine::renderAll(*instrument, demoSchedule(), settings, sink);

  const Level level = measure(sink.channels());
  std::printf("plugin   %s\n", instrument->name().c_str());
  std::printf("channels %d\n", instrument->outputChannelCount());
  std::printf("frames   %d\n", sink.frameCount());
  std::printf("peak     %.6f\n", static_cast<double>(level.peak));
  std::printf("rms      %.6f\n", level.rms);

  if (!fuwa::audio::writeWav(outPath, sink.channels(), settings.sampleRate, error)) {
    std::fprintf(stderr, "書き出せない: %s\n", error.c_str());
    return 1;
  }
  std::printf("wrote    %s\n", outPath.string().c_str());

  if (level.peak <= 0.0f) {
    std::fprintf(stderr, "無音だった\n");
    return 1;
  }

  if (playAfter) {
    std::printf("playing...\n");
    if (!fuwa::audio::play(sink.channels(), settings.sampleRate, error)) {
      std::fprintf(stderr, "再生できない: %s\n", error.c_str());
      return 1;
    }
  }
  return 0;
}
