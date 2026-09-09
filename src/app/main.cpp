#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "audio/device.h"
#include "audio/level.h"
#include "audio/sink.h"
#include "audio/wav.h"
#include "engine/player.h"
#include "model/note.h"
#include "plugin/vst3.h"

// Step 0 の動作確認用。VST3 の音源を読み込み、決め打ちのノートを鳴らして WAV に書く。
// 耳で確かめられないので、書き出しと同時にピークと実効値を報告する。
namespace {

struct Command {
  enum class Kind { Usage, List, Render };

  Kind kind = Kind::Usage;
  std::filesystem::path pluginPath;
  std::filesystem::path outPath = "out.wav";
  std::string className;  // 空なら最初のインストゥルメント
  bool playAfter = false;
};

Command parse(const std::vector<std::string>& args) {
  Command command;
  if (args.empty()) {
    return command;
  }

  if (args[0] == "--list") {
    if (args.size() != 2) {
      return command;
    }
    command.kind = Command::Kind::List;
    command.pluginPath = args[1];
    return command;
  }

  std::size_t at = 0;
  if (args[at] == "--play") {
    command.playAfter = true;
    ++at;
  }
  if (at >= args.size()) {
    return command;
  }

  command.kind = Command::Kind::Render;
  command.pluginPath = args[at];
  if (at + 1 < args.size()) {
    command.outPath = args[at + 1];
  }
  if (at + 2 < args.size()) {
    command.className = args[at + 2];
  }
  return command;
}

int usage(const char* program) {
  std::fprintf(stderr,
               "usage:\n"
               "  %s [--play] <plugin.vst3> [out.wav] [class-name]\n"
               "  %s --list <plugin.vst3>\n",
               program, program);
  return 2;
}

fuwa::engine::Schedule demoSchedule() {
  fuwa::engine::Schedule schedule;
  schedule.tempo = fuwa::model::Tempo(120.0);
  const std::int16_t pitches[] = {60, 62, 64, 65, 67};
  double beat = 0.0;
  for (std::int16_t pitch : pitches) {
    schedule.notes.push_back({beat, 0.5, pitch, 0.8f});
    beat += 0.5;
  }
  return schedule;
}

int listClasses(const std::filesystem::path& pluginPath) {
  std::string error;
  const auto classes = fuwa::plugin::vst3::listClasses(pluginPath, error);
  if (classes.empty()) {
    std::fprintf(stderr, "cannot read the classes: %s\n", error.c_str());
    return 1;
  }
  for (const auto& info : classes) {
    std::printf("%-32s %-40s %s\n", info.name.c_str(), info.subCategories.c_str(),
                info.isInstrument ? "instrument" : "effect");
  }
  return 0;
}

int render(const Command& command) {
  std::string error;
  auto instrument = fuwa::plugin::vst3::load(command.pluginPath, command.className, error);
  if (!instrument) {
    std::fprintf(stderr, "cannot load: %s\n", error.c_str());
    return 1;
  }

  const fuwa::engine::RenderSettings settings;
  if (!instrument->prepare(settings.sampleRate, settings.blockSize, error)) {
    std::fprintf(stderr, "cannot prepare: %s\n", error.c_str());
    return 1;
  }

  fuwa::audio::BufferSink sink;
  fuwa::engine::renderAll(*instrument, demoSchedule(), settings, sink);

  const fuwa::audio::Level level = fuwa::audio::measureLevel(sink.channels());
  std::printf("plugin   %s\n", instrument->name().c_str());
  std::printf("channels %d\n", instrument->outputChannelCount());
  std::printf("frames   %d\n", sink.frameCount());
  std::printf("peak     %.6f\n", static_cast<double>(level.peak));
  std::printf("rms      %.6f\n", level.rms);

  if (!fuwa::audio::writeWav(command.outPath, sink.channels(), settings.sampleRate, error)) {
    std::fprintf(stderr, "cannot write: %s\n", error.c_str());
    return 1;
  }
  std::printf("wrote    %s\n", command.outPath.string().c_str());

  if (level.peak <= 0.0f) {
    std::fprintf(stderr, "the output was silent\n");
    return 1;
  }

  if (command.playAfter) {
    std::printf("playing...\n");
    if (!fuwa::audio::play(sink.channels(), settings.sampleRate, error)) {
      std::fprintf(stderr, "cannot play: %s\n", error.c_str());
      return 1;
    }
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  const Command command = parse(std::vector<std::string>(argv + 1, argv + argc));
  switch (command.kind) {
    case Command::Kind::List:
      return listClasses(command.pluginPath);
    case Command::Kind::Render:
      return render(command);
    case Command::Kind::Usage:
      break;
  }
  return usage(argv[0]);
}
