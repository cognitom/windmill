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

/* process_kana_qmark() のHIDレポートを検証する (issue #17)。
 *
 * かなレイヤーの「も」(LT(2,KC_M)) をShiftを押しながらタップすると、
 * KC_LNG2 → KC_SLSH → KC_LNG1 の順で送出し、半角「?」を入力する。
 * キーコードの並びだけでなく、送出中もShiftの修飾が保持され続けることを
 * レポート列で確かめる (issue #18 と同じ理由で、キーコードの一致だけでは
 * 修飾の出方の崩れに気づけない)。
 *
 * 起動直後のベースレイヤーは英数 (issue #22) なので、かなレイヤー上の
 * 挙動を見るには先に MY_LCTL をダブルタップしてかなへ切り替えておく必要がある。 */

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_layer.h"
#include "action_util.h"
#include "test_keymap.hpp"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;

class KanaQmark : public WindmillTest {};

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

// Shiftなしなら通常どおり KC_M が出る
TEST_F(KanaQmark, without_shift_sends_plain_keycode) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_M));
        EXPECT_EMPTY_REPORT(driver);
    }
    tap_key(key(POS_MO), 120);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

// Shiftを押しながらのタップは KC_LNG2 -> KC_SLSH -> KC_LNG1 に化ける
TEST_F(KanaQmark, shifted_tap_sends_halfwidth_question_mark) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto mi = key(POS_MI); // 右親指Shift
    auto mo = key(POS_MO);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));               // み ホールド確定
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT, KC_LNG2));
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT, KC_SLSH));
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT, KC_LNG1));
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);                            // み 解放
    }

    mi.press();
    run_one_scan_loop();
    idle_for(150);
    tap_key(mo, 120);
    idle_for(120);
    mi.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* ホールドすれば通常どおりSymレイヤーに乗る。かなベースのままなので、数字は
 * 既存のIMEラップ (is_sym_ime_wrap_target) で KC_LNG2 -> KC_1 -> KC_LNG1 に
 * なる。process_kana_qmark がホールドに割り込んでいないことの確認 */
TEST_F(KanaQmark, hold_still_enters_sym_layer) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto mo  = key(POS_MO);
    auto one = key(0, 1); // Symレイヤーでの数字段

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LNG2));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_1));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LNG1));
        EXPECT_EMPTY_REPORT(driver);
    }

    mo.press();
    run_one_scan_loop();
    idle_for(50);     // タップ確定前に、別キーの押下でホールドへ割り込ませる
    tap_key(one, 60);
    idle_for(60);
    mo.release();
    run_one_scan_loop();
    idle_for(60);
    VERIFY_AND_CLEAR(driver);
}
