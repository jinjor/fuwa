#include "audio/device.h"

#include <AudioToolbox/AudioToolbox.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

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
  std::atomic<bool> finished{false};
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

struct UnitHandle {
  AudioUnit unit = nullptr;
  bool initialized = false;
  bool running = false;

  ~UnitHandle() {
    if (running) {
      AudioOutputUnitStop(unit);
    }
    if (initialized) {
      AudioUnitUninitialize(unit);
    }
    if (unit != nullptr) {
      AudioComponentInstanceDispose(unit);
    }
  }
};

}  // namespace

bool play(const std::vector<std::vector<float>>& channels, double sampleRate, std::string& error) {
  if (channels.empty() || channels.front().empty()) {
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

  // 再生に使うものは UnitHandle より先に作る。ローカルの破棄は宣言の逆順なので、
  // 逆にするとユニットを止める前にこれらが消え、コールバックが死んだものを読む。
  std::vector<const float*> pointers;
  pointers.reserve(channels.size());
  for (const std::vector<float>& channel : channels) {
    pointers.push_back(channel.data());
  }

  Playback playback;
  playback.channels = pointers.data();
  playback.channelCount = static_cast<std::int32_t>(channels.size());
  playback.frameCount = static_cast<std::int64_t>(channels.front().size());

  UnitHandle handle;
  if (AudioComponentInstanceNew(component, &handle.unit) != noErr) {
    error = "cannot create the output unit";
    return false;
  }

  const auto channelCount = static_cast<UInt32>(channels.size());

  // 非インターリーブの 32bit float。チャンネルごとにバッファが分かれるので、
  // レンダリング結果をそのまま書き写せる。
  AudioStreamBasicDescription format{};
  format.mSampleRate = sampleRate;
  format.mFormatID = kAudioFormatLinearPCM;
  format.mFormatFlags =
      kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
  format.mFramesPerPacket = 1;
  format.mChannelsPerFrame = channelCount;
  format.mBitsPerChannel = 32;
  format.mBytesPerFrame = sizeof(float);
  format.mBytesPerPacket = sizeof(float);

  if (AudioUnitSetProperty(handle.unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0,
                           &format, sizeof(format)) != noErr) {
    error = "cannot set the output format";
    return false;
  }

  AURenderCallbackStruct callback{};
  callback.inputProc = renderCallback;
  callback.inputProcRefCon = &playback;
  if (AudioUnitSetProperty(handle.unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input,
                           0, &callback, sizeof(callback)) != noErr) {
    error = "cannot set the render callback";
    return false;
  }

  if (AudioUnitInitialize(handle.unit) != noErr) {
    error = "cannot initialize the output unit";
    return false;
  }
  handle.initialized = true;

  if (AudioOutputUnitStart(handle.unit) != noErr) {
    error = "cannot start playback";
    return false;
  }
  handle.running = true;

  while (!playback.finished.load(std::memory_order_acquire)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  // 最後のバッファがデバイスから出きるのを待つ。120ms に根拠はない。
  // 本来はデバイスのレイテンシを問い合わせて決めるべきだが、Step 0 では
  // 鳴らして確かめられれば足りるので余裕を見た定数で済ませている。
  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  return true;
}

}  // namespace fuwa::audio
