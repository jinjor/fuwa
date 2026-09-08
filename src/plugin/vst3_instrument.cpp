#include "plugin/vst3.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/processdata.h"

namespace fuwa::plugin::vst3 {
namespace {

using namespace Steinberg;

// FUWA_DEBUG=1 でホストの内部状態を stderr に出す。
// プラグインが無音のとき、どの段階で落ちているかを切り分けるため。
bool debugEnabled() {
  static const bool on = std::getenv("FUWA_DEBUG") != nullptr;
  return on;
}

template <typename... Args>
void debugLog(const char* format, Args... args) {
  if (!debugEnabled()) {
    return;
  }
  std::fprintf(stderr, "[fuwa] ");
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
  std::fprintf(stderr, format, args...);
#pragma clang diagnostic pop
  std::fprintf(stderr, "\n");
}

// サブカテゴリに Instrument を含むものが音源。持たないものはエフェクト。
bool isInstrumentClass(const VST3::Hosting::ClassInfo& info) {
  const auto& subs = info.subCategories();
  return std::find(subs.begin(), subs.end(), "Instrument") != subs.end();
}

// ホストコンテキストはプロセスに一つあればよい。PlugProvider が参照する。
Vst::IHostApplication& hostContext() {
  static Vst::HostApplication host;
  static bool registered = [] {
    Vst::PluginContextFactory::instance().setPluginContext(&host);
    return true;
  }();
  (void)registered;
  return host;
}

class Vst3Instrument final : public Instrument {
 public:
  ~Vst3Instrument() override {
    if (processor_) {
      processor_->setProcessing(false);
    }
    if (component_) {
      component_->setActive(false);
    }
    data_.unprepare();
    provider_ = nullptr;
    processor_ = nullptr;
    component_ = nullptr;
  }

  bool open(const std::filesystem::path& path, std::string_view className, std::string& error) {
    hostContext();

    module_ = VST3::Hosting::Module::create(path.string(), error);
    if (!module_) {
      return false;
    }

    const auto classes = module_->getFactory().classInfos();
    auto it = classes.end();
    if (className.empty()) {
      // 指定がなければインストゥルメントを選ぶ。エフェクトを掴むと音が出ない。
      it = std::find_if(classes.begin(), classes.end(), [](const auto& info) {
        return info.category() == kVstAudioEffectClass && isInstrumentClass(info);
      });
    } else {
      it = std::find_if(classes.begin(), classes.end(), [&](const auto& info) {
        return info.category() == kVstAudioEffectClass && info.name() == className;
      });
    }
    if (it == classes.end()) {
      error = className.empty() ? "インストゥルメントが見つからない: " + path.string()
                                : "クラスが見つからない: " + std::string(className);
      return false;
    }

    name_ = it->name();
    provider_ = owned(new Vst::PlugProvider(module_->getFactory(), *it, true));
    if (!provider_->initialize()) {
      error = "プラグインの初期化に失敗: " + name_;
      return false;
    }

    component_ = provider_->getComponentPtr();
    processor_ = FUnknownPtr<Vst::IAudioProcessor>(component_);
    if (!component_ || !processor_) {
      error = "IAudioProcessor を取得できない: " + name_;
      return false;
    }
    return true;
  }

  bool prepare(double sampleRate, std::int32_t maxBlockSize, std::string& error) override {
    // kOffline を使うプラグインは少なく、実装が枯れていないことがある。
    // オフラインのレンダリングでも kRealtime で回す。
    Vst::ProcessSetup setup{};
    setup.processMode = Vst::kRealtime;
    setup.symbolicSampleSize = Vst::kSample32;
    setup.maxSamplesPerBlock = maxBlockSize;
    setup.sampleRate = sampleRate;

    // バス配置を明示しないと音を出さないプラグインがある。
    // 既定の配置をそのまま読み返して、それを受け入れると伝える。
    if (!negotiateBuses(error)) {
      return false;
    }

    if (processor_->setupProcessing(setup) != kResultOk) {
      error = "setupProcessing に失敗: " + name_;
      return false;
    }

    // 使うバスだけを有効にする。無効のままだと音が出ない。
    activateAll(Vst::kAudio, Vst::kInput);
    activateAll(Vst::kAudio, Vst::kOutput);
    activateAll(Vst::kEvent, Vst::kInput);

    if (component_->setActive(true) != kResultOk) {
      error = "setActive に失敗: " + name_;
      return false;
    }
    processor_->setProcessing(true);

    if (!data_.prepare(*component_, maxBlockSize, Vst::kSample32)) {
      error = "バッファを用意できない: " + name_;
      return false;
    }

    debugLog("bus audio-in=%d audio-out=%d event-in=%d event-out=%d",
             component_->getBusCount(Vst::kAudio, Vst::kInput),
             component_->getBusCount(Vst::kAudio, Vst::kOutput),
             component_->getBusCount(Vst::kEvent, Vst::kInput),
             component_->getBusCount(Vst::kEvent, Vst::kOutput));
    debugLog("data numInputs=%d numOutputs=%d", data_.numInputs, data_.numOutputs);

    channelCount_ = data_.numOutputs > 0 ? data_.outputs[0].numChannels : 0;
    if (channelCount_ <= 0) {
      error = "出力チャンネルがない: " + name_;
      return false;
    }

    context_ = {};
    context_.sampleRate = sampleRate;
    context_.tempo = 120.0;
    context_.timeSigNumerator = 4;
    context_.timeSigDenominator = 4;
    context_.state = Vst::ProcessContext::kPlaying | Vst::ProcessContext::kTempoValid |
                     Vst::ProcessContext::kTimeSigValid | Vst::ProcessContext::kProjectTimeMusicValid;

    data_.inputEvents = &events_;
    data_.inputParameterChanges = &paramsIn_;
    data_.outputParameterChanges = &paramsOut_;
    data_.processContext = &context_;
    data_.processMode = Vst::kRealtime;
    data_.symbolicSampleSize = Vst::kSample32;

    syncControllerState();
    dumpParameters();
    return true;
  }

  void render(const NoteEvent* events, std::size_t eventCount, float* const* out,
              std::int32_t channelCount, std::int32_t frameCount) override {
    events_.clear();
    paramsIn_.clearQueue();
    paramsOut_.clearQueue();

    for (std::size_t i = 0; i < eventCount; ++i) {
      const NoteEvent& e = events[i];
      Vst::Event ev{};
      ev.busIndex = 0;
      ev.sampleOffset = e.sampleOffset;
      ev.flags = Vst::Event::kIsLive;
      if (e.on) {
        ev.type = Vst::Event::kNoteOnEvent;
        ev.noteOn.channel = 0;
        ev.noteOn.pitch = e.pitch;
        ev.noteOn.velocity = e.velocity;
        ev.noteOn.noteId = -1;
      } else {
        ev.type = Vst::Event::kNoteOffEvent;
        ev.noteOff.channel = 0;
        ev.noteOff.pitch = e.pitch;
        ev.noteOff.velocity = e.velocity;
        ev.noteOff.noteId = -1;
      }
      events_.addEvent(ev);
    }

    data_.numSamples = frameCount;
    const tresult result = processor_->process(data_);
    if (result != kResultOk && !processReported_) {
      processReported_ = true;
      debugLog("process が失敗を返した: %d", static_cast<int>(result));
    }
    if (eventCount > 0) {
      debugLog("events=%zu offset=%d pitch=%d on=%d", eventCount, events[0].sampleOffset,
               static_cast<int>(events[0].pitch), events[0].on ? 1 : 0);
    }

    const std::int32_t available = std::min(channelCount, channelCount_);
    for (std::int32_t ch = 0; ch < channelCount; ++ch) {
      float* dst = out[ch];
      if (ch < available) {
        std::memcpy(dst, data_.outputs[0].channelBuffers32[ch],
                    static_cast<std::size_t>(frameCount) * sizeof(float));
      } else {
        std::fill_n(dst, frameCount, 0.0f);
      }
    }

    context_.projectTimeSamples += frameCount;
    context_.projectTimeMusic +=
        static_cast<double>(frameCount) / context_.sampleRate * context_.tempo / 60.0;
  }

  std::int32_t outputChannelCount() const override { return channelCount_; }
  const std::string& name() const override { return name_; }

 private:
  // コンポーネントの状態をコントローラに渡す。PlugProvider はここまでやらない。
  void syncControllerState() {
    auto controller = provider_->getControllerPtr();
    if (!controller) {
      return;
    }
    MemoryStream stream;
    if (component_->getState(&stream) != kResultOk) {
      return;
    }
    stream.seek(0, IBStream::kIBSeekSet, nullptr);
    controller->setComponentState(&stream);
  }

  void dumpParameters() {
    if (!debugEnabled()) {
      return;
    }
    auto controller = provider_->getControllerPtr();
    if (!controller) {
      debugLog("controller なし");
      return;
    }
    const int32 count = controller->getParameterCount();
    debugLog("parameters=%d", count);
    for (int32 i = 0; i < count && i < 24; ++i) {
      Vst::ParameterInfo info{};
      if (controller->getParameterInfo(i, info) != kResultOk) {
        continue;
      }
      char title[128] = {};
      for (int n = 0; n < 127 && info.title[n]; ++n) {
        title[n] = static_cast<char>(info.title[n]);
      }
      debugLog("  [%d] %-24s value=%.3f default=%.3f", static_cast<int>(info.id), title,
               controller->getParamNormalized(info.id), info.defaultNormalizedValue);
    }
  }

  bool negotiateBuses(std::string& error) {
    std::vector<Vst::SpeakerArrangement> inputs;
    std::vector<Vst::SpeakerArrangement> outputs;
    collectArrangements(Vst::kInput, inputs);
    collectArrangements(Vst::kOutput, outputs);

    if (processor_->setBusArrangements(inputs.data(), static_cast<int32>(inputs.size()),
                                       outputs.data(),
                                       static_cast<int32>(outputs.size())) != kResultOk) {
      error = "バス配置を受け付けない: " + name_;
      return false;
    }
    return true;
  }

  void collectArrangements(Vst::BusDirection dir, std::vector<Vst::SpeakerArrangement>& out) {
    const int32 count = component_->getBusCount(Vst::kAudio, dir);
    for (int32 i = 0; i < count; ++i) {
      Vst::SpeakerArrangement arrangement = Vst::SpeakerArr::kStereo;
      processor_->getBusArrangement(dir, i, arrangement);
      out.push_back(arrangement);
    }
  }

  void activateAll(Vst::MediaType type, Vst::BusDirection dir) {
    const int32 count = component_->getBusCount(type, dir);
    for (int32 i = 0; i < count; ++i) {
      component_->activateBus(type, dir, i, true);
    }
  }

  VST3::Hosting::Module::Ptr module_;
  IPtr<Vst::PlugProvider> provider_;
  IPtr<Vst::IComponent> component_;
  IPtr<Vst::IAudioProcessor> processor_;
  Vst::HostProcessData data_;
  Vst::EventList events_{256};
  Vst::ParameterChanges paramsIn_;
  Vst::ParameterChanges paramsOut_;
  Vst::ProcessContext context_{};
  std::string name_;
  std::int32_t channelCount_ = 0;
  bool processReported_ = false;
};

}  // namespace

std::vector<ClassInfo> listClasses(const std::filesystem::path& bundlePath, std::string& error) {
  hostContext();
  auto module = VST3::Hosting::Module::create(bundlePath.string(), error);
  if (!module) {
    return {};
  }

  std::vector<ClassInfo> out;
  for (const auto& info : module->getFactory().classInfos()) {
    if (info.category() != kVstAudioEffectClass) {
      continue;
    }
    out.push_back({info.name(), info.subCategoriesString(), isInstrumentClass(info)});
  }
  return out;
}

std::unique_ptr<Instrument> load(const std::filesystem::path& bundlePath,
                                 std::string_view className, std::string& error) {
  auto instrument = std::make_unique<Vst3Instrument>();
  if (!instrument->open(bundlePath, className, error)) {
    return nullptr;
  }
  return instrument;
}

}  // namespace fuwa::plugin::vst3
