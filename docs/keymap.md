# 実装メモ

以前のWindmillは、当時のQMKに無かった機能を `windmill.c` に自前実装していました
(独自の tap-hold「Mod Seq / Quick Tap」、`KA_*` 独自キーコードによるかな変換、
ローマ字かなエミュレーション、IME種別5種の切り替えなど)。

現在は、そのほとんどをQMK標準の機能に置き換えています。移植元は
[geonix41/minipeg48](https://github.com/cognitom/qmk_firmware_geonix41/tree/geonix41-customized-layout/keyboards/geonix41/minipeg48)
です。

## 何をQMK標準に置き換えたか

| 以前の独自実装 | 現在 |
|--|--|
| Mod Seq (`windmill_modtap` / `windmill_layertap` / `windmill_modlayertap`) | QMK標準の `MT()` / `LT()` + `hold_on_other_key_press` |
| Quick Tap (`SECOND_TAPPING_TERM` / `THIRD_TAPPING_TERM`) | 廃止。`tapping.term` のみ |
| `KA_*` 独自キーコード + `translate_kana_to_ascii()` | 廃止。レイヤー0を素のQWERTY+数字段にして、OSのIMEの「かな入力」に任せる |
| ローマ字かなエミュレーション (`JA_ROKA`) | 廃止 |
| IME種別5種 (`IME_WIN` 〜 `IME_IOS`) | `MY_WIN` / `MY_ANDR` の2種のみ (「」の出し分け用) |
| ALT/GUIのweakmod (単打時に修飾を送らない) | `LGUI_T()` / `LALT_T()` |
| かな入力時のCtrl+矢印の回避 | 廃止。Ctrlホールド中は英数レイヤーになるので不要 |
| `is_kana` のEEPROM保存 | 廃止。`default_layer_set()` で切り替える (EEPROMを書かない) |
| IME種別・言語モードのLEDインジケーター | 廃止 |

残っている独自実装は、LEDの配色まわりと、下記のキー処理だけです。

## 残っているキー処理 (`firmware/windmill.c`)

### `MY_LCTL` — tap: 英数 / double-tap: かな / hold: Ctrl + 英数レイヤー

QMKの Tap Dance ではなく手書きの状態機械 (`td_phase`) にしています。理由は、

- 別キーの割り込みで tapping term を待たずに即ホールド確定させたい (`pre_process_record_kb()`)
- tap / double-tap の確定時に `KC_LNG1`/`KC_LNG2` と一緒に `default_layer_set()` も呼びたい

ホールド中は Ctrl と同時に英数レイヤーを有効化するので、かな入力中でも
<kbd>Ctrl</kbd>+<kbd>C</kbd> などが英字のショートカットとして通ります。

### `process_shift_pair()` — Shiftで別の記号を出す

QMKの [Key Override](https://docs.qmk.fm/features/key_overrides) 相当ですが、
`my_shift_pairs[]` のテーブル引きで実装しています (移植元がフラッシュ節約のため
Key Override を無効化していたのを、そのまま持ってきたもの)。

`MY_W` 〜 `MY_A` は `keycode - MY_W` でテーブルを引くので、`windmill.h` の enum の
並び順を変えないこと。

### `process_thumb_shift()` — 親指Shift中のタップで半角スペース

`LSFT_T(KC_B)` / `RSFT_T(KC_N)` のホールドでShiftを開始したあと、そのキー自身を
タップすると半角スペースを出します。ただしShift開始後に他のキーを押していた場合
(`thumb_shift_dirty`) は、誤入力ガードとして何も出しません。

### Symレイヤーの数字・記号のIMEラップ

ベースがかな (レイヤー0) のときにSymレイヤーの数字・記号を打つと全角になってしまうので、
`KC_LNG2` → キー → `KC_LNG1` の順に送出します。IMEの切り替えが非同期なので、間に
`LT2_IME_WAIT_MS` (30ms) のウェイトを挟んでいます。効かない場合は増やし、
もたつくなら減らしてください。

## LEDの配色

キーコードの種類ごとに色を割り当てる独自実装で、こちらは以前のままです。

- `keymap.c` の `windmill_process_keycolor_user()` がキーコードを配色カテゴリに分類する
- `keymap.c` の `colorset[][6]` が `{明るい時のRGB, 暗い時のRGB}` を持つ
- 各キーボードの `.c` にある `lighting_map[]` が「行×列」の並びをLEDの番号に対応させる
- 起動時に全レイヤー分を `cached_keycolormap[]` に焼いておき、レイヤーが変わるたびに
  上のレイヤーから透過でない色を拾って `cached_keycolors[]` を作り直す

`MT()` / `LT()` は `windmill_base_keycode()` でタップ側のキーコードに展開してから分類します。
`S(KC_1)` などのシフト付きキーコードは展開せず、記号として分類します。

ベースレイヤー (かな/英数) の切り替えでも塗り直す必要があるので、`layer_state_set_kb()` と
`default_layer_state_set_kb()` の両方から更新しています。

キーボードごとの出力先は次の通り。

- Technik (RGB Matrix): `rgb_matrix_indicators_kb()` で毎フレーム描き直す
- YMD40 (RGBLight): レイヤーが変わったときなどに `rgblight_setrgb_at()` で直接書き込む
