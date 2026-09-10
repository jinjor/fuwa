add_subdirectory(fixtures)

# 層の依存の向きは実行して確かめるものではないので、ソースを読んで検査する。
add_test(NAME test_layers COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/layers.sh ${CMAKE_SOURCE_DIR})
add_test(NAME test_realtime COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/realtime.sh ${CMAKE_SOURCE_DIR})

# 依存を増やしたくないので、テストフレームワークは使わず assert と終了コードで済ませる。
function(fuwa_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE fuwa_options)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME ${name} COMMAND ${name})
endfunction()

fuwa_test(test_wav audio/wav_test.cpp)
target_link_libraries(test_wav PRIVATE fuwa_audio)

fuwa_test(test_midi model/midi_test.cpp)
target_link_libraries(test_midi PRIVATE fuwa_model)

fuwa_test(test_project model/project_test.cpp)
target_link_libraries(test_project PRIVATE fuwa_model)

fuwa_test(test_protocol api/protocol_test.cpp)
target_link_libraries(test_protocol PRIVATE fuwa_api)

fuwa_test(test_player engine/player_test.cpp)
target_link_libraries(test_player PRIVATE fuwa_engine)

# ホストが実際に音を出せるかは、SDK 同梱の mda を相手に確かめる。
add_executable(test_vst3 plugin/vst3_test.cpp)
target_link_libraries(test_vst3 PRIVATE fuwa_engine fuwa_options)
target_include_directories(test_vst3 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_dependencies(test_vst3 mda-vst3)
add_test(NAME test_vst3 COMMAND test_vst3 $<TARGET_BUNDLE_DIR:mda-vst3>)

# ノートを置いてから音になるまでを、実在の音源で通しで確かめる。
add_executable(test_studio engine/studio_test.cpp)
target_link_libraries(test_studio PRIVATE fuwa_engine fuwa_options)
target_include_directories(test_studio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_dependencies(test_studio mda-vst3)
add_test(NAME test_studio COMMAND test_studio $<TARGET_BUNDLE_DIR:mda-vst3>)

# CLI からサーバまで。ソケットを本当に張るのはここだけ。
add_test(NAME test_ipc COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/api/ipc.sh
  $<TARGET_FILE:fuwa> $<TARGET_BUNDLE_DIR:mda-vst3> ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/scale.mid)

# hostchecker はホストの振る舞いを検査して結果をメッセージで返す。
# このテストだけは SDK のヘッダ（logevents.h）を直接読むので、自分で SDK を要求する。
# 要るのはヘッダだけなのでリンクはせず、include パスだけをもらう。
add_executable(test_hostcheck plugin/hostcheck_test.cpp)
target_link_libraries(test_hostcheck PRIVATE fuwa_engine fuwa_options)
target_include_directories(test_hostcheck SYSTEM PRIVATE
  $<TARGET_PROPERTY:vst3_base,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(test_hostcheck PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/vendor/vst3sdk/public.sdk/samples/vst/hostchecker/source)
add_dependencies(test_hostcheck hostchecker)
add_test(NAME test_hostcheck COMMAND test_hostcheck $<TARGET_BUNDLE_DIR:hostchecker>)
