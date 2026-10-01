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

/* 設定レイヤーは左右のFnを両方ホールドしている間だけ有効になる (issue #62)。
 *
 * どのレイヤーで解決されたかは、他のテストと同じくレポートで判定する。
 * MY_WIN / MY_ANDR の位置は、Fnレイヤーでは KC_F11 / KC_F12 なのでレポートが出る。
 * 設定レイヤーではレポートが出ず、かわりにOSの設定が変わる。OSは言語切替
 * (MY_LCTL) のタップで見る。英数からのタップで、Windows は KC_LNG1、Android は
 * Ctrl+Space を送る (test_lang_toggle.cpp 参照)。
 *
 * 壊しやすいのは離すほう。QMKのレイヤーは参照カウントを持たないので、片方の
 * Fnを離したときにFnレイヤーごと落ちたり、設定レイヤーが残ったりしないことを
 * 左右それぞれで見る。かなレイヤーの LT(3,KC_C) / LT(3,KC_COMMA) と英数レイヤーの
 * MO(3) とでキーコードが違うので、両方のベースで通す。 */

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_layer.h"
#include "action_util.h"
#include "test_keymap.hpp"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;

/* OSの設定はEEPROMに残るので、Android へ変えるテストはスイートを分ける。
 * QMKのテスト基盤はスイートの頭でEEPROMを初期化する (SetUpTestCase) */
class ConfLayer : public WindmillTest {};
class ConfLayerAlpha : public WindmillTest {};
class ConfLayerKana : public WindmillTest {};
class ConfLayerKanaInterrupt : public WindmillTest {};

// 起動直後の英数から MY_LCTL 1回タップでかなへ切り替える。レポートの中身は問わない
static void switch_to_kana(WindmillTest* f, TestDriver& driver) {
    EXPECT_ANY_REPORT(driver).Times(AnyNumber());

    f->tap_key(f->key(POS_LCTL), 120);
    f->settle();

    VERIFY_AND_CLEAR(driver);
}

/* 英数から言語切替を1回タップして、送られたキーでOSを確かめる。
 * タップでベースレイヤーが反転するので、先に英数へ戻してから叩く */
static void expect_lang_toggle(WindmillTest* f, TestDriver& driver, bool android) {
    default_layer_set((layer_state_t)1 << LAYER_ALPHA);

    {
        InSequence s;
        if (android) {
            EXPECT_REPORT(driver, (KC_LEFT_CTRL));
            EXPECT_REPORT(driver, (KC_LEFT_CTRL, KC_SPC));
            EXPECT_REPORT(driver, (KC_LEFT_CTRL));
        } else {
            EXPECT_REPORT(driver, (KC_LNG1));
        }
        EXPECT_EMPTY_REPORT(driver);
    }

    f->tap_key(f->key(POS_LCTL), 120);
    f->settle();
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA); // 次のテストへかなを持ち越さない
}

/* Fnが片方だけなら、左右どちらでもFnレイヤーのまま。設定キーには届かない */
TEST_F(ConfLayer, single_fn_stays_on_fn_layer) {
    TestDriver driver;
    set_windmill_keymap();

    for (auto fn : {key(POS_FN_L), key(POS_FN_R)}) {
        {
            InSequence s;
            EXPECT_REPORT(driver, (KC_F12)); // Fnレイヤーで解決される
            EXPECT_EMPTY_REPORT(driver);
        }

        fn.press();
        run_one_scan_loop();
        idle_for(250);
        tap_key(key(POS_ANDR), 50);
        idle_for(50);
        fn.release();
        run_one_scan_loop();
        idle_for(120);
        VERIFY_AND_CLEAR(driver);
    }

    expect_lang_toggle(this, driver, false); // OSは変わっていない
}

/* 左を先に離すと、右が残っているのでFnレイヤーへ戻る。右も離せばベースへ戻る。
 * 左の layer_off() をそのまま通すと、右を押しているのにFnレイヤーごと落ちる */
TEST_F(ConfLayer, releasing_left_fn_returns_to_fn_layer) {
    TestDriver driver;
    set_windmill_keymap();

    auto fn_l = key(POS_FN_L);
    auto fn_r = key(POS_FN_R);
    auto win  = key(POS_WIN); // 設定 = MY_WIN (既定のままなので何も変わらない) / Fn = KC_F11 / 英数 = KC_A

    {
        InSequence s;
        // 設定レイヤーではレポートが出ない
        EXPECT_REPORT(driver, (KC_F11)); // 左を離した後はFnレイヤー
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_A)); // 右も離した後は英数
        EXPECT_EMPTY_REPORT(driver);
    }

    fn_l.press();
    run_one_scan_loop();
    fn_r.press();
    run_one_scan_loop();
    idle_for(50);
    tap_key(win, 50);
    idle_for(50);
    fn_l.release();
    run_one_scan_loop();
    idle_for(50);
    tap_key(win, 50);
    idle_for(50);
    fn_r.release();
    run_one_scan_loop();
    idle_for(50);
    tap_key(win, 50);
    idle_for(50);
    VERIFY_AND_CLEAR(driver);
}

/* 右を先に離しても同じ。離したほうを押し直せば、また設定レイヤーへ入る */
TEST_F(ConfLayer, releasing_right_fn_returns_to_fn_layer_and_back) {
    TestDriver driver;
    set_windmill_keymap();

    auto fn_l = key(POS_FN_L);
    auto fn_r = key(POS_FN_R);
    auto win  = key(POS_WIN);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_F11)); // 右を離した後はFnレイヤー
        EXPECT_EMPTY_REPORT(driver);
        // 右を押し直すと設定レイヤー。レポートは出ない
        EXPECT_REPORT(driver, (KC_A)); // 両方離した後は英数
        EXPECT_EMPTY_REPORT(driver);
    }

    fn_l.press();
    run_one_scan_loop();
    fn_r.press();
    run_one_scan_loop();
    idle_for(50);
    fn_r.release();
    run_one_scan_loop();
    idle_for(50);
    tap_key(win, 50);
    idle_for(50);
    fn_r.press();
    run_one_scan_loop();
    idle_for(50);
    tap_key(win, 50);
    idle_for(50);
    fn_r.release();
    run_one_scan_loop();
    fn_l.release();
    run_one_scan_loop();
    idle_for(50);
    tap_key(win, 50);
    idle_for(50);
    VERIFY_AND_CLEAR(driver);
}

/* かなベースでは Fn が LT() になる。片方を離すイベントを消費しても、
 * 残りを離せばかなへ戻る (Fnレイヤーへ貼りつかない) */
TEST_F(ConfLayer, releasing_one_fn_on_kana_returns_to_fn_layer) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto fn_l = key(POS_FN_L); // そ  LT(3,KC_C)
    auto fn_r = key(POS_FN_R); // ね  LT(3,KC_COMMA)
    auto win  = key(POS_WIN);  // 設定 = MY_WIN / Fn = KC_F11 / かな = KC_Q

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_F11)); // 左を離した後はFnレイヤー
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_Q)); // 右も離した後はかな
        EXPECT_EMPTY_REPORT(driver);
    }

    fn_l.press();
    run_one_scan_loop();
    fn_r.press();
    run_one_scan_loop();
    idle_for(250); // TAPPING_TERM 超え。ホールド確定
    tap_key(win, 50);
    idle_for(50);
    fn_l.release();
    run_one_scan_loop();
    idle_for(50);
    tap_key(win, 50);
    idle_for(50);
    fn_r.release();
    run_one_scan_loop();
    idle_for(50);
    tap_key(win, 50);
    idle_for(50);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

/* 英数ベース (左右とも MO(3)) で、両方押している間は設定レイヤー */
TEST_F(ConfLayerAlpha, both_fn_reach_conf_layer) {
    TestDriver driver;
    set_windmill_keymap();

    EXPECT_NO_REPORT(driver);
    tap_on_conf(POS_ANDR);
    settle();
    VERIFY_AND_CLEAR(driver);

    expect_lang_toggle(this, driver, true);
}

/* かなベース (LT(3,KC_C) / LT(3,KC_COMMA)) でも同じ */
TEST_F(ConfLayerKana, both_fn_reach_conf_layer) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    EXPECT_NO_REPORT(driver);
    tap_on_conf(POS_ANDR);
    settle();
    VERIFY_AND_CLEAR(driver);

    expect_lang_toggle(this, driver, true);
}

/* 別キー割り込みでホールドが確定する経路 (HOLD_ON_OTHER_KEY_PRESS)。
 * TAPPING_TERM を待たずに3つ続けて押しても、割り込んだ設定キー自身が
 * 設定レイヤーで解決される。「そ」「ね」も出ない */
TEST_F(ConfLayerKanaInterrupt, both_fn_confirmed_by_interrupt) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto fn_l = key(POS_FN_L);
    auto fn_r = key(POS_FN_R);

    EXPECT_NO_REPORT(driver);
    fn_l.press();
    run_one_scan_loop();
    idle_for(20);
    fn_r.press();
    run_one_scan_loop();
    idle_for(20);
    tap_key(key(POS_ANDR), 20);
    idle_for(20);
    fn_r.release();
    run_one_scan_loop();
    fn_l.release();
    run_one_scan_loop();
    settle();
    VERIFY_AND_CLEAR(driver);

    expect_lang_toggle(this, driver, true);
}
