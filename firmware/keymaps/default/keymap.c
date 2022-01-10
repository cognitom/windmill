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
    _SYM_L,
    _SYM_R,
    _FN,
};

enum custom_keycodes {
  THUM_L3 = SAFE_RANGE, THUM_L2, THUM_L1, THUM_R1, THUM_R2, THUM_R3,
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

  [_MAIN] = LAYOUT_ortho_4x12(
    KC_ESC,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_ENT,
    KC_TAB,  KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT,
    KC_BSPC, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_UP,   KC_RGHT,
    KC_LCTL, KC_LGUI, KC_LALT, THUM_L3, THUM_L2, THUM_L1, THUM_R1, THUM_R2, THUM_R3, KC_APP,  KC_LEFT, KC_DOWN
  ),

  [_KANA] = LAYOUT_ortho_4x12(
    KC_ESC,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_ENT,
    KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC,
    KC_BSPC, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT,
    KC_LCTL, KC_Z,    KC_X,    _______, _______, _______, _______, _______, _______, KC_DOT,  KC_SLSH, KC_GRV
  ),

  [_SYM] = LAYOUT_ortho_4x12(
    _______, KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    _______,
    _______, KC_EXLM, KC_AT,   KC_HASH, KC_DLR,  KC_PERC, KC_CIRC, KC_AMPR, KC_ASTR, KC_LPRN, KC_RPRN, KC_GRV,
    _______, KC_EQL,  KC_PLUS, KC_MINS, KC_UNDS, KC_LBRC, KC_RBRC, KC_TILD, KC_LCBR, KC_RCBR, _______, _______,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______
  ),

  [_SYM_L] = LAYOUT_ortho_4x12(
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
    _______, _______, _______, _______, _______, KC_PIPE, KC_PIPE, _______, _______, _______, _______, _______
  ),

  [_SYM_R] = LAYOUT_ortho_4x12(
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
    _______, _______, _______, _______, _______, KC_QUES, KC_QUES, _______, _______, _______, _______, _______
  ),

  [_FN] = LAYOUT_ortho_4x12(
    RESET,   _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, RGB_TOG,
    KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,
    KC_DEL,  KC_PSCR, KC_INS,  _______, _______, KC_BRID, KC_BRIU, KC_MUTE, KC_VOLD, KC_VOLU, KC_PGUP, KC_END,
    _______, _______, _______, _______, _______, KC_SPC,  KC_SPC,  _______, _______, _______, KC_HOME, KC_PGDN
  ),

};

/*
 * Colors
 */

#define HSV_BASE    43, 43, 63
#define RGB_SPECIAL 0x1c, 0x11, 0x00
#define RGB_SYMBOL  0x11, 0x22, 0x22
#define RGB_NUMBER  0x66, 0x66, 0x44
#define RGB_BRACKET 0x22, 0x33, 0x00
#define RGB_FUNCKEY 0x66, 0x66, 0x44
#define RGB_MEDIA   0x00, 0x33, 0x55

#define HSV_BASE_DARK    0, 0, 0
#define RGB_SPECIAL_DARK 0x04, 0x03, 0x00
#define RGB_SYMBOL_DARK  0x02, 0x04, 0x03
#define RGB_NUMBER_DARK  0x06, 0x06, 0x04
#define RGB_BRACKET_DARK 0x02, 0x03, 0x00
#define RGB_FUNCKEY_DARK 0x06, 0x06, 0x04
#define RGB_MEDIA_DARK   0x00, 0x03, 0x05

enum keyset_types {
  KEYS_ALPHA_SPECIALS,
  KEYS_ALPHA_SYMBOLS,
  KEYS_ALPHA_BRACKETS,
  KEYS_NUMBERS,
  KEYS_SYMBOLS,
  KEYS_BRACKETS,
  KEYS_FUNC,
  KEYS_MEDIA,
  KEYS_KANA_SPECIALS,
  KEYS_KANA_SYMBOLS,
  KEYS_KANA_SHIFTED_SYMBOLS,
  KEYS_KANA_BRACKETS,
};
const int keysets[][32] = {
  [KEYS_ALPHA_SPECIALS]       = {10, 21, 22, 34, 44, 45, 46, 47, 48, 49, 54, 55, 56, 57},
  [KEYS_ALPHA_SYMBOLS]        = {32, 33, 42, 43, 50, 53},
  [KEYS_ALPHA_BRACKETS]       = {42, 43},
  [KEYS_NUMBERS]              = {11, 12, 13, 14, 15, 16, 17, 18, 19, 20},
  [KEYS_SYMBOLS]              = {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 35, 36, 37, 38, 41},
  [KEYS_BRACKETS]             = {31, 32, 39, 40, 42, 43},
  [KEYS_FUNC]                 = {22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 35, 36, 44, 45, 51, 52, 56, 57},
  [KEYS_MEDIA]                = {41, 42, 43},
  [KEYS_KANA_SPECIALS]        = {10, 21, 22, 34, 46},
  [KEYS_KANA_SYMBOLS]         = {33},
  [KEYS_KANA_SHIFTED_SYMBOLS] = {42, 43, 44, 45},
  [KEYS_KANA_BRACKETS]        = {31, 32},
};

/*
 * RGB Matrix Timeout
 */

#define RGBMATRIX_TIMEOUT 3 // in minutes
static uint16_t idle_timer = 0;
static uint8_t halfmin_counter = 0;
static bool led_on = true;
static bool led_darkmode = false;

void refresh_rgb_matrix_timeout(void) {
  if (led_on == false) {
    if (led_darkmode) rgb_matrix_sethsv(HSV_BASE_DARK);
    else rgb_matrix_sethsv(HSV_BASE);
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

void send_kana(void) {
  clear_keyboard();
  tap_code(KC_LANG1); // Mac, Microsoft IME
  tap_code(KC_HENK); // Mozc
}

void send_alpha(void) {
  clear_keyboard();
  tap_code(KC_LANG2); // Mac, Microsoft IME
  tap_code(KC_MHEN); // Mozc
}

void kana_on(void) {
  is_kana = true;
  layer_on(_KANA);
  send_kana();
}

void kana_off(void) {
  is_kana = false;
  layer_off(_KANA);
  send_alpha();
}

/*
 * Utility methods
 */

void set_color_to_keyset(uint8_t red, uint8_t green, uint8_t blue, uint8_t red_d, uint8_t green_d, uint8_t blue_d, int keyset_type) {
  for (int i = 0; i < 32; ++i) {
    if (keysets[keyset_type][i]) {
      if (led_darkmode) rgb_matrix_set_color(keysets[keyset_type][i], red_d, green_d, blue_d);
      else rgb_matrix_set_color(keysets[keyset_type][i], red, green, blue);
    }
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
    set_color_to_keyset(RGB_SPECIAL, RGB_SPECIAL_DARK, KEYS_KANA_SPECIALS);
    set_color_to_keyset(RGB_SYMBOL, RGB_SYMBOL_DARK, KEYS_KANA_SYMBOLS);
    if (shifted) {
      set_color_to_keyset(RGB_SYMBOL, RGB_SYMBOL_DARK, KEYS_KANA_SHIFTED_SYMBOLS);
      set_color_to_keyset(RGB_BRACKET, RGB_BRACKET_DARK, KEYS_KANA_BRACKETS);
    }
  } else {
    set_color_to_keyset(RGB_SPECIAL, RGB_SPECIAL_DARK, KEYS_ALPHA_SPECIALS);
    set_color_to_keyset(RGB_SYMBOL, RGB_SYMBOL_DARK, KEYS_ALPHA_SYMBOLS);
    if (shifted) set_color_to_keyset(RGB_BRACKET, RGB_BRACKET_DARK, KEYS_ALPHA_BRACKETS);
  }

  if (layer_state_is(_SYM)) {
    set_color_to_keyset(RGB_NUMBER, RGB_NUMBER_DARK, KEYS_NUMBERS);
    set_color_to_keyset(RGB_SYMBOL, RGB_SYMBOL_DARK, KEYS_SYMBOLS);
    set_color_to_keyset(RGB_BRACKET, RGB_BRACKET_DARK, KEYS_BRACKETS);
    return;
  }
  
  if (layer_state_is(_FN)) {
    set_color_to_keyset(RGB_FUNCKEY, RGB_FUNCKEY_DARK, KEYS_FUNC);
    set_color_to_keyset(RGB_MEDIA, RGB_MEDIA_DARK, KEYS_MEDIA);
    return;
  }
}

static int shift_counter = 0; // Shift(L1,R1)キーの後にいくつキーが押されたか
static uint16_t shift_timer = 0;
static int shift_mode = 0; // Shiftキー(L1,R1)自体がいくつ押されているか
static int sym_counter = 0; // Sym(L2,R2)キーの後にいくつキーが押されたか
static uint16_t sym_timer = 0;
static int fn_counter = 0; // Fn(L3,R3)キーの後にいくつキーが押されたか
static uint16_t fn_timer = 0;

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
  bool pressed = record->event.pressed;
  if (pressed) {
    if (led_on == false) {
      refresh_rgb_matrix_timeout();
      return false;
    } else {
      refresh_rgb_matrix_timeout();
    }
    ++shift_counter;
    ++sym_counter;
    ++fn_counter;
  }

  uint8_t mod_state = get_mods();
  bool ctrled = (mod_state & MOD_MASK_CTRL);
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
    case KC_ESC:
      if (pressed && is_kana) {
        // かな入力中のみ、ESCでかな入力をオフに
        if (ctrled) {
          // ただし、Ctrl押下中はプレーンなESCを送出
          del_mods(MOD_MASK_CTRL);
          tap_code(KC_ESC);
          set_mods(mod_state);
        } else {
          kana_off();
        }
        return false;
      }
      break;
    case THUM_L1:
    case THUM_R1:
      if (pressed) {
        ++shift_mode;
        if (shift_mode > 2) shift_mode = 2;
        switch (shift_mode) {
          case 1:
            register_mods(MOD_MASK_SHIFT);
            shift_counter = 0;
            shift_timer = timer_read();
            break;
          case 2:
            if (is_kana) unregister_mods(MOD_MASK_SHIFT);
            register_code(KC_SPC);
            break;
        }
      } else {
        switch (shift_mode) {
          case 1:
            unregister_mods(MOD_MASK_SHIFT);
            break;
          case 2:
            unregister_code(KC_SPC);
            if (is_kana) register_mods(MOD_MASK_SHIFT);
            break;
        }
        --shift_mode;
        if (shift_mode < 0) {
          shift_mode = 0;
        }
        if (shift_counter == 0 && timer_elapsed(shift_timer) < TAPPING_TERM) {
          if (is_kana) {
            if (ctrled) {
              del_mods(MOD_MASK_CTRL); // Ctrlを無効化
              tap_code(KC_SPC); // スペース
              set_mods(mod_state);
            } else {
              if (keycode == THUM_L1) tap_code(KC_B); // 「こ」
              if (keycode == THUM_R1) tap_code(KC_N); // 「み」
            }
          } else {
            tap_code(KC_SPC); // スペース
          }
        }
      }
      return false;
      break;
    case THUM_L2:
    case THUM_R2:
      if (pressed) {
        if (is_kana) send_alpha();
        layer_on(_SYM);
        if (keycode == THUM_L2) layer_on(_SYM_L); // sym -> space で「|」を出力するように
        if (keycode == THUM_R2) layer_on(_SYM_R); // sym -> space で「?」を出力するように
        sym_counter = 0;
        sym_timer = timer_read();
      } else {
        if (is_kana) send_kana();
        layer_off(_SYM);
        layer_off(_SYM_L);
        layer_off(_SYM_R);
        if (sym_counter == 0 && timer_elapsed(sym_timer) < TAPPING_TERM) {
          if (is_kana) {
            if (keycode == THUM_L2) tap_code(KC_V); // 「ひ」
            if (keycode == THUM_R2) tap_code(KC_M); // 「も」
          } else {
            if (keycode == THUM_L2) tap_code(KC_BSLS); // "\"
            if (keycode == THUM_R2) tap_code(KC_SLSH); // "/"
          }
        }
      }
      return false;
      break;
    case THUM_L3:
    case THUM_R3:
      if (pressed) {
        layer_on(_FN);
        fn_counter = 0;
        fn_timer = timer_read();
      } else {
        layer_off(_FN);
        if (fn_counter == 0 && timer_elapsed(fn_timer) < TAPPING_TERM) {
          if (is_kana && !ctrled) {
            if (keycode == THUM_L3) tap_code(KC_C); // 「そ」
            if (keycode == THUM_R3) tap_code(KC_COMM); // 「ね」
          } else {
            del_mods(MOD_MASK_CTRL); // 常にCtrlを無効化
            if (keycode == THUM_L3) kana_off();
            if (keycode == THUM_R3) kana_on();
            set_mods(mod_state);
          }
        }
      }
      return false;
    case KC_UP:
    case KC_DOWN:
    case KC_LEFT:
    case KC_RIGHT:
      if (is_kana && ctrled && pressed) {
        // かな入力中は修飾キーを無効化
        del_mods(MOD_MASK_CTRL);
        tap_code(keycode);
        set_mods(mod_state);
        return false;
      }
      break;
    case KC_W: // へ
      if (is_kana && shifted && pressed) {
        del_mods(MOD_MASK_SHIFT);
        tap_code(KC_EQL);
        set_mods(mod_state);
        return false;
      }
      break;
    case KC_R: // む
      if (is_kana && shifted && pressed) { 
        del_mods(MOD_MASK_SHIFT);
        tap_code(KC_BSLS);
        set_mods(mod_state);
        return false;
      }
      break;
    case KC_U: // ほ
      if (is_kana && shifted && pressed) {
        del_mods(MOD_MASK_SHIFT);
        tap_code(KC_MINS);
        set_mods(mod_state);
        return false;
      }
      break;
    case KC_A: // っ
      if (is_kana && shifted && pressed) { 
        tap_code(KC_Z);
        return false;
      }
      break;
    case KC_O: // 「
      if (is_kana && shifted && pressed) { 
        tap_code(KC_LBRC);
        return false;
      }
      break;
    case KC_P: // 」
      if (is_kana && shifted && pressed) { 
        tap_code(KC_RBRC);
        return false;
      }
      break;
    case KC_LBRC: // ゜
      if (is_kana && shifted && pressed) { 
        del_mods(MOD_MASK_SHIFT);
        tap_code(KC_RBRC);
        set_mods(mod_state);
        return false;
      }
      break;
    case KC_K: // 、
      if (is_kana && shifted && pressed) { 
        tap_code(KC_COMM);
        return false;
      }
      break;
    case KC_L: // 。
      if (is_kana && shifted && pressed) { 
        tap_code(KC_DOT);
        return false;
      }
      break;
    case KC_SCLN: // ・
      if (is_kana && shifted && pressed) { 
        tap_code(KC_SLSH);
        return false;
      }
      break;
    case KC_QUOT: // ー
    case KC_GRV:
      if (is_kana && shifted && pressed) { 
        tap_code(KC_MINS);
        return false;
      }
      break;
    case RGB_TOG:
      if (pressed) {
        led_darkmode = !led_darkmode;
        if (led_darkmode) rgb_matrix_sethsv(HSV_BASE_DARK);
        else rgb_matrix_sethsv(HSV_BASE);
      }
      return false;
  }

  return true;
}

void matrix_scan_user(void) {
  process_rgb_matrix_timeout();
}

