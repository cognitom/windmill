#! /usr/bin/env bash
set -euo pipefail

# qmk は compose.agent.yaml の qmk サービスのものを呼ぶ。コンテナ越しの呼び出しは
# ホストの cwd をコンテナ側へ読み替えるので、リポジトリの中から呼ぶ
cd "$(dirname "$0")/.."
project=windmill

# lint は keyboard.json とレイアウト定義しか見ないので、
# geonix41 もベンダーブロブやコアへのパッチ無しでここで見られる
for keyboard in technik ymd40 minipeg48; do
  qmk lint -kb "$project/$keyboard" -km default --strict
done

# geonix41 の lint は debounce の指摘1件だけ既知の誤検知として見逃す。
# (消すと起動時の待ちが変わる。firmware/geonix41/readme.md 参照)
# それ以外の指摘が出たらちゃんと落とす。
lint_out=$(qmk lint -kb "$project/geonix41" -km default --strict 2>&1 || true)
echo "$lint_out"
# ☒ の行から、既知の1件と最後のまとめ行を除いて、まだ残るものがあれば落とす
if echo "$lint_out" | grep '☒' \
     | grep -v 'duplicates default value of "5"' \
     | grep -qv 'Lint check failed for'; then
  echo '🚨 geonix41 の lint に既知以外の指摘がある' 1>&2
  exit 1
fi
echo 'Ψ geonix41 は既知の1件だけなので通過とみなす'
