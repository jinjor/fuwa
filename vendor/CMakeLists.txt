# VST3 SDK のうちホストに必要なソースだけをコンパイルする。
# SDK 同梱の CMake はサンプルプラグインまで含めて巨大なので使わない。
set(SDK ${CMAKE_CURRENT_SOURCE_DIR}/vst3sdk)

add_library(vst3sdk STATIC
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
  ${SDK}/public.sdk/source/vst/hosting/connectionproxy.cpp
  ${SDK}/public.sdk/source/vst/hosting/eventlist.cpp
  ${SDK}/public.sdk/source/vst/hosting/hostclasses.cpp
  ${SDK}/public.sdk/source/vst/hosting/module.cpp
  ${SDK}/public.sdk/source/vst/hosting/module_mac.mm
  ${SDK}/public.sdk/source/vst/hosting/parameterchanges.cpp
  ${SDK}/public.sdk/source/vst/hosting/pluginterfacesupport.cpp
  ${SDK}/public.sdk/source/vst/hosting/plugprovider.cpp
  ${SDK}/public.sdk/source/vst/hosting/processdata.cpp
)

target_include_directories(vst3sdk SYSTEM PUBLIC ${SDK})
target_compile_definitions(vst3sdk PUBLIC
  $<$<CONFIG:Debug>:DEVELOPMENT=1>
  $<$<NOT:$<CONFIG:Debug>>:RELEASE=1>)

# SDK 側の警告まで拾っても直せないので、fuwa_options は付けない。
target_link_libraries(vst3sdk PUBLIC "-framework CoreFoundation" "-framework Foundation")
set_source_files_properties(
  ${SDK}/public.sdk/source/vst/hosting/module_mac.mm
  ${SDK}/public.sdk/source/common/threadchecker_mac.mm
  PROPERTIES COMPILE_FLAGS "-fobjc-arc")
