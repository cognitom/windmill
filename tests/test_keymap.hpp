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

/* テスト用のキーマップ。firmware/technik/keymaps/default/keymap.c と同じ内容を
 * QMK のテスト基盤 (set_keymap) に載せられる形で持つ。geonix41 / minipeg48 /
 * ymd40 も、windmill.c が見る範囲は同じ配置なのでこれ1枚で足りる。
 *
 * 実機の keymap.c は LAYOUT_ortho_4x12 マクロと PROGMEM に依存していて
 * そのままは使えないため、ここだけ二重管理になる。配置を変えたら両方直すこと。 */

#pragma once

#include "test_fixture.hpp"
#include "test_keymap_key.hpp"

extern "C" {
#include "windmill.h"
}

// clang-format off
static const uint16_t windmill_keymap[LAYER_SIZE][MATRIX_ROWS][MATRIX_COLS] = {

  [LAYER_KANA] = {
    {KC_ESC,  KC_1,         KC_2,         KC_3,        KC_4,          KC_5,           KC_6,           KC_7,          KC_8,           KC_9,       KC_0,    KC_ENT},
    {KC_TAB,  KC_Q,         MY_W,         KC_E,        MY_R,          KC_T,           KC_Y,           MY_U,          KC_I,           MY_O,       MY_P,    MY_LBRC},
    {KC_BSPC, MY_A,         KC_S,         KC_D,        KC_F,          KC_G,           KC_H,           KC_J,          MY_K,           MY_L,       MY_SCLN, MY_QUOT},
    {MY_LCTL, LGUI_T(KC_Z), LALT_T(KC_X), LT(3,KC_C),  LT(2,KC_V),    LSFT_T(KC_B),   LSFT_T(KC_N),   LT(2,KC_M),    LT(3,KC_COMMA), KC_DOT,     KC_SLSH, KC_GRV},
  },

  [LAYER_ALPHA] = {
    {KC_TRNS, KC_Q,         KC_W,         KC_E,        KC_R,          KC_T,           KC_Y,           KC_U,          KC_I,           KC_O,       KC_P,    KC_TRNS},
    {KC_TRNS, KC_A,         KC_S,         KC_D,        KC_F,          KC_G,           KC_H,           KC_J,          KC_K,           KC_L,       KC_SCLN, KC_QUOT},
    {KC_TRNS, KC_Z,         KC_X,         KC_C,        KC_V,          KC_B,           KC_N,           KC_M,          KC_COMM,        KC_DOT,     KC_UP,   KC_RGHT},
    {KC_TRNS, KC_LGUI,      KC_LALT,      MO(3),       LT(2,KC_BSLS), LSFT_T(KC_SPC), LSFT_T(KC_SPC), LT(2,KC_SLSH), MO(3),          KC_APP,     KC_LEFT, KC_DOWN},
  },

  [LAYER_SYM] = {
    {KC_TRNS, KC_1,         KC_2,         KC_3,        KC_4,          KC_5,           KC_6,           KC_7,          KC_8,           KC_9,       KC_0,    KC_TRNS},
    {KC_TRNS, S(KC_1),      S(KC_2),      S(KC_3),     S(KC_4),       S(KC_5),        S(KC_6),        S(KC_7),       S(KC_8),        S(KC_9),    S(KC_0), KC_GRV},
    {KC_TRNS, KC_EQL,       S(KC_EQL),    KC_MINS,     S(KC_MINS),    KC_LBRC,        KC_RBRC,        S(KC_GRV),     S(KC_LBRC),     S(KC_RBRC), KC_UP,   KC_RGHT},
    {KC_TRNS, KC_TRNS,      KC_TRNS,      KC_TRNS,     KC_TRNS,       S(KC_BSLS),     S(KC_SLSH),     KC_TRNS,       KC_TRNS,        KC_TRNS,    KC_LEFT, KC_DOWN},
  },

  [LAYER_FN] = {
    {MY_WIN,  KC_NO,        KC_NO,        MY_ANDR,     KC_NO,         KC_NO,          KC_NO,          KC_NO,         KC_NO,          KC_NO,      QK_BOOT, MY_DARK},
    {KC_F1,   KC_F2,        KC_F3,        KC_F4,       KC_F5,         KC_F6,          KC_F7,          KC_F8,         KC_F9,          KC_F10,     KC_F11,  KC_F12},
    {KC_DEL,  KC_PSCR,      KC_NO,        KC_NO,       KC_NO,         KC_BRID,        KC_BRIU,        KC_MUTE,       KC_VOLD,        KC_VOLU,    KC_UP,   KC_RGHT},
    {KC_TRNS, KC_TRNS,      KC_TRNS,      KC_TRNS,     KC_TRNS,       KC_TRNS,        KC_TRNS,        KC_TRNS,       KC_TRNS,        KC_TRNS,    KC_LEFT, KC_DOWN},
  },
};
// clang-format on

/* かなレイヤー上の位置。コメントの文字は JISかな入力での出力 */
#define POS_LCTL 3, 0  // 英数/かな
#define POS_TSU 3, 1   // つ  LGUI_T(KC_Z)  英数レイヤーでは KC_LGUI
#define POS_SA 3, 2    // さ  LALT_T(KC_X)  英数レイヤーでは KC_LALT
#define POS_NU 0, 1    // ぬ  KC_1          英数レイヤーでは "q"
#define POS_KO 3, 5    // こ  左親指Shift
#define POS_MI 3, 6    // み  右親指Shift
#define POS_NO 2, 8    // の  MY_K   Shift時 S(KC_COMM) = 、
#define POS_RA 1, 9    // ら  MY_O   Shift時 S(KC_LBRC)
#define POS_SU 1, 4    // す  MY_R   Shift時 KC_BSLS  (Shiftを外す必要がある)
#define POS_NA 1, 7    // な  MY_U   Shift時 KC_MINS = ほ (Shiftを外す必要がある)
#define POS_HA 2, 4    // は  KC_F   英数レイヤーでは "f"
#define POS_MO 3, 7    // も  LT(2,KC_M)   Shift+タップで半角?

class WindmillTest : public TestFixture {
   public:
    void set_windmill_keymap() {
        for (uint8_t layer = 0; layer < LAYER_SIZE; ++layer)
            for (uint8_t row = 0; row < MATRIX_ROWS; ++row)
                for (uint8_t col = 0; col < MATRIX_COLS; ++col)
                    add_key(KeymapKey(layer, col, row, windmill_keymap[layer][row][col]));
    }

    KeymapKey key(uint8_t row, uint8_t col) {
        return KeymapKey(LAYER_KANA, col, row, windmill_keymap[LAYER_KANA][row][col]);
    }

    // 押しっぱなしのキーが無い、落ち着いた状態にする
    void settle() {
        idle_for(TD_DTAP_TERM_MS + TAPPING_TERM);
    }

    static constexpr unsigned TD_DTAP_TERM_MS = 180; // windmill.c の TD_DTAP_TERM
    static constexpr unsigned IME_WAIT_MS     = 10;  // windmill.c の IME_WAIT_MS
};
