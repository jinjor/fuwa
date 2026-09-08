#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "plugin/instrument.h"

// VST3 固有の入り口。将来 AU を足すなら plugin/au.h を隣に置く。
namespace fuwa::plugin::vst3 {

// バンドルに入っているクラスの一覧。ひとつのバンドルが複数の音源を持つことがある。
struct ClassInfo {
  std::string name;
  std::string subCategories;  // 例 "Instrument|Synth"
  bool isInstrument = false;
};

std::vector<ClassInfo> listClasses(const std::filesystem::path& bundlePath, std::string& error);

// className が空なら最初のインストゥルメントを選ぶ。
// 失敗したら nullptr を返し、error に理由を書く。
std::unique_ptr<Instrument> load(const std::filesystem::path& bundlePath,
                                 std::string_view className, std::string& error);

}  // namespace fuwa::plugin::vst3
