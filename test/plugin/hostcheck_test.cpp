#include <algorithm>
#include <cstdio>
#include <map>
#include <string>

#include "audio/sink.h"
#include "check.h"
#include "engine/player.h"
#include "logevents.h"  // hostchecker が持つ ID と説明文の対応表
#include "plugin/vst3.h"

// hostchecker はホスト側の振る舞いを検査するためのプラグイン。
// 規格から外れた呼び方をしていると、その内容をメッセージで報告してくる。
// ここではそれを受け取って読める形にし、Error が出ていないことを確かめる。
namespace {

struct Report {
  std::int64_t count = 0;
  const char* description = "";
  const char* severity = "";
};

}  // namespace

int main(int argc, char** argv) {
  CHECK(argc >= 2);
  const std::filesystem::path bundle = argv[1];

  std::map<std::int64_t, Report> reports;
  auto observer = [&](const char* id, const fuwa::plugin::vst3::MessageAttributes& attributes) {
    if (std::string(id) != "LogEvent") {
      return;
    }
    std::int64_t logId = 0;
    std::int64_t count = 0;
    if (!attributes.getInt("ID", logId) || !attributes.getInt("Count", count)) {
      return;
    }
    if (logId < 0 || logId >= kNumLogEvents) {
      return;
    }
    reports[logId] = {count, logEventDescriptions[logId], logEventSeverity[logId]};
  };

  std::string error;
  auto instrument = fuwa::plugin::vst3::load(bundle, "", observer, error);
  CHECK(instrument != nullptr);

  fuwa::engine::RenderSettings settings;
  settings.tailSeconds = 0.1;
  CHECK(instrument->prepare(settings.sampleRate, settings.blockSize, error));

  fuwa::engine::Schedule schedule;
  schedule.bpm = 120.0;
  schedule.notes.push_back({0.0, 0.5, 60, 0.8f});
  schedule.notes.push_back({0.5, 0.5, 64, 0.8f});

  fuwa::audio::BufferSink sink;
  fuwa::engine::renderAll(*instrument, schedule, settings, sink);

  // 破棄のときにも検査結果が送られてくるので、ここで手放す。
  instrument.reset();

  int errors = 0;
  for (const auto& [id, report] : reports) {
    const bool isError = std::string(report.severity) == LOG_ERR;
    if (isError) {
      ++errors;
    }
    std::printf("%-5s x%-4lld %s\n", report.severity, static_cast<long long>(report.count),
                report.description);
  }
  std::printf("--- %zu 件の報告、うち Error %d 件 ---\n", reports.size(), errors);

  CHECK(errors == 0);
  return 0;
}
