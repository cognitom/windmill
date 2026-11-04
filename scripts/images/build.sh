#! /usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.." # リポジトリの直下へ移る

# docs/images/layout-*.svg から同名の PNG を作る。フォント入りの使い捨てコンテナで回す
# (compose.agent.yaml の images サービス)。CI でも同じに動くよう tty は取らない
docker compose -f compose.agent.yaml run --rm -T images
