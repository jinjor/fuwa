cmake_minimum_required(VERSION 3.24)
project(fuwa CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT APPLE)
  message(FATAL_ERROR "fuwa は macOS 専用")
endif()

# 開発ビルドではサニタイザを常時有効にする。詳細は docs/architecture.md
option(FUWA_SANITIZE "Enable ASan/UBSan" ON)

add_library(fuwa_options INTERFACE)
target_compile_options(fuwa_options INTERFACE
  -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-unused-parameter)

if(FUWA_SANITIZE)
  target_compile_options(fuwa_options INTERFACE
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
  target_link_options(fuwa_options INTERFACE -fsanitize=address,undefined)
endif()

add_subdirectory(vendor)
add_subdirectory(src)

enable_testing()
add_subdirectory(test)
