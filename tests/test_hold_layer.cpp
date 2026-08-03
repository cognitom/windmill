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

/* 修飾キーのホールド中だけ英数レイヤーを重ねる挙動を検証する (issue #34)。
 *
 * かなレイヤーの「ぬ」の位置は KC_1 だが、英数レイヤーでは KC_Q になる。
 * ホールド中にこの位置を打って KC_Q が出れば、レイヤーが移っている。
 *
 * 見るのはキーコードだけでなくレポート列。修飾が乗ったまま出ること、
 * ホールドを離した後にレイヤーも修飾も残らないことまで固定する。 */

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_layer.h"
#include "action_util.h"
#include "test_keymap.hpp"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;

class HoldLayer : public WindmillTest {};

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

/* Win(つ) をホールドしている間は英数レイヤー。
 * ホールドは TAPPING_TERM 経過で確定させる */
TEST_F(HoldLayer, gui_hold_switches_to_alpha_layer) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto tsu = key(POS_TSU); // つ (LGUI_T(KC_Z))
    auto nu  = key(POS_NU);  // ぬ (かな: KC_1 / 英数: KC_Q)

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_GUI));          // ホールド確定
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_Q));    // 英数レイヤーで解決される
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_EMPTY_REPORT(driver);                   // つ 解放
    }

    tsu.press();
    run_one_scan_loop();
    idle_for(250); // TAPPING_TERM 超え
    tap_key(nu, 120);
    idle_for(120);
    tsu.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* 別キー割り込みでホールドが確定する場合 (HOLD_ON_OTHER_KEY_PRESS)。
 *
 * この経路ではレイヤーを上げるのと割り込みキーのキーコード解決が同じスキャンに
 * 入る。上げるのが後になると、割り込みキーだけ「ぬ」(KC_1) のまま出てしまう。 */
TEST_F(HoldLayer, gui_hold_by_interrupt_switches_layer) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto tsu = key(POS_TSU);
    auto nu  = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_GUI));       // 割り込みでホールド確定
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_Q));
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_EMPTY_REPORT(driver);
    }

    tsu.press();
    run_one_scan_loop();
    nu.press(); // TAPPING_TERM を待たずに確定
    run_one_scan_loop();
    nu.release();
    run_one_scan_loop();
    idle_for(120);
    tsu.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

// Alt(さ) も同じ
TEST_F(HoldLayer, alt_hold_switches_to_alpha_layer) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto sa = key(POS_SA); // さ (LALT_T(KC_X))
    auto nu = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_ALT));
        EXPECT_REPORT(driver, (KC_LEFT_ALT, KC_Q));
        EXPECT_REPORT(driver, (KC_LEFT_ALT));
        EXPECT_EMPTY_REPORT(driver);
    }

    sa.press();
    run_one_scan_loop();
    idle_for(250);
    tap_key(nu, 120);
    idle_for(120);
    sa.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

// Ctrl(MY_LCTL) のホールド。従来からの挙動の回帰テスト
TEST_F(HoldLayer, ctrl_hold_switches_to_alpha_layer) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto lctl = key(POS_LCTL);
    auto nu   = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_CTRL));       // 割り込みでホールド確定
        EXPECT_REPORT(driver, (KC_LEFT_CTRL, KC_Q));
        EXPECT_REPORT(driver, (KC_LEFT_CTRL));
        EXPECT_EMPTY_REPORT(driver);
    }

    lctl.press();
    run_one_scan_loop();
    idle_for(250);
    tap_key(nu, 120);
    idle_for(120);
    lctl.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* タップ (つ) はかなのまま。ホールドの後始末がレイヤーに残っていないことも見る */
TEST_F(HoldLayer, gui_tap_stays_on_kana_layer) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto tsu = key(POS_TSU);
    auto nu  = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_Z));   // つ。Winは出ない
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_1));   // ぬ。かなレイヤーのまま
        EXPECT_EMPTY_REPORT(driver);
    }

    tap_key(tsu, 50); // TAPPING_TERM 未満
    idle_for(120);
    tap_key(nu, 120);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* Win と Alt を重ねて押し、片方だけ離す。QMKのレイヤー状態は参照カウントを
 * 持たないので、素のままだと先に離したほうでレイヤーごと落ちてしまう */
TEST_F(HoldLayer, gui_and_alt_overlap_keeps_layer) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto tsu = key(POS_TSU);
    auto sa  = key(POS_SA);
    auto nu  = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_LEFT_ALT));
        EXPECT_REPORT(driver, (KC_LEFT_GUI));                // さ を先に離す
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_Q));          // つ が残っているので英数のまま
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_EMPTY_REPORT(driver);
    }

    tsu.press();
    run_one_scan_loop();
    idle_for(250);
    sa.press();
    run_one_scan_loop();
    idle_for(250);
    sa.release();
    run_one_scan_loop();
    idle_for(120);
    tap_key(nu, 120);
    idle_for(120);
    tsu.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* Ctrl と Win を重ねて押し、Ctrl を先に離す。
 * Ctrl は自前の状態機械、Win はQMKのmod-tapと出どころが違うので、
 * レイヤーの数え方が共通になっていないとここで落ちる */
TEST_F(HoldLayer, ctrl_and_gui_overlap_keeps_layer) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto lctl = key(POS_LCTL);
    auto tsu  = key(POS_TSU);
    auto nu   = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_CTRL));                // つ の押下で割り込み確定
        EXPECT_REPORT(driver, (KC_LEFT_CTRL, KC_LEFT_GUI));
        EXPECT_REPORT(driver, (KC_LEFT_GUI));                 // Ctrl を先に離す
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_Q));           // つ が残っているので英数のまま
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_EMPTY_REPORT(driver);
    }

    lctl.press();
    run_one_scan_loop();
    tsu.press();
    run_one_scan_loop();
    idle_for(250);
    lctl.release();
    run_one_scan_loop();
    idle_for(120);
    tap_key(nu, 120);
    idle_for(120);
    tsu.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}
