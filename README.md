# Fuwa

Vibe Coding の思想を音楽制作に適用した DAW。

作者個人のためのプロジェクトです。コードは公開していますが、Issue / Pull Request は受け付けていません。

## ビルド

macOS 専用。Xcode Command Line Tools が要る。

```sh
./setup.sh
```

道具を `.tools/` に置き、ビルドしてテストまで通す。以降は:

```sh
cmake --build build
ctest --test-dir build
cmake --build build --target format
```

## 使う

**UI はまだ無い。** 本来ユーザーが触るのは画面（トラック一覧・トランスポート・ミキサー・
セッションパネル）で、UI もエージェントも同じ API のクライアントになる。詳しくは
[docs/purpose.md](docs/purpose.md)。以下は API しか無い現段階の使い方。

実行ファイルは `build/src/fuwa`。PATH の通ったところに symlink しておくと楽。
サーバを建てて、別のシェルから操作する。

```sh
fuwa serve                                                     # 待ち受ける
fuwa instrument main ~/Library/Audio/Plug-Ins/VST3/"Surge XT.vst3"  # 音源を差す
fuwa notes set main 1-4 melody.mid                             # 小節範囲を置き換える
fuwa play
```

エージェントに使い方を教えるための Skill が [skills/fuwa](skills/fuwa) にある。
曲を作る用のディレクトリを作って、そこから使う。

```sh
# -n を付けること。付けないと、張り直したときにリンクが symlink の「中」に作られる
ln -sfn ~/projects/fuwa/skills/fuwa ~/.claude/skills/fuwa   # Claude Code
ln -sfn ~/projects/fuwa/skills/fuwa ~/.codex/skills/fuwa    # Codex
```

fuwa のリポジトリの中でエージェントを起動しないこと。ソースと `CLAUDE.md` が目に入って、
Skill を使わずソースを読みに行く。

道具立てと決まりごとは [docs/development.md](docs/development.md)、
設計は [docs/architecture.md](docs/architecture.md)。
