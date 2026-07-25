# 導入方法

## 準備

1. [Boardsource Technik](https://boardsource.xyz/store/5ffb9b01edd0447f8023fdb2) または YMDK YMD40 v2 を入手
2. PC/Macに[QMK Toolbox](https://github.com/qmk/qmk_toolbox/releases)をインストール
3. Windmillの[コンパイル済みファームウェア](https://github.com/cognitom/windmill/releases)をダウンロード
   (Technikは `windmill_technik.hex`、YMD40は `windmill_ymd40.hex`)

## ファームウェアの書き込み

1. QMK Toolboxからファームウェアファイルを指定
2. キーボードを接続
3. キーボードのリセットボタンを爪楊枝などで押す
4. `Flash`実行

![screenshot](images/qmk.png)

※なお、Macについては未検証なので、調整が必要かもしれません。

## IMEの設定

IMEの入力方式として「かな入力」を選択しておきます。ファームウェア側ではかなへの変換をせず、
標準のJISかな配置がそのまま出るようにしているためです。

## Androidに繋いで使う場合

Android側の「物理キーボードのレイアウト」を **「英語（アメリカ）」** にしておくこと
(設定 → システム → 言語と入力 → 物理キーボード → 該当キーボード → キーボードレイアウトを設定、
から日本語IMEの項目を開いて選択する)。

このキーボードはUS配列 (ANSI) としてキーコードを送るため、日本語IMEを使う場合でも
レイアウトは「日本語 109A 配列」ではなく「英語（アメリカ）」が正しい。109A配列のままだと
Android側がJIS配列として解釈するので、`@` `[` `]` `:` などの記号がキーマップどおりに入りません。

あわせて、Fnレイヤー最上段の左から4番目 (`MY_ANDR`) を押してAndroidモードにしておくこと
(<kbd>Shift</kbd>+<kbd>O</kbd> / <kbd>Shift</kbd>+<kbd>P</kbd> の「」がAndroidのIME向けの
キーコードに切り替わります)。Windows等に戻すときは最上段の左端 (`MY_WIN`)。
