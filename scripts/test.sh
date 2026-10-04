#! /usr/bin/env bash
set -euo pipefail

# qmk は compose.agent.yaml の qmk サービスのものを呼ぶ (lint.sh 参照)
cd "$(dirname "$0")/.."
project=windmill
# tests/ の直下が位置＝列の配線 (technik など)、tests/geonix41/ が geonix41 の配線。
# 入れ子のテストは -t windmill では走らないので並べて指定する
targets=("$project" "$project/geonix41")

# qmk test-c は -t に合うテストが無いと、黙って QMK の全テストを走らせる。
# tests/ がマウントされていない環境でそうならないよう、先に在るか確かめる
# (grep -q へパイプすると、qmk が SIGPIPE で落ちたとき pipefail で誤判定する)
tests=$(qmk test-c --list)
args=()
for target in "${targets[@]}"; do
  if ! grep -qx "$target" <<< "$tests"; then
    echo "🚨  qmk から tests/$target が見えません。compose.agent.yaml の qmk サービスで動いているか確かめてください" 1>&2
    exit 1
  fi
  args+=(-t "$target")
done

qmk test-c "${args[@]}"
