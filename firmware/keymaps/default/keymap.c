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
#include "kana_keycodes.h"

#ifdef CONSOLE_ENABLE
  #include "print.h"
#endif

enum layers {
    _MAIN,
    _KANA,
    _SYM,
    _SYM_L,
    _SYM_R,
    _FN,
};

// TODO: QMKのバージョンを上げて、以下2行を削除
#define KC_LNG1 KC_LANG1
#define KC_LNG2 KC_LANG2

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

  [_MAIN] = LAYOUT_ortho_4x12(
    KC_ESC,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_ENT,
    KC_TAB,  KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT,
    KC_BSPC, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_UP,   KC_RGHT,
    KC_LCTL, KC_LGUI, KC_LALT, KC_LNG2, KC_BSLS, KC_SPC,  KC_SPC,  KC_SLSH, KC_LNG1, KC_APP,  KC_LEFT, KC_DOWN
  ),

  [_KANA] = LAYOUT_ortho_4x12(
    _______, KA_NU,   KA_FU,   KA_A,    KA_U,    KA_E,    KA_O,    KA_YA,   KA_YU,   KA_YO,   KA_WA,   _______,
    _______, KA_TA,   KA_TE,   KA_I,    KA_SU,   KA_KA,   KA_N,    KA_NA,   KA_NI,   KA_RA,   KA_SE,   KA_DAKU,
    _______, KA_CHI,  KA_TO,   KA_SHI,  KA_HA,   KA_KI,   KA_KU,   KA_MA,   KA_NO,   KA_RI,   KA_RE,   KA_KE,
    _______, KA_TSU,  KA_SA,   KA_SO,   KA_HI,   KA_KO,   KA_MI,   KA_MO,   KA_NE,   KA_RU,   KA_ME,   KA_RO
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
void update_rgb_matrix_timeout(void) {
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

bool process_rgb_matrix_timeout(uint16_t keycode, keyrecord_t *record) {
  if (record->event.pressed) {
    if (!led_on) {
      refresh_rgb_matrix_timeout();
      return false;
    } else {
      refresh_rgb_matrix_timeout();
    }
  }
  return true;
}

/*
 * Kana
 */
 
static bool is_kana = false;

void send_kana(void) {
  clear_keyboard();
  tap_code(KC_LNG1); // Mac, Microsoft IME
  tap_code(KC_INT4); // Mozc
}

void send_alpha(void) {
  clear_keyboard();
  tap_code(KC_LNG2); // Mac, Microsoft IME
  tap_code(KC_INT5); // Mozc
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

void toggle_darkmode(void) {
  led_darkmode = !led_darkmode;
  if (led_darkmode) rgb_matrix_sethsv(HSV_BASE_DARK);
  else rgb_matrix_sethsv(HSV_BASE);
}

void set_color_to_keyset(uint8_t red, uint8_t green, uint8_t blue, uint8_t red_d, uint8_t green_d, uint8_t blue_d, int keyset_type) {
  for (int i = 0; i < 32; ++i) {
    if (keysets[keyset_type][i]) {
      if (led_darkmode) rgb_matrix_set_color(keysets[keyset_type][i], red_d, green_d, blue_d);
      else rgb_matrix_set_color(keysets[keyset_type][i], red, green, blue);
    }
  }
}

// 一時的に修飾キーを外した状態で、tap_codeする
void tap_code_wo_mod(uint16_t keycode, uint8_t mod_mask) {
  uint8_t mod_state = get_mods();
  del_mods(mod_mask);
  tap_code(keycode);
  set_mods(mod_state);
}

// Mod Seq
static uint16_t first_mod_keycode = 0; //
static int first_mod_row = 0;
static int first_mod_col = 0;
static int mod_follower_counter = 0;
static uint16_t mod_timer = 0;
void start_mod_sequence(uint16_t keycode, keyrecord_t *record) {
  first_mod_keycode = keycode;
  first_mod_row = record->event.key.row;
  first_mod_col = record->event.key.col;
  mod_follower_counter = 0;
  mod_timer = timer_read();
}
void end_mod_sequence(void) {
  first_mod_keycode = 0;
  mod_follower_counter = 0;
}
bool is_mod_seq_first(uint16_t keycode, keyrecord_t *record) {
  return keycode == first_mod_keycode
    && first_mod_row == record->event.key.row
    && first_mod_col == record->event.key.col;
}
bool is_mod_seq_started(void) {
  return !!first_mod_keycode;
}
bool is_mod_pressed_within(uint16_t ms) {
  if (!first_mod_keycode) return false;
  return timer_elapsed(mod_timer) < ms;
}
int get_mod_follower(void) {
  return mod_follower_counter;
}
bool process_mod_sequence(uint16_t keycode, keyrecord_t *record) {
  if (record->event.pressed && first_mod_keycode) ++mod_follower_counter;
  return true;
}

// かな入力時のシフト側の変則的な処理
// Shiftキーそのものの処理との兼ね合いで、別レイヤーにすることが難しく、
// キーごとのアドホックな処理になっている。
// TODO: 別レイヤーにして、プログラム的に抽出するハックを追加して、この関数からキー定義を追い出すこと。
bool process_kana_shifted(uint16_t keycode, keyrecord_t *record) {
  uint8_t mod_state = get_mods();
  bool shifted = (mod_state & MOD_MASK_SHIFT);
  if (!is_kana) return true;
  if (shifted) unregister_mods(MOD_MASK_SHIFT);
  switch (keycode) {
    case KA_A:    tap_code16(KA_XA);   break; // ぁ
    case KA_U:    tap_code16(KA_XU);   break; // ぅ
    case KA_E:    tap_code16(KA_XE);   break; // ぇ
    case KA_O:    tap_code16(KA_XO);   break; // ぉ
    case KA_YA:   tap_code16(KA_XYA);  break; // ゃ
    case KA_YU:   tap_code16(KA_XYU);  break; // ゅ
    case KA_YO:   tap_code16(KA_XYO);  break; // ょ
    case KA_WA:   tap_code16(KA_WO);   break; // を
    case KA_TE:   tap_code16(KA_HE);   break; // へ
    case KA_I:    tap_code16(KA_XI);   break; // ぃ
    case KA_SU:   tap_code16(KA_MU);   break; // む
    case KA_NA:   tap_code16(KA_HO);   break; // ほ
    case KA_RA:   tap_code16(KA_LKAK); break; // 「
    case KA_SE:   tap_code16(KA_RKAK); break; // 」
    case KA_DAKU: tap_code16(KA_HAN);  break; // ゜
    case KA_CHI:
    case KA_TSU:  tap_code16(KA_XTSU); break; // っ
    case KA_NO:
    case KA_NE:   tap_code16(KA_TEN);  break; // 、
    case KA_RI:
    case KA_RU:   tap_code16(KA_MARU); break; // 。
    case KA_RE:
    case KA_ME:   tap_code16(KA_NAKA); break; // ・
    case KA_KE:
    case KA_RO:   tap_code16(KA_CHOU); break; // ー
    case KA_KO:
    case KA_MI:   tap_code16(KC_SPC);  break; // スペース
    default:
      if (shifted) register_mods(MOD_MASK_SHIFT);
      return true;
  }
  if (shifted) register_mods(MOD_MASK_SHIFT);
  return false;
}

// かな入力時のみ必要な処理
bool process_kana(uint16_t keycode, keyrecord_t *record) {
  uint8_t mod_state = get_mods();
  bool ctrled = (mod_state & MOD_MASK_CTRL);
  bool shifted = (mod_state & MOD_MASK_SHIFT);
  bool pressed = record->event.pressed;

  if (!is_kana) return true;

  switch (keycode) {
    // Shift
    case KA_KO:
    case KA_MI:
      if (pressed) {
        if (!is_mod_seq_started()) {
          // 修飾キー(dual role)が最初に押された
          // 例. み(press) <-- イマココ
          start_mod_sequence(keycode, record);
          register_mods(MOD_MASK_SHIFT);
          return false;
        }
        return process_kana_shifted(keycode, record);
      }
      if (!is_mod_seq_first(keycode, record)) return false;

      unregister_mods(MOD_MASK_SHIFT);
      if (get_mod_follower() == 0 && is_mod_pressed_within(TAPPING_TERM)) {
        // 時間内に単独でタップされた
        // 例. み(press)
        //     み(release) <-- イマココ
        tap_code(keycode);
      }
      end_mod_sequence();
      return false;

    case KC_LCTL:
      // CTRLキーを押した場合かなレイヤーをオフ
      if (pressed) layer_off(_KANA);
      else layer_on(_KANA);
      break;

    case KC_ESC:
      if (pressed) {
        // ESCでかな入力をオフに ※ただし、Ctrl押下中はプレーンなESCを送出 
        if (ctrled) tap_code_wo_mod(KC_ESC, MOD_MASK_CTRL);
        else kana_off();
        return false;
      }
      break;

    case KC_UP:
    case KC_DOWN:
    case KC_LEFT:
    case KC_RIGHT:
      if (ctrled && pressed) {
        tap_code_wo_mod(keycode, MOD_MASK_CTRL); // 修飾キーを無効化
        return false;
      }
      break;
  }
  // シフト側の処理
  if (shifted && pressed) {
    if (!process_kana_shifted(keycode, record)) return false;
  }

  return true;
}

// 英字入力時のみ必要な処理
bool process_alpha(uint16_t keycode, keyrecord_t *record) {
  bool pressed = record->event.pressed;

  if (is_kana) return true;
  
  switch (keycode) {
    // Shift
    case KC_SPC:
      if (pressed) {
        if (!is_mod_seq_started()) {
          // 修飾キー(dual role)が最初に押された
          // 例. Space(press) <-- イマココ
          start_mod_sequence(keycode, record);
          register_mods(MOD_MASK_SHIFT);
          return false;
        }
        return true;
      }
      if (!is_mod_seq_first(keycode, record)) return true;

      unregister_mods(MOD_MASK_SHIFT);
      if (get_mod_follower() == 0 && is_mod_pressed_within(TAPPING_TERM)) {
        // 時間内に単独でタップされた
        // 例. Space(press)
        //     Space(release) <-- イマココ
        tap_code(keycode);
      }
      end_mod_sequence();
      return false;

    // Sym
    case KC_BSLS:
    case KC_SLSH:
      if (pressed) {
        if (!is_mod_seq_started()) {
          // 修飾キー(dual role)が最初に押された
          // 例. / (press) <-- イマココ
          start_mod_sequence(keycode, record);
          layer_on(_SYM);
          if (keycode == KC_BSLS) layer_on(_SYM_L); // sym -> space で「|」を出力するように
          if (keycode == KC_SLSH) layer_on(_SYM_R); // sym -> space で「?」を出力するように
          return false;
        }
        return true;
      }
      if (!is_mod_seq_first(keycode, record)) return true;

      layer_off(_SYM);
      layer_off(_SYM_L);
      layer_off(_SYM_R);
      if (get_mod_follower() == 0 && is_mod_pressed_within(TAPPING_TERM)) {
        // 時間内に単独でタップされた
        // 例. / (press)
        //     / (release) <-- イマココ
        tap_code(keycode);
      }
      end_mod_sequence();
      return false;
  }

  return true;
}

#define STICKY_TERM 150
static uint16_t queued_keycode = 0;
static keypos_t queued_key = { .row = 0, .col = 0 };
bool process_sticky_term(uint16_t keycode, keyrecord_t *record) {
  bool pressed = record->event.pressed;

  // 修飾キーが押されたのち、STICKY_TERM時間内であればキーを送出せずqueued_keycodeに入れて待つ
  // 例. み(press)
  //     わ(press) <-- イマココ
  if (pressed) {
    if (is_mod_pressed_within(STICKY_TERM)) {
      if (get_mod_follower() == 1) {
        queued_keycode = keycode;
        queued_key = record->event.key;
        return false;
      }
      // ただし、後続が2つ以上押された時点で、STICKY_TERM内でもキューをクリア
      if (get_mod_follower() == 2) {
        if (process_kana_shifted(queued_keycode, record)) tap_code16(queued_keycode);
      }
    }
    queued_keycode = 0;
    return true;
  }
  if (!queued_keycode) return true;

  // ここから、queued_keycodeがある場合の処理
  uint16_t queued = queued_keycode;
  queued_keycode = 0;

  // キューに入れたキーがリリースされた場合 
  // 例. み(press)
  //     わ(press)
  //     わ(release) <-- イマココ
  //     み(release)
  if (keycode == queued) {
    if (process_kana_shifted(keycode, record)) tap_code16(keycode);
    return false;
  }

  // ダブルロールキーがリリースされた場合
  switch (keycode) {
    // Shift
    case KA_KO:
    case KA_MI:
    case KC_SPC: // 英字配列の場合
      // 例. み(press)
      //     わ(press)
      //     み(release) <-- イマココ
      //     わ(release)
      if (is_mod_pressed_within(STICKY_TERM)) {
        unregister_mods(MOD_MASK_SHIFT);
        tap_code(keycode); // 例.「み」
        tap_code(queued); // 例.「わ」
        return true;
      }
      if (process_kana_shifted(queued, record)) register_code(queued); // 例.「を」
      return true;

    // Sym
    case KC_BSLS:
    case KC_SLSH:
      // 例. / (press)
      //     p (press)
      //     / (release) <-- イマココ
      //     p (release)
      if (is_mod_pressed_within(STICKY_TERM)) {
        tap_code(keycode); // 例. "/"
        tap_code16(keymap_key_to_keycode(_MAIN, queued_key)); // 例. "p"
        return true;
      }
      register_code(queued); // 例. "0"
      return true;

    // Fn
    case KC_LNG1:
    case KC_LNG2:
      // 例. かな (press)
      //     ; (press)
      //     かな (release) <-- イマココ
      //     ; (release)
      if (is_mod_pressed_within(STICKY_TERM)) {
        // STICKY_TERMに満たなかった場合、言語切り替えののちそのレイヤーの文字を送出
        if (keycode == KC_LNG1) {
          kana_on();
          tap_code(keymap_key_to_keycode(_KANA, queued_key)); // 例. "せ"
        }
        if (keycode == KC_LNG2) {
          kana_off();
          tap_code(keymap_key_to_keycode(_MAIN, queued_key)); // 例. ";"
        }
        return true;
      }
      register_code(queued); // 例. "F11"
      return true;
  }
  return true;
}

/*
 * QMK callbacks
 */

void keyboard_post_init_user(void) {
#ifdef CONSOLE_ENABLE
  debug_enable=true;
#endif
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
  /*
  if (debug_digit) {
    rgb_matrix_set_color(10 + debug_digit, RGB_RED);
  }
  */
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
  bool pressed = record->event.pressed;

  if (!process_rgb_matrix_timeout(keycode, record)) return false;
  if (!process_mod_sequence(keycode, record)) return false;
  if (!process_sticky_term(keycode, record)) return false;

  // 英字入力
  if (!process_alpha(keycode, record)) return false;

  // かな入力
  if (!process_kana(keycode, record)) return false;
  
  // その他
  switch (keycode) {
    // Fn
    case KC_LNG1:
    case KC_LNG2:
      if (pressed) {
        if (!is_mod_seq_started()) {
          // 修飾キー(dual role)が最初に押された
          // 例. 英数 (press) <-- イマココ
          start_mod_sequence(keycode, record);
          layer_on(_FN);
        }
        return false;
      }
      if (!is_mod_seq_first(keycode, record)) return false;

      layer_off(_FN);
      if (get_mod_follower() == 0 && is_mod_pressed_within(TAPPING_TERM)) {
        // 時間内に単独でタップされた
        // 例. 英数 (press)
        //     英数 (release) <-- イマココ
        if (keycode == KC_LNG1) kana_on();
        if (keycode == KC_LNG2) kana_off();
      }
      end_mod_sequence();
      return false;

    case RGB_TOG:
      if (pressed) toggle_darkmode();
      return false;
  }

  return true;
}

void matrix_scan_user(void) {
  update_rgb_matrix_timeout();
}

