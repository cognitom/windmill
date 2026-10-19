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

/* 英数レイヤーの親指Shift (SandS) の持ち替えを検証する (issue #40、issue #88)。
 *
 * 左は LSFT_T(KC_SPC)、右は RSFT_T(KC_SPC)。以前は左右とも LSFT_T(KC_SPC) で
 * 修飾ビットを共有していたため、先に離したほうの unregister_mods() でShiftごと落ち、
 * 列で左右を見分けて離すイベントを消す必要があった。左右が別のビットになった今は
 * QMK の標準処理だけで途切れない。それが崩れていないことをレポート列で見る。
 * (かなレイヤーの こ / み も左Shift / 右Shift で、同じ形。test_thumb_shift.cpp 参照)
 *
 * POS_KO / POS_MI はかなレイヤー上の位置を指す名前だが、実体は物理位置
 * (row=3, col=5/6) で、英数レイヤーが有効な間はそのまま英数側の親指Shiftを指す。
 *
 * 起動直後のベースレイヤーは英数 (issue #22) なので、切り替えずにそのまま打つ。
 * MY_LCTL はトグル (issue #53) で、タップするとかなへ移ってしまう。 */

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_layer.h"
#include "action_util.h"
#include "test_keymap.hpp"

using testing::_;
using testing::InSequence;

class AlphaThumbShift : public WindmillTest {};

/* 左 → 右 の持ち替え。左を離しても、右が押されている限りShiftは続く */
TEST_F(AlphaThumbShift, handover_left_to_right_keeps_shift) {
    TestDriver driver;
    set_windmill_keymap();

    auto sft_l = key(POS_KO); // 左親指Shift (英数レイヤーでは LSFT_T(KC_SPC))
    auto sft_r = key(POS_MI); // 右親指Shift (英数レイヤーでは RSFT_T(KC_SPC))
    auto nu    = key(POS_NU); // ぬ。英数レイヤーでは "q"

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));                 // 左 ホールド確定
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_RIGHT_SHIFT)); // 右 ホールド確定
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));                // 左 解放。右Shiftは残る
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT, KC_Q));          // 持ち替え後も Shift が乗る
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);                            // 右 解放
    }

    sft_l.press();
    run_one_scan_loop();
    idle_for(250); // TAPPING_TERM 超え。ホールド確定
    sft_r.press();
    run_one_scan_loop();
    idle_for(250);
    sft_l.release(); // 左を離す。右はまだ押されている
    run_one_scan_loop();
    idle_for(120);
    tap_key(nu, 120);
    idle_for(120);
    sft_r.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* 右 → 左 の持ち替え。逆向きでも同じ */
TEST_F(AlphaThumbShift, handover_right_to_left_keeps_shift) {
    TestDriver driver;
    set_windmill_keymap();

    auto sft_l = key(POS_KO);
    auto sft_r = key(POS_MI);
    auto nu    = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_RIGHT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_Q));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }

    sft_r.press();
    run_one_scan_loop();
    idle_for(250);
    sft_l.press();
    run_one_scan_loop();
    idle_for(250);
    sft_r.release();
    run_one_scan_loop();
    idle_for(120);
    tap_key(nu, 120);
    idle_for(120);
    sft_l.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

// 両方離せばShiftも落ちる。持ち替えの後始末が残っていないことの確認
TEST_F(AlphaThumbShift, both_released_clears_shift) {
    TestDriver driver;
    set_windmill_keymap();

    auto sft_l = key(POS_KO);
    auto sft_r = key(POS_MI);
    auto nu    = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_RIGHT_SHIFT));
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);   // 両方離れた
        EXPECT_REPORT(driver, (KC_Q)); // Shiftは残っていない
        EXPECT_EMPTY_REPORT(driver);
    }

    sft_l.press();
    run_one_scan_loop();
    idle_for(250);
    sft_r.press();
    run_one_scan_loop();
    idle_for(250);
    sft_l.release();
    run_one_scan_loop();
    idle_for(120);
    sft_r.release();
    run_one_scan_loop();
    idle_for(120);
    tap_key(nu, 120);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* 左をホールドして右をタップすると Shift+Space。タップ側はスペースとして出て、
 * ホールド側の左Shiftが乗る */
TEST_F(AlphaThumbShift, hold_left_tap_right_sends_shift_space) {
    TestDriver driver;
    set_windmill_keymap();

    auto sft_l = key(POS_KO);
    auto sft_r = key(POS_MI);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_SPACE));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }

    sft_l.press();
    run_one_scan_loop();
    idle_for(250);
    tap_key(sft_r, 120); // TAPPING_TERM 未満で離す
    idle_for(120);
    sft_l.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

// 右をホールドして左をタップしても同じ。乗るのは右Shift
TEST_F(AlphaThumbShift, hold_right_tap_left_sends_shift_space) {
    TestDriver driver;
    set_windmill_keymap();

    auto sft_l = key(POS_KO);
    auto sft_r = key(POS_MI);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT, KC_SPACE));
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }

    sft_r.press();
    run_one_scan_loop();
    idle_for(250);
    tap_key(sft_l, 120);
    idle_for(120);
    sft_r.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}
