#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "model/note.h"

namespace fuwa::model {

// SMF（Standard MIDI File）を読んだ結果。
struct MidiSong {
  std::vector<Note> notes;  // 拍（四分音符）単位。ファイルの先頭が 0
  double bpm = 0.0;         // 最初の Set Tempo。無ければ 0
};

// バイト列を読む。ファイルを開くのは呼ぶ側の仕事（モデルは I/O を知らない）。
//
// 対応するのは format 0 / 1 / 2 で、分解能は tick-per-quarter のみ。
// SMPTE のタイムコードは扱わない（曲の分解能を秒で持つ形式で、拍の話にならない）。
bool readMidi(std::span<const std::byte> data, MidiSong& song, std::string& error);

}  // namespace fuwa::model
