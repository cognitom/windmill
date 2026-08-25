# ユニットテスト

QMK のテスト基盤 (`make test:<name>`, gtest/gmock) に `firmware/windmill.c` を
そのまま載せて、**ホストへ送出されるHIDレポート列**を検証する。実機もエミュレータも要らない。

```bash
$ bash scripts/test.sh
```

`scripts/build.sh` と同じ Docker イメージ (`scripts/Dockerfile` の `QMK_VERSION`) を使う。
geonix41 のベンダーブロブは使わないので、取得は走らない。

## なぜレポート列を見るのか

issue #18 は「言語切り替え直後の1打鍵目だけ Shift が効かず、`、` ではなく `ね` が出る」というもので、
原因は `process_shift_pair()` が

```
実Shiftを外す → shifted 側の weak Shift を付ける → 外す
```

と修飾を入れ替えていたことだった。ホストから見ると1打鍵目だけ

```
[RSFT]  →  [LSFT]  →  [LSFT + KC_COMM]     ← 右Shiftを離して左Shiftを押す1本が挟まる
```

となる。**送出されるキーコード自体は正しい**ので、キーコードだけ突き合わせても気づけない。
だからこのテストは `EXPECT_REPORT` でレポートを1本ずつ固定している。

## 間隔まで見ることがある

issue #36 は、そのレポート列すら1打鍵目と2打鍵目で完全に同じで、**違うのは送出の間隔だけ**だった。
親指Shiftのホールドが別キー割り込みで確定する (`HOLD_ON_OTHER_KEY_PRESS`) ため、1打鍵目は
`[LSFT]` → `[]` → `[KC_MINS]` が全て同じスキャンで出てしまい、実機のIMEがShiftの上げ下げを
取りこぼしていた。この手の不具合はレポート列では固定できないので、
`EXPECT_REPORT(...).WillOnce(...)` で `timer_read()` を控えて間隔も突き合わせる
(`ShiftPair.unshifted_pair_waits_for_ime_on_first_keypress`)。

「こ」+「み」同時押しの半角スペースも同じ罠だった
(`ThumbShift.space_drops_shift_in_its_own_report`)。こちらは `del_mods()`/`set_mods()` が
レポートを送らないぶん、1本目が `[LSFT]` → `[KC_SPC]` と「Shiftを離す」と「Spaceを押す」を
まとめた形になっていて、レポート列と間隔の両方を見る必要がある。

## レイヤーもレポートで見る

issue #34 の「Ctrl / Win / Alt のホールド中だけ英数レイヤーへ移す」は、`layer_state` を
直接覗くのではなく**「ぬ」の位置を打って出力を見る**ことで判定している (`test_hold_layer.cpp`)。
かなレイヤーでは `KC_1`、英数レイヤーでは `KC_Q` なので、どちらで解決されたかがレポートに出る。
実装の内部状態ではなく、打鍵したときにホストへ何が届くかを固定するため。

## 構成

| ファイル | 中身 |
|--|--|
| `config.h` | マトリクスサイズと tapping 設定。実機の `keyboard.json` / `config.h` と揃える |
| `test.mk` | `firmware/windmill.c` をテストへリンクする |
| `test_keymap.hpp` | テスト用キーマップと `WindmillTest` フィクスチャ |
| `test_shift_pair.cpp` | 親指Shift + `process_shift_pair()` のレポート列 |
| `test_kana_qmark.cpp` | かなレイヤーの「も」でのShift+タップ (半角`?`) のレポート列 |
| `test_thumb_shift.cpp` | 左右の親指Shiftの持ち替え (ハンドオーバー) と同時押しスペースのレポート列 |
| `test_alpha_thumb_shift.cpp` | 英数レイヤーの親指Shift (左右とも同じキーコード) の持ち替え (ハンドオーバー) のレポート列 |
| `test_hold_layer.cpp` | Ctrl / Win / Alt のホールド中だけ英数レイヤーへ移ることのレポート列 |

`test_keymap.hpp` のキーマップは `firmware/technik/keymaps/default/keymap.c` と同じ内容。
実機側は `LAYOUT_ortho_4x12` マクロと PROGMEM に依存していてそのままは読めないため、
ここだけ二重管理になっている。**キー配置を変えたら両方直すこと。**

## QMK 側へのパッチ

`patches/qmk-test-harness.patch` で `tests/test_common/` に2箇所だけ手を入れている。
コンテナは使い捨てなので剥がす処理はない。

- `matrix.c`: `matrix_scan_kb()` を weak にして `matrix_scan_user()` を呼ぶようにする。
  windmill.c が `matrix_scan_kb()` を実装しているため、そのままだと多重定義になる
- `test_common.h`: `MATRIX_ROWS` / `MATRIX_COLS` を `#ifndef` で囲む。
  既定が 4x10 なので、実機と同じ 4x12 にできない
