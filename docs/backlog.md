# バックログ

Fuwa のバックログ。フェーズの計画は [roadmap.md](roadmap.md) にあり、ここはそれとは別に
「あとでやること」を積んでおく場所。バグも手直しも機能追加も同じ土俵で扱う。

## 読み方

- `Type`: `bug`, `polish`, `feature`, `investigate`
- `Now`: 今の作業に直結するもの
- `Next`: その次に効きそうなもの
- `Later`: 欲しいが、今すぐでなくてよいもの
- 詳細が要る item は `docs/backlog-details/<ID>-*.md` に置く
- 優先順位はユーザーが決める。勝手に並べ替えない
- 終わった item は行ごと消す。済んだ印は残さない。何をやったかは git log にある
- ID は使い回さない

## Now

| ID | Type | Item | Why now |
|---|---|---|---|

## Next

| ID | Type | Item | Notes |
|---|---|---|---|
| I3 | investigate | 商用音源が鳴らない | 読み込みは成功するのに無音になるものが多い。Retrologue と Surge XT は鳴る。調査記録は [I3-commercial-plugins.md](backlog-details/I3-commercial-plugins.md)。VST3 ホストを作っている動機そのものなので、MVP のレビューが終わったら最優先 |
| F1 | feature | 音源を発見する手段が無い | 仮の音源は AI が選ぶ方針（[purpose.md](purpose.md) の「人間は編集しない」）なのに、インストール済みの音源を一覧する API が無い。`fuwa plugins` は指定したバンドルの中を見るだけ。今は Skill が標準フォルダを `ls` させていて、フォルダのパスも Skill にベタ書きになっている。AI が音源を選ぶ前提が成立していない。[F1-instrument-discovery.md](backlog-details/F1-instrument-discovery.md) |
| B1 | bug | プラグインの `silenceFlags` を無視している | 無音のブロックを飛ばす最適化をしていない。正しさには影響しないが、トラックが増えると無駄が効いてくる |
| B2 | bug | レイテンシ補償がない | `getLatencySamples` を呼んでいない。1 トラックでは気づかないが、複数トラックにするとトラック間がズレる |
| B3 | bug | 出力バス 0 しか見ていない | マルチアウトの音源で 2 本目以降が取れない |
| B4 | bug | イベントバスが 0 決め打ち | 複数のイベント入力を持つ音源に届かない |

## Later

| ID | Type | Item | Notes |
|---|---|---|---|
| B5 | bug | `addRef` / `release` が常に 1 を返す | ホストがプラグインより長生きする前提で書いてある。寿命が絡む使い方をするなら数える必要がある |
| I1 | investigate | 壊れたプラグインからホストを守る | 今は in-process で `dlopen` していて、落ちれば fuwa ごと落ちる。スキャンを別プロセスにするか、`moduleinfo.json` で開かずに済ませるか。音源選択 UI を作るときに効いてくる |
| I2 | investigate | リアルタイム安全性を機械で検査する | `test/realtime.sh` は grep なので呼び出した先を見ない。RealtimeSanitizer が本物だが、コンパイラを Clang 20 以降にする必要がある |

## 決めていないこと

- なし
