# ユニットテスト

QMK のテスト基盤 (`make test:<name>`, gtest/gmock) に `firmware/windmill.c` を
そのまま載せて、**ホストへ送出されるHIDレポート列**を検証する。実機もエミュレータも要らない。

```bash
$ bash scripts/test.sh
```

`compose.agent.yaml` の `qmk` サービスの中で `qmk test-c` を走らせる
(QMKのバージョンは `Dockerfile` の `QMK_VERSION`)。geonix41 のベンダーブロブは使わない。

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
| `test_key_output.cpp` | 記号とかなのキー (独自キーコード `KN_*` / `SY_*`) の出力を、かな・英数・記号の3レイヤーの全キーぶん総当たりで見る。期待値は独自キーコードにする前の出力そのもので、出力表 (`key_outputs[]`) の書き漏らしや取り違えを拾う。押しっぱなしでリピートが効くこと、記号を続けて打っても Shift が次のキーへ漏れないことも見る (issue #73) |
| `test_kana_qmark.cpp` | かなレイヤーの「も」でのShift+タップ (半角`?`) のレポート列 |
| `test_thumb_shift.cpp` | 左右の親指Shiftの持ち替え (ハンドオーバー) と同時押しスペースのレポート列 |
| `test_alpha_thumb_shift.cpp` | 英数レイヤーの親指Shift (左右とも同じキーコード) の持ち替え (ハンドオーバー) のレポート列 |
| `test_hold_layer.cpp` | Ctrl / Win / Alt のホールド中だけ英数レイヤーへ移ることのレポート列 |
| `test_lang_toggle.cpp` | Ctrl (`MY_LCTL`) のタップでの言語切替。OSごとの送るキーとベースレイヤーの反転、Fn+Ctrl (`MY_IME`) がIMEだけを切り替えることのレポート列 |
| `test_conf_layer.cpp` | 左右のFnを両方ホールドしている間だけ設定レイヤーへ移ること。片方を離したときにFnレイヤーへ戻ること |
| `test_fn_layer.cpp` | Fnレイヤーのファンクションキーが数字キーに準じた位置にあること。最左列の Esc が透過のまま出ること、Fn+Tab が Caps Lock になること |
| `test_android_mods.cpp` | 対象OSが Android のときの Win / Alt。重ね押しで後から押したほうが出ないこと、Win+. が Alt+. として出ること。Win を外す前後に空打ちが挟まっていることも見る |
| `geonix41/` | geonix41 の配線で親のテストを通し直す。`config.h` で最下段の matrix の列と、左右の Fn / 親指Shift の列 (`FN_L_COL` など) を geonix41 に合わせ、`test_conf_layer.cpp` と `test_alpha_thumb_shift.cpp` をそのまま取り込む (issue #68) |
| `test_host_os.cpp` | `MY_WIN` / `MY_ANDR` の設定を接続先 (USB / BLE1〜3 / 2.4G) ごとに覚えること。接続先は `windmill_board_host()` をこのファイルで差し替えて切り替える。旧形式の設定の引き継ぎも見る |
| `test_host_layout.cpp` | `MY_JIS` / `MY_US` の設定を接続先ごとに覚えること。既定が JIS であること、EEPROM上の位置、起動し直しても残ること、この設定が入る前のEEPROMが JIS として読めること。JIS の列が入るまでは、どちらを選んでも出力が US のままであることも見る (issue #74) |

`test_keymap.hpp` のキーマップは `firmware/technik/keymaps/default/keymap.c` と同じ内容。
実機側は `LAYOUT_ortho_4x12` マクロと PROGMEM に依存していてそのままは読めないため、
ここだけ二重管理になっている。**キー配置を変えたら両方直すこと。**

キーマップは見た目の位置で書いてある。windmill.c は左右の Fn / 親指Shift を
matrix の列で見分けるが、geonix41 は最下段の列が見た目の並びと違うので、
「位置＝列」の配線だけで通しても列の設定の誤りに気づけない (issue #68)。
そこで `test_keymap.hpp` は最下段の列を `WINDMILL_TEST_ROW3_COLS` で引き直してから
matrix へ載せ、`geonix41/` はそこを geonix41 の配線に差し替えて同じテストを走らせる。
入れ子のテストは `qmk test-c -t windmill` では走らないので、`scripts/test.sh` が
両方を指定している。

## QMK 側へのパッチ

`patches/qmk-test-harness.patch` で `tests/test_common/` に2箇所だけ手を入れている。
イメージのビルド時に当てている (`Dockerfile`)。`tests/test_common/` しか触らないので、
同じイメージでファームウェアをビルドしても影響しない。

- `matrix.c`: `matrix_scan_kb()` を weak にして `matrix_scan_user()` を呼ぶようにする。
  windmill.c が `matrix_scan_kb()` を実装しているため、そのままだと多重定義になる
- `test_common.h`: `MATRIX_ROWS` / `MATRIX_COLS` を `#ifndef` で囲む。
  既定が 4x10 なので、実機と同じ 4x12 にできない
