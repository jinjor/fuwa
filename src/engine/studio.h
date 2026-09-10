#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "audio/device.h"
#include "audio/sink.h"
#include "model/project.h"

// 音を出す側ひとそろい。音源を持ち、プロジェクトを鳴らす。
//
// VST3 はこの下（plugin 層）に閉じている。api はここしか見ないので、
// 音源の差し替えが api に漏れない。
namespace fuwa::engine {

class Studio {
 public:
  Studio();
  ~Studio();

  Studio(const Studio&) = delete;
  Studio& operator=(const Studio&) = delete;

  // バンドルに入っている音源の名前。ひとつのバンドルが複数の音源を持つことがある。
  std::vector<std::string> listInstruments(const std::filesystem::path& bundle,
                                           std::string& error) const;

  // 音源を読んでトラックに差す。className が空なら最初の音源を選ぶ。
  bool setInstrument(model::TrackId track, const std::filesystem::path& bundle,
                     std::string_view className, std::string& error);

  // 差してある音源の名前。差していなければ空。
  std::string instrumentName(model::TrackId track) const;

  // fromBar から曲の終わりまでを sink に書く。再生も書き出しもここを通る。
  bool render(const model::Project& project, int fromBar, audio::Sink& sink, std::string& error);

  // レンダリングしてから流す。戻るのは鳴らし始めた時点で、鳴り終わるのは待たない。
  bool play(const model::Project& project, int fromBar, std::string& error);
  void stop();
  bool playing() const;

  // 今流しているものの長さと大きさ。耳を使わずに「鳴っているか」を確かめるため。
  std::int64_t renderedFrames() const;
  float renderedPeak() const;
  double positionSeconds() const;
  double sampleRate() const;

 private:
  struct Instruments;

  // 流している音の置き場。Device より先に宣言する。破棄は宣言の逆順なので、
  // 逆にするとコールバックが動いたまま音が消える。
  std::vector<std::vector<float>> playing_;
  std::vector<const float*> pointers_;
  float peak_ = 0.0f;
  double sampleRate_ = 48000.0;

  std::unique_ptr<Instruments> instruments_;
  audio::Device device_;
};

}  // namespace fuwa::engine
