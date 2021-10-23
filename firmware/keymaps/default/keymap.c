/* Copyright 2021 Tsutomu Kawamura
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

enum layers {
    _MAIN,
    _SYM,
    _FN,
};

// Readability keycodes
#define LOWER LT(_SYM, KC_F23)
#define RAISE LT(_SYM, KC_F24)
#define FUNC  MO(_FN)
#define SNADS LSFT_T(KC_SPC)

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

  [_MAIN] = LAYOUT_ortho_4x12(
    KC_ESC,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_ENT,
    KC_MINS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT,
    KC_TAB,  KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_UP,   KC_RGHT,
    KC_LCTL, KC_LGUI, KC_LALT, KC_BSPC, LOWER,   SNADS,   SNADS,   RAISE,   KC_DEL,  FUNC,    KC_LEFT, KC_DOWN
  ),

  [_SYM] = LAYOUT_ortho_4x12(
    _______, KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    _______,
    KC_TILD, KC_EXLM, KC_AT,   KC_HASH, KC_DLR,  KC_PERC, KC_CIRC, KC_AMPR, KC_ASTR, KC_LPRN, KC_RPRN, KC_GRV,
    _______, KC_QUES, KC_EQL,  KC_PLUS, KC_PIPE, KC_LBRC, KC_RBRC, KC_BSLS, KC_LCBR, KC_RCBR, KC_SLSH, XXXXXXX,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, XXXXXXX, XXXXXXX
  ),

  [_FN] = LAYOUT_ortho_4x12(
    RESET,   _______, _______, _______, _______, _______, _______, _______, _______, _______, RGB_MOD, RGB_TOG,
    KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,
    _______, KC_PSCR, KC_INS,  _______, _______, KC_BRID, KC_BRIU, KC_MUTE, KC_VOLD, KC_VOLU, KC_PGUP, KC_END,
    _______, _______, _______, _______, _______, KC_SPC,  KC_SPC,  _______, _______, _______, KC_HOME, KC_PGDN
  )

};

/*
 * Colors
 */

#define HSV_BASE    43, 43, 63
#define RGB_SPECIAL 0x66, 0x66, 0x44
#define RGB_MODKEY  0x22, 0x33, 0x00
#define RGB_CURSOR  0x66, 0x66, 0x44
#define RGB_DANGER  0xFF, 0x66, 0x00
#define RGB_SYMBOL  0x22, 0x44, 0x44
#define RGB_NUMBER  0x00, 0x33, 0x88
#define RGB_BRACKET 0xFF, 0x00, 0xCC
#define RGB_FUNCKEY 0x44, 0x11, 0x11
#define RGB_MEDIA   0x66, 0x03, 0x03

/*
 * RGB Matrix Timeout
 */

#define RGBMATRIX_TIMEOUT 1 // in minutes
static uint16_t idle_timer = 0;
static uint8_t halfmin_counter = 0;
static bool led_on = true;

void refresh_rgb_matrix_timeout(void) {
  if (led_on == false) {
    rgb_matrix_sethsv(HSV_BASE);
    led_on = true;
  }
  idle_timer = timer_read();
  halfmin_counter = 0;
}
void process_rgb_matrix_timeout(void) {
  if (idle_timer == 0) idle_timer = timer_read();

  if (led_on && timer_elapsed(idle_timer) > 30000) {
    halfmin_counter++;
    idle_timer = timer_read();
  }

  if (led_on && halfmin_counter >= RGBMATRIX_TIMEOUT * 2) {
    rgb_matrix_sethsv(HSV_OFF);
    rgb_matrix_set_color_all(RGB_OFF);
    led_on = false;
    halfmin_counter = 0;
  }
}

/*
 * QMK callbacks
 */

void keyboard_post_init_user(void) {
  rgb_matrix_mode(RGB_MATRIX_NONE);
  rgb_matrix_sethsv(HSV_BASE);
}

void rgb_matrix_indicators_user(void) {
  if (led_on == false) {
    return;
  }

  rgb_matrix_set_color(10, RGB_SPECIAL);
  rgb_matrix_set_color(21, RGB_SPECIAL);
  rgb_matrix_set_color(34, RGB_SPECIAL);
  // mod keys
  rgb_matrix_set_color(46, RGB_MODKEY);
  rgb_matrix_set_color(47, RGB_MODKEY);
  rgb_matrix_set_color(48, RGB_MODKEY);
  rgb_matrix_set_color(55, RGB_MODKEY);
  // layer and spaces
  rgb_matrix_set_color(50, RGB_MODKEY);
  rgb_matrix_set_color(51, RGB_MODKEY);
  rgb_matrix_set_color(52, RGB_MODKEY);
  rgb_matrix_set_color(53, RGB_MODKEY);
  // del, bs
  rgb_matrix_set_color(49, RGB_DANGER);
  rgb_matrix_set_color(54, RGB_DANGER);
  // symbols
  rgb_matrix_set_color(22, RGB_SYMBOL);
  rgb_matrix_set_color(32, RGB_SYMBOL);
  rgb_matrix_set_color(33, RGB_SYMBOL);
  rgb_matrix_set_color(42, RGB_SYMBOL);
  rgb_matrix_set_color(43, RGB_SYMBOL);

  if (layer_state_is(_SYM)) {
    for (uint8_t i = 11; i <= 20; i++) {
      rgb_matrix_set_color(i, RGB_NUMBER);
    }
    for (uint8_t i = 22; i <= 33; i++) {
      rgb_matrix_set_color(i, RGB_SYMBOL);
    }
    for (uint8_t i = 35; i <= 44; i++) {
      rgb_matrix_set_color(i, RGB_SYMBOL);
    }
    // brackets
    rgb_matrix_set_color(31, RGB_BRACKET);
    rgb_matrix_set_color(32, RGB_BRACKET);
    rgb_matrix_set_color(39, RGB_BRACKET);
    rgb_matrix_set_color(40, RGB_BRACKET);
    rgb_matrix_set_color(42, RGB_BRACKET);
    rgb_matrix_set_color(43, RGB_BRACKET);
    return;
  } else {  
    // cursors
    rgb_matrix_set_color(44, RGB_CURSOR);
    rgb_matrix_set_color(45, RGB_CURSOR);
    rgb_matrix_set_color(56, RGB_CURSOR);
    rgb_matrix_set_color(57, RGB_CURSOR);
  }
  
  if (layer_state_is(_FN)) {
    for (uint8_t i = 22; i <= 33; i++) {
      rgb_matrix_set_color(i, RGB_FUNCKEY);
    }
    // cursors
    rgb_matrix_set_color(44, RGB_FUNCKEY);
    rgb_matrix_set_color(45, RGB_FUNCKEY);
    rgb_matrix_set_color(56, RGB_FUNCKEY);
    rgb_matrix_set_color(57, RGB_FUNCKEY);
    // spaces
    rgb_matrix_set_color(51, RGB_FUNCKEY);
    rgb_matrix_set_color(52, RGB_FUNCKEY);
    // sound
    rgb_matrix_set_color(41, RGB_MEDIA);
    rgb_matrix_set_color(42, RGB_MEDIA);
    rgb_matrix_set_color(43, RGB_MEDIA);
    // other fn
    rgb_matrix_set_color(35, RGB_FUNCKEY);
    rgb_matrix_set_color(36, RGB_FUNCKEY);
    return;
  }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
  if (record->event.pressed) refresh_rgb_matrix_timeout();
  return true;
}

void matrix_scan_user(void) {
  process_rgb_matrix_timeout();
}

