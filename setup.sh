#!/bin/sh
# 開発環境を揃えて、揃ったことを確かめる。
#
# マシンには何も入れない。道具は .tools/ の下に置く（gitignore 済み）。
# リポジトリを消せば道具も消える。
#
# グローバルに要るのは Xcode Command Line Tools だけ。コンパイラと macOS SDK は
# 他所から持ってこられないため、ここだけは例外。
set -eu
cd "$(dirname "$0")"

LLVM_VERSION=18.1.8
LLVM_SHA256=4573b7f25f46d2a9c8882993f091c52f416c83271db6f5b213c93f0bd0346a10
CMAKE_VERSION=3.30.5
CMAKE_MINIMUM=3.24

TOOLS=.tools

if ! xcode-select -p >/dev/null 2>&1; then
  echo "Xcode Command Line Tools が要る。xcode-select --install を実行すること。" >&2
  exit 1
fi

# --- clang-format / clang-tidy ---
#
# LLVM の公式リリースから、要る分だけを取り出して置く。版が変わると整形結果も
# 指摘内容も変わるので、URL とハッシュで厳密に固定する。
#
# 配布物は 5GB あるが、実際に使うのは 2 つの実行ファイルと、clang-tidy が解析に
# 使う組み込みヘッダだけ。展開後 120MB に収まる。
llvm_dir="$TOOLS/llvm-$LLVM_VERSION"
if [ ! -x "$llvm_dir/bin/clang-format" ]; then
  arch=$(uname -m)
  if [ "$arch" != arm64 ]; then
    echo "arm64 用の配布物しかない（uname -m = $arch）。" >&2
    exit 1
  fi
  mkdir -p "$TOOLS"
  tarball="$TOOLS/llvm-$LLVM_VERSION.tar.xz"
  if [ ! -f "$tarball" ]; then
    echo "LLVM $LLVM_VERSION を取得する（800MB）..."
    curl -fL --progress-bar -o "$tarball.part" \
      "https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION/clang+llvm-$LLVM_VERSION-arm64-apple-macos11.tar.xz"
    mv "$tarball.part" "$tarball"
  fi
  echo "$LLVM_SHA256  $tarball" | shasum -a 256 -c -
  echo "展開する..."
  rm -rf "$llvm_dir.part"
  mkdir -p "$llvm_dir.part"
  prefix="clang+llvm-$LLVM_VERSION-arm64-apple-macos11"
  tar -xJf "$tarball" -C "$llvm_dir.part" --strip-components=1 \
    "$prefix/bin/clang-format" \
    "$prefix/bin/clang-tidy" \
    "$prefix/bin/run-clang-tidy" \
    "$prefix/lib/clang/${LLVM_VERSION%%.*}/include"
  mv "$llvm_dir.part" "$llvm_dir"
  rm -f "$tarball"
fi

# --- CMake ---
#
# 版差が成果物に出ないので、手元にあるものが新しければそれを使う。
# 無いか古ければ .tools/ に落とす。
cmake=$(command -v cmake || true)
if [ -n "$cmake" ]; then
  have=$("$cmake" --version | head -1 | sed 's/[^0-9.]*//')
  oldest=$(printf '%s\n%s\n' "$have" "$CMAKE_MINIMUM" | sort -V | head -1)
  if [ "$oldest" != "$CMAKE_MINIMUM" ]; then
    cmake=
  fi
fi
if [ -z "$cmake" ]; then
  cmake_dir="$TOOLS/cmake-$CMAKE_VERSION"
  if [ ! -x "$cmake_dir/CMake.app/Contents/bin/cmake" ]; then
    mkdir -p "$cmake_dir.part"
    echo "CMake $CMAKE_VERSION を取得する..."
    curl -fL --progress-bar \
      "https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION-macos-universal.tar.gz" \
      | tar -xzf - -C "$cmake_dir.part" --strip-components=1
    rm -rf "$cmake_dir"
    mv "$cmake_dir.part" "$cmake_dir"
  fi
  cmake="$PWD/$cmake_dir/CMake.app/Contents/bin/cmake"
fi
ctest="$(dirname "$cmake")/ctest"

# --- VST3 SDK ---
git submodule update --init --recursive

# --- 揃ったことを確かめる ---
"$cmake" -B build
"$cmake" --build build
"$ctest" --test-dir build --output-on-failure
"$cmake" --build build --target format-check
"$cmake" --build build --target tidy
