#! /usr/bin/env bash
set -euo pipefail

# qmk は compose.agent.yaml の qmk サービスのものを呼ぶ (lint.sh 参照)
cd "$(dirname "$0")/.."
project=windmill

# qmk test-c は -t に合うテストが無いと、黙って QMK の全テストを走らせる。
# tests/ がマウントされていない環境でそうならないよう、先に在るか確かめる
# (grep -q へパイプすると、qmk が SIGPIPE で落ちたとき pipefail で誤判定する)
tests=$(qmk test-c --list)
if ! grep -qx "$project" <<< "$tests"; then
  echo "🚨  qmk から tests/$project が見えません。compose.agent.yaml の qmk サービスで動いているか確かめてください" 1>&2
  exit 1
fi

qmk test-c -t "$project"
