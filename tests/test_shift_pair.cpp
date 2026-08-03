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

/* 親指Shift + process_shift_pair() のHIDレポートを検証する。
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

class ShiftPair : public WindmillTest {};

/* MY_LCTL 1回タップで英数、2回タップでかな。
 * 1回タップは TD_DTAP_TERM 経過後に matrix_scan_kb() が確定させる。 */
static void tap_lctl(WindmillTest* f, int times) {
    auto lctl = f->key(POS_LCTL);
    for (int i = 0; i < times; ++i) {
        lctl.press();
        f->run_one_scan_loop();
        f->idle_for(120);
        lctl.release();
        f->run_one_scan_loop();
        if (i + 1 < times) f->idle_for(60); // TD_DTAP_TERM 未満
    }
    f->settle();
}

/* issue #18 の再現手順そのもの。
 *
 *   MY_LCTL タップ         : 英数へ
 *   は の位置 (英数では f) : 1文字入力
 *   MY_LCTL ダブルタップ   : かなへ
 *   み を押しながら の を2回
 *
 * 修正前は1打鍵目だけ [RSFT] -> [LSFT] -> [LSFT + KC_COMM] と、右Shiftを離して
 * 左Shiftを押し直すレポートが挟まっていた。technik ではその1文字だけ Shift が
 * 無視されて「、」ではなく「ね」になっていた。
 *
 * 現在は親指Shiftが左右とも左Shift (issue #37) で、process_shift_pair() は
 * 実Shiftを外してから同じ左Shiftで戻すだけなので、そもそも入れ替わりうる
 * 相手が無い (issue #39)。それでも回帰テストとして残す。 */
TEST_F(ShiftPair, thumb_shift_keeps_held_shift_on_first_keypress) {
    TestDriver driver;
    set_windmill_keymap();

    // 英数へ切り替えて1文字打ち、かなへ戻す。ここのレポートは問わない
    EXPECT_ANY_REPORT(driver).Times(AnyNumber());
    tap_lctl(this, 1);
    tap_key(key(POS_HA), 120);
    settle();
    tap_lctl(this, 2);
    VERIFY_AND_CLEAR(driver);

    auto mi = key(POS_MI);
    auto no = key(POS_NO);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));            // み ホールド確定
        EXPECT_EMPTY_REPORT(driver);                       // 1打鍵目: 実Shiftを外す
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));             // weak Shiftで戻す
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_COMMA));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));             // 実Shiftを戻す
        EXPECT_EMPTY_REPORT(driver);                       // 2打鍵目: 実Shiftを外す
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));             // weak Shiftで戻す
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_COMMA));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));             // 実Shiftを戻す
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

// Shiftなしなら素のキーコードが出る
TEST_F(ShiftPair, without_shift_sends_plain_keycode) {
    TestDriver driver;
    set_windmill_keymap();
    settle();

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_K));
        EXPECT_EMPTY_REPORT(driver);
    }
    tap_key(key(POS_NO), 120);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* shifted 側が Shift 付きキーコードのとき、実Shiftをいったん外して
 * weak Shiftで送り、戻すこと。 */
TEST_F(ShiftPair, shifted_pair_reuses_held_shift) {
    TestDriver driver;
    set_windmill_keymap();
    settle();

    auto mi = key(POS_MI);
    auto ra = key(POS_RA); // MY_O -> S(KC_LBRC)

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);           // 実Shiftを外す
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // weak Shiftで戻す
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_LEFT_BRACKET));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);           // また外す
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // 実Shiftを戻す
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

/* shifted 側が Shift 不要なキー (MY_R -> バックスラッシュ) では、いったん
 * Shift を外して戻す。del_mods()/set_mods() はレポートを送らないので、
 * ここは register 系でなければ戻りがホストへ伝わらない。 */
TEST_F(ShiftPair, unshifted_pair_drops_and_restores_shift) {
    TestDriver driver;
    set_windmill_keymap();
    settle();

    auto mi = key(POS_MI);
    auto su = key(POS_SU); // MY_R -> KC_BSLS

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);             // Shift を外す
        EXPECT_REPORT(driver, (KC_BACKSLASH));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));  // Shift を戻す (レポートが出ること)
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
