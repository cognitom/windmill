# Windmill for YMDK YMD40 v2

40%キーボード YMDK YMD40 v2 (キー下RGB 51灯版) 向けの Windmill キーマップ。
キー処理と配色は親フォルダの `windmill.c` を参照。

* MCU: atmega32u4 / ブートローダ: atmel-dfu
* LED: WS2812 x51 + シングルカラーのバックライト

本家QMKの `ymdk/ymd40/v2` は RGB が 8灯の想定になっており実機と合わないため、
このリポジトリで独自にキーボード定義を持っている。

ビルド:

    make windmill/ymd40:default

書き込み:

    make windmill/ymd40:default:flash
