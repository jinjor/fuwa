#include "plugin/vst3.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/vst/hosting/processdata.h"

namespace fuwa::plugin::vst3 {
namespace {

using namespace Steinberg;

// FUWA_DEBUG=1 でホストの内部状態を stderr に出す。
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

// ホストコンテキストはプロセスに一つあればよい。
Vst::IHostApplication& hostContext() {
  static Vst::HostApplication host;
  return host;
}

// 以下の2つはホストが所有し、プラグインより長生きする。
// 参照カウントで解放されては困るので addRef/release は数えない。
class HostComponentHandler final : public Vst::IComponentHandler {
 public:
  tresult PLUGIN_API beginEdit(Vst::ParamID) override { return kResultOk; }
  tresult PLUGIN_API performEdit(Vst::ParamID, Vst::ParamValue) override { return kResultOk; }
  tresult PLUGIN_API endEdit(Vst::ParamID) override { return kResultOk; }
  tresult PLUGIN_API restartComponent(int32 flags) override {
    debugLog("restartComponent flags=%d", static_cast<int>(flags));
    return kResultOk;
  }

  tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override {
    QUERY_INTERFACE(iid, obj, FUnknown::iid, Vst::IComponentHandler)
    QUERY_INTERFACE(iid, obj, Vst::IComponentHandler::iid, Vst::IComponentHandler)
    *obj = nullptr;
    return kNoInterface;
  }
  uint32 PLUGIN_API addRef() override { return 1; }
  uint32 PLUGIN_API release() override { return 1; }
};

class Attributes final : public MessageAttributes {
 public:
  explicit Attributes(Vst::IAttributeList* list) : list_(list) {}

  bool getInt(const char* key, std::int64_t& value) const override {
    int64 raw = 0;
    if (list_ == nullptr || list_->getInt(key, raw) != kResultOk) {
      return false;
    }
    value = raw;
    return true;
  }

 private:
  Vst::IAttributeList* list_;
};

// 処理側と制御側の間に挟まり、流れるメッセージを覗いてから素通しする。
class MessageTap final : public Vst::IConnectionPoint {
 public:
  void setup(Vst::IConnectionPoint* target, MessageObserver* observer) {
    target_ = target;
    observer_ = observer;
  }

  tresult PLUGIN_API connect(Vst::IConnectionPoint*) override { return kResultOk; }
  tresult PLUGIN_API disconnect(Vst::IConnectionPoint*) override { return kResultOk; }

  tresult PLUGIN_API notify(Vst::IMessage* message) override {
    if (message != nullptr) {
      const char* id = message->getMessageID();
      if (id != nullptr) {
        debugLog("message %s", id);
        if (observer_ != nullptr && *observer_) {
          const Attributes attributes(message->getAttributes());
          (*observer_)(id, attributes);
        }
      }
    }
    return target_ != nullptr ? target_->notify(message) : kResultOk;
  }

  tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override {
    QUERY_INTERFACE(iid, obj, FUnknown::iid, Vst::IConnectionPoint)
    QUERY_INTERFACE(iid, obj, Vst::IConnectionPoint::iid, Vst::IConnectionPoint)
    *obj = nullptr;
    return kNoInterface;
  }
  uint32 PLUGIN_API addRef() override { return 1; }
  uint32 PLUGIN_API release() override { return 1; }

 private:
  Vst::IConnectionPoint* target_ = nullptr;
  MessageObserver* observer_ = nullptr;
};

class Vst3Instrument final : public Instrument {
 public:
  explicit Vst3Instrument(MessageObserver observer) : observer_(std::move(observer)) {}

  ~Vst3Instrument() override {
    if (processor_) {
      processor_->setProcessing(false);
    }
    if (component_) {
      component_->setActive(false);
    }
    data_.unprepare();

    if (componentPoint_) {
      componentPoint_->disconnect(&tap_);
    }
    if (controllerPoint_ && componentPoint_) {
      controllerPoint_->disconnect(componentPoint_);
    }
    if (controller_) {
      controller_->setComponentHandler(nullptr);
    }
  }

  bool open(const std::filesystem::path& path, std::string_view className, std::string& error) {
    module_ = VST3::Hosting::Module::create(path.string(), error);
    if (!module_) {
      return false;
    }

    const auto& factory = module_->getFactory();
    const auto classes = factory.classInfos();
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

    component_ = factory.createInstance<Vst::IComponent>(it->ID());
    if (!component_) {
      error = "コンポーネントを作れない: " + name_;
      return false;
    }
    if (component_->initialize(&hostContext()) != kResultOk) {
      error = "コンポーネントを初期化できない: " + name_;
      return false;
    }

    processor_ = FUnknownPtr<Vst::IAudioProcessor>(component_);
    if (!processor_) {
      error = "IAudioProcessor を取得できない: " + name_;
      return false;
    }

    return setupController(factory, error);
  }

  bool prepare(double sampleRate, std::int32_t maxBlockSize, std::string& error) override {
    // バス配置を明示しないと音を出さないプラグインがある。
    // 既定の配置をそのまま読み返して、それを受け入れると伝える。
    if (!negotiateBuses(error)) {
      return false;
    }

    // kOffline を使うプラグインは少なく、実装が枯れていないことがある。
    // オフラインのレンダリングでも kRealtime で回す。
    Vst::ProcessSetup setup{};
    setup.processMode = Vst::kRealtime;
    setup.symbolicSampleSize = Vst::kSample32;
    setup.maxSamplesPerBlock = maxBlockSize;
    setup.sampleRate = sampleRate;

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
                     Vst::ProcessContext::kTimeSigValid |
                     Vst::ProcessContext::kProjectTimeMusicValid;

    data_.inputEvents = &events_;
    data_.inputParameterChanges = &paramsIn_;
    data_.outputParameterChanges = &paramsOut_;
    data_.processContext = &context_;
    data_.processMode = Vst::kRealtime;
    data_.symbolicSampleSize = Vst::kSample32;

    debugLog("bus audio-in=%d audio-out=%d event-in=%d channels=%d",
             component_->getBusCount(Vst::kAudio, Vst::kInput),
             component_->getBusCount(Vst::kAudio, Vst::kOutput),
             component_->getBusCount(Vst::kEvent, Vst::kInput), channelCount_);
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
  bool setupController(const VST3::Hosting::PluginFactory& factory, std::string& error) {
    // 処理側と制御側が分かれていないプラグインもある。
    controller_ = FUnknownPtr<Vst::IEditController>(component_);
    const bool separate = !controller_;

    if (separate) {
      TUID controllerId;
      if (component_->getControllerClassId(controllerId) != kResultOk) {
        // 制御側を持たないプラグインもある。音を出すだけなら困らない。
        debugLog("制御側のクラス ID がない: %s", name_.c_str());
        return true;
      }
      controller_ = factory.createInstance<Vst::IEditController>(VST3::UID(controllerId));
      if (!controller_) {
        debugLog("制御側を作れない: %s", name_.c_str());
        return true;
      }
      if (controller_->initialize(&hostContext()) != kResultOk) {
        error = "制御側を初期化できない: " + name_;
        return false;
      }
    }

    controller_->setComponentHandler(&handler_);

    if (separate) {
      componentPoint_ = FUnknownPtr<Vst::IConnectionPoint>(component_);
      controllerPoint_ = FUnknownPtr<Vst::IConnectionPoint>(controller_);
      if (componentPoint_ && controllerPoint_) {
        // 処理側の相手を tap にして、流れるメッセージを覗いてから制御側へ渡す。
        tap_.setup(controllerPoint_, &observer_);
        componentPoint_->connect(&tap_);
        controllerPoint_->connect(componentPoint_);
      }
    }

    // コンポーネントの状態を制御側へ渡す。これを省くと、制御側はパラメータを
    // 初期値のまま（多くは 0）だと思い込む。SDK の PlugProvider はここをやらない。
    //
    // 必ず接続の後に行う。接続で受け取った情報を使ってパラメータ表を組み立てる
    // プラグインがあり、先にこれを呼ぶと未初期化のまま参照して落ちる。
    MemoryStream stream;
    if (component_->getState(&stream) == kResultOk) {
      stream.seek(0, IBStream::kIBSeekSet, nullptr);
      controller_->setComponentState(&stream);
    }
    return true;
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

  MessageObserver observer_;
  HostComponentHandler handler_;
  MessageTap tap_;

  VST3::Hosting::Module::Ptr module_;
  IPtr<Vst::IComponent> component_;
  IPtr<Vst::IEditController> controller_;
  IPtr<Vst::IAudioProcessor> processor_;
  IPtr<Vst::IConnectionPoint> componentPoint_;
  IPtr<Vst::IConnectionPoint> controllerPoint_;

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
                                 std::string_view className, MessageObserver observer,
                                 std::string& error) {
  auto instrument = std::make_unique<Vst3Instrument>(std::move(observer));
  if (!instrument->open(bundlePath, className, error)) {
    return nullptr;
  }
  return instrument;
}

std::unique_ptr<Instrument> load(const std::filesystem::path& bundlePath,
                                 std::string_view className, std::string& error) {
  return load(bundlePath, className, {}, error);
}

}  // namespace fuwa::plugin::vst3
