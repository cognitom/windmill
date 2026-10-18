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

/* かなレイヤーの親指Shift (こ / み) そのもののレポート列を検証する。
 *
 * こ は左Shift、み は右Shift で、それぞれ自分の Shift だけを上げ下げする
 * (issue #83)。片方を押したままもう片方を押して離す「持ち替え」(ハンドオーバー)
 * で、Shiftが途切れないことを確かめる。以前は左右とも左Shiftで (issue #37)、
 * QMKの修飾ビットを共有するぶん、持ち替えを自前で数える必要があった。
 *
 * タップ・ホールドの判定そのもの (150ms / 250ms) は test_tap_hold.cpp が見る。 */

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

/* 左 → 右 の持ち替え。左を離しても、右が押されている限りShiftは続く。
 * 左右で別の Shift なので、持ち替えの間は両方が乗ったレポートを挟む */
TEST_F(ThumbShift, handover_left_to_right_keeps_shift) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto ko = key(POS_KO); // 左親指Shift
    auto mi = key(POS_MI); // 右親指Shift
    auto ha = key(POS_HA); // は (KC_F)

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));                  // こ ホールド確定
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_RIGHT_SHIFT));  // み ホールド確定
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));                 // こ 解放。右は残る
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT, KC_F));           // 持ち替え後も Shift が乗る
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);                             // み 解放
    }

    ko.press();
    run_one_scan_loop();
    idle_for(250); // 250ms 超え。ホールド確定
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

/* こ の Shift 中に み をタップした半角スペース (issue #36)。
 *
 * 実機では、1打鍵目だけスペースにShiftが乗っていた。以前は
 * del_mods()/set_mods() で real_mods を書き換えていて、この2つはレポートを
 * 送らない。そのためホストへ届く1本目が
 *
 *   [LSFT] (ホールド確定) → [KC_SPC]     ← Shiftを離すのとSpaceを押すのが同時
 *
 * という形になっていた。2打鍵目以降が無事なのは前のレポートから間が空いている
 * だけの話で、process_shift_pair() の「Shiftを外して送る」経路 (issue #36) と
 * 同じ症状。そこでShiftを外すレポートが1本独立していることと、送出の間隔の
 * 両方を見る。
 *
 * み は こ を押して 150ms 以上たってから押す。それより早いと こ はまだ決まって
 * おらず、み は こ の Shift 中に押したことにならない (test_tap_hold.cpp の
 * ケース b) */
TEST_F(ThumbShift, space_drops_shift_in_its_own_report) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto ko = key(POS_KO); // 左親指Shift
    auto mi = key(POS_MI); // 右親指Shift

    uint16_t at_hold = 0, at_drop = 0, at_space = 0;

    {
        InSequence s;
        // こ ホールド確定 → Shiftを外す → スペース。この3本の間隔を控える
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)).WillOnce(InvokeWithoutArgs([&] { at_hold = timer_read(); }));
        EXPECT_EMPTY_REPORT(driver).WillOnce(InvokeWithoutArgs([&] { at_drop = timer_read(); }));
        EXPECT_REPORT(driver, (KC_SPACE)).WillOnce(InvokeWithoutArgs([&] { at_space = timer_read(); }));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));  // Shiftを戻す (ホストへ届くこと)
        EXPECT_EMPTY_REPORT(driver);             // 2打鍵目。レポート列は1打鍵目と同じ
        EXPECT_REPORT(driver, (KC_SPACE));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);             // こ 解放
    }

    ko.press();
    idle_for(200);
    mi.press(); // こ は次のスキャンでホールドに決まる。み はその Shift 中に押したことになる
    idle_for(30);
    mi.release(); // すぐ離すのでタップ = スペース
    run_one_scan_loop();
    idle_for(120);
    tap_key(mi, 50); // 2打鍵目
    idle_for(120);
    ko.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);

    // ホールド確定 → Shift外し → スペース が同じスキャンに固まっていないこと
    EXPECT_GE(uint16_t(at_drop - at_hold), uint16_t(IME_WAIT_MS));
    EXPECT_GE(uint16_t(at_space - at_drop), uint16_t(IME_WAIT_MS));
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
