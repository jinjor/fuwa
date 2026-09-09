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

道具立てと決まりごとは [docs/development.md](docs/development.md)、
設計は [docs/architecture.md](docs/architecture.md)。
