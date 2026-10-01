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

/* Fnレイヤーのファンクションキーは数字キーに準じた位置に置く (issue #64)。
 * F1〜F10 は最上段の「1」〜「0」の位置、F11 / F12 は2段目の先頭2つ。
 *
 * 最左列の Esc / Tab は透過にしてあり、Fnをホールドしたままでもそのまま出る。
 * 以前は2段目の左端から F1〜F12 を並べていたので、Fn+Tab が F1 だった。
 *
 * Fn は英数レイヤーでは MO(3)、かなレイヤーでは LT(3,KC_C) とキーコードが違う。
 * 透過キーの落ちる先はどちらのベースでも同じ (英数でも透過) なので、両方で通す。 */

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_layer.h"
#include "action_util.h"
#include "test_keymap.hpp"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;

class FnLayer : public WindmillTest {};

// Fnホールド中に上から順に叩く位置と、そこで出るキー
static const struct {
    uint8_t row;
    uint8_t col;
    uint8_t keycode;
} fn_keys[] = {
    {0, 0, KC_ESC}, // 透過
    {0, 1, KC_F1},  {0, 2, KC_F2}, {0, 3, KC_F3}, {0, 4, KC_F4}, {0, 5, KC_F5},
    {0, 6, KC_F6},  {0, 7, KC_F7}, {0, 8, KC_F8}, {0, 9, KC_F9}, {0, 10, KC_F10},
    {1, 0, KC_TAB}, // 透過
    {1, 1, KC_F11}, {1, 2, KC_F12},
};

// 起動直後の英数から MY_LCTL 1回タップでかなへ切り替える。レポートの中身は問わない
static void switch_to_kana(WindmillTest* f, TestDriver& driver) {
    EXPECT_ANY_REPORT(driver).Times(AnyNumber());

    f->tap_key(f->key(POS_LCTL), 120);
    f->settle();

    VERIFY_AND_CLEAR(driver);
}

// Fnをホールドしたまま fn_keys を順に叩いて、1打鍵ずつレポートを突き合わせる
static void expect_fn_keys(WindmillTest* f, TestDriver& driver, KeymapKey fn) {
    {
        InSequence s;
        for (const auto& k : fn_keys) {
            EXPECT_REPORT(driver, (k.keycode));
            EXPECT_EMPTY_REPORT(driver);
        }
    }

    fn.press();
    f->run_one_scan_loop();
    f->idle_for(250); // TAPPING_TERM 超え。ホールド確定
    for (const auto& k : fn_keys) {
        f->tap_key(f->key(k.row, k.col), 50);
        f->idle_for(50);
    }
    fn.release();
    f->run_one_scan_loop();
    f->idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* 英数ベース (MO(3))。左右どちらのFnでも同じ */
TEST_F(FnLayer, function_keys_follow_number_row_on_alpha) {
    TestDriver driver;
    set_windmill_keymap();

    expect_fn_keys(this, driver, key(POS_FN_L));
    expect_fn_keys(this, driver, key(POS_FN_R));
}

/* かなベース (LT(3,KC_C) / LT(3,KC_COMMA))。「そ」「ね」は出ない */
TEST_F(FnLayer, function_keys_follow_number_row_on_kana) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    expect_fn_keys(this, driver, key(POS_FN_L));
    expect_fn_keys(this, driver, key(POS_FN_R));

    default_layer_set((layer_state_t)1 << LAYER_ALPHA); // 次のテストへかなを持ち越さない
}

/* F11 / F12 より右の2段目には何も置いていない。レポートは出ない */
TEST_F(FnLayer, rest_of_second_row_is_empty) {
    TestDriver driver;
    set_windmill_keymap();

    auto fn = key(POS_FN_L);

    EXPECT_NO_REPORT(driver);
    fn.press();
    run_one_scan_loop();
    idle_for(250);
    for (uint8_t col = 3; col < MATRIX_COLS; ++col) {
        tap_key(key(1, col), 50);
        idle_for(50);
    }
    fn.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}
