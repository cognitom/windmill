# 導入方法

## 準備

1. 対応キーボードを入手
2. Windmillの[コンパイル済みファームウェア](https://github.com/cognitom/windmill/releases)から、機種に合ったファイルをダウンロード

| キーボード | ファームウェア |
|--|--|
| Technik | `windmill_technik.hex` |
| YMD40 | `windmill_ymd40.hex` |
| Geonix48 | `windmill_minipeg48.hex` |
| Geonix Rev.2.5 | `windmill_geonix41.bin` |

## ファームウェアの書き込み

Windows (PC) からでも、Android (スマホ) からでも書き込めます。

### Windows から書き込む

#### Technik / YMD40 / Geonix48

[QMK Toolbox](https://github.com/qmk/qmk_toolbox/releases) をインストールしておきます。

1. QMK Toolboxからファームウェアファイル (`.hex`) を指定
2. キーボードを接続
3. 左右の<kbd>Fn</kbd>を両方押しながら<kbd>Enter</kbd>で書き込み可能なモードに<br>※v3.1.0 以前のファームウェアが入っている場合は <kbd>Fn</kbd> + <kbd>P</kbd><br>※あるいは、キーボード基板裏のリセットボタンを爪楊枝などで押す
4. `Flash`実行

![screenshot](images/qmk.png)

※なお、Macについては未検証なので、調整が必要かもしれません。

#### Geonix Rev.2.5

ブートローダーが USB ドライブとして見えるので、ファイルをコピーするだけで書き込めます。

1. <kbd>Esc</kbd>を押しながら、キーボードを接続
2. 現れた USB ドライブへ、エクスプローラーで `windmill_geonix41.bin` をコピー

### Android から書き込む

キーボードとは USB OTG (OTG ケーブルや変換アダプタ) でつなぎます。

#### Technik / YMD40 / Geonix48

1. スマホとキーボードを USB OTG でつなぐ
2. 左右の<kbd>Fn</kbd>を両方押しながら<kbd>Enter</kbd>で書き込み可能なモードに
3. Android の Chrome で [AVRFlashOnWeb](https://tmk.github.io/AVRFlashOnWeb/) を開き、ファームウェアファイル (`.hex`) を選んで書き込む

※Geonix48 で書き込めることを確かめています。

#### Geonix Rev.2.5

1. <kbd>Esc</kbd>を押しながら、スマホに USB OTG でつなぐ
2. 現れた USB ドライブへ、ファイルアプリで `windmill_geonix41.bin` をコピー

## IMEの設定

キーボードレイアウトは日本語 (JIS) と英語 (US) のどちらでも構いません。キーボード側は既定で日本語 (JIS) 向けのキーを送るので、英語 (US) にした場合は、左右の<kbd>Fn</kbd>を両方押しながら<kbd>X</kbd>で US に合わせます ([接続先の配列の切り替え](../README.md#接続先の配列の切り替え))。IMEの入力方式として「かな入力」を選択しておきます。

※以前のバージョンで必要だった、ローマ字変換テーブルの置き換えなどは不要です。

### Android

Android では、対象OSを Android に切り替えた上で ([対象OSの切り替え](../README.md#対象osの切り替え))、Android アプリ「Windmill 言語切り替え」を入れておきます。

- 入手先: [cognitom/windmill-android のリリース](https://github.com/cognitom/windmill-android/releases) (最初の版は [v0.1.0](https://github.com/cognitom/windmill-android/releases/tag/v0.1.0))
- 導入手順: [windmill-android の README](https://github.com/cognitom/windmill-android#導入) を見てください。APK から入れて、ユーザー補助サービスと英語用の IME を有効にします

かな/英数の切り替えで、キーボードはかなへは<kbd>変換</kbd>、英数へは<kbd>無変換</kbd>を送ります。Android 自体にはこれらのキーで入力言語を指定する仕組みが無いので、アプリがこのキーを受け取って IME を切り替えます。

| キー | アプリが切り替える先 |
|--|--|
| <kbd>変換</kbd> (かなへ) | 日本語側の IME。既定は Gboard で、アプリの設定画面で変えられます |
| <kbd>無変換</kbd> (英数へ) | 英語用の IME「Windmill 英語」 |

物理キーボードを外すと、日本語側の IME へ戻ります。アプリが無いと、Ctrl のタップでかな/英数が切り替わりません。
