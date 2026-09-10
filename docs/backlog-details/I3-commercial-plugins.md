# I3: 商用音源が鳴らない — 調査記録

2026-09-11 に測った事実のみ。推測は「まだ分かっていないこと」に隔離してある。

## 測り方

- **発音**: オフラインでレンダリングしてバッファのピークを測る（スピーカーには出さない）。
  単音、pitch 60、120 BPM、48kHz、ブロック 512、断りがなければ長さ 1 拍
- **読み込み**: `fuwa plugins <bundle>`
- **注意**: `fuwa plugins` はクライアントで、プラグインを読むのは**サーバー側のプロセス**。
  クライアントのアーキテクチャは結果に関係しない。x86 の測定はサーバーごと x86 で立て直す必要がある

## 環境

| | |
|---|---|
| マシン | macOS arm64 |
| fuwa | arm64 ビルドと x86_64 ビルド（Rosetta）の両方で測定 |
| x86_64 ビルドの作り方 | `cmake -S . -B build-x86 -DCMAKE_OSX_ARCHITECTURES=x86_64 -DFUWA_SANITIZE=OFF && cmake --build build-x86` |
| Cubase 10 | 実行ファイルは x86_64 のみ |
| eLicenserCore.framework | x86_64 と arm64 の**両方**のスライスを持つ |
| Soft-eLicenser | `SeLicenser.sel` が存在 |

## プラグインのアーキテクチャ

| プラグイン | arm64 |
|---|---|
| HALion Sonic / The Grand 3 | あり |
| Steinberg Classics（CS40 / Neon / VB-1 / LM-7） | あり |
| Surge XT / Elemental Player / SINE Player | あり |
| HALion 6 / Padshop / Retrologue / Dark Planet / Hypnotic Dance / Triebwerk / Symphonic Orchestra | なし（x86_64 のみ） |
| Groove Agent | なし（i386 + x86_64） |

arm64 のホストから arm64 スライスを持たないバンドルを読むと、macOS が
`doesn't contain a version for the current architecture` を返す。

## 測定結果

x86_64 のサーバーで測定（断りがある行を除く）。

| プラグイン | 読み込み | 発音 |
|---|---|---|
| **Retrologue** | 成功 | **peak 0.1138**。14 ノートの曲をスピーカーから再生して確認済み |
| **Surge XT** | 成功 | 鳴る。ノートの位置・長さ・note off すべて正しいことを確認 |
| Padshop | 成功 | 0.0000 |
| HALion Sonic | 成功 | 0.0000 |
| HALion Sonic SE | 成功 | 0.0000 |
| HALion 6 | 成功 | 0.0000 |
| The Grand 3 | 成功 | 0.0000 |
| Dark Planet | 成功 | 0.0000 |
| Neon | 成功 | 0.0000（pitch 36/38/42/48/60/72、長さ 4 拍でも同じ） |
| CS40 / VB-1 | 成功 | 0.0000 |
| LM-7 | 成功 | 0.0000（pitch 36/38/42） |
| Prologue（Cubase の `SynthEngine.vst3`） | **失敗** `cannot initialize the controller` | — |
| Groove Agent SE | **失敗** `cannot initialize the component` | — |
| DSK World StringZ | 失敗（実行ファイルが読めない） | — |

Groove Agent / Triebwerk / Symphonic Orchestra / Hypnotic Dance / LoopMash は読み込みのみ確認、発音は未測定。
Mystic / Spector は `fuwa plugins` の一覧に現れない。

## ライセンス

- Steinberg Classics を Steinberg Activation Manager で**アクティベート済み**（「アクティブ」表示）。
  アクティベート後も Neon / CS40 / VB-1 / LM-7 は 0.0000 のまま
- Absolute 3 / Cubase Pro 10 / Cubase Studio 5 / Zero Downtime は **eLicenser**
- **Retrologue は eLicenser 製品で、鳴った**

## 規格適合の検査

- SDK の **hostchecker**: 今回の失敗する並び（ドレミファソ）を含めて **Error 0 / Warning 0**
- SDK の **validator**: vendored のソースだけで建つ。自己テスト 51/0
  - mda **1598/0 failed**、Surge XT 45/**2 failed**、JUCE AudioPluginDemo 47/0
- **SDK のテストスイートはプラグインに一度もノートを送らない**。`NoteOnEvent` が現れるのは
  構造体のサイズとアラインメントの検査だけ

## 効果がなかった実験

- 制御側が持つ全パラメータ値を最初のブロックで処理側へ送る → Neon / CS40 / VB-1 とも変化なし（コードは戻した）

## fuwa のホスト実装の現状

実装済み: `IHostApplication`（SDK の `HostApplication`、`IPlugInterfaceSupport` 込み）、
`IComponentHandler`、`IConnectionPoint`（処理側 ↔ 制御側の双方向、メッセージの覗き見）、
`getState` → `setComponentState`。

未実装: `IUnitInfo` / プログラムリスト、`.vstpreset` の読み込み、`IPlugView`、
再生中のパラメータ変更、レイテンシ補償、`silenceFlags`、出力バス 1 本目以外、イベントバス 0 以外。

## 関連して分かったこと（fuwa 以外の事実）

- **mda（SDK 同梱のサンプル音源）は、ブロック内 `sampleOffset` が 0 でない 16 の倍数のイベントを捨てる**。
  同じノートを 24000 フレーム目（オフセット 448）に置くと無音、24001 フレーム目に置くと鳴る。
  `SampleAccurateBaseProcessor::process` の `event.sampleOffset <= data.numSamples` が原因
- JUCE の `AudioPluginDemo` は全ノートが鳴るが note off が効かない
- 自作の JUCE プラグインで届いた MIDI を記録したところ、**fuwa は 5 ノートの on/off を
  正しい音程・フレーム・チャンネルで送っていた**
- 自作の JUCE サイン波シンセでは、ノートの位置・長さ・note off すべて正しく再生された

## まだ分かっていないこと

- Neon / CS40 / VB-1 / LM-7 が無音の理由。ライセンス、パラメータ供給、音程の範囲は否定済み
- HALion 系 / The Grand 3 / Padshop / Dark Planet が無音の理由
- Prologue / Groove Agent SE が `initialize` で失敗する理由
- 上の 3 つが同じ原因かどうか
- Mystic / Spector がどこにいるか

## この件が Skill に与えている制約

鳴る音源が Surge XT しか確かめられていないため、`skills/fuwa/SKILL.md` は音源を
**Surge XT に固定**している。本来は purpose.md の通り、仮の音源は AI が見繕う。
この調査が片付いたら固定を外す。外すときの文面は [F1](F1-instrument-discovery.md) に置いてある。

## 次の一手

JUCE のホスト実装（`juce_VST3PluginFormat.cpp`）を使う**ヘッドレスのプローブ**を書き、
同じプラグインを読ませて発音を測る。人の手は要らない。

- JUCE で鳴る → fuwa の穴。JUCE のソースと `src/plugin/vst3.cpp` を突き合わせられる
- JUCE でも無音 → fuwa 側の問題ではないことが分かる

なお Prologue / Groove Agent SE は Cubase.app と Steinberg の Components に置かれている
Cubase 内蔵音源。ホストを見て拒否している可能性がある場合、**ホスト名を偽って回避することはしない**。
