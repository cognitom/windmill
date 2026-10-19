/* Copyright 2026 Tsutomu Kawamura
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* 親指Shift で Shift 面のキーを打ったときのHIDレポートを検証する。
 *
 * 以前は同じキーの Shift 時の出力を別に持っていた (process_shift_pair)。今は
 * こ / み のホールド中に Shift 面 (LAYER_KANA_SHIFT) が上がり、そこのキーを
 * 1キー1出力で送る (issue #82)。押されている Shift の扱いは変わらない。
 *
 * ここで見たいのは「どの文字が出たか」ではなく「どんなレポート列が出たか」。
 * issue #18 は修飾の付け外しが1本余計なレポートを挟むのが原因で、キーコード
 * だけ見ていると気づけなかった。 */

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_layer.h"
#include "action_util.h"
#include "test_keymap.hpp"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;
using testing::InvokeWithoutArgs;

class ShiftPair : public WindmillTest {};

// MY_LCTL 1回タップで英数⇔かなを切り替える (issue #53)
static void tap_lctl(WindmillTest* f) {
    f->tap_key(f->key(POS_LCTL), 120);
    f->settle();
}

/* 起動直後の英数からかなへ切り替える。レポートの中身は問わない。
 * 以前はここを飛ばしても、同じスイートの前のテストが残したかなのまま
 * 動いていた。テストごとに英数へ戻すようにした (issue #53) ので明示する */
static void switch_to_kana(WindmillTest* f, TestDriver& driver) {
    EXPECT_ANY_REPORT(driver).Times(AnyNumber());
    tap_lctl(f);
    VERIFY_AND_CLEAR(driver);
}

/* issue #18 の再現手順そのもの。
 *
 *   は の位置 (英数では f) : 英数で1文字入力 (起動直後は英数)
 *   MY_LCTL タップ         : かなへ
 *   み を押しながら の を2回
 *
 * 修正前は1打鍵目だけ [RSFT] -> [LSFT] -> [LSFT + KC_COMM] と、右Shiftを離して
 * 左Shiftを押し直すレポートが挟まっていた。technik ではその1文字だけ Shift が
 * 無視されて「、」ではなく「ね」になっていた。
 *
 * 「み」は右Shift (issue #85)。Shift 面の「、」は S(KC_COMM) だが、weak Shift (左Shift) を
 * 付けず、押されている右Shiftをそのまま使うので、入れ替わりは起きない。 */
TEST_F(ShiftPair, thumb_shift_keeps_held_shift_on_first_keypress) {
    TestDriver driver;
    set_windmill_keymap();

    // 英数 (起動直後) で1文字打ち、かなへ切り替える。ここのレポートは問わない
    EXPECT_ANY_REPORT(driver).Times(AnyNumber());
    tap_key(key(POS_HA), 120);
    settle();
    tap_lctl(this);
    VERIFY_AND_CLEAR(driver);

    auto mi = key(POS_MI);
    auto no = key(POS_NO);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));            // み ホールド確定
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT, KC_COMMA));  // 1打鍵目
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT, KC_COMMA));  // 2打鍵目
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);                       // み 解放
    }

    mi.press();
    run_one_scan_loop();
    idle_for(150);
    tap_key(no, 120);
    idle_for(120);
    tap_key(no, 120);
    idle_for(120);
    mi.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* issue #36 の再現手順。issue #18 と同じ形だが、こちらは Shift 面のキーが
 * Shift無しで送るもの (「ほ」= KC_MINS)。
 *
 *   は の位置 (英数では f) : 英数で1文字入力 (起動直後は英数)
 *   MY_LCTL タップ         : かなへ
 *   み を押しながら な を2回
 *
 * 実機では1打鍵目だけ「ほ」ではなく「ー」(= S(KC_MINS) の出力) になっていた。
 * 外したはずのShiftが乗ったまま KC_MINS が届いている、ということになる。
 *
 * ただしレポート列は1打鍵目も2打鍵目も同じで、違うのは間隔だけだった。
 * 別キー割り込みでホールドが確定するので、
 * 1打鍵目は [LSFT] → [] → [KC_MINS] が全て同じスキャンで出る。
 * そこで、レポート列に加えて**送出の間隔**も固定する。 */
TEST_F(ShiftPair, unshifted_pair_waits_for_ime_on_first_keypress) {
    TestDriver driver;
    set_windmill_keymap();

    // 英数 (起動直後) で1文字打ち、かなへ切り替える。ここのレポートは問わない
    EXPECT_ANY_REPORT(driver).Times(AnyNumber());
    tap_key(key(POS_HA), 120);
    settle();
    tap_lctl(this);
    VERIFY_AND_CLEAR(driver);

    auto mi = key(POS_MI);
    auto na = key(POS_NA);

    uint16_t at_hold = 0, at_drop = 0, at_key = 0;

    {
        InSequence s;
        // み ホールド確定 → Shiftを外す → 「ほ」。この3本の間隔を控える
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT)).WillOnce(InvokeWithoutArgs([&] { at_hold = timer_read(); }));
        EXPECT_EMPTY_REPORT(driver).WillOnce(InvokeWithoutArgs([&] { at_drop = timer_read(); }));
        EXPECT_REPORT(driver, (KC_MINUS)).WillOnce(InvokeWithoutArgs([&] { at_key = timer_read(); }));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));  // Shiftを戻す
        EXPECT_EMPTY_REPORT(driver);             // 2打鍵目。レポート列は1打鍵目と同じ
        EXPECT_REPORT(driver, (KC_MINUS));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);             // み 解放
    }

    mi.press();
    run_one_scan_loop();
    idle_for(150); // 150ms 以上たってから な を押すので、その押下でホールドが確定する
    tap_key(na, 120);
    idle_for(120);
    tap_key(na, 120);
    idle_for(120);
    mi.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);

    // ホールド確定 → Shift外し → キー送出 が同じスキャンに固まっていないこと
    EXPECT_GE(uint16_t(at_drop - at_hold), uint16_t(IME_WAIT_MS));
    EXPECT_GE(uint16_t(at_key - at_drop), uint16_t(IME_WAIT_MS));
}

// Shiftなしなら素のキーコードが出る
TEST_F(ShiftPair, without_shift_sends_plain_keycode) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_K));
        EXPECT_EMPTY_REPORT(driver);
    }
    tap_key(key(POS_NO), 120);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* Shift 面のキーが Shift 付きのもの (「「」= JIS で S(KC_RBRC)) なら、押されている Shift をそのまま使う。
 * 修飾の付け外しレポートが挟まらないこと。 */
TEST_F(ShiftPair, shifted_pair_reuses_held_shift) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto mi = key(POS_MI);
    auto ra = key(POS_RA); // Shift 面では KN_LKAKKO -> S(KC_RBRC)

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT, KC_RIGHT_BRACKET));
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }

    mi.press();
    run_one_scan_loop();
    idle_for(150);
    tap_key(ra, 120);
    idle_for(120);
    mi.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* Shift 面のキーが Shift 不要なもの (「む」= JIS の ] のキー、KC_NUHS) では、いったん
 * Shift を外して戻す。del_mods()/set_mods() はレポートを送らないので、
 * ここは register 系でなければ戻りがホストへ伝わらない。 */
TEST_F(ShiftPair, unshifted_pair_drops_and_restores_shift) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto mi = key(POS_MI);
    auto su = key(POS_SU); // Shift 面では KN_MU -> KC_NUHS

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);             // Shift を外す
        EXPECT_REPORT(driver, (KC_NONUS_HASH));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));  // Shift を戻す (レポートが出ること)
        EXPECT_EMPTY_REPORT(driver);             // み 解放
    }

    mi.press();
    run_one_scan_loop();
    idle_for(150);
    tap_key(su, 120);
    idle_for(120);
    mi.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}
