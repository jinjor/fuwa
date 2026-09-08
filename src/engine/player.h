#pragma once

#include <cstdint>
#include <vector>

#include "audio/sink.h"
#include "model/note.h"
#include "plugin/instrument.h"

// モデルの音をプラグインに通して出口へ流す。
namespace fuwa::engine {

struct Schedule {
  std::vector<model::Note> notes;
  double bpm = 120.0;
};

struct RenderSettings {
  double sampleRate = 48000.0;
  std::int32_t blockSize = 512;
  double tailSeconds = 1.0;  // 最後のノートが終わってからも録り続ける長さ
};

// schedule を頭から終わりまで鳴らし、sink に書く。
void renderAll(plugin::Instrument& instrument, const Schedule& schedule,
               const RenderSettings& settings, audio::Sink& sink);

}  // namespace fuwa::engine
