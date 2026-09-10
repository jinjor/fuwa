#include "api/command.h"

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <span>
#include <sstream>

#include "model/midi.h"

namespace fuwa::api {
namespace {

constexpr int kOk = 0;
constexpr int kFailed = 1;
constexpr int kBadUsage = 2;

Response ok(std::string text) { return {kOk, std::move(text)}; }
Response failed(std::string text) { return {kFailed, std::move(text)}; }
Response badUsage(std::string text) { return {kBadUsage, std::move(text)}; }

bool parseInt(std::string_view text, int& value) {
  const auto* end = text.data() + text.size();
  const auto parsed = std::from_chars(text.data(), end, value);
  return parsed.ec == std::errc{} && parsed.ptr == end;
}

// 小節の区間。"17" は 17 小節目だけ、"17-20" は 17 から 20 まで（両端を含む）。
bool parseBars(std::string_view text, int& first, int& last, std::string& error) {
  const std::size_t dash = text.find('-');
  const std::string_view head = text.substr(0, dash);
  const std::string_view tail = dash == std::string_view::npos ? head : text.substr(dash + 1);

  if (!parseInt(head, first) || !parseInt(tail, last)) {
    error = "a bar range looks like 17 or 17-20";
    return false;
  }
  if (first < 1 || last < first) {
    error = "a bar range must start at 1 or later and end at or after it starts";
    return false;
  }
  return true;
}

bool readFile(const std::filesystem::path& path, std::string& out, std::string& error) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    error = "cannot open " + path.string();
    return false;
  }
  out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  if (file.bad()) {
    error = "cannot read " + path.string();
    return false;
  }
  return true;
}

std::string fixed(double value, int digits) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(digits) << value;
  return out.str();
}

std::string padded(const std::string& text, int width) {
  std::ostringstream out;
  out << std::left << std::setw(width) << text;
  return out.str();
}

}  // namespace

Commands::Commands(model::Project& project, engine::Studio& studio)
    : project_(project), studio_(studio) {}

Response Commands::run(const std::vector<std::string>& args) {
  if (args.empty()) {
    return help();
  }

  const std::string& verb = args[0];
  if (verb == "help") {
    return help();
  }
  if (verb == "status") {
    return status();
  }
  if (verb == "tracks") {
    return tracks();
  }
  if (verb == "plugins") {
    return plugins(args);
  }
  if (verb == "instrument") {
    return instrument(args);
  }
  if (verb == "notes") {
    return notes(args);
  }
  if (verb == "play") {
    return play(args);
  }
  if (verb == "stop") {
    studio_.stop();
    return ok("stopped");
  }
  if (verb == "quit") {
    studio_.stop();
    quit_ = true;
    return ok("bye");
  }
  return badUsage("no such command: " + verb + "\nrun `fuwa help` to see what there is");
}

Response Commands::help() const {
  return ok(
      "fuwa\n"
      "  tracks                          list the tracks and their instruments\n"
      "  plugins <bundle.vst3>           list the instruments a plugin bundle holds\n"
      "  instrument <track> <bundle.vst3> [class]\n"
      "                                  load an instrument into a track\n"
      "  notes set <track> <bars> <file.mid>\n"
      "                                  replace the notes in a bar range\n"
      "  notes clear <track> <bars>      remove the notes in a bar range\n"
      "  notes list <track> [bars]       show the notes in a bar range\n"
      "  play [bar]                      play from a bar (default 1)\n"
      "  stop                            stop playing\n"
      "  status                          show what is going on\n"
      "  quit                            shut fuwa down\n"
      "\n"
      "<track> is a track name or id. <bars> is 17 or 17-20, counting from 1 and\n"
      "including both ends.");
}

Response Commands::status() const {
  std::ostringstream out;
  out << "tempo    " << fixed(project_.tempo().bpm(), 1) << " BPM\n";
  out << "meter    " << project_.meter().beatsPerBar() << "/" << project_.meter().beatUnit()
      << "\n";
  // 4 小節ちょうどの曲は 4 小節。最後のノートが 5 小節目の頭で終わっても数えない。
  const double bars = project_.endBeat() / project_.meter().barLengthInBeats();
  out << "length   " << static_cast<int>(std::ceil(bars - 1e-9)) << " bars\n";
  if (studio_.playing()) {
    out << "playing  yes, at " << fixed(studio_.positionSeconds(), 2) << " s\n";
  } else {
    out << "playing  no\n";
  }
  return ok(out.str());
}

Response Commands::tracks() const {
  std::ostringstream out;
  out << padded("id", 6) << padded("name", 20) << "instrument\n";
  for (const model::Track& track : project_.tracks()) {
    const std::string name = studio_.instrumentName(track.id);
    out << padded(std::to_string(model::toNumber(track.id)), 6) << padded(track.name, 20)
        << (name.empty() ? "-" : name) << "\n";
  }
  return ok(out.str());
}

Response Commands::plugins(const std::vector<std::string>& args) {
  if (args.size() != 2) {
    return badUsage("usage: plugins <bundle.vst3>");
  }
  std::string error;
  const std::vector<std::string> names = studio_.listInstruments(args[1], error);
  if (!error.empty()) {
    return failed(error);
  }
  if (names.empty()) {
    return failed("that bundle has no instruments in it");
  }
  std::ostringstream out;
  for (const std::string& name : names) {
    out << name << "\n";
  }
  return ok(out.str());
}

Response Commands::instrument(const std::vector<std::string>& args) {
  if (args.size() < 3 || args.size() > 4) {
    return badUsage("usage: instrument <track> <bundle.vst3> [class]");
  }
  std::string error;
  const model::Track* track = resolveTrack(args[1], error);
  if (track == nullptr) {
    return failed(error);
  }
  const std::string className = args.size() == 4 ? args[3] : std::string();
  if (!studio_.setInstrument(track->id, args[2], className, error)) {
    return failed(error);
  }
  return ok("track " + track->name + " now plays " + studio_.instrumentName(track->id));
}

Response Commands::notes(const std::vector<std::string>& args) {
  if (args.size() < 3) {
    return badUsage(
        "usage: notes set <track> <bars> <file.mid>\n"
        "       notes clear <track> <bars>\n"
        "       notes list <track> [bars]");
  }

  const std::string& what = args[1];
  std::string error;
  const model::Track* track = resolveTrack(args[2], error);
  if (track == nullptr) {
    return failed(error);
  }

  model::Range range;
  range.track = track->id;
  const bool hasBars = args.size() > 3;
  if (hasBars && !parseBars(args[3], range.firstBar, range.lastBar, error)) {
    return badUsage(error);
  }

  if (what == "list") {
    if (!hasBars) {
      range.firstBar = 1;
      range.lastBar = project_.meter().locate(project_.endBeat()).bar;
    }
    const std::vector<model::Note> found = project_.notesIn(range);
    if (found.empty()) {
      return ok("no notes there");
    }
    std::ostringstream out;
    out << padded("bar", 6) << padded("beat", 8) << padded("pitch", 7) << padded("length", 9)
        << "velocity\n";
    for (const model::Note& note : found) {
      const model::Meter::Position at = project_.meter().locate(note.startBeat);
      out << padded(std::to_string(at.bar), 6) << padded(fixed(at.beat + 1.0, 3), 8)
          << padded(std::to_string(note.pitch), 7) << padded(fixed(note.lengthBeats, 3), 9)
          << fixed(static_cast<double>(note.velocity), 2) << "\n";
    }
    return ok(out.str());
  }

  if (what == "clear") {
    if (!hasBars) {
      return badUsage("usage: notes clear <track> <bars>");
    }
    const model::ReplaceResult result = project_.clearRange(range);
    if (!result.ok) {
      return failed(result.error);
    }
    return ok("removed " + std::to_string(result.removed) + " notes");
  }

  if (what == "set") {
    if (args.size() != 5) {
      return badUsage("usage: notes set <track> <bars> <file.mid>");
    }
    std::string bytes;
    if (!readFile(args[4], bytes, error)) {
      return failed(error);
    }
    model::MidiSong song;
    if (!model::readMidi(std::as_bytes(std::span<const char>(bytes)), song, error)) {
      return failed(error);
    }
    const model::ReplaceResult result = project_.replaceRange(range, song.notes);
    if (!result.ok) {
      return failed(result.error);
    }

    std::ostringstream out;
    out << "bars " << range.firstBar << "-" << range.lastBar << " on " << track->name << ": "
        << result.inserted << " notes in, " << result.removed << " out";
    if (result.dropped > 0) {
      out << "\ndropped " << result.dropped << " notes that started past bar " << range.lastBar;
    }
    if (song.bpm > 0.0 && std::abs(song.bpm - project_.tempo().bpm()) > 0.01) {
      out << "\nthe file asks for " << fixed(song.bpm, 1) << " BPM; fuwa plays at "
          << fixed(project_.tempo().bpm(), 1) << " (tempo changes are not supported yet)";
    }
    return ok(out.str());
  }

  return badUsage("no such thing to do with notes: " + what);
}

Response Commands::play(const std::vector<std::string>& args) {
  if (args.size() > 2) {
    return badUsage("usage: play [bar]");
  }
  int bar = 1;
  if (args.size() == 2 && (!parseInt(args[1], bar) || bar < 1)) {
    return badUsage("the bar number starts at 1");
  }

  std::string error;
  if (!studio_.play(project_, bar, error)) {
    return failed(error);
  }

  std::ostringstream out;
  out << "playing from bar " << bar << "\n";
  out << "length   "
      << fixed(static_cast<double>(studio_.renderedFrames()) / studio_.sampleRate(), 2) << " s\n";
  out << "peak     " << fixed(static_cast<double>(studio_.renderedPeak()), 4);
  if (studio_.renderedPeak() <= 0.0f) {
    out << "\nnothing came out of the instrument; the output is silent";
    return {kFailed, out.str()};
  }
  return ok(out.str());
}

const model::Track* Commands::resolveTrack(std::string_view text, std::string& error) const {
  int id = 0;
  if (parseInt(text, id) && id > 0) {
    const model::Track* track = project_.find(model::TrackId{static_cast<std::uint32_t>(id)});
    if (track != nullptr) {
      return track;
    }
  }
  const model::Track* track = project_.findByName(text);
  if (track == nullptr) {
    error = "no such track: " + std::string(text) + "\nrun `fuwa tracks` to see what there is";
  }
  return track;
}

}  // namespace fuwa::api
