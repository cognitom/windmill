#! /usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.." # リポジトリの直下へ移る

# geonix41 のベンダーブロブ。リポジトリには置けないので毎回ここで揃える
# (取得済みなら何もしない)
python3 ./scripts/geonix41/fetch-vendor-blob.py

# コアにパッチを当てるので、常駐の qmk サービスではなく使い捨てのコンテナで回す
# (compose.agent.yaml の geonix41 サービス)。CI でも同じに動くよう tty は取らない
docker compose -f compose.agent.yaml run --rm -T geonix41
