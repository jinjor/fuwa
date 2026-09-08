#pragma once

#include <cstdint>
#include <vector>

// オーディオの出口。デバイスに出すかバッファに溜めるかの違いをここで吸収する。
// テストで音を確かめられるのはこの境界があるため。
namespace fuwa::audio {

class Sink {
 public:
  virtual ~Sink() = default;

  Sink(const Sink&) = delete;
  Sink& operator=(const Sink&) = delete;

  virtual void write(const float* const* channels, std::int32_t channelCount,
                     std::int32_t frameCount) = 0;

 protected:
  Sink() = default;
};

// 書かれたものを全部持っておく。オフラインのレンダリングとテスト用。
class BufferSink final : public Sink {
 public:
  void write(const float* const* channels, std::int32_t channelCount,
             std::int32_t frameCount) override;

  const std::vector<std::vector<float>>& channels() const { return channels_; }
  std::int32_t frameCount() const;

 private:
  std::vector<std::vector<float>> channels_;
};

}  // namespace fuwa::audio
