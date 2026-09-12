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

/* MY_LCTL のタップでの言語切替 (issue #53)。
 *
 * 以前は 1回タップ = KC_LNG2 (英数)、2回タップ = KC_LNG1 (かな) だったのを、
 * タップのたびに英数⇔かなを入れ替える形にした。見たいのは次の2つ。
 *
 * - IMEへ送るキーが MY_WIN / MY_ANDR で出し分けられていること。
 *   Windows は切り替え先に合わせて KC_LNG1 / KC_LNG2 を交互に送る (モードを
 *   直接指定するので、IMEとずれてもタップし直せば揃う)。Android は
 *   Ctrl+Space のトグル
 * - キーボード側のベースレイヤーも一緒に反転していること。IMEの状態は
 *   読めないので、ここがずれるとかなのつもりで英字配列を打つことになる
 *
 * レイヤーは test_hold_layer.cpp と同じく、「ぬ」の位置を打って判定する。
 * かなレイヤーでは KC_1、英数レイヤーでは KC_Q が出る。 */

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_layer.h"
#include "action_util.h"
#include "test_keymap.hpp"

using testing::_;
using testing::InSequence;

#define POS_FN 3, 3   // そ  LT(3,KC_C)  英数レイヤーでは MO(3)
#define POS_ANDR 0, 3 // Fnレイヤーの MY_ANDR

class LangToggle : public WindmillTest {};

/* is_android はEEPROMに残るので、Android向けのテストはスイートを分ける。
 * QMKのテスト基盤はスイートの頭でEEPROMを初期化する (SetUpTestCase) ので、
 * 他のスイートへは持ち越さない */
class LangToggleAndroid : public WindmillTest {};

// Windows向け (既定) の言語切替の1回ぶん。lng は KC_LNG1 (かな) か KC_LNG2 (英数)
static void expect_switch_win(TestDriver& driver, uint8_t lng) {
    EXPECT_REPORT(driver, (lng));
    EXPECT_EMPTY_REPORT(driver);
}

/* タップのたびに KC_LNG1 / KC_LNG2 を交互に送り、ベースレイヤーも
 * 英数 → かな → 英数 と入れ替わる */
TEST_F(LangToggle, tap_toggles_ime_and_base_layer) {
    TestDriver driver;
    set_windmill_keymap();

    auto lctl = key(POS_LCTL);
    auto nu   = key(POS_NU); // かな = KC_1 / 英数 = KC_Q

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_Q)); // 起動直後は英数
        EXPECT_EMPTY_REPORT(driver);
        expect_switch_win(driver, KC_LNG1); // → かな
        EXPECT_REPORT(driver, (KC_1));
        EXPECT_EMPTY_REPORT(driver);
        expect_switch_win(driver, KC_LNG2); // → 英数
        EXPECT_REPORT(driver, (KC_Q));
        EXPECT_EMPTY_REPORT(driver);
    }

    tap_key(nu, 120);
    idle_for(120);
    tap_key(lctl, 120);
    settle();
    tap_key(nu, 120);
    idle_for(120);
    tap_key(lctl, 120);
    settle();
    tap_key(nu, 120);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* 素早い2回タップも2回のトグル。以前のようにダブルタップで「かな」を
 * 意味することはなく、英数 → かな → 英数 と戻ってくる */
TEST_F(LangToggle, quick_double_tap_toggles_twice) {
    TestDriver driver;
    set_windmill_keymap();

    auto lctl = key(POS_LCTL);
    auto nu   = key(POS_NU);

    {
        InSequence s;
        expect_switch_win(driver, KC_LNG1);
        expect_switch_win(driver, KC_LNG2);
        EXPECT_REPORT(driver, (KC_Q)); // 英数へ戻っている
        EXPECT_EMPTY_REPORT(driver);
    }

    tap_key(lctl, 50);
    idle_for(60); // 以前の TD_DTAP_TERM (180ms) 未満
    tap_key(lctl, 50);
    settle();
    tap_key(nu, 120);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* ダブルタップを待たないので、離した時点でトグルが確定する。
 * 直後のキーは切り替え後のレイヤーで解決される */
TEST_F(LangToggle, key_right_after_tap_resolves_on_new_layer) {
    TestDriver driver;
    set_windmill_keymap();

    auto lctl = key(POS_LCTL);
    auto nu   = key(POS_NU);

    {
        InSequence s;
        expect_switch_win(driver, KC_LNG1);
        EXPECT_REPORT(driver, (KC_1)); // もう かな
        EXPECT_EMPTY_REPORT(driver);
    }

    tap_key(lctl, 50);
    tap_key(nu, 50); // 間を空けない
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* ホールド (Ctrl) ではトグルを送らず、ベースレイヤーも動かさない。
 * 別キー割り込みで確定する経路 (HOLD_ON_OTHER_KEY_PRESS) で見る */
TEST_F(LangToggle, hold_does_not_toggle) {
    TestDriver driver;
    set_windmill_keymap();

    auto lctl = key(POS_LCTL);
    auto nu   = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_CTRL));       // ぬ の押下でホールド確定
        EXPECT_REPORT(driver, (KC_LEFT_CTRL, KC_Q)); // Ctrl+Q
        EXPECT_REPORT(driver, (KC_LEFT_CTRL));
        EXPECT_EMPTY_REPORT(driver);                 // Ctrl 解放。LNG1 は出ない
        EXPECT_REPORT(driver, (KC_Q));               // 英数のまま
        EXPECT_EMPTY_REPORT(driver);
    }

    lctl.press();
    run_one_scan_loop();
    nu.press();
    run_one_scan_loop();
    idle_for(120);
    nu.release();
    run_one_scan_loop();
    idle_for(120);
    lctl.release();
    run_one_scan_loop();
    settle();
    tap_key(nu, 120);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* MY_ANDR を押した後は Ctrl+Space を送る。ベースレイヤーの反転は同じ */
TEST_F(LangToggleAndroid, tap_sends_ctrl_space) {
    TestDriver driver;
    set_windmill_keymap();

    auto fn   = key(POS_FN);
    auto andr = key(POS_ANDR);
    auto lctl = key(POS_LCTL);
    auto nu   = key(POS_NU);

    // Fn+MY_ANDR はレポートを出さない
    EXPECT_NO_REPORT(driver);
    fn.press();
    run_one_scan_loop();
    tap_key(andr, 50);
    fn.release();
    run_one_scan_loop();
    settle();
    VERIFY_AND_CLEAR(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_CTRL));
        EXPECT_REPORT(driver, (KC_LEFT_CTRL, KC_SPC));
        EXPECT_REPORT(driver, (KC_LEFT_CTRL));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_1)); // かなへ
        EXPECT_EMPTY_REPORT(driver);
    }

    tap_key(lctl, 120);
    settle();
    tap_key(nu, 120);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}
