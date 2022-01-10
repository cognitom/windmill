![cover](docs/images/cover.png)

# Windmill
Windmill is a keymap for 40% keyboard.

このキー配列は、40%キーボード向けに作成したものです。

- 風車状のカーソル配置
- かな入力対応 (ほぼJISかな配列)
- SandS (Space and Shift)
- [Boardsource Technik](https://boardsource.xyz/store/5ffb9b01edd0447f8023fdb2)用のファームウェア作成済み

## キー配列

### 英字入力時

- ESC(✕)とEnter(○)が対称配置
- BSが最左列
- 下段中央の6キーは、それぞれホールドでShift/Sym/Fnの割り当て

![main](docs/images/layout-main.png)

### かな入力時

- 英語配列では修飾キーが並ぶ最下段も含めて、フルに4段を使う
- 「ほ」「へ」「む」「ー」のみシフト側へ
- 「こ」「み」同時押しでスペースキー
- ESCで英語配列に戻る

![kana](docs/images/layout-kana.png)

### 記号とファンクションキー

英字入力、かな入力ともに、最下段中央付近のキーをホールドすると、記号(Sym)またはファンクションキー(Fn)の入力になる。配置については英字配列の図で、各キーの添え字を参照。

| 最下段中央 | L3 | L2 | L1 | R1 | R2 | R3 |
|--|:--:|:--:|:--:|:--:|:--:|:--:|
| タップ (英数) | 英数 | \ | Space | Space | / | かな |
| タップ (かな) | そ | ひ | こ | み | も | ね |
| ホールド | Fn | Sym | Shift | Shift | Sym | Fn |

## かな入力のセットアップ

IMEの入力方式として「かな入力」を選択しておきます。

※以前のバージョンで必要だった、ローマ字変換テーブルの置き換えなどは不要です。

## 導入方法

### 準備

1. [Boardsource Technik](https://boardsource.xyz/store/5ffb9b01edd0447f8023fdb2)を入手
2. PC/Macに[QMK Toolbox](https://github.com/qmk/qmk_toolbox/releases)をインストール
3. Windmillの[コンパイル済みファームウェア](https://github.com/cognitom/windmill/releases)をダウンロード

### ファームウェアの書き込み

1. QMK Toolboxからファームウェアファイルを指定
2. キーボードを接続
3. キーボードのリセットボタンを爪楊枝などで押す
4. `Flash`実行

![screenshot](docs/images/qmk.png)

※なお、Macについては未検証なので、調整が必要かもしれません。

## ファームウェアの作成

[コンパイルしたもの](https://github.com/cognitom/windmill/releases)も用意していますが、自分で作成することも可能です。

必要な環境は以下の通り。

- Windows(WSL2)、または Mac
- [QMK Toolbox](https://github.com/qmk/qmk_toolbox/releases)
- Docker

上記整っていれば、QMKのドキュメントにある[セットアップ手順](https://docs.qmk.fm/#/newbs_getting_started)は不要。このリポジトリをクローンして`build.sh`を実行すればOKです。

```bash
$ git clone https://github.com/cognitom/windmill.git
$ cd windmill
$ bash scripts/build.sh
```

`output`ディレクトリ内にファームウェアが作成されます。
