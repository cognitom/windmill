# Windmill for Boardsource Technik-O

40%キーボード [Boardsource Technik-O](https://boardsource.xyz/store/5ffb9b01edd0447f8023fdb2)
向けの Windmill キーマップ。キー処理と配色は親フォルダの `windmill.c` を参照。

* MCU: atmega32u4 / ブートローダ: atmel-dfu
* LED: WS2812 x58 (キー下48 + アンダーグロー10)

ビルド:

    make windmill/technik:default

書き込み (PCB裏のリセットボタンを押してブートローダ起動、または Esc を押しながらUSB接続):

    make windmill/technik:default:flash
