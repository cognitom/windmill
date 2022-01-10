![cover](docs/images/cover.png)

# Windmill
Windmill is a keymap for 40% keyboards.

このキー配列は、40%キーボード向けに作成したものです。

- 風車状のカーソル配置
- かな入力対応 (ほぼJISかな配列)
- SandS (Space and Shift)
- [Boardsource Technik](https://boardsource.xyz/store/5ffb9b01edd0447f8023fdb2)用の[ファームウェア](https://github.com/cognitom/windmill/releases)作成済み

詳しくは、以下をどうぞ。

- [キー配列 (↓)](#キー配列)
- [追加機能 (↓)](#追加機能)
- [導入方法](docs/install.md)
- [ファームウェアの作成](docs/build.md)

## キー配列

### 英字入力時

- ESC(✕)とEnter(○)が対称配置
- BSが最左列
- Fn, Sym, Shiftは、ホールド時に有効

![main](docs/images/layout-main.png)

### かな入力時

- 英語配列では修飾キーが並ぶ最下段も含めて、フルに4段を使う
- 「ほ」「へ」「む」「ー」のみシフト側へ
- 「こ」「み」同時押しでスペースキー
- GUI, Alt, Fn, Sym, Shiftは、ホールド時に有効 [※補足](docs/README.md#ALT,GUIは、修飾キーかつ単打である問題)
- ESCで英語配列に戻る

![kana](docs/images/layout-kana.png)

### 記号とファンクションキー

英字入力、かな入力ともに、最下段中央付近のキーをホールドすると、記号(Sym)またはファンクションキー(Fn)の入力になります。配置については英字配列の図で、各キーの添え字を参照。

| 最下段中央 | L3 | L2 | L1 | R1 | R2 | R3 |
|--|:--:|:--:|:--:|:--:|:--:|:--:|
| タップ (英数) | 英数 | \ | Space | Space | / | かな |
| タップ (かな) | そ | ひ | こ | み | も | ね |
| ホールド | Fn | Sym | Shift | Shift | Sym | Fn |

## 追加機能

### ダークモード

デフォルトの状態はLEDが明るいので、暗い部屋で使う場合に光量を落として使えるダークモードを用意しています。使い方は、次の通り。

- <kbd>Fn</kbd> + <kbd>Enter</kbd> を押す

ダークモードを解除するには、もう一度上記のキーを押します。
