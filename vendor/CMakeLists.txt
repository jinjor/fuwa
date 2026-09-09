# VST3 SDK のうち必要なソースだけをコンパイルする。
# SDK 同梱の CMake はサンプルまで含めて巨大なので使わない。
#
#   vst3_base     ホストとプラグインの双方が使う土台
#   vst3_hosting  プラグインを読み込む側だけが使う（fuwa 本体）
#   vst3_plugin   プラグインを作る側だけが使う（テスト用の音源）
set(SDK ${CMAKE_CURRENT_SOURCE_DIR}/vst3sdk)

add_library(vst3_base STATIC
  ${SDK}/pluginterfaces/base/conststringtable.cpp
  ${SDK}/pluginterfaces/base/coreiids.cpp
  ${SDK}/pluginterfaces/base/funknown.cpp
  ${SDK}/pluginterfaces/base/ustring.cpp

  ${SDK}/base/source/baseiids.cpp
  ${SDK}/base/source/fbuffer.cpp
  ${SDK}/base/source/fdebug.cpp
  ${SDK}/base/source/fdynlib.cpp
  ${SDK}/base/source/fobject.cpp
  ${SDK}/base/source/fstreamer.cpp
  ${SDK}/base/source/fstring.cpp
  ${SDK}/base/source/timer.cpp
  ${SDK}/base/source/updatehandler.cpp
  ${SDK}/base/thread/source/fcondition.cpp
  ${SDK}/base/thread/source/flock.cpp

  ${SDK}/public.sdk/source/common/commoniids.cpp
  ${SDK}/public.sdk/source/common/commonstringconvert.cpp
  ${SDK}/public.sdk/source/common/memorystream.cpp
  ${SDK}/public.sdk/source/common/pluginview.cpp
  ${SDK}/public.sdk/source/common/threadchecker_mac.mm

  ${SDK}/public.sdk/source/vst/vstinitiids.cpp
  ${SDK}/public.sdk/source/vst/utility/stringconvert.cpp
)
target_include_directories(vst3_base SYSTEM PUBLIC ${SDK})
target_compile_definitions(vst3_base PUBLIC
  $<$<CONFIG:Debug>:DEVELOPMENT=1>
  $<$<NOT:$<CONFIG:Debug>>:RELEASE=1>)
target_link_libraries(vst3_base PUBLIC "-framework CoreFoundation" "-framework Foundation")
set_source_files_properties(${SDK}/public.sdk/source/common/threadchecker_mac.mm
  PROPERTIES COMPILE_FLAGS "-fobjc-arc")

add_library(vst3_hosting STATIC
  ${SDK}/public.sdk/source/vst/hosting/connectionproxy.cpp
  ${SDK}/public.sdk/source/vst/hosting/eventlist.cpp
  ${SDK}/public.sdk/source/vst/hosting/hostclasses.cpp
  ${SDK}/public.sdk/source/vst/hosting/module.cpp
  ${SDK}/public.sdk/source/vst/hosting/module_mac.mm
  ${SDK}/public.sdk/source/vst/hosting/parameterchanges.cpp
  ${SDK}/public.sdk/source/vst/hosting/pluginterfacesupport.cpp

  ${SDK}/public.sdk/source/vst/hosting/processdata.cpp
)
target_link_libraries(vst3_hosting PUBLIC vst3_base)
set_source_files_properties(${SDK}/public.sdk/source/vst/hosting/module_mac.mm
  PROPERTIES COMPILE_FLAGS "-fobjc-arc")

# テスト用の音源をビルドするためだけに要る。fuwa 本体はこれを使わない。
# OBJECT にしているのは、静的ライブラリだとエントリポイント（bundleEntry）が
# 誰からも参照されず、リンカに捨てられてバンドルとして成立しなくなるため。
add_library(vst3_plugin OBJECT
  ${SDK}/public.sdk/source/vst/vstaudioeffect.cpp
  ${SDK}/public.sdk/source/vst/vstbus.cpp
  ${SDK}/public.sdk/source/vst/vstcomponent.cpp
  ${SDK}/public.sdk/source/vst/vstcomponentbase.cpp
  ${SDK}/public.sdk/source/vst/vsteditcontroller.cpp
  ${SDK}/public.sdk/source/vst/vstnoteexpressiontypes.cpp
  ${SDK}/public.sdk/source/vst/vstparameters.cpp
  ${SDK}/public.sdk/source/vst/utility/vst2persistence.cpp
  ${SDK}/public.sdk/source/main/moduleinit.cpp
  ${SDK}/public.sdk/source/main/pluginfactory.cpp
  ${SDK}/public.sdk/source/main/macmain.cpp
)
target_link_libraries(vst3_plugin PUBLIC vst3_base)
