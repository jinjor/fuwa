#include "model/project.h"

#include <cmath>
#include <cstdio>

#include "check.h"

// 範囲への操作の意味論を確かめる。境界の扱いを決めているのはこの層。
namespace {

using fuwa::model::Project;
using fuwa::model::Range;

bool near(double a, double b) { return std::abs(a - b) < 1e-6; }

Range bars(const Project& project, int first, int last) {
  Range range;
  range.track = project.tracks().front().id;
  range.firstBar = first;
  range.lastBar = last;
  return range;
}

}  // namespace

int main() {
  // 小節と拍の対応。4/4 なら 1 小節は 4 拍で、17 小節目の頭は 64 拍。
  {
    const fuwa::model::Meter meter;
    CHECK(near(meter.beatsAtBar(1), 0.0));
    CHECK(near(meter.beatsAtBar(17), 64.0));
    CHECK(meter.locate(64.0).bar == 17);
    CHECK(near(meter.locate(65.5).beat, 1.5));

    // 6/8 の 1 小節は四分音符 3 つぶん。
    const fuwa::model::Meter compound(6, 8);
    CHECK(near(compound.barLengthInBeats(), 3.0));
    CHECK(near(compound.beatsAtBar(3), 6.0));
  }

  // 置いたノートは範囲の頭からの相対位置に並ぶ。
  {
    Project project;
    const auto result =
        project.replaceRange(bars(project, 17, 20), {{0.0, 1.0, 60, 0.8f}, {2.0, 0.5, 62, 0.8f}});
    CHECK(result.ok);
    CHECK(result.inserted == 2);
    CHECK(result.removed == 0);
    CHECK(result.dropped == 0);

    const auto& notes = project.tracks().front().notes;
    CHECK(notes.size() == 2);
    CHECK(near(notes[0].startBeat, 64.0));
    CHECK(near(notes[1].startBeat, 66.0));
  }

  // 範囲の外で始まるノートは捨てて、その数を返す。
  {
    Project project;
    const auto result =
        project.replaceRange(bars(project, 1, 1), {{0.0, 1.0, 60, 0.8f}, {4.0, 1.0, 62, 0.8f}});
    CHECK(result.ok);
    CHECK(result.inserted == 1);
    CHECK(result.dropped == 1);
  }

  // 置き換えの判定はノートの開始位置。またいで伸びているノートは長さのまま残る。
  {
    Project project;
    CHECK(project.replaceRange(bars(project, 1, 1), {{0.0, 8.0, 60, 0.8f}}).ok);
    CHECK(project.replaceRange(bars(project, 2, 2), {{0.0, 1.0, 62, 0.8f}}).ok);

    const auto& notes = project.tracks().front().notes;
    CHECK(notes.size() == 2);
    CHECK(near(notes[0].lengthBeats, 8.0));  // 1 小節目のロングトーンは切られない
    CHECK(notes[1].pitch == 62);

    // 2 小節目を消しても、またいでいるノートは消えない。
    const auto cleared = project.clearRange(bars(project, 2, 2));
    CHECK(cleared.ok);
    CHECK(cleared.removed == 1);
    CHECK(project.tracks().front().notes.size() == 1);
  }

  // 範囲の問い合わせも同じ判定。
  {
    Project project;
    CHECK(project
              .replaceRange(bars(project, 1, 4),
                            {{0.0, 1.0, 60, 0.8f}, {4.0, 1.0, 62, 0.8f}, {8.0, 1.0, 64, 0.8f}})
              .ok);
    CHECK(project.notesIn(bars(project, 2, 2)).size() == 1);
    CHECK(project.notesIn(bars(project, 2, 3)).size() == 2);
    CHECK(project.notesIn(bars(project, 5, 8)).empty());
    CHECK(near(project.endBeat(), 9.0));
  }

  // 不変条件を破るノートは受け付けず、そのとき状態も変えない。
  {
    Project project;
    CHECK(project.replaceRange(bars(project, 1, 1), {{0.0, 1.0, 60, 0.8f}}).ok);

    const auto tooHigh = project.replaceRange(bars(project, 1, 1), {{0.0, 1.0, 200, 0.8f}});
    CHECK(!tooHigh.ok);
    CHECK(!tooHigh.error.empty());
    CHECK(project.tracks().front().notes.size() == 1);
    CHECK(project.tracks().front().notes[0].pitch == 60);

    CHECK(!project.replaceRange(bars(project, 1, 1), {{0.0, 0.0, 60, 0.8f}}).ok);
    CHECK(!project.replaceRange(bars(project, 1, 1), {{0.0, 1.0, 60, 2.0f}}).ok);
  }

  // 無い宛先。
  {
    Project project;
    Range unknown = bars(project, 1, 1);
    unknown.track = fuwa::model::TrackId{999};
    CHECK(!project.replaceRange(unknown, {}).ok);

    Range backwards = bars(project, 4, 2);
    CHECK(!project.replaceRange(backwards, {}).ok);
    CHECK(!project.replaceRange(bars(project, 0, 1), {}).ok);
  }

  std::printf("project ok\n");
  return 0;
}
