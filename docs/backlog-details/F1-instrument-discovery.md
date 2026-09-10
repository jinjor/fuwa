# F1: 音源を発見する手段が無い

## なぜ要るか

[purpose.md](../purpose.md) の「人間は編集しない」で、仮の音源は AI が選ぶと決めている
（ピアノと言われたらそれらしいピアノを見繕う。当たるかは運なので、人間が後から UI で選び直す）。
AI が見繕うには、何がインストールされているかを知る必要がある。

## 今の状態

- `fuwa plugins <bundle.vst3>` は**指定したバンドルの中**を見るだけ。横断して並べる手段は無い
- そのうえ鳴らない音源が多いため（[I3](I3-commercial-plugins.md)）、`skills/fuwa/SKILL.md` は
  音源を Surge XT に固定している。**F1 と I3 の両方が片付かないと固定は外せない**

## macOS の VST3 の置き場所

SDK の `public.sdk/source/vst/hosting/module_mac.mm` の `Module::getModulePaths()` が探す場所。

```
~/Library/Audio/Plug-Ins/VST3          ユーザー（NSUserDomainMask）
/Library/Audio/Plug-Ins/VST3           システム（NSLocalDomainMask）
/Network/Library/Audio/Plug-Ins/VST3   ネットワーク（SDK では TODO のまま未実装）
<アプリ>.app/Contents/VST3             ホストアプリ自身に同梱されたもの
```

最後のものは実在する。Cubase 内蔵の Prologue や LoopMash は `Cubase 10.app/Contents/VST3` にいた。
ただし `getApplicationModules()` が見るのは**自分自身の**バンドルなので、他のアプリの中は探さない。

## 固定を外すときの下敷き

Surge XT への固定を入れる前に SKILL.md にあった文面。そのまま戻すのではなく、一覧 API が
できたら「フォルダを見る」の部分をそれに置き換える。

> ## 音源を選ぶ
>
> ユーザーがピアノと言ったら、ピアノらしい音のするものを見つけて差す。
>
> インストール済みの音源を一覧する手段は fuwa にまだ無い。macOS の VST3 はこの場所にある。
>
> ```
> ~/Library/Audio/Plug-Ins/VST3
> /Library/Audio/Plug-Ins/VST3
> /Network/Library/Audio/Plug-Ins/VST3
> <アプリ>.app/Contents/VST3
> ```
>
> `fuwa plugins <bundle.vst3>` で 1 つのバンドルの中身が分かる。バンドルは複数の音源を持ちうる。
>
> ```sh
> fuwa instrument main "/path/to/Some Synth.vst3" [class]
> ```
>
> クラス名を省くと、バンドルの最初の音源が選ばれる。
>
> 読み込めるのに音が出ないプラグインがある。`fuwa play` のピークが 0 で、`fuwa notes list` に
> ノートが見えているなら、ノートではなくプラグインを疑う。

## 決めていないこと

- 走査を誰がやるか。`fuwa` の起動時か、コマンドを叩いたときか
- バンドルを開かずに済ませるか（`moduleinfo.json` を読む）。開くと壊れたプラグインで落ちる（[I1](../backlog.md)）
- 一覧に何を出すか。名前だけか、サブカテゴリ（Synth / Piano / Drum など）も出すか
