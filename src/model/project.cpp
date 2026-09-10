#include "model/project.h"

#include <algorithm>

namespace fuwa::model {
namespace {

// 拍の比較に使う許容誤差。小節の境界は掛け算で作るので、
// 「ちょうど境界にあるノート」が丸め誤差で片側に落ちないようにする。
constexpr double kEpsilon = 1e-9;

void sortByStart(std::vector<Note>& notes) {
  std::stable_sort(notes.begin(), notes.end(),
                   [](const Note& a, const Note& b) { return a.startBeat < b.startBeat; });
}

}  // namespace

Project::Project() { tracks_.push_back({TrackId{1}, "main", {}}); }

const Track* Project::find(TrackId id) const {
  for (const Track& track : tracks_) {
    if (track.id == id) {
      return &track;
    }
  }
  return nullptr;
}

Track* Project::findMutable(TrackId id) {
  for (Track& track : tracks_) {
    if (track.id == id) {
      return &track;
    }
  }
  return nullptr;
}

const Track* Project::findByName(std::string_view name) const {
  for (const Track& track : tracks_) {
    if (track.name == name) {
      return &track;
    }
  }
  return nullptr;
}

double Project::endBeat() const {
  double end = 0.0;
  for (const Track& track : tracks_) {
    for (const Note& note : track.notes) {
      end = std::max(end, note.startBeat + note.lengthBeats);
    }
  }
  return end;
}

void Project::beatSpan(const Range& range, double& begin, double& end) const {
  begin = meter_.beatsAtBar(range.firstBar);
  end = meter_.beatsAtBar(range.lastBar + 1);
}

std::vector<Note> Project::notesIn(const Range& range) const {
  std::vector<Note> found;
  const Track* track = find(range.track);
  if (track == nullptr || !range.valid()) {
    return found;
  }

  double begin = 0.0;
  double end = 0.0;
  beatSpan(range, begin, end);
  for (const Note& note : track->notes) {
    if (note.startBeat >= begin - kEpsilon && note.startBeat < end - kEpsilon) {
      found.push_back(note);
    }
  }
  return found;
}

ReplaceResult Project::replaceRange(const Range& range, const std::vector<Note>& notes) {
  ReplaceResult result;
  if (!range.valid()) {
    result.error = "the bar range must start at 1 or later and end at or after it starts";
    return result;
  }
  Track* track = findMutable(range.track);
  if (track == nullptr) {
    result.error = "no such track";
    return result;
  }

  double begin = 0.0;
  double end = 0.0;
  beatSpan(range, begin, end);

  // 置くものを先に作る。途中で弾かれても状態を壊さないため。
  std::vector<Note> incoming;
  incoming.reserve(notes.size());
  for (const Note& note : notes) {
    Note placed = note;
    placed.startBeat += begin;
    if (placed.startBeat >= end - kEpsilon) {
      ++result.dropped;
      continue;
    }
    if (!valid(placed)) {
      result.error = "a note is out of range (pitch 0-127, velocity 0.0-1.0, length > 0)";
      return result;
    }
    incoming.push_back(placed);
  }

  const auto removed = std::remove_if(track->notes.begin(), track->notes.end(), [&](const Note& n) {
    return n.startBeat >= begin - kEpsilon && n.startBeat < end - kEpsilon;
  });
  result.removed = static_cast<int>(std::distance(removed, track->notes.end()));
  track->notes.erase(removed, track->notes.end());

  track->notes.insert(track->notes.end(), incoming.begin(), incoming.end());
  sortByStart(track->notes);

  result.ok = true;
  result.inserted = static_cast<int>(incoming.size());
  return result;
}

ReplaceResult Project::clearRange(const Range& range) { return replaceRange(range, {}); }

}  // namespace fuwa::model
