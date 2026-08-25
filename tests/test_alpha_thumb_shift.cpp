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

/* 英数レイヤーの親指Shiftのハンドオーバーを検証する (issue #40)。
 *
 * かなレイヤーの親指Shiftは左右で違うキーコード (THUMB_SHIFT_B/N) を使うが、
 * 英数レイヤーは元から左右とも LSFT_T(KC_SPC) で同じキーコード。かなレイヤー
 * (issue #37/#38) と同じく、QMKの修飾は参照カウントを持たないため、素のまま
 * だと片方を持ち替えたときに先に離したほうの unregister_mods() でShiftごと
 * 落ちる。test_thumb_shift.cpp と同じ観点で、持ち替え前後のレポート列を見る。
 *
 * POS_KO / POS_MI はかなレイヤー上の位置を指す名前だが、実体は物理位置
 * (row=3, col=5/6) で、英数レイヤーが有効な間はそのまま英数側の
 * ALPHA_THUMB_SHIFT (LSFT_T(KC_SPC)) を指す。 */

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_layer.h"
#include "action_util.h"
#include "test_keymap.hpp"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;

class AlphaThumbShift : public WindmillTest {};

// MY_LCTL タップで英数へ切り替える。レポートの中身は問わない
static void switch_to_alpha(WindmillTest* f, TestDriver& driver) {
    EXPECT_ANY_REPORT(driver).Times(AnyNumber());

    auto lctl = f->key(POS_LCTL);
    lctl.press();
    f->run_one_scan_loop();
    f->idle_for(120);
    lctl.release();
    f->run_one_scan_loop();
    f->settle();

    VERIFY_AND_CLEAR(driver);
}

/* 左 → 右 の持ち替え。左を離しても、右が押されている限りShiftは続く */
TEST_F(AlphaThumbShift, handover_left_to_right_keeps_shift) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_alpha(this, driver);

    auto sft_l = key(POS_KO); // 左親指Shift (英数レイヤーでは LSFT_T(KC_SPC))
    auto sft_r = key(POS_MI); // 右親指Shift
    auto nu    = key(POS_NU); // ぬ。英数レイヤーでは "q"

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));       // 左 ホールド確定
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_Q));  // 持ち替え後も Shift が乗る
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);                   // 右 解放
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
    switch_to_alpha(this, driver);

    auto sft_l = key(POS_KO);
    auto sft_r = key(POS_MI);
    auto nu    = key(POS_NU);

    {
        InSequence s;
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
    switch_to_alpha(this, driver);

    auto sft_l = key(POS_KO);
    auto sft_r = key(POS_MI);
    auto nu    = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver); // 両方離れた
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
