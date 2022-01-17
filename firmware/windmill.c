/* Copyright 2020 Boardsource
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

#include "windmill.h"

#ifdef CONSOLE_ENABLE
  #include "print.h"
#endif

#ifdef RGB_MATRIX_ENABLE
led_config_t g_led_config = { {
    { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21},
    { 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45},
    { 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57}
}, {
    {220, 17}, {172, 17}, {112, 17}, { 50, 17}, {  4, 17}, {  4, 56}, { 50, 56}, {112, 56}, {172, 56}, {220, 56},
    {  0,  0}, { 20,  0}, { 40,  0}, { 61,  0}, { 81,  0}, {101,  0}, {122,  0}, {142,  0}, {162,  0}, {183,  0}, {203,  0}, {224,  0},
    {  0, 21}, { 20, 21}, { 40, 21}, { 61, 21}, { 81, 21}, {101, 21}, {122, 21}, {142, 21}, {162, 21}, {183, 21}, {203, 21}, {224, 21},
    {  0, 42}, { 20, 42}, { 40, 42}, { 61, 42}, { 81, 42}, {101, 42}, {122, 42}, {142, 42}, {162, 42}, {183, 42}, {203, 42}, {224, 42},
    {  0, 64}, { 20, 64}, { 40, 64}, { 61, 64}, { 81, 64}, {101, 64}, {122, 64}, {142, 64}, {162, 64}, {183, 64}, {203, 64}, {224, 64}
}, {
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 1,
    1, 1, 1, 1, 1, 4, 4, 1, 1, 1, 1, 1
} };
#endif

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

static int _ALPHA;
static int _KANA;
static int _KANA_SHIFTED;
static int _SYM;
static int _FN;
void init_windmill_layers(int alpha_layer, int kana_layer, int kana_shifted_layer, int sym_layer, int fn_layer) {
  _ALPHA = alpha_layer;
  _KANA = kana_layer;
  _KANA_SHIFTED = kana_shifted_layer;
  _SYM = sym_layer;
  _FN = fn_layer;
}

/*
 * Mod Seq
 */

static uint16_t first_mod_keycode = 0;
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
void stop_mod_sequence(void) {
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

/*
 * Kana
 */

static bool _is_kana = false;
bool is_kana(void) {
  return _is_kana;
}

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
  _is_kana = true;
  layer_on(_KANA);
  send_kana();
}

void kana_off(void) {
  _is_kana = false;
  layer_off(_KANA);
  send_alpha();
}

uint16_t translate_kana_to_ascii(uint16_t keycode) {
  switch (keycode) {
    case KA_A:    return KC_3;    // あ
    case KA_I:    return KC_E;    // い
    case KA_U:    return KC_4;    // う
    case KA_E:    return KC_5;    // え
    case KA_O:    return KC_6;    // お
    case KA_KA:   return KC_T;    // か
    case KA_KI:   return KC_G;    // き
    case KA_KU:   return KC_H;    // く
    case KA_KE:   return KC_QUOT; // け
    case KA_KO:   return KC_B;    // こ
    case KA_SA:   return KC_X;    // さ
    case KA_SHI:  return KC_D;    // し
    case KA_SU:   return KC_R;    // す
    case KA_SE:   return KC_P;    // せ
    case KA_SO:   return KC_C;    // そ
    case KA_TA:   return KC_Q;    // た
    case KA_CHI:  return KC_A;    // ち
    case KA_TSU:  return KC_Z;    // つ
    case KA_TE:   return KC_W;    // て
    case KA_TO:   return KC_S;    // と
    case KA_NA:   return KC_U;    // な
    case KA_NI:   return KC_I;    // に
    case KA_NU:   return KC_1;    // ぬ
    case KA_NE:   return KC_COMM; // ね
    case KA_NO:   return KC_K;    // の
    case KA_HA:   return KC_F;    // は
    case KA_HI:   return KC_V;    // ひ
    case KA_FU:   return KC_2;    // ふ
    case KA_HE:   return KC_EQL;  // へ
    case KA_HO:   return KC_MINS; // ほ
    case KA_MA:   return KC_J;    // ま
    case KA_MI:   return KC_N;    // み
    case KA_MU:   return KC_BSLS; // む
    case KA_ME:   return KC_SLSH; // め
    case KA_MO:   return KC_M;    // も
    case KA_YA:   return KC_7;    // や
    case KA_YU:   return KC_8;    // ゆ
    case KA_YO:   return KC_9;    // よ
    case KA_RA:   return KC_O;    // ら
    case KA_RI:   return KC_L;    // り
    case KA_RU:   return KC_DOT;  // る
    case KA_RE:   return KC_SCLN; // れ
    case KA_RO:   return KC_GRV;  // ろ
    case KA_WA:   return KC_0;    // わ
    case KA_WO:   return KC_RPRN; // を
    case KA_N:    return KC_Y;    // ん
    case KA_XA:   return KC_HASH; // ぁ
    case KA_XI:   return S(KC_E); // ぃ
    case KA_XU:   return KC_DLR;  // ぅ
    case KA_XE:   return KC_PERC; // ぇ
    case KA_XO:   return KC_CIRC; // ぉ
    case KA_XTSU: return S(KC_Z); // っ
    case KA_XYA:  return KC_AMPR; // ゃ
    case KA_XYU:  return KC_ASTR; // ゅ
    case KA_XYO:  return KC_LPRN; // ょ
    case KA_LKAK: return KC_LCBR; // 「
    case KA_RKAK: return KC_RCBR; // 」
    case KA_DAKU: return KC_LBRC; // ゛
    case KA_HAN:  return KC_RBRC; // ゜
    case KA_TEN:  return KC_LT;   // 、
    case KA_MARU: return KC_GT;   // 。
    case KA_NAKA: return KC_QUES; // ・
    case KA_CHOU: return KC_UNDS; // ー
    case KA_SPC:  return KC_SPC;  // Space
  }
  return KC_NO;
}

bool process_kana_keycode(uint16_t keycode, keyrecord_t *record) {
  uint8_t mod_state = get_mods();
  bool shifted = (mod_state & MOD_MASK_SHIFT);
  uint16_t translated = translate_kana_to_ascii(keycode);
  if (translated == KC_NO) return true;
  
  if (shifted) unregister_mods(MOD_MASK_SHIFT);
  tap_code16(translated);
  if (shifted) register_mods(MOD_MASK_SHIFT);
  return false;
}

/*
 * RGB Matrix Timeout
 */

#define RGBMATRIX_TIMEOUT 3 // in minutes
static uint16_t idle_timer = 0;
static uint8_t halfmin_counter = 0;
static bool led_on = true;
static bool led_darkmode = false;

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



void keyboard_post_init_kb(void) {
#ifdef CONSOLE_ENABLE
  debug_enable = true;
#endif
  rgb_matrix_mode(RGB_MATRIX_NONE);
  rgb_matrix_sethsv(HSV_BASE);

  keyboard_post_init_user();
}

void rgb_matrix_indicators_kb(void) {
  if (led_on == false) return;

  rgb_matrix_indicators_user();
}

void matrix_scan_kb(void) {
  update_rgb_matrix_timeout();

  matrix_scan_user();
}
