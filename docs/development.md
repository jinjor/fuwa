# 開発

設計は [architecture.md](architecture.md)。

## 環境

```sh
./setup.sh
```

道具を `.tools/` に置き、ビルドしてテストと整形と静的解析まで通す。マシンには何も
入れない（Xcode Command Line Tools だけは要る）。手を入れる前後にも走らせて緑を保つ。

`.tools/` に置くのは LLVM の clang-format と clang-tidy で、版は URL と sha256 で固定
してある。CMake は手元のものが 3.24 以上ならそれを使う。コンパイラは Command Line
Tools のもの。

## ビルド構成

CMake。書き方は「モダン CMake」に限る。

- 設定は `target_*` でターゲットに紐付ける。`include_directories` `add_definitions`
  `set(CMAKE_CXX_FLAGS ...)` などグローバルに効く命令は使わない
- 共通のコンパイルオプションは `fuwa_options`（INTERFACE ライブラリ）に集約する
- ソースは名指しする。`file(GLOB ...)` は整形対象の列挙にだけ使う
- `vendor/` のサードパーティは必要なソースだけ名指しして自前のターゲットにする。
  VST3 SDK 同梱の CMake は使わない

## 整形

```sh
cmake --build build --target format
cmake --build build --target format-check
```

`.clang-format` が唯一の設定。`SortIncludes` は切ってあるので、整形で挙動が変わることは
ない（include の並べ替えは意味を変えうる）。

## 静的解析

```sh
cmake --build build --target tidy
```

`.clang-tidy` が設定。バグを見つける系のチェックだけを有効にしている。

実行中に出る「N warnings generated.」は libc++ のヘッダ内で検出されて捨てられた分の数
なので無視してよい。指摘は `warning:` / `error:` の行に出る。

clang-tidy は Apple の libc++ を見つけられない。Command Line Tools がヘッダを SDK の中に
置いているのに探しに行かないため。`-isystem` で渡してある。

## サニタイザ

開発ビルドでは ASan と UBSan を常時有効にし、常に緑を保つ。

読み込んだプラグイン側の問題が報告されることがある。自分では直せないので抑制リストに
入れる。
