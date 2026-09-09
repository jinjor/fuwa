// hostchecker のうち処理側だけを登録するファクトリ。
//
// SDK 同梱のファクトリは制御側も登録するが、制御側は VSTGUI に依存していて
// 画面を持たない fuwa では使えない。検査そのものは処理側が行い、結果は
// メッセージで送られてくるので、処理側だけを建てれば足りる。
#include "public.sdk/source/main/pluginfactory.h"

#include "cids.h"
#include "hostcheckerprocessor.h"
#include "version.h"

BEGIN_FACTORY_DEF("fuwa", "https://github.com/jinjor/fuwa", "mailto:noreply@example.com")

DEF_CLASS2(INLINE_UID_FROM_FUID(Steinberg::Vst::HostCheckerProcessorUID),
           Steinberg::PClassInfo::kManyInstances, kVstAudioEffectClass, stringPluginName,
           Steinberg::Vst::kDistributable, "Fx|Instrument", FULL_VERSION_STR, kVstVersionString,
           Steinberg::Vst::HostCheckerProcessor::createInstance)

END_FACTORY
