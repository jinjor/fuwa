#!/bin/sh
# リアルタイムスレッドで走るコードに、確保やロックが紛れていないか見張る。
# 規約は docs/architecture.md の「リアルタイムの制約」。
#
# 検査するのは `// realtime-begin` から `// realtime-end` までの範囲だけ。
# grep でしかないので、呼び出した先で確保していても気づかない。あくまで
# 「うっかり書いた」を止めるためのもので、安全の証明ではない。
# 本物の検査は RealtimeSanitizer（コンパイラを Clang 20 以降にできたら）。
set -eu
cd "${1:-.}"

# リアルタイム側に現れてはいけないもの。
forbidden='std::vector|std::string|std::map|std::function|new |delete |malloc|free\(|std::mutex|lock\(|lock_guard|printf|std::cout|throw |std::make_'

status=0
found=0
for file in $(find src \( -name '*.h' -o -name '*.cpp' -o -name '*.mm' \) | sort); do
  if ! grep -q 'realtime-begin' "$file"; then
    continue
  fi
  found=$((found + 1))
  if [ "$(grep -c 'realtime-begin' "$file")" != "$(grep -c 'realtime-end' "$file")" ]; then
    echo "$file: realtime-begin と realtime-end の数が合わない" >&2
    status=1
    continue
  fi
  hits=$(awk '/realtime-begin/ { inside = 1; next }
              /realtime-end/   { inside = 0; next }
              inside            { print FILENAME ":" FNR ": " $0 }' "$file" |
         grep -E "$forbidden" || true)
  if [ -n "$hits" ]; then
    echo "$hits" | sed 's/^/リアルタイム側で使えないものがある: /' >&2
    status=1
  fi
done

if [ "$found" -eq 0 ]; then
  echo "realtime-begin の付いた範囲がひとつもない。消したのなら test/realtime.sh も消すこと。" >&2
  exit 1
fi
if [ "$status" -eq 0 ]; then
  echo "リアルタイム側は $found ファイル、確保もロックもない"
fi
exit "$status"
