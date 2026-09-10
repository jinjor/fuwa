# 層ごとにライブラリを分ける。依存の向きは docs/architecture.md の通り。
add_library(fuwa_model STATIC model/project.cpp model/midi.cpp)
target_include_directories(fuwa_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fuwa_model PRIVATE fuwa_options)

add_library(fuwa_audio STATIC audio/sink.cpp audio/wav.cpp audio/device.cpp audio/level.cpp)
target_include_directories(fuwa_audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fuwa_audio
  PUBLIC "-framework AudioToolbox" "-framework CoreAudio" "-framework CoreFoundation"
  PRIVATE fuwa_options)

add_library(fuwa_plugin STATIC plugin/vst3.cpp)
target_include_directories(fuwa_plugin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fuwa_plugin PRIVATE vst3_hosting fuwa_options)

add_library(fuwa_engine STATIC engine/player.cpp engine/studio.cpp)
target_include_directories(fuwa_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fuwa_engine PUBLIC fuwa_model fuwa_audio fuwa_plugin PRIVATE fuwa_options)

add_library(fuwa_api STATIC
  api/client.cpp api/command.cpp api/protocol.cpp api/server.cpp api/socket.cpp)
target_include_directories(fuwa_api PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fuwa_api PUBLIC fuwa_engine PRIVATE vendor_json fuwa_options)

add_executable(fuwa app/main.cpp)
target_link_libraries(fuwa PRIVATE fuwa_api fuwa_options)
