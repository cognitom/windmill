# Geonix41 (ES32 FS026 / Cortex-M0)
#
# windmill で唯一 rules.mk / config.h を持つ機種。ES32 は QMK が知らない MCU なので、
# keyboard.json だけでは ChibiOS のボード設定を書けない。

# Board: <chibios-contrib>/os/hal/boards/FS026
BOARD = FS026

MCU  = cortex-m0
ARMV = 6

MCU_FAMILY = ES32
MCU_SERIES = FS026

MCU_LDSCRIPT ?= FS026
MCU_STARTUP  ?= FS026

USE_FPU ?= no

# EEPROMは rdr_lib (librdrcommon.a) の実装を使う
EEPROM_DRIVER = custom
NO_USB_STARTUP_CHECK = yes
BLUETOOTH_CUSTOM = yes

DEBOUNCE_TYPE = asym_eager_defer_pk

# ES32 の CMSIS ヘッダ (chibios-contrib 同梱の system_fs026.h) はインクルードガードの
# マクロ名が綴り違いで、GCC 15 の -Wheader-guard に引っかかる。ベンダー由来のコードなので
# 直さず警告だけ落とす。QMK は -Werror なので、これがないとビルドが止まる。
CFLAGS += -Wno-error=header-guard

# ベンダーブロブ。無線 (BLE/2.4G)・電源管理・LEDドライバと、キースキャン後段の
# Key_Value_Dispose() を提供する。scripts/fetch-vendor-blob.py がリポジトリの vendor/ に
# 置き、build.sh が QMK ツリーの lib/rdr_lib/ へマウントする。
#
# 単一オブジェクト (rdr_common.o) なので、リンクすると del_key_from_report() など
# コア関数の実装も一緒に入ってくる。patches/qmk-core-rdr-lib.patch を当てた
# report.c 側では同名の定義を落としてあり、この行とパッチは対で扱うこと。
LDFLAGS += -L$(LIB_PATH)/rdr_lib -lrdrcommon
