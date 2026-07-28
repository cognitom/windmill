#! /usr/bin/env bash
set -euo pipefail

if [ ! -f /.dockerenv ]; then
    echo '🚨  Do not run it outside a Docker container.' 1>&2
    exit 1
fi

host_uid=$(ls -n "$0" | awk '{print $3}') # study who my owner is
host_gid=$(ls -n "$0" | awk '{print $4}') # study what I belong to

# 素のQMKで通る機種。geonix41 より先にビルドすること (下のコメント参照)
for keyboard in technik ymd40 minipeg48; do
  qmk lint -kb "windmill/$keyboard" -km default --strict
  make "windmill/$keyboard:default"
  mv "windmill_${keyboard}_default.hex" "/output/windmill_${keyboard}.hex"
done

# ここから先は素のQMKではなくなる。
#
# geonix41 のベンダーブロブ (librdrcommon.a) は HIDレポートの送出経路を QMK標準から
# 丸ごと差し替えるため、コアにパッチが要る。しかもブロブは単一オブジェクトなので、
# リンクすると del_key_from_report() などコア関数の実装まで持ち込まれる。
# パッチ側ではその同名定義を落としてあり、**この状態で geonix41 以外をビルドすると
# 未定義参照で落ちる**。だから最後に回している。
#
# コンテナは --rm の使い捨てなので、パッチを剥がす処理は要らない。
echo '--- applying vendor patches (QMK core is no longer pristine from here) ---'
git apply /patches/qmk-core-rdr-lib.patch
git -C lib/chibios-contrib apply /patches/es32-fs026-geonix41.patch

# geonix41 の lint は debounce の指摘1件だけ既知の誤検知として見逃す。
# (消すと起動時の待ちが変わる。firmware/geonix41/readme.md 参照)
# それ以外の指摘が出たらちゃんと落とす。
lint_out=$(qmk lint -kb "windmill/geonix41" -km default --strict 2>&1 || true)
echo "$lint_out"
# ☒ の行から、既知の1件と最後のまとめ行を除いて、まだ残るものがあれば落とす
if echo "$lint_out" | grep '☒' \
     | grep -v 'duplicates default value of "5"' \
     | grep -qv 'Lint check failed for'; then
  echo '🚨 geonix41 の lint に既知以外の指摘がある' 1>&2
  exit 1
fi

make "windmill/geonix41:default"
mv "windmill_geonix41_default.bin" "/output/windmill_geonix41.bin"

chown "$host_uid:$host_gid" /output/*.hex /output/*.bin
