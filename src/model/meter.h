#pragma once

#include <cmath>

namespace fuwa::model {

// 拍子。小節と拍の対応を答える。
//
// 拍は四分音符を 1 とする（テンポの BPM と同じ単位）。6/8 の 1 小節は 3 拍。
// テンポと同じく、本来は位置つきの列（MIDI の Time Signature と同じで曲の途中で
// 変わる）。今は曲全体で一定だが、列になっても小節 ↔ 拍の変換を答えるのはこの型のまま。
class Meter {
 public:
  Meter() = default;
  Meter(int beatsPerBar, int beatUnit) : beatsPerBar_(beatsPerBar), beatUnit_(beatUnit) {}

  int beatsPerBar() const { return beatsPerBar_; }
  int beatUnit() const { return beatUnit_; }

  // 1 小節の長さ（四分音符いくつぶんか）。
  double barLengthInBeats() const {
    return static_cast<double>(beatsPerBar_) * 4.0 / static_cast<double>(beatUnit_);
  }

  // 小節番号は 1 から数える。1 小節目の頭が拍 0。
  double beatsAtBar(int bar) const { return static_cast<double>(bar - 1) * barLengthInBeats(); }

  // 拍がどの小節の何拍目かを答える。beat は小節の頭からの拍数。
  struct Position {
    int bar = 1;
    double beat = 0.0;
  };
  Position locate(double beats) const {
    const double barLength = barLengthInBeats();
    const double bars = std::floor(beats / barLength);
    Position position;
    position.bar = static_cast<int>(bars) + 1;
    position.beat = beats - bars * barLength;
    return position;
  }

 private:
  int beatsPerBar_ = 4;
  int beatUnit_ = 4;
};

}  // namespace fuwa::model
