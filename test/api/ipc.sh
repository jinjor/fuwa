#!/bin/sh
# CLI からサーバまでを一続きに通す。ソケットを本当に張って確かめるのはここだけ。
#
#   $1  fuwa の実行ファイル
#   $2  音源のバンドル
#   $3  MIDI ファイル
set -eu

fuwa=$1
bundle=$2
midi=$3

# Unix domain socket のパスは 100 バイト強しか入らないので、短いところに作る。
work=$(mktemp -d /tmp/fuwa-test.XXXXXX)
FUWA_SOCKET=$work/s
export FUWA_SOCKET

server=
cleanup() {
  if [ -n "$server" ]; then
    kill "$server" 2>/dev/null || true
  fi
  rm -rf "$work"
}
trap cleanup EXIT

status=0

# check <説明> <期待する終了コード> <出力に含まれるべき文字列（無ければ ""）> <コマンド...>
check() {
  what=$1
  want=$2
  contains=$3
  shift 3
  out=$("$@" 2>&1) && code=0 || code=$?
  if [ "$code" != "$want" ]; then
    echo "$what: 終了コードが $want のはずが $code" >&2
    echo "$out" >&2
    status=1
    return
  fi
  if [ -n "$contains" ] && ! printf '%s\n' "$out" | grep -q "$contains"; then
    echo "$what: 出力に $contains が無い" >&2
    echo "$out" >&2
    status=1
  fi
}

"$fuwa" serve >"$work/log" 2>&1 &
server=$!

# 待ち受けが立つまで待つ。立たなければ諦める。
waited=0
while [ ! -S "$FUWA_SOCKET" ]; do
  waited=$((waited + 1))
  if [ "$waited" -gt 100 ]; then
    echo "サーバが立たなかった" >&2
    cat "$work/log" >&2
    exit 1
  fi
  sleep 0.1
done

check "tracks"                0 "main"        "$fuwa" tracks
check "help"                  0 "notes set"   "$fuwa" help
check "知らないコマンド"      2 "no such"     "$fuwa" wobble
check "引数が足りない"        2 "usage"       "$fuwa" notes set
check "無いトラック"          1 "no such"     "$fuwa" notes list nosuchtrack
check "壊れた小節指定"        2 "bar range"   "$fuwa" notes list main 20-17
check "無いファイル"          1 "cannot open" "$fuwa" notes set main 1 "$work/nope.mid"
check "MIDI でないファイル"   1 "not a MIDI"  "$fuwa" notes set main 1 "$work/log"
check "音源を差す前の再生"    1 "no instrument" "$fuwa" play

check "notes set"             0 "5 notes in"  "$fuwa" notes set main 17-20 "$midi"
check "notes list"            0 "^17 "        "$fuwa" notes list main 17
check "範囲の外"              0 "no notes"    "$fuwa" notes list main 1
check "status"                0 "120.0 BPM"   "$fuwa" status
check "notes clear"           0 "removed 5"   "$fuwa" notes clear main 17-20
check "消えたこと"            0 "no notes"    "$fuwa" notes list main 17

check "instrument"            0 "mda"         "$fuwa" instrument main "$bundle"
check "instrument が載った"   0 "mda"         "$fuwa" tracks

# 引数に改行や引用符が入っても、行が割れずにそのまま届く。
check "厄介な引数"            1 "no such track" "$fuwa" notes list "$(printf 'a\nb "c"')"

check "quit"                  0 "bye"         "$fuwa" quit
wait "$server" 2>/dev/null || true
server=

if [ -e "$FUWA_SOCKET" ]; then
  echo "quit の後にソケットが残っている" >&2
  status=1
fi

check "止まった後" 1 "not running" "$fuwa" tracks

# 繋がらない理由を取り違えないこと。権限で弾かれたのを「起動していない」と言うと、
# 呼んだ側が動いているサーバーを止めて立て直そうとする。root では chmod が効かないので飛ばす。
if [ "$(id -u)" != 0 ]; then
  locked=$work/locked
  mkdir "$locked"
  : > "$locked/s"
  chmod 000 "$locked"
  out=$(FUWA_SOCKET=$locked/s "$fuwa" tracks 2>&1) && code=0 || code=$?
  chmod 755 "$locked"
  if [ "$code" != 1 ] || printf '%s\n' "$out" | grep -q "not running"; then
    echo "権限で弾かれたのに「起動していない」と言っている" >&2
    echo "$out" >&2
    status=1
  fi
fi

if [ "$status" -eq 0 ]; then
  echo "CLI からサーバまで通っている"
fi
exit "$status"
