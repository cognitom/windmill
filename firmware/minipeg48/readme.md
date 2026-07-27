# Windmill for minipeg48

40%キーボード [sporewoh minipeg48](https://github.com/ChrisChrisLoLo/minipeg48)
(Pro Micro互換・4x12オーソリニア) 向けの Windmill キーマップ。
キー処理は親フォルダの `windmill.c` を参照。

* MCU: atmega32u4 / ブートローダ: atmel-dfu
* LED: なし

LED非搭載なので `windmill.c` の配色処理は `WINDMILL_LED_ENABLE` によって
コンパイルから外れる。あわせて、レイヤー3の `MY_DARK` (明るさ 強/弱) も置いていない。

キーボード定義は
[cognitom/qmk_firmware_geonix41](https://github.com/cognitom/qmk_firmware_geonix41/tree/geonix41-customized-layout/keyboards/geonix41/minipeg48)
から持ってきたもの。本家QMKには収録されていない。

ビルド:

    make windmill/minipeg48:default

書き込み (Escキー = マトリクス(0,0) を押しながらUSB接続でブートローダ起動。
効かない場合はPCB裏のリセットボタン。レイヤー3の `QK_BOOT` でも入れる):

    make windmill/minipeg48:default:flash
