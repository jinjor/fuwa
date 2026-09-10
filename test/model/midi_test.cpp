#include "model/midi.h"

#include <cstdint>
#include <vector>

#include "check.h"

// SMF を組み立てて読ませる。ファイルを置かずに済むので、仕様の隅を突きやすい。
namespace {

using Bytes = std::vector<std::uint8_t>;

void appendVariableLength(Bytes& out, std::uint32_t value) {
  Bytes reversed{static_cast<std::uint8_t>(value & 0x7F)};
  value >>= 7;
  while (value > 0) {
    reversed.insert(reversed.begin(), static_cast<std::uint8_t>((value & 0x7F) | 0x80));
    value >>= 7;
  }
  out.insert(out.end(), reversed.begin(), reversed.end());
}

void appendBigEndian(Bytes& out, std::uint32_t value, int width) {
  for (int shift = (width - 1) * 8; shift >= 0; shift -= 8) {
    out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
  }
}

void appendTag(Bytes& out, const char* tag) {
  for (int i = 0; i < 4; ++i) {
    out.push_back(static_cast<std::uint8_t>(tag[i]));
  }
}

Bytes file(std::uint16_t format, std::uint16_t division, const std::vector<Bytes>& tracks) {
  Bytes out;
  appendTag(out, "MThd");
  appendBigEndian(out, 6, 4);
  appendBigEndian(out, format, 2);
  appendBigEndian(out, static_cast<std::uint32_t>(tracks.size()), 2);
  appendBigEndian(out, division, 2);
  for (const Bytes& track : tracks) {
    appendTag(out, "MTrk");
    appendBigEndian(out, static_cast<std::uint32_t>(track.size()), 4);
    out.insert(out.end(), track.begin(), track.end());
  }
  return out;
}

Bytes endOfTrack() { return {0x00, 0xFF, 0x2F, 0x00}; }

void append(Bytes& out, const Bytes& more) { out.insert(out.end(), more.begin(), more.end()); }

bool read(const Bytes& bytes, fuwa::model::MidiSong& song, std::string& error) {
  return fuwa::model::readMidi(
      std::as_bytes(std::span<const std::uint8_t>(bytes.data(), bytes.size())), song, error);
}

bool near(double a, double b) { return std::abs(a - b) < 1e-6; }

}  // namespace

int main() {
  // ふつうの note on / note off。
  {
    Bytes track;
    appendVariableLength(track, 0);
    append(track, {0x90, 60, 100});
    appendVariableLength(track, 480);
    append(track, {0x80, 60, 0});
    append(track, endOfTrack());

    fuwa::model::MidiSong song;
    std::string error;
    CHECK(read(file(0, 480, {track}), song, error));
    CHECK(error.empty());
    CHECK(song.notes.size() == 1);
    CHECK(near(song.notes[0].startBeat, 0.0));
    CHECK(near(song.notes[0].lengthBeats, 1.0));
    CHECK(song.notes[0].pitch == 60);
    CHECK(near(static_cast<double>(song.notes[0].velocity), 100.0 / 127.0));
  }

  // ランニングステータスと、velocity 0 の note on（note off の省略形）。
  {
    Bytes track;
    appendVariableLength(track, 0);
    append(track, {0x90, 60, 64});  // ここでステータスが決まる
    appendVariableLength(track, 240);
    append(track, {62, 64});  // ステータス省略
    appendVariableLength(track, 240);
    append(track, {60, 0});  // velocity 0 なので note off
    appendVariableLength(track, 240);
    append(track, {62, 0});
    append(track, endOfTrack());

    fuwa::model::MidiSong song;
    std::string error;
    CHECK(read(file(0, 240, {track}), song, error));
    CHECK(song.notes.size() == 2);
    CHECK(song.notes[0].pitch == 60);
    CHECK(near(song.notes[0].startBeat, 0.0));
    CHECK(near(song.notes[0].lengthBeats, 2.0));
    CHECK(song.notes[1].pitch == 62);
    CHECK(near(song.notes[1].startBeat, 1.0));
    CHECK(near(song.notes[1].lengthBeats, 2.0));
  }

  // Set Tempo を拾う。データバイトが 0x80 以上でもステータスと取り違えない。
  {
    Bytes track;
    appendVariableLength(track, 0);
    append(track, {0xFF, 0x51, 0x03});
    appendBigEndian(track, 400000, 3);  // 150 BPM
    appendVariableLength(track, 0);
    append(track, {0x90, 60, 100});
    appendVariableLength(track, 96);
    append(track, {0x80, 60, 0});
    append(track, endOfTrack());

    fuwa::model::MidiSong song;
    std::string error;
    CHECK(read(file(0, 96, {track}), song, error));
    CHECK(near(song.bpm, 150.0));
    CHECK(song.notes.size() == 1);
  }

  // format 1。トラックはそれぞれ時刻 0 から始まり、読んだ結果は位置順に並ぶ。
  {
    Bytes first;
    appendVariableLength(first, 480);
    append(first, {0x90, 72, 100});
    appendVariableLength(first, 480);
    append(first, {0x80, 72, 0});
    append(first, endOfTrack());

    Bytes second;
    appendVariableLength(second, 0);
    append(second, {0x91, 48, 100});
    appendVariableLength(second, 240);
    append(second, {0x81, 48, 0});
    append(second, endOfTrack());

    fuwa::model::MidiSong song;
    std::string error;
    CHECK(read(file(1, 480, {first, second}), song, error));
    CHECK(song.notes.size() == 2);
    CHECK(song.notes[0].pitch == 48);
    CHECK(song.notes[1].pitch == 72);
    CHECK(near(song.notes[1].startBeat, 1.0));
  }

  // 閉じられないまま終わったノートは、トラックの終わりで閉じる。
  {
    Bytes track;
    appendVariableLength(track, 0);
    append(track, {0x90, 60, 100});
    appendVariableLength(track, 480);
    append(track, {0xFF, 0x2F, 0x00});  // デルタは上で書いたので、ここは中身だけ

    fuwa::model::MidiSong song;
    std::string error;
    CHECK(read(file(0, 480, {track}), song, error));
    CHECK(song.notes.size() == 1);
    CHECK(near(song.notes[0].lengthBeats, 1.0));
  }

  // 知らないチャンクは読み飛ばす（仕様がそう求めている）。
  {
    Bytes track;
    appendVariableLength(track, 0);
    append(track, {0x90, 60, 100});
    appendVariableLength(track, 480);
    append(track, {0x80, 60, 0});
    append(track, endOfTrack());

    Bytes bytes = file(0, 480, {});
    appendTag(bytes, "XYZW");
    appendBigEndian(bytes, 3, 4);
    append(bytes, {1, 2, 3});
    appendTag(bytes, "MTrk");
    appendBigEndian(bytes, static_cast<std::uint32_t>(track.size()), 4);
    append(bytes, track);
    bytes[11] = 1;  // ntrks を 1 に直す

    fuwa::model::MidiSong song;
    std::string error;
    CHECK(read(bytes, song, error));
    CHECK(song.notes.size() == 1);
  }

  // 受け付けないもの。
  {
    fuwa::model::MidiSong song;
    std::string error;

    CHECK(!read(Bytes{'R', 'I', 'F', 'F', 0, 0, 0, 0}, song, error));
    CHECK(!error.empty());

    // SMPTE のタイムコードは扱わない。
    CHECK(!read(file(0, 0xE728, {endOfTrack()}), song, error));

    // 途中で切れたファイル。
    Bytes truncated = file(0, 480, {endOfTrack()});
    truncated.resize(truncated.size() - 2);
    CHECK(!read(truncated, song, error));

    CHECK(!read(Bytes{}, song, error));
  }

  std::printf("midi ok\n");
  return 0;
}
