# 書き方は「モダン CMake」に統一する。設定はすべてターゲットに紐付け、
# グローバルに効く命令（include_directories / add_definitions など）は使わない。
# 詳細は docs/development.md の「ビルド構成の書き方」
cmake_minimum_required(VERSION 3.24)
project(fuwa CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT APPLE)
  message(FATAL_ERROR "fuwa は macOS 専用")
endif()

# 開発ビルドではサニタイザを常時有効にする。詳細は docs/development.md
option(FUWA_SANITIZE "Enable ASan/UBSan" ON)

add_library(fuwa_options INTERFACE)
target_compile_options(fuwa_options INTERFACE
  -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-unused-parameter)

if(FUWA_SANITIZE)
  target_compile_options(fuwa_options INTERFACE
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
  target_link_options(fuwa_options INTERFACE -fsanitize=address,undefined)
endif()

# サブモジュールが取れていないと、この先の失敗が原因を指さないので先に見る。
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/vendor/vst3sdk/public.sdk/source/vst/hosting/module.cpp)
  message(FATAL_ERROR
    "VST3 SDK が取れていない。./setup.sh を実行すること。")
endif()

add_subdirectory(vendor)
add_subdirectory(src)

enable_testing()
add_subdirectory(test)

# 道具は ./setup.sh が .tools/ の下に置く。マシンには入れない。
# 版はここで固定する。整形結果も指摘内容も版で変わるため。
set(FUWA_LLVM_VERSION 18.1.8)
set(FUWA_LLVM_BIN ${CMAKE_SOURCE_DIR}/.tools/llvm-${FUWA_LLVM_VERSION}/bin)

find_program(CLANG_FORMAT clang-format PATHS ${FUWA_LLVM_BIN} NO_DEFAULT_PATH)
find_program(CLANG_TIDY clang-tidy PATHS ${FUWA_LLVM_BIN} NO_DEFAULT_PATH)
find_program(RUN_CLANG_TIDY run-clang-tidy PATHS ${FUWA_LLVM_BIN} NO_DEFAULT_PATH)
if(NOT CLANG_FORMAT OR NOT CLANG_TIDY OR NOT RUN_CLANG_TIDY)
  message(FATAL_ERROR "LLVM ${FUWA_LLVM_VERSION} が ${FUWA_LLVM_BIN} に無い。./setup.sh を実行すること。")
endif()

# 整形。設定は .clang-format。
file(GLOB_RECURSE FUWA_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/src/*.h ${CMAKE_SOURCE_DIR}/src/*.cpp
  ${CMAKE_SOURCE_DIR}/test/*.h ${CMAKE_SOURCE_DIR}/test/*.cpp)
add_custom_target(format COMMAND ${CLANG_FORMAT} -i ${FUWA_SOURCES})
add_custom_target(format-check COMMAND ${CLANG_FORMAT} --dry-run -Werror ${FUWA_SOURCES})

# 静的解析。設定は .clang-tidy。
#
# clang-tidy は Apple の libc++ を見つけられない。Command Line Tools がヘッダを
# SDK の中に置いているのに、そこを探しに行かないため。明示的に渡す。
file(GLOB_RECURSE FUWA_TIDY_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/src/*.cpp ${CMAKE_SOURCE_DIR}/test/*.cpp)
add_custom_target(tidy
  COMMAND ${RUN_CLANG_TIDY} -p ${CMAKE_BINARY_DIR} -quiet
          -clang-tidy-binary ${CLANG_TIDY}
          -extra-arg=-isystem${CMAKE_OSX_SYSROOT}/usr/include/c++/v1
          ${FUWA_TIDY_SOURCES}
  USES_TERMINAL)
