#include "audio/device.h"

#include <AudioToolbox/AudioToolbox.h>

#include <algorithm>
#include <atomic>

namespace fuwa::audio {
namespace {

// リアルタイムコールバックが触る唯一の状態。
// 音は再生前に用意し終えているので、コールバックは書き写すだけで済む。
//
// std::vector ではなく生のポインタで持つ。リアルタイム側に渡るものは
// あらかじめ確保しておく決まりで、コンテナを持ち込むとその決まりが緩む。
struct Playback {
  const float* const* channels = nullptr;  // channelCount 本の配列
  std::int32_t channelCount = 0;
  std::int64_t frameCount = 0;
  std::atomic<std::int64_t> position{0};
  std::atomic<bool> finished{true};
};

// realtime-begin
OSStatus renderCallback(void* refCon, AudioUnitRenderActionFlags* flags, const AudioTimeStamp*,
                        UInt32, UInt32 frameCount, AudioBufferList* data) {
  auto* playback = static_cast<Playback*>(refCon);
  const std::int64_t start = playback->position.load(std::memory_order_relaxed);
  const std::int64_t available = std::max<std::int64_t>(0, playback->frameCount - start);
  const auto toCopy = static_cast<std::int64_t>(std::min<std::int64_t>(frameCount, available));

  for (UInt32 bus = 0; bus < data->mNumberBuffers; ++bus) {
    auto* out = static_cast<float*>(data->mBuffers[bus].mData);
    // デバイスが要求する本数がこちらより多ければ、最後のチャンネルを繰り返す。
    const auto last = static_cast<UInt32>(playback->channelCount - 1);
    const float* source = playback->channels[std::min<UInt32>(bus, last)];

    for (std::int64_t i = 0; i < toCopy; ++i) {
      out[i] = source[start + i];
    }
    for (std::int64_t i = toCopy; i < frameCount; ++i) {
      out[i] = 0.0f;
    }
  }

  playback->position.store(start + toCopy, std::memory_order_relaxed);
  if (toCopy < frameCount) {
    playback->finished.store(true, std::memory_order_release);
    if (flags != nullptr) {
      *flags |= kAudioUnitRenderAction_OutputIsSilence;
    }
  }
  return noErr;
}
// realtime-end

}  // namespace

// ユニットより Playback を先に宣言する。メンバの破棄は宣言の逆順なので、
// 逆にするとユニットを止める前に Playback が消え、コールバックが死んだものを読む。
struct Device::Impl {
  Playback playback;
  AudioUnit unit = nullptr;
  bool initialized = false;
  bool running = false;
};

Device::Device() : impl_(std::make_unique<Impl>()) {}

Device::~Device() { stop(); }

bool Device::start(const float* const* channels, std::int32_t channelCount, std::int64_t frameCount,
                   double sampleRate, std::string& error) {
  stop();

  if (channels == nullptr || channelCount <= 0 || frameCount <= 0) {
    error = "nothing to play";
    return false;
  }

  AudioComponentDescription description{};
  description.componentType = kAudioUnitType_Output;
  description.componentSubType = kAudioUnitSubType_DefaultOutput;
  description.componentManufacturer = kAudioUnitManufacturer_Apple;

  AudioComponent component = AudioComponentFindNext(nullptr, &description);
  if (component == nullptr) {
    error = "no default output device";
    return false;
  }

  impl_->playback.channels = channels;
  impl_->playback.channelCount = channelCount;
  impl_->playback.frameCount = frameCount;
  impl_->playback.position.store(0, std::memory_order_relaxed);
  impl_->playback.finished.store(false, std::memory_order_release);

  if (AudioComponentInstanceNew(component, &impl_->unit) != noErr) {
    error = "cannot create the output unit";
    stop();
    return false;
  }

  // 非インターリーブの 32bit float。チャンネルごとにバッファが分かれるので、
  // レンダリング結果をそのまま書き写せる。
  AudioStreamBasicDescription format{};
  format.mSampleRate = sampleRate;
  format.mFormatID = kAudioFormatLinearPCM;
  format.mFormatFlags =
      kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
  format.mFramesPerPacket = 1;
  format.mChannelsPerFrame = static_cast<UInt32>(channelCount);
  format.mBitsPerChannel = 32;
  format.mBytesPerFrame = sizeof(float);
  format.mBytesPerPacket = sizeof(float);

  if (AudioUnitSetProperty(impl_->unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0,
                           &format, sizeof(format)) != noErr) {
    error = "cannot set the output format";
    stop();
    return false;
  }

  AURenderCallbackStruct callback{};
  callback.inputProc = renderCallback;
  callback.inputProcRefCon = &impl_->playback;
  if (AudioUnitSetProperty(impl_->unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input,
                           0, &callback, sizeof(callback)) != noErr) {
    error = "cannot set the render callback";
    stop();
    return false;
  }

  if (AudioUnitInitialize(impl_->unit) != noErr) {
    error = "cannot initialize the output unit";
    stop();
    return false;
  }
  impl_->initialized = true;

  if (AudioOutputUnitStart(impl_->unit) != noErr) {
    error = "cannot start playback";
    stop();
    return false;
  }
  impl_->running = true;
  return true;
}

void Device::stop() {
  // AudioOutputUnitStop はコールバックが抜けるまで戻らない。
  // これが戻った後なら、呼んだ側はバッファを捨ててよい。
  if (impl_->running) {
    AudioOutputUnitStop(impl_->unit);
    impl_->running = false;
  }
  if (impl_->initialized) {
    AudioUnitUninitialize(impl_->unit);
    impl_->initialized = false;
  }
  if (impl_->unit != nullptr) {
    AudioComponentInstanceDispose(impl_->unit);
    impl_->unit = nullptr;
  }
  impl_->playback.finished.store(true, std::memory_order_release);
  impl_->playback.channels = nullptr;
  impl_->playback.channelCount = 0;
  impl_->playback.frameCount = 0;
}

bool Device::playing() const {
  return impl_->running && !impl_->playback.finished.load(std::memory_order_acquire);
}

std::int64_t Device::position() const {
  return impl_->playback.position.load(std::memory_order_relaxed);
}

}  // namespace fuwa::audio
