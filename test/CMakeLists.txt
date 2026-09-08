# 依存を増やしたくないので、テストフレームワークは使わず assert と終了コードで済ませる。
function(fuwa_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE fuwa_options)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME ${name} COMMAND ${name})
endfunction()

fuwa_test(test_wav audio/wav_test.cpp)
target_link_libraries(test_wav PRIVATE fuwa_audio)

fuwa_test(test_player engine/player_test.cpp)
target_link_libraries(test_player PRIVATE fuwa_engine)
