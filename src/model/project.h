#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "model/meter.h"
#include "model/note.h"
#include "model/range.h"
#include "model/tempo.h"
#include "model/track.h"

// プロジェクトの状態と、それを変える規則。ファイル I/O もリアルタイム処理も知らない。
namespace fuwa::model {

// 範囲を置き換えた結果。API の返事にそのまま使えるだけの情報を持たせる。
struct ReplaceResult {
  bool ok = false;
  std::string error;  // ok が false のときだけ埋まる
  int inserted = 0;
  int removed = 0;
  int dropped = 0;  // 範囲の外で始まるので捨てたノートの数
};

class Project {
 public:
  // トラックを 1 本持って始まる。増やす手段は MVP には無い。
  Project();

  const Tempo& tempo() const { return tempo_; }
  const Meter& meter() const { return meter_; }

  const std::vector<Track>& tracks() const { return tracks_; }
  const Track* find(TrackId id) const;

  // 名前でも引けるようにするのは入り口の便宜。アドレスはあくまで id。
  const Track* findByName(std::string_view name) const;

  // 曲の終わり（最後のノートが鳴り終わる拍）。ノートが無ければ 0。
  double endBeat() const;

  // 範囲を notes で置き換える。
  //
  // - 範囲に「開始が」入っているノートを消す。またいで伸びているノートは長さのまま残す
  // - notes の位置は範囲の頭からの相対。0 拍目が firstBar の頭になる
  // - 範囲の外で始まるノートは捨て、その数を dropped に返す
  ReplaceResult replaceRange(const Range& range, const std::vector<Note>& notes);

  // 範囲に開始が入っているノートを消す。
  ReplaceResult clearRange(const Range& range);

  // 範囲に開始が入っているノート。位置は曲頭からの絶対値のまま。
  std::vector<Note> notesIn(const Range& range) const;

 private:
  Track* findMutable(TrackId id);
  // 範囲を拍の半開区間 [begin, end) にする。
  void beatSpan(const Range& range, double& begin, double& end) const;

  Tempo tempo_;
  Meter meter_;
  std::vector<Track> tracks_;
};

}  // namespace fuwa::model
