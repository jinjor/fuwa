#!/bin/sh
# 層の依存の向きを検査する。規約は docs/architecture.md の「依存の向き」。
#
# 規約は時間が経てば忘れる。忘れた頃にここが落ちる。
set -eu
cd "${1:-.}"

# 層ごとに、include してよい層。自分自身は常によい。
allow_model=""
allow_plugin=""
allow_audio=""
allow_engine="model plugin audio"
allow_api="engine model"
allow_app="api engine model audio plugin"

status=0
report() {
  echo "$1" >&2
  status=1
}

for file in $(find src \( -name '*.h' -o -name '*.cpp' -o -name '*.mm' \) | sort); do
  layer=$(echo "$file" | cut -d/ -f2)
  eval "allowed=\${allow_$layer-__unknown__}"
  if [ "$allowed" = __unknown__ ]; then
    report "$file: 層 '$layer' は docs/architecture.md にない。test/layers.sh を更新すること。"
    continue
  fi

  # VST3 SDK のヘッダを読めるのは plugin だけ。上位層に VST3 の型を漏らさないため。
  # <> でも "" でも見る。片方だけ見ていると、書き方を変えるだけで抜けられる。
  if [ "$layer" != plugin ] &&
     grep -qE '^#include [<"](pluginterfaces|public\.sdk|base)/' "$file"; then
    report "$file: VST3 SDK のヘッダを読めるのは plugin 層だけ"
  fi

  for included in $(grep -oE '^#include "[a-z]+/' "$file" |
                    sed 's/.*"//; s|/$||' | grep -vxE 'pluginterfaces|base' | sort -u); do
    case " $allowed $layer " in
      *" $included "*) ;;
      *) report "$file: $layer は $included を知らないはず" ;;
    esac
  done
done

if [ "$status" -eq 0 ]; then
  echo "層の依存の向きは守られている"
fi
exit "$status"
