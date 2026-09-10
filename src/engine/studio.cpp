#include "engine/studio.h"

#include <algorithm>
#include <cmath>
#include <map>

#include "engine/player.h"
#include "plugin/vst3.h"

namespace fuwa::engine {
namespace {

// 曲の最後のノートが鳴り終わってからも、余韻のぶん録り続ける。
constexpr double kTailSeconds = 1.5;

RenderSettings renderSettings() {
  RenderSettings settings;
  settings.tailSeconds = kTailSeconds;
  return settings;
}

// トラックのノートを、fromBeat を 0 とした再生用の並びに直す。
// fromBeat をまたいで鳴っているノートは、頭を切って途中から鳴らす。
std::vector<model::Note> notesFrom(const model::Track& track, double fromBeat) {
  std::vector<model::Note> notes;
  for (const model::Note& note : track.notes) {
    const double end = note.startBeat + note.lengthBeats;
    if (end <= fromBeat) {
      continue;
    }
    model::Note shifted = note;
    shifted.startBeat = std::max(0.0, note.startBeat - fromBeat);
    shifted.lengthBeats = end - fromBeat - shifted.startBeat;
    notes.push_back(shifted);
  }
  return notes;
}

void mixInto(std::vector<std::vector<float>>& into, const std::vector<std::vector<float>>& from) {
  if (into.size() < from.size()) {
    into.resize(from.size());
  }
  for (std::size_t channel = 0; channel < from.size(); ++channel) {
    std::vector<float>& target = into[channel];
    const std::vector<float>& source = from[channel];
    if (target.size() < source.size()) {
      target.resize(source.size(), 0.0f);
    }
    for (std::size_t i = 0; i < source.size(); ++i) {
      target[i] += source[i];
    }
  }
}

// 長さの違うトラックを混ぜた後は、短いチャンネルを無音で埋めて揃える。
void padToSameLength(std::vector<std::vector<float>>& channels) {
  std::size_t longest = 0;
  for (const std::vector<float>& channel : channels) {
    longest = std::max(longest, channel.size());
  }
  for (std::vector<float>& channel : channels) {
    channel.resize(longest, 0.0f);
  }
}

float peakOf(const std::vector<std::vector<float>>& channels) {
  float peak = 0.0f;
  for (const std::vector<float>& channel : channels) {
    for (float sample : channel) {
      peak = std::max(peak, std::abs(sample));
    }
  }
  return peak;
}

}  // namespace

// 音源はトラックごとに 1 つ。VST3 の型はここまでで、ヘッダには出さない。
struct Studio::Instruments {
  std::map<model::TrackId, std::unique_ptr<plugin::Instrument>> byTrack;
};

Studio::Studio() : instruments_(std::make_unique<Instruments>()) {}

Studio::~Studio() {
  // 音を止めてから音源を捨てる。順序を間違えると鳴っている最中に足元が消える。
  device_.stop();
}

std::vector<std::string> Studio::listInstruments(const std::filesystem::path& bundle,
                                                 std::string& error) const {
  error.clear();
  std::vector<std::string> names;
  for (const plugin::vst3::ClassInfo& info : plugin::vst3::listClasses(bundle, error)) {
    if (info.isInstrument) {
      names.push_back(info.name);
    }
  }
  return names;
}

bool Studio::setInstrument(model::TrackId track, const std::filesystem::path& bundle,
                           std::string_view className, std::string& error) {
  auto instrument = plugin::vst3::load(bundle, className, error);
  if (!instrument) {
    return false;
  }

  const RenderSettings settings = renderSettings();
  if (!instrument->prepare(settings.sampleRate, settings.blockSize, error)) {
    return false;
  }

  // 差し替える前に止める。鳴っている音は今の音源のものなので、残しておけない。
  stop();
  instruments_->byTrack[track] = std::move(instrument);
  return true;
}

std::string Studio::instrumentName(model::TrackId track) const {
  const auto found = instruments_->byTrack.find(track);
  if (found == instruments_->byTrack.end()) {
    return {};
  }
  return found->second->name();
}

bool Studio::render(const model::Project& project, int fromBar, audio::Sink& sink,
                    std::string& error) {
  if (fromBar < 1) {
    error = "the bar number starts at 1";
    return false;
  }
  if (instruments_->byTrack.empty()) {
    error = "no instrument is loaded; run `fuwa instrument` first";
    return false;
  }

  const RenderSettings settings = renderSettings();
  const double fromBeat = project.meter().beatsAtBar(fromBar);

  std::vector<std::vector<float>> mixed;
  int played = 0;
  for (const model::Track& track : project.tracks()) {
    const auto found = instruments_->byTrack.find(track.id);
    if (found == instruments_->byTrack.end()) {
      continue;
    }
    Schedule schedule;
    schedule.tempo = project.tempo();
    schedule.notes = notesFrom(track, fromBeat);
    if (schedule.notes.empty()) {
      continue;
    }

    audio::BufferSink buffer;
    renderAll(*found->second, schedule, settings, buffer);
    mixInto(mixed, buffer.channels());
    ++played;
  }

  if (played == 0) {
    error = "there is nothing to play from that bar";
    return false;
  }

  padToSameLength(mixed);

  std::vector<const float*> pointers;
  pointers.reserve(mixed.size());
  for (const std::vector<float>& channel : mixed) {
    pointers.push_back(channel.data());
  }
  sink.write(pointers.data(), static_cast<std::int32_t>(mixed.size()),
             static_cast<std::int32_t>(mixed.front().size()));
  return true;
}

bool Studio::play(const model::Project& project, int fromBar, std::string& error) {
  audio::BufferSink buffer;
  if (!render(project, fromBar, buffer, error)) {
    return false;
  }

  // 鳴っている音を止めてから差し替える。Device はバッファを借りるだけなので、
  // 止まるまで前の中身を捨てられない。
  device_.stop();

  playing_ = buffer.channels();
  sampleRate_ = renderSettings().sampleRate;
  peak_ = peakOf(playing_);

  pointers_.clear();
  pointers_.reserve(playing_.size());
  for (const std::vector<float>& channel : playing_) {
    pointers_.push_back(channel.data());
  }

  return device_.start(pointers_.data(), static_cast<std::int32_t>(playing_.size()),
                       static_cast<std::int64_t>(playing_.empty() ? 0 : playing_.front().size()),
                       sampleRate_, error);
}

void Studio::stop() { device_.stop(); }

bool Studio::playing() const { return device_.playing(); }

std::int64_t Studio::renderedFrames() const {
  return static_cast<std::int64_t>(playing_.empty() ? 0 : playing_.front().size());
}

float Studio::renderedPeak() const { return peak_; }

double Studio::positionSeconds() const {
  return static_cast<double>(device_.position()) / sampleRate_;
}

double Studio::sampleRate() const { return sampleRate_; }

}  // namespace fuwa::engine
