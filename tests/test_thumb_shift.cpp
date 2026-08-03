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

/* 親指Shiftそのもののレポート列を検証する (issue #37)。
 *
 * 左右の親指Shiftはどちらも同じ左Shiftなので、片方を押したままもう片方を
 * 押して離す「持ち替え」(ハンドオーバー) でShiftが途切れないことを確かめる。
 * QMKの修飾はビットマスクで参照カウントを持たないため、素のままだと後から
 * 離したほうの unregister_mods() でShiftが落ちてしまう。 */

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_layer.h"
#include "action_util.h"
#include "test_keymap.hpp"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;

class ThumbShift : public WindmillTest {};

// MY_LCTL ダブルタップでかなへ切り替える。レポートの中身は問わない
static void switch_to_kana(WindmillTest* f, TestDriver& driver) {
    EXPECT_ANY_REPORT(driver).Times(AnyNumber());

    auto lctl = f->key(POS_LCTL);
    for (int i = 0; i < 2; ++i) {
        lctl.press();
        f->run_one_scan_loop();
        f->idle_for(120);
        lctl.release();
        f->run_one_scan_loop();
        if (i == 0) f->idle_for(60); // TD_DTAP_TERM未満
    }
    f->settle();

    VERIFY_AND_CLEAR(driver);
}

/* 左 → 右 の持ち替え。左を離しても、右が押されている限りShiftは続く。
 * 左右が同じ左Shiftになったので、持ち替えの前後でレポートは [LSFT] のまま
 * 変わらない (以前は右Shiftとの入れ替えレポートが挟まっていた) */
TEST_F(ThumbShift, handover_left_to_right_keeps_shift) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto ko = key(POS_KO); // 左親指Shift
    auto mi = key(POS_MI); // 右親指Shift
    auto ha = key(POS_HA); // は (KC_F)

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));         // 左 ホールド確定
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_F));   // 持ち替え後も Shift が乗る
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);                    // 右 解放
    }

    ko.press();
    run_one_scan_loop();
    idle_for(250); // TAPPING_TERM 超え。ホールド確定
    mi.press();
    run_one_scan_loop();
    idle_for(250);
    ko.release(); // 左を離す。右はまだ押されている
    run_one_scan_loop();
    idle_for(120);
    tap_key(ha, 120);
    idle_for(120);
    mi.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* 右 → 左 の持ち替え。逆向きでも同じ */
TEST_F(ThumbShift, handover_right_to_left_keeps_shift) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto ko = key(POS_KO);
    auto mi = key(POS_MI);
    auto ha = key(POS_HA);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_F));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }

    mi.press();
    run_one_scan_loop();
    idle_for(250);
    ko.press();
    run_one_scan_loop();
    idle_for(250);
    mi.release();
    run_one_scan_loop();
    idle_for(120);
    tap_key(ha, 120);
    idle_for(120);
    ko.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

// 両方離せばShiftも落ちる。持ち替えの後始末が残っていないことの確認
TEST_F(ThumbShift, both_released_clears_shift) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto ko = key(POS_KO);
    auto mi = key(POS_MI);
    auto ha = key(POS_HA);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);  // 両方離れた
        EXPECT_REPORT(driver, (KC_F));// Shiftは残っていない
        EXPECT_EMPTY_REPORT(driver);
    }

    ko.press();
    run_one_scan_loop();
    idle_for(250);
    mi.press();
    run_one_scan_loop();
    idle_for(250);
    ko.release();
    run_one_scan_loop();
    idle_for(120);
    mi.release();
    run_one_scan_loop();
    idle_for(120);
    tap_key(ha, 120);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}
