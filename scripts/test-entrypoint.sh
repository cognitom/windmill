#! /usr/bin/env bash
set -euo pipefail

if [ ! -f /.dockerenv ]; then
    echo '🚨  Do not run it outside a Docker container.' 1>&2
    exit 1
fi

project=$1

# QMKのテスト基盤に2箇所だけ手を入れる (patches/qmk-test-harness.patch 参照)。
# コンテナは --rm の使い捨てなので、剥がす処理は要らない
git apply /patches/qmk-test-harness.patch

make "test:$project"
