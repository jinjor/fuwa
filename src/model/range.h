#pragma once

#include "model/track.h"

namespace fuwa::model {

// 操作の宛先。トラックと小節の区間。
// 小節は 1 から数え、両端を含む（17-20 は 17, 18, 19, 20 の 4 小節）。
struct Range {
  TrackId track{};
  int firstBar = 1;
  int lastBar = 1;

  bool valid() const { return firstBar >= 1 && lastBar >= firstBar; }
  int barCount() const { return lastBar - firstBar + 1; }
};

}  // namespace fuwa::model
