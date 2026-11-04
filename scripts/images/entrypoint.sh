#! /usr/bin/env bash
set -euo pipefail

# compose.agent.yaml の images サービスから使い捨てで呼ばれる (scripts/images/build.sh)
cd /windmill/docs/images

# README の PNG は 2 倍の大きさで持っている (742×291 → 1484×582)
for svg in layout-*.svg; do
  rsvg-convert -z 2 -o "${svg%.svg}.png" "$svg"
  # rootful の docker では root の持ち物になるので、リポジトリの持ち主に揃える
  chown "$(stat -c '%u:%g' /windmill)" "${svg%.svg}.png"
done
