#include "engine/player.h"

#include <algorithm>
#include <cmath>

namespace fuwa::engine {
namespace {

struct TimedEvent {
  std::int64_t frame;
  plugin::NoteEvent event;
};

// 拍から秒への変換はモデルが答える。ここが知っているのはサンプルレートだけ。
std::int64_t beatsToFrames(double beats, const model::Tempo& tempo, double sampleRate) {
  return static_cast<std::int64_t>(std::llround(tempo.secondsAt(beats) * sampleRate));
}

// ノート一覧を、フレーム位置の昇順に並んだ note on/off の列にする。
std::vector<TimedEvent> flatten(const Schedule& schedule, double sampleRate) {
  std::vector<TimedEvent> events;
  events.reserve(schedule.notes.size() * 2);

  for (const model::Note& note : schedule.notes) {
    const std::int64_t start = beatsToFrames(note.startBeat, schedule.tempo, sampleRate);
    const std::int64_t end =
        beatsToFrames(note.startBeat + note.lengthBeats, schedule.tempo, sampleRate);
    events.push_back({start, {0, note.pitch, note.velocity, true}});
    events.push_back({std::max(end, start + 1), {0, note.pitch, 0.0f, false}});
  }

  std::stable_sort(events.begin(), events.end(),
                   [](const TimedEvent& a, const TimedEvent& b) { return a.frame < b.frame; });
  return events;
}

}  // namespace

void renderAll(plugin::Instrument& instrument, const Schedule& schedule,
               const RenderSettings& settings, audio::Sink& sink) {
  const std::vector<TimedEvent> events = flatten(schedule, settings.sampleRate);
  const std::int64_t lastEvent = events.empty() ? 0 : events.back().frame;
  const std::int64_t total =
      lastEvent + static_cast<std::int64_t>(settings.tailSeconds * settings.sampleRate);

  const std::int32_t channelCount = instrument.outputChannelCount();
  std::vector<std::vector<float>> buffers(
      static_cast<std::size_t>(channelCount),
      std::vector<float>(static_cast<std::size_t>(settings.blockSize), 0.0f));
  std::vector<float*> pointers(static_cast<std::size_t>(channelCount));
  for (std::int32_t ch = 0; ch < channelCount; ++ch) {
    pointers[static_cast<std::size_t>(ch)] = buffers[static_cast<std::size_t>(ch)].data();
  }

  std::vector<plugin::NoteEvent> block;
  block.reserve(64);

  std::size_t next = 0;
  for (std::int64_t frame = 0; frame < total; frame += settings.blockSize) {
    const std::int32_t frames =
        static_cast<std::int32_t>(std::min<std::int64_t>(settings.blockSize, total - frame));

    block.clear();
    while (next < events.size() && events[next].frame < frame + frames) {
      plugin::NoteEvent event = events[next].event;
      event.sampleOffset = static_cast<std::int32_t>(events[next].frame - frame);
      block.push_back(event);
      ++next;
    }

    instrument.render(block.data(), block.size(), pointers.data(), channelCount, frames);
    sink.write(pointers.data(), channelCount, frames);
  }
}

}  // namespace fuwa::engine
