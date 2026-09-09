# ライセンスも外部依存も要らない検証用の音源。
# SDK 同梱の mda プラグインをそのままビルドして .vst3 バンドルにする。
# これがあるので「音が出ないのはホストのせいか音源のせいか」を切り分けられる。
set(MDA ${CMAKE_SOURCE_DIR}/vendor/vst3sdk/public.sdk/samples/vst/mda-vst3/source)

file(GLOB MDA_SOURCES ${MDA}/*.cpp)

add_library(mda-vst3 MODULE ${MDA_SOURCES})
target_link_libraries(mda-vst3 PRIVATE vst3_plugin)
target_include_directories(mda-vst3 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(mda-vst3 PROPERTIES
  BUNDLE TRUE
  BUNDLE_EXTENSION vst3
  MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
  MACOSX_BUNDLE_EXECUTABLE_NAME mda-vst3
  MACOSX_BUNDLE_GUI_IDENTIFIER com.steinberg.mda-vst3
  MACOSX_BUNDLE_BUNDLE_NAME mda-vst3
  SUFFIX "")

# プラグイン側のサニタイザは外す。ホストの検証が目的なので、
# 他人が書いたコードの指摘を追いかけても得るものがない。
target_compile_options(mda-vst3 PRIVATE -fno-sanitize=address,undefined)
target_link_options(mda-vst3 PRIVATE -fno-sanitize=address,undefined)

# ホストが規格通りに振る舞っているかを検査するプラグイン。
# SDK 同梱の hostchecker のうち処理側だけを建てる。制御側は VSTGUI に
# 依存していて画面を持たない fuwa では使えないが、検査は処理側が行い
# 結果はメッセージで送られてくるので、これで足りる。
set(HC ${CMAKE_SOURCE_DIR}/vendor/vst3sdk/public.sdk/samples/vst/hostchecker/source)

add_library(hostchecker MODULE
  ${HC}/eventlistcheck.cpp
  ${HC}/eventlogger.cpp
  ${HC}/hostcheck.cpp
  ${HC}/hostcheckerprocessor.cpp
  ${HC}/parameterchangescheck.cpp
  ${HC}/processcontextcheck.cpp
  ${HC}/processsetupcheck.cpp
  ${CMAKE_SOURCE_DIR}/vendor/vst3sdk/public.sdk/source/vst/utility/dataexchange.cpp
  hostchecker_factory.cpp)

target_link_libraries(hostchecker PRIVATE vst3_plugin)
target_include_directories(hostchecker PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${HC}
  ${CMAKE_SOURCE_DIR}/vendor/vst3sdk/vstgui4)
set_target_properties(hostchecker PROPERTIES
  BUNDLE TRUE
  BUNDLE_EXTENSION vst3
  MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
  MACOSX_BUNDLE_EXECUTABLE_NAME hostchecker
  MACOSX_BUNDLE_GUI_IDENTIFIER com.steinberg.hostchecker
  MACOSX_BUNDLE_BUNDLE_NAME hostchecker
  SUFFIX "")
target_compile_options(hostchecker PRIVATE -fno-sanitize=address,undefined)
target_link_options(hostchecker PRIVATE -fno-sanitize=address,undefined)
