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

/* かなレイヤーの親指Shift (こ / み) のレポート列を検証する。
 *
 * こ は左Shift、み は右Shift で、それぞれ自分の Shift だけを上げ下げする (issue #85)。
 * 以前は左右とも左Shiftで、片方を押したままもう片方を押して離す「持ち替え」
 * (ハンドオーバー) で Shift が途切れないよう参照カウントを自前で持っていた (issue #37)。
 * 別々の Shift になったので、持ち替えは修飾の重なりとしてそのまま出る。
 *
 * 同時押しの半角スペースの規則そのものは test_bottom_row.cpp で見る。ここでは
 * スペースの前に Shift を下ろすレポートが独立していて、間が空いていることを見る。 */

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

class ThumbShift : public WindmillTest {};

// 起動直後の英数から MY_LCTL 1回タップでかなへ切り替える。レポートの中身は問わない
static void switch_to_kana(WindmillTest* f, TestDriver& driver) {
    EXPECT_ANY_REPORT(driver).Times(AnyNumber());

    f->tap_key(f->key(POS_LCTL), 120);
    f->settle();

    VERIFY_AND_CLEAR(driver);
}

/* 左 (こ) を押したまま右 (み) を押し、左を先に離す。右が残っているので Shift は
 * 途切れず、持ち替えた後のキーにも乗る。
 *
 * 2つ目の み は こ と組になりうるので、み が決まるまで こ の解放も待つ。
 * み が 250ms でホールドに決まった時点で、両方の Shift が重なってから左が外れる */
TEST_F(ThumbShift, handover_left_to_right_keeps_shift) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto ko = key(POS_KO); // 左親指Shift
    auto mi = key(POS_MI); // 右親指Shift
    auto ha = key(POS_HA); // は (KC_F)

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));                 // こ ホールド確定
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_RIGHT_SHIFT)); // み ホールド確定
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));                // こ 解放
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT, KC_F));          // 持ち替え後も Shift が乗る
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);                            // み 解放
    }

    ko.press();
    run_one_scan_loop();
    idle_for(250); // 250ms 押し続けてホールド確定
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

TEST_F(ThumbShift, handover_right_to_left_keeps_shift) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto ko = key(POS_KO);
    auto mi = key(POS_MI);
    auto ha = key(POS_HA);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_RIGHT_SHIFT));
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
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_RIGHT_SHIFT));
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);   // 両方離れた
        EXPECT_REPORT(driver, (KC_F)); // Shiftは残っていない
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

/* 半角スペースの前に Shift を下ろすレポートが1本独立していて、スペースとの間が
 * 空いていること。
 *
 * こ を押してから み をタップし、こ を 250ms ちょうどで離す。こ はそのスキャンで
 * ホールドに決まって左Shiftを出し、同じスキャンの解放で組が決まる。Shift の上げ下げと
 * スペースが同じスキャンに固まると、IMEがShiftの上げ下げを追い切れず、Shift が
 * 乗ったまま解釈される (issue #36 と同じ罠)。del_mods() で外すとレポートが出ず、
 * 「Shiftを離す」と「Spaceを押す」が1本にまとまってしまう */
TEST_F(ThumbShift, space_drops_shift_in_its_own_report) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto ko = key(POS_KO); // 左親指Shift
    auto mi = key(POS_MI); // 右親指Shift

    uint16_t at_drop = 0, at_space = 0;
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // こ ホールド確定
        EXPECT_EMPTY_REPORT(driver).WillOnce(InvokeWithoutArgs([&] { at_drop = timer_read(); }));
        EXPECT_REPORT(driver, (KC_SPACE)).WillOnce(InvokeWithoutArgs([&] { at_space = timer_read(); }));
        EXPECT_EMPTY_REPORT(driver);
    }

    ko.press();
    run_one_scan_loop();
    idle_for(99);
    mi.press(); // 100ms
    run_one_scan_loop();
    idle_for(99);
    mi.release(); // 200ms
    run_one_scan_loop();
    idle_for(49);
    ko.release(); // 250ms
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);

    EXPECT_GE(uint16_t(at_space - at_drop), uint16_t(IME_WAIT_MS));
}

/* 半角スペースのあとに Shift は戻らない。組のキーはどちらも離れているので、
 * 次のキーは素のまま出る */
TEST_F(ThumbShift, no_shift_left_after_space) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto ko = key(POS_KO);
    auto mi = key(POS_MI);
    auto ha = key(POS_HA);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_SPACE));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_F));
        EXPECT_EMPTY_REPORT(driver);
    }

    ko.press();
    run_one_scan_loop();
    idle_for(300); // こ はホールドで左Shiftが出ている
    tap_key(mi, 50);
    idle_for(50);
    ko.release(); // ここで組が決まる
    run_one_scan_loop();
    idle_for(120);
    tap_key(ha, 50);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}
