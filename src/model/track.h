#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "model/note.h"

namespace fuwa::model {

// トラックを指すのは安定した id。表示名は改名で変わるので、アドレスには使わない。
enum class TrackId : std::uint32_t {};

inline std::uint32_t toNumber(TrackId id) { return static_cast<std::uint32_t>(id); }

struct Track {
  TrackId id{};
  std::string name;
  std::vector<Note> notes;  // startBeat の昇順。Project が保つ
};

}  // namespace fuwa::model
