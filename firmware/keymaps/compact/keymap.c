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
    _KANA,
    _SYM,
    _FN,
};

enum custom_keycodes {
  KANA = SAFE_RANGE,
  EISU,
};

// Readability keycodes
#define FUNC MO(_FN)

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

  [_MAIN] = LAYOUT_ortho_4x12(
    KC_ESC,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_ENT,
    KC_MINS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT,
    KC_BSPC, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_UP,   KC_RGHT,
    KC_LCTL, KC_LGUI, KC_LALT, EISU,    KC_BSLS, KC_SPC,  KC_SPC,  KC_SLSH, KANA,    KC_TAB,  KC_LEFT, KC_DOWN
  ),

  [_KANA] = LAYOUT_ortho_4x12(
    _______, KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    _______,
    KC_UNDS, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, KC_LBRC,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
    _______, _______, _______, _______, KC_GRV,  _______, _______, _______, _______, _______, _______, _______
  ),

  [_SYM] = LAYOUT_ortho_4x12(
    _______, KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    _______,
    KC_TILD, KC_EXLM, KC_AT,   KC_HASH, KC_DLR,  KC_PERC, KC_CIRC, KC_AMPR, KC_ASTR, KC_LPRN, KC_RPRN, KC_GRV,
    _______, _______, KC_EQL,  KC_PLUS, _______, KC_LBRC, KC_RBRC, _______, KC_LCBR, KC_RCBR, _______, _______,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______
  ),

  [_FN] = LAYOUT_ortho_4x12(
    RESET,   _______, _______, _______, _______, _______, _______, _______, _______, _______, RGB_MOD, RGB_TOG,
    KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,
    KC_DEL,  KC_PSCR, KC_INS,  _______, _______, KC_BRID, KC_BRIU, KC_MUTE, KC_VOLD, KC_VOLU, KC_PGUP, KC_END,
    _______, _______, _______, _______, _______, KC_SPC,  KC_SPC,  _______, _______, _______, KC_HOME, KC_PGDN
  ),

};

/*
 * Colors
 */

typedef struct {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  int len;
  int ps[32];
} ColorPos;

#define HSV_BASE    43, 43, 63
#define RGB_SPECIAL 0x22, 0x11, 0x00
#define RGB_SYMBOL  0x22, 0x44, 0x44
#define RGB_NUMBER  0x66, 0x66, 0x44
#define RGB_BRACKET 0x22, 0x33, 0x00
#define RGB_FUNCKEY 0x66, 0x66, 0x44
#define RGB_MEDIA   0x00, 0x33, 0x55

ColorPos colorset[] = {
  // main
  {RGB_SPECIAL, 13, {10, 21, 34, 44, 45, 46, 47, 48, 49, 54, 55, 56, 57}},
  {RGB_SYMBOL,   7, {22, 32, 33, 42, 43, 50, 53}},
  {RGB_BRACKET,  2, {42, 43}},
  // sym
  {RGB_NUMBER,  10, {11, 12, 13, 14, 15, 16, 17, 18, 19, 20}},
  {RGB_SYMBOL,  16, {22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 36, 37, 39, 40}},
  {RGB_BRACKET,  6, {31, 32, 39, 40, 42, 43}},
  // fn
  {RGB_FUNCKEY, 20, {22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 35, 36, 44, 45, 51, 52, 56, 57}},
  {RGB_MEDIA,    3, {41, 42, 43}},
  // kana
  {RGB_SYMBOL,   2, {22, 33}},
  // かな記号
  {RGB_SYMBOL,   3, {42, 43}},
  {RGB_BRACKET,  2, {39, 40}},
};

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
 * Kana
 */
 
static bool is_kana = false;

void kana_on(void) {
  is_kana = true;
  layer_on(_KANA);

  tap_code(KC_LANG1); // Mac
  tap_code(KC_HENK); // Win
}

void kana_off(void) {
  is_kana = false;
  layer_off(_KANA);

  tap_code(KC_LANG2); // Mac
  tap_code(KC_MHEN); // Win
}

/*
 * Utility methods
 */

void set_color_range(ColorPos cp) {
  for (int i = 0; i < cp.len; ++i) {
    rgb_matrix_set_color(cp.ps[i], cp.red, cp.green, cp.blue);
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
  uint8_t mod_state = get_mods();
  bool shifted = (mod_state & MOD_MASK_SHIFT);
  
  if (layer_state_is(_KANA)) {
    set_color_range(colorset[0]);
    set_color_range(colorset[8]);
    if (shifted) {
      set_color_range(colorset[9]);
      set_color_range(colorset[10]);
    }
  } else {
    set_color_range(colorset[0]);
    set_color_range(colorset[1]);
    if (shifted) {
      set_color_range(colorset[2]);
    }
  }

  if (layer_state_is(_SYM)) {
    set_color_range(colorset[3]);
    set_color_range(colorset[4]);
    set_color_range(colorset[5]);
    return;
  }
  
  if (layer_state_is(_FN)) {
    set_color_range(colorset[6]);
    set_color_range(colorset[7]);
    return;
  }
}

static int shift_counter = 0;
static uint16_t shift_timer = 0;
static int shift_mode = 0;
static int sym_counter = 0;
static uint16_t sym_timer = 0;
static int fn_counter = 0;
static uint16_t fn_timer = 0;
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
  bool pressed = record->event.pressed;
  if (pressed) {
    ++shift_counter;
    refresh_rgb_matrix_timeout();
  }

  uint8_t mod_state = get_mods();
  bool shifted = (mod_state & MOD_MASK_SHIFT);

  switch (keycode) {
    case KC_LCTL:
      // かな入力中にCTRLキーを押した場合かなレイヤーをオフ
      if (is_kana) {
        if (pressed) {
          layer_off(_KANA);
        } else {
          layer_on(_KANA);
        }
      }
      break;
    case KC_SPC:
      if (pressed) {
        ++shift_mode;
        if (shift_mode > 2) {
          shift_mode = 2;
        }
        switch (shift_mode) {
          case 1:
            register_mods(MOD_MASK_SHIFT);
            shift_counter = 0;
            shift_timer = timer_read();
            break;
          case 2:
            tap_code(KC_SPC);
            break;
        }
      } else {
        if (shift_mode == 1) {
          unregister_mods(MOD_MASK_SHIFT);
        }
        --shift_mode;
        if (shift_mode < 0) {
          shift_mode = 0;
        }
        if (shift_counter == 0 && timer_elapsed(shift_timer) < TAPPING_TERM) {
          tap_code(KC_SPC);
        }
      }
      return false;
      break;
    case KC_BSLS: // ホールドでSYMキー
    case KC_SLSH:
      if (!is_kana) {
        if (pressed) {
          layer_on(_SYM);
          sym_counter = 0;
          sym_timer = timer_read();
        } else {
          layer_off(_SYM);
          if (sym_counter == 0 && timer_elapsed(sym_timer) < TAPPING_TERM) {
            tap_code(keycode);
          }
        }
        return false;
      }
      break;
    case KANA: // ホールドでFNキー
      if (pressed) {
        layer_on(_FN);
        fn_counter = 0;
        fn_timer = timer_read();
      } else {
        layer_off(_FN);
        if (fn_counter == 0 && timer_elapsed(fn_timer) < TAPPING_TERM) {
          kana_on();
        }
      }
      return false;
      break;
    case EISU: // ホールドでFNキー
      if (pressed) {
        layer_on(_FN);
        fn_counter = 0;
        fn_timer = timer_read();
      } else {
        layer_off(_FN);
        if (fn_counter == 0 && timer_elapsed(fn_timer) < TAPPING_TERM) {
          kana_off();
        }
      }
      return false;
      break;
    case KC_LBRC: // ゜
      if (is_kana && shifted && pressed) { 
        del_mods(MOD_MASK_SHIFT); tap_code(KC_RBRC); set_mods(mod_state);
        return false;
      }
      break;
    case KC_UNDS: // ・
      if (is_kana && shifted && pressed) { 
        tap_code(KC_SLSH);
        return false;
      }
      break;
    case KC_B: // 「
      if (is_kana && shifted && pressed) { 
        tap_code(KC_LBRC);
        return false;
      }
      break;
    case KC_N: // 」
      if (is_kana && shifted && pressed) { 
        tap_code(KC_RBRC);
        return false;
      }
      break;
    case KC_1: // へ
      if (is_kana && shifted && pressed) {
        del_mods(MOD_MASK_SHIFT); tap_code(KC_EQL); set_mods(mod_state);
        return false;
      }
      break;
    case KC_2: // ほ
      if (is_kana && shifted && pressed) {
        del_mods(MOD_MASK_SHIFT); tap_code(KC_MINS); set_mods(mod_state);
        return false;
      }
      break;
    case KC_A: // 
      if (is_kana && shifted && pressed) {
        del_mods(MOD_MASK_SHIFT); tap_code(KC_Q); set_mods(mod_state);
        return false;
      }
      break;
    case KC_S: // 
      if (is_kana && shifted && pressed) {
        del_mods(MOD_MASK_SHIFT); tap_code(KC_W); set_mods(mod_state);
        return false;
      }
      break;
    case KC_D: // 
      if (is_kana && shifted && pressed) {
        del_mods(MOD_MASK_SHIFT); tap_code(KC_E); set_mods(mod_state);
        return false;
      }
      break;
    case KC_F: // 
      if (is_kana && shifted && pressed) {
        del_mods(MOD_MASK_SHIFT); tap_code(KC_R); set_mods(mod_state);
        return false;
      }
      break;
    case KC_G: // 
      if (is_kana && shifted && pressed) {
        del_mods(MOD_MASK_SHIFT); tap_code(KC_T); set_mods(mod_state);
        return false;
      }
      break;
    case KC_H: // 
      if (is_kana && shifted && pressed) {
        del_mods(MOD_MASK_SHIFT); tap_code(KC_Y); set_mods(mod_state);
        return false;
      }
      break;
    case KC_J: // 
      if (is_kana && shifted && pressed) {
        del_mods(MOD_MASK_SHIFT); tap_code(KC_U); set_mods(mod_state);
        return false;
      }
      break;
    case KC_K: // 
      if (is_kana && shifted && pressed) {
        del_mods(MOD_MASK_SHIFT); tap_code(KC_I); set_mods(mod_state);
        return false;
      }
      break;
    case KC_L: // 
      if (is_kana && shifted && pressed) {
        del_mods(MOD_MASK_SHIFT); tap_code(KC_O); set_mods(mod_state);
        return false;
      }
      break;
    case KC_SCLN: // 
      if (is_kana && shifted && pressed) {
        del_mods(MOD_MASK_SHIFT); tap_code(KC_P); set_mods(mod_state);
        return false;
      }
      break;
    case KC_X: // 
      if (is_kana && shifted && pressed) { 
        del_mods(MOD_MASK_SHIFT); tap_code(KC_QUOT); set_mods(mod_state);
        return false;
      }
      break;
    case KC_M: // む
      if (is_kana && shifted && pressed) { 
        del_mods(MOD_MASK_SHIFT); tap_code(KC_BSLS); set_mods(mod_state);
        return false;
      }
      break;
  }

  return true;
}

void matrix_scan_user(void) {
  process_rgb_matrix_timeout();
}
