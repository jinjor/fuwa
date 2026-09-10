#include "model/midi.h"

#include <algorithm>
#include <cstdint>

// SMF（Standard MIDI File）の読み取り。仕様どおりに読む。
//
//   MThd <長さ 6> <format 2> <ntrks 2> <division 2>
//   MTrk <長さ>   <デルタタイム + イベント> の繰り返し
//
// デルタタイムは可変長数値（7 bit ずつ、最上位ビットが「続く」の印）。
// イベントのステータスバイトは省略できる（ランニングステータス）。省略されたときは
// 直前のチャンネルメッセージのステータスを使い回す。SysEx とメタイベントはこれを断ち切る。
namespace fuwa::model {
namespace {

// 読み取り位置つきのバイト列。範囲外に出たら失敗の印を立て、以降は何も読まない。
class Reader {
 public:
  Reader(std::span<const std::byte> data) : data_(data) {}

  bool ok() const { return ok_; }
  std::size_t position() const { return at_; }
  std::size_t remaining() const { return ok_ ? data_.size() - at_ : 0; }

  std::uint8_t byte() {
    if (!ensure(1)) {
      return 0;
    }
    return static_cast<std::uint8_t>(data_[at_++]);
  }

  std::uint32_t bigEndian(std::size_t count) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < count; ++i) {
      value = (value << 8) | byte();
    }
    return value;
  }

  // 可変長数値。仕様上 4 バイトまで。
  std::uint32_t variableLength() {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
      const std::uint8_t b = byte();
      value = (value << 7) | static_cast<std::uint32_t>(b & 0x7F);
      if ((b & 0x80) == 0) {
        return value;
      }
    }
    ok_ = false;
    return 0;
  }

  void skip(std::size_t count) {
    if (ensure(count)) {
      at_ += count;
    }
  }

  void seek(std::size_t at) {
    if (at > data_.size()) {
      ok_ = false;
      return;
    }
    at_ = at;
  }

  bool matches(const char* tag) {
    if (!ensure(4)) {
      return false;
    }
    for (int i = 0; i < 4; ++i) {
      if (static_cast<char>(data_[at_ + static_cast<std::size_t>(i)]) != tag[i]) {
        return false;
      }
    }
    at_ += 4;
    return true;
  }

 private:
  bool ensure(std::size_t count) {
    if (!ok_ || data_.size() - at_ < count) {
      ok_ = false;
      return false;
    }
    return true;
  }

  std::span<const std::byte> data_;
  std::size_t at_ = 0;
  bool ok_ = true;
};

// note on を受けてから、対になる note off を待っているもの。
struct Pending {
  std::uint8_t channel;
  std::uint8_t pitch;
  float velocity;
  std::uint64_t startTick;
};

void close(std::vector<Pending>& pending, std::uint8_t channel, std::uint8_t pitch,
           std::uint64_t tick, double ticksPerQuarter, std::vector<Note>& notes) {
  // 同じ音程が重なって鳴っていることがあるので、先に鳴り始めたものから閉じる。
  const auto found = std::find_if(pending.begin(), pending.end(), [&](const Pending& p) {
    return p.channel == channel && p.pitch == pitch;
  });
  if (found == pending.end()) {
    return;
  }
  const auto length = static_cast<double>(tick - found->startTick) / ticksPerQuarter;
  if (length > 0.0) {
    notes.push_back({static_cast<double>(found->startTick) / ticksPerQuarter, length,
                     static_cast<std::int16_t>(pitch), found->velocity});
  }
  pending.erase(found);
}

// 1 トラックぶんを読む。トラックはそれぞれ時刻 0 から始まる。
bool readTrack(Reader& reader, std::size_t trackEnd, double ticksPerQuarter, MidiSong& song,
               std::string& error) {
  std::vector<Pending> pending;
  std::uint64_t tick = 0;
  std::uint8_t status = 0;

  while (reader.ok() && reader.position() < trackEnd) {
    tick += reader.variableLength();

    std::uint8_t first = reader.byte();
    if (first < 0x80) {
      // ランニングステータス。読んだバイトは最初のデータバイト。
      if (status == 0) {
        error = "the MIDI file uses running status before any status byte";
        return false;
      }
      reader.seek(reader.position() - 1);
      first = status;
    }

    if (first == 0xFF) {
      const std::uint8_t type = reader.byte();
      const std::uint32_t length = reader.variableLength();
      const std::size_t at = reader.position();
      if (type == 0x51 && length == 3 && song.bpm == 0.0) {
        const std::uint32_t microseconds = reader.bigEndian(3);
        if (microseconds > 0) {
          song.bpm = 60000000.0 / static_cast<double>(microseconds);
        }
        reader.seek(at);
      }
      reader.skip(length);
      status = 0;
      continue;
    }
    if (first == 0xF0 || first == 0xF7) {
      reader.skip(reader.variableLength());
      status = 0;
      continue;
    }

    status = first;
    const std::uint8_t kind = first & 0xF0;
    const std::uint8_t channel = first & 0x0F;
    if (kind == 0xC0 || kind == 0xD0) {
      reader.byte();  // データ 1 バイトだけのメッセージ
      continue;
    }

    const std::uint8_t data1 = reader.byte();
    const std::uint8_t data2 = reader.byte();
    if (kind == 0x90 && data2 > 0) {
      // 同じ音程が鳴りっぱなしのまま重ねられたら、先のものを閉じてから始める。
      close(pending, channel, data1, tick, ticksPerQuarter, song.notes);
      pending.push_back({channel, data1, static_cast<float>(data2) / 127.0f, tick});
    } else if (kind == 0x80 || kind == 0x90) {
      // velocity 0 の note on は note off（仕様が認めている省略の形）。
      close(pending, channel, data1, tick, ticksPerQuarter, song.notes);
    }
  }

  if (!reader.ok()) {
    error = "the MIDI file ends in the middle of a track";
    return false;
  }

  // 閉じられないまま終わったノートは、トラックの終わりで閉じる。
  while (!pending.empty()) {
    const Pending held = pending.front();
    close(pending, held.channel, held.pitch, tick, ticksPerQuarter, song.notes);
  }
  reader.seek(trackEnd);
  return true;
}

}  // namespace

bool readMidi(std::span<const std::byte> data, MidiSong& song, std::string& error) {
  song = MidiSong{};

  Reader reader(data);
  if (!reader.matches("MThd")) {
    error = "not a MIDI file (the header chunk is missing)";
    return false;
  }
  const std::uint32_t headerLength = reader.bigEndian(4);
  if (!reader.ok() || headerLength < 6) {
    error = "the MIDI header is too short";
    return false;
  }
  const std::size_t headerEnd = reader.position() + headerLength;
  reader.bigEndian(2);  // format。トラックの並べ方の違いで、ノートの読み方は変わらない
  const std::uint32_t trackCount = reader.bigEndian(2);
  const std::uint32_t division = reader.bigEndian(2);
  if (!reader.ok()) {
    error = "the MIDI header is truncated";
    return false;
  }
  if ((division & 0x8000) != 0) {
    error = "SMPTE time code is not supported; use ticks per quarter note";
    return false;
  }
  const auto ticksPerQuarter = static_cast<double>(division & 0x7FFF);
  if (ticksPerQuarter <= 0.0) {
    error = "the MIDI file has no time resolution";
    return false;
  }
  reader.seek(headerEnd);

  // 仕様は知らないチャンクを読み飛ばすことを求めている。
  for (std::uint32_t read = 0; read < trackCount && reader.remaining() >= 8;) {
    const bool isTrack = reader.matches("MTrk");
    if (!isTrack) {
      reader.skip(4);
    }
    const std::uint32_t length = reader.bigEndian(4);
    if (!reader.ok() || length > reader.remaining()) {
      error = "a MIDI chunk claims more bytes than the file has";
      return false;
    }
    const std::size_t chunkEnd = reader.position() + length;
    if (!isTrack) {
      reader.seek(chunkEnd);
      continue;
    }
    if (!readTrack(reader, chunkEnd, ticksPerQuarter, song, error)) {
      return false;
    }
    ++read;
  }

  std::stable_sort(song.notes.begin(), song.notes.end(),
                   [](const Note& a, const Note& b) { return a.startBeat < b.startBeat; });
  return true;
}

}  // namespace fuwa::model
