#pragma once

// テンポ。拍と時間の対応を答える。
namespace fuwa::model {

// 今は曲全体で一定だが、本来は位置つきの列（MIDI の Set Tempo と同じで、曲の途中で
// 変わる）。ここが列になっても、拍から秒への変換を答えるのはこの型のままにする。
// サンプルレートを持ち込まないこと。フレームへの換算は出力側の都合。
class Tempo {
 public:
  Tempo() = default;
  explicit Tempo(double bpm) : bpm_(bpm) {}

  double bpm() const { return bpm_; }

  // 曲頭から数えた拍数に対応する秒数。
  double secondsAt(double beats) const { return beats * 60.0 / bpm_; }

 private:
  double bpm_ = 120.0;
};

}  // namespace fuwa::model
