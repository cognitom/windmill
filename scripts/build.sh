#! /usr/bin/env bash
set -euo pipefail

# qmk は compose.agent.yaml の qmk サービスのものを呼ぶ (lint.sh 参照)
cd "$(dirname "$0")/.."
project=windmill

# geonix41 は QMK コアにパッチが要るので、ここでは作らない (scripts/geonix41/build.sh)
for keyboard in technik ymd40 minipeg48; do
  qmk compile -kb "$project/$keyboard" -km default
  # ファームウェアはコンテナの中の .build/ にできる。compose.agent.yaml が
  # そこをリポジトリの .build/ へマウントしているので、ホスト側から拾える
  cp ".build/${project}_${keyboard}_default.hex" "output/${project}_${keyboard}.hex"
done
