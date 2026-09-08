# 層ごとにライブラリを分ける。依存の向きは docs/architecture.md の通り。
add_library(fuwa_model INTERFACE)
target_include_directories(fuwa_model INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(fuwa_audio STATIC audio/sink.cpp audio/wav.cpp)
target_include_directories(fuwa_audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fuwa_audio PRIVATE fuwa_options)

add_library(fuwa_plugin STATIC plugin/vst3_instrument.cpp)
target_include_directories(fuwa_plugin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fuwa_plugin PUBLIC vst3sdk PRIVATE fuwa_options)

add_library(fuwa_engine STATIC engine/player.cpp)
target_include_directories(fuwa_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fuwa_engine PUBLIC fuwa_model fuwa_audio fuwa_plugin PRIVATE fuwa_options)

add_executable(fuwa app/main.cpp)
target_link_libraries(fuwa PRIVATE fuwa_engine fuwa_options)
