#include "audio/device.h"

#include <AudioToolbox/AudioToolbox.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>

namespace fuwa::audio {
namespace {

// リアルタイムコールバックが触る唯一の状態。
// 音は再生前に用意し終えているので、コールバックは書き写すだけで済む。
struct Playback {
  const std::vector<std::vector<float>>* channels = nullptr;
  std::int64_t frameCount = 0;
  std::atomic<std::int64_t> position{0};
  std::atomic<bool> finished{false};
};

OSStatus renderCallback(void* refCon, AudioUnitRenderActionFlags* flags, const AudioTimeStamp*,
                        UInt32, UInt32 frameCount, AudioBufferList* data) {
  auto* playback = static_cast<Playback*>(refCon);
  const std::int64_t start = playback->position.load(std::memory_order_relaxed);
  const std::int64_t available = std::max<std::int64_t>(0, playback->frameCount - start);
  const auto toCopy = static_cast<std::int64_t>(std::min<std::int64_t>(frameCount, available));

  for (UInt32 bus = 0; bus < data->mNumberBuffers; ++bus) {
    auto* out = static_cast<float*>(data->mBuffers[bus].mData);
    const auto& source = (*playback->channels)[std::min<std::size_t>(
        bus, playback->channels->size() - 1)];

    for (std::int64_t i = 0; i < toCopy; ++i) {
      out[i] = source[static_cast<std::size_t>(start + i)];
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
    error = "鳴らすものがない";
    return false;
  }

  AudioComponentDescription description{};
  description.componentType = kAudioUnitType_Output;
  description.componentSubType = kAudioUnitSubType_DefaultOutput;
  description.componentManufacturer = kAudioUnitManufacturer_Apple;

  AudioComponent component = AudioComponentFindNext(nullptr, &description);
  if (component == nullptr) {
    error = "既定の出力デバイスが見つからない";
    return false;
  }

  UnitHandle handle;
  if (AudioComponentInstanceNew(component, &handle.unit) != noErr) {
    error = "出力ユニットを作れない";
    return false;
  }

  const auto channelCount = static_cast<UInt32>(channels.size());

  // 非インターリーブの 32bit float。チャンネルごとにバッファが分かれるので、
  // レンダリング結果をそのまま書き写せる。
  AudioStreamBasicDescription format{};
  format.mSampleRate = sampleRate;
  format.mFormatID = kAudioFormatLinearPCM;
  format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked |
                        kAudioFormatFlagIsNonInterleaved;
  format.mFramesPerPacket = 1;
  format.mChannelsPerFrame = channelCount;
  format.mBitsPerChannel = 32;
  format.mBytesPerFrame = sizeof(float);
  format.mBytesPerPacket = sizeof(float);

  if (AudioUnitSetProperty(handle.unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0,
                           &format, sizeof(format)) != noErr) {
    error = "出力フォーマットを設定できない";
    return false;
  }

  Playback playback;
  playback.channels = &channels;
  playback.frameCount = static_cast<std::int64_t>(channels.front().size());

  AURenderCallbackStruct callback{};
  callback.inputProc = renderCallback;
  callback.inputProcRefCon = &playback;
  if (AudioUnitSetProperty(handle.unit, kAudioUnitProperty_SetRenderCallback,
                           kAudioUnitScope_Input, 0, &callback, sizeof(callback)) != noErr) {
    error = "コールバックを設定できない";
    return false;
  }

  if (AudioUnitInitialize(handle.unit) != noErr) {
    error = "出力ユニットを初期化できない";
    return false;
  }
  handle.initialized = true;

  if (AudioOutputUnitStart(handle.unit) != noErr) {
    error = "再生を開始できない";
    return false;
  }
  handle.running = true;

  while (!playback.finished.load(std::memory_order_acquire)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  // 最後のバッファがデバイスから出きるのを待つ
  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  return true;
}

}  // namespace fuwa::audio
