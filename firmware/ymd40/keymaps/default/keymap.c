/* Copyright 2021-2026 Tsutomu Kawamura
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

#include QMK_KEYBOARD_H
#include "windmill.h"

/* レイヤー0(かな)とレイヤー1(英数)がベースレイヤーで、MY_LCTL のタップ/
 * ダブルタップで切り替わる。レイヤー1で透過のキーはレイヤー0へ落ちる。 */
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

  [LAYER_KANA] = LAYOUT_ortho_4x12(
    KC_ESC,  KC_1,         KC_2,         KC_3,        KC_4,        KC_5,          KC_6,          KC_7,        KC_8,           KC_9,    KC_0,    KC_ENT,
    KC_TAB,  KC_Q,         MY_W,         KC_E,        MY_R,        KC_T,          KC_Y,          MY_U,        KC_I,           MY_O,    MY_P,    MY_LBRC,
    KC_BSPC, MY_A,         KC_S,         KC_D,        KC_F,        KC_G,          KC_H,          KC_J,        MY_K,           MY_L,    MY_SCLN, MY_QUOT,
    MY_LCTL, LGUI_T(KC_Z), LALT_T(KC_X), LT(3,KC_C),  LT(2,KC_V),  LSFT_T(KC_B),  RSFT_T(KC_N),  LT(2,KC_M),  LT(3,KC_COMMA), KC_DOT,  KC_SLSH, KC_GRV
  ),

  [LAYER_ALPHA] = LAYOUT_ortho_4x12(
    _______, KC_Q,         KC_W,         KC_E,        KC_R,        KC_T,          KC_Y,          KC_U,        KC_I,           KC_O,    KC_P,    _______,
    _______, KC_A,         KC_S,         KC_D,        KC_F,        KC_G,          KC_H,          KC_J,        KC_K,           KC_L,    KC_SCLN, KC_QUOT,
    _______, KC_Z,         KC_X,         KC_C,        KC_V,        KC_B,          KC_N,          KC_M,        KC_COMM,        KC_DOT,  KC_UP,   KC_RGHT,
    _______, KC_LGUI,      KC_LALT,      MO(3),       LT(2,KC_BSLS), LSFT_T(KC_SPC), LSFT_T(KC_SPC), LT(2,KC_SLSH), MO(3),    KC_APP,  KC_LEFT, KC_DOWN
  ),

  [LAYER_SYM] = LAYOUT_ortho_4x12(
    _______, KC_1,         KC_2,         KC_3,        KC_4,        KC_5,          KC_6,          KC_7,        KC_8,           KC_9,    KC_0,    _______,
    _______, S(KC_1),      S(KC_2),      S(KC_3),     S(KC_4),     S(KC_5),       S(KC_6),       S(KC_7),     S(KC_8),        S(KC_9), S(KC_0), KC_GRV,
    _______, KC_EQL,       S(KC_EQL),    KC_MINS,     S(KC_MINS),  KC_LBRC,       KC_RBRC,       S(KC_GRV),   S(KC_LBRC),     S(KC_RBRC), KC_UP, KC_RGHT,
    _______, _______,      _______,      _______,     _______,     S(KC_BSLS),    S(KC_SLSH),    _______,     _______,        _______, KC_LEFT, KC_DOWN
  ),

  [LAYER_FN] = LAYOUT_ortho_4x12(
    MY_WIN,  KC_NO,        KC_NO,        MY_ANDR,     KC_NO,       KC_NO,         KC_NO,         KC_NO,       KC_NO,          KC_NO,   QK_BOOT, MY_DARK,
    KC_F1,   KC_F2,        KC_F3,        KC_F4,       KC_F5,       KC_F6,         KC_F7,         KC_F8,       KC_F9,          KC_F10,  KC_F11,  KC_F12,
    KC_DEL,  KC_PSCR,      KC_NO,        KC_NO,       KC_NO,       KC_BRID,       KC_BRIU,       KC_MUTE,     KC_VOLD,        KC_VOLU, KC_UP,   KC_RGHT,
    _______, _______,      _______,      _______,     _______,     _______,       _______,       _______,     _______,        _______, KC_LEFT, KC_DOWN
  ),

};

/*
 * Color settings
 */

enum keycolors {
  CL_CONFIG,
  CL_BASE,
  CL_SPECIAL,
  CL_SYMBOL,
  CL_NUMBER,
  CL_BRACKET,
  CL_FUNC,
  CL_MEDIA,
};

const uint8_t colorset[][6] = {
  //             Light              Dark
  //             (R,    G,    B   ) (R,    G,    B   )
  [CL_CONFIG]  = {0x66, 0x66, 0x33,  0x06, 0x06, 0x03},
  [CL_BASE]    = {0x00, 0x00, 0x00,  0x00, 0x00, 0x00},
  [CL_SPECIAL] = {0xFF, 0x00, 0x00,  0x11, 0x00, 0x00},
  [CL_SYMBOL]  = {0xFF, 0xCC, 0x00,  0x09, 0x06, 0x00},
  [CL_NUMBER]  = {0x00, 0x66, 0xFF,  0x09, 0x06, 0x00},
  [CL_BRACKET] = {0x00, 0xFF, 0x00,  0x09, 0x06, 0x00},
  [CL_FUNC]    = {0xFF, 0x66, 0x00,  0x09, 0x06, 0x00},
  [CL_MEDIA]   = {0xFF, 0xCC, 0x00,  0x09, 0x06, 0x00},
};

/* dual-role キーはタップ側のキーコードに展開されてから渡される。
 * S(KC_x) はそのまま (シフト付きの記号として分類したいため)。 */
uint8_t windmill_process_keycolor_user(uint16_t keycode) {
  switch (keycode) {
    case MY_WIN: case MY_ANDR: case MY_DARK: case QK_BOOT:
      return CL_CONFIG;
    case KC_ENT ... KC_TAB: case KC_DEL: case KC_RIGHT ... KC_UP:
    case KC_APP: case KC_INT1 ... KC_LNG2: case KC_LCTL ... KC_RGUI:
    case MY_LCTL: case QK_MOMENTARY ... QK_MOMENTARY_MAX:
      return CL_SPECIAL;
    case KC_MINS ... KC_EQL: case KC_BSLS ... KC_SLSH:
    case S(KC_1) ... S(KC_8): case S(KC_MINS) ... S(KC_EQL):
    case S(KC_BSLS) ... S(KC_GRV): case S(KC_SLSH):
    case MY_SCLN: case MY_QUOT:
      return CL_SYMBOL;
    case KC_1 ... KC_0:
      return CL_NUMBER;
    case S(KC_COMM) ... S(KC_DOT): case S(KC_9) ... S(KC_0):
    case KC_LBRC ... KC_RBRC: case S(KC_LBRC) ... S(KC_RBRC):
    case MY_LBRC:
      return CL_BRACKET;
    case KC_F1 ... KC_PGUP: case KC_END ... KC_PGDN:
      return CL_FUNC;
    case KC_MUTE ... KC_VOLD: case KC_BRIU ... KC_BRID:
      return CL_MEDIA;
  }
  return CL_BASE;
}

/*
 * QMK callbacks
 */

void keyboard_post_init_user(void) {
  windmill_init_keycolors((uint8_t*)colorset);
}
