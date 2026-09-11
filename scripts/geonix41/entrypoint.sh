#! /usr/bin/env bash
set -euo pipefail

if [ ! -f /.dockerenv ]; then
    echo '🚨  Do not run it outside a Docker container.' 1>&2
    exit 1
fi

# compose.agent.yaml の geonix41 サービスから使い捨てで呼ばれる (scripts/geonix41/build.sh)
cd /qmk_firmware

# ここから先は素のQMKではなくなる。
#
# geonix41 のベンダーブロブ (librdrcommon.a) は HIDレポートの送出経路を QMK標準から
# 丸ごと差し替えるため、コアにパッチが要る。しかもブロブは単一オブジェクトなので、
# リンクすると del_key_from_report() などコア関数の実装まで持ち込まれる。
# パッチ側ではその同名定義を落としてあり、**この状態で geonix41 以外をビルドすると
# 未定義参照で落ちる**。常駐の qmk サービスと分けているのはこのため。
#
# コンテナは --rm の使い捨てなので、パッチを剥がす処理は要らない。
git apply /windmill/patches/qmk-core-rdr-lib.patch
git -C lib/chibios-contrib apply /windmill/patches/es32-fs026-geonix41.patch

make "windmill/geonix41:default"
mv "windmill_geonix41_default.bin" "/windmill/output/windmill_geonix41.bin"

# rootful の docker では root の持ち物になるので、リポジトリの持ち主に揃える
chown "$(stat -c '%u:%g' /windmill)" /windmill/output/windmill_geonix41.bin
