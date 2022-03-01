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

#include "windmill.h"

#ifdef CONSOLE_ENABLE
  #include "print.h"
#endif

typedef union {
  uint32_t raw;
  struct {
    bool is_kana: 1;
    bool led_darkmode: 1;
    uint8_t ime_type: 8;
    uint8_t lang_type: 8;
  };
} keyboard_config_t;
keyboard_config_t keyboard_config;

static int _ALPHA;
static int _NUMPAD;
static int _KANA;
static int _SYM;
void windmill_init_layers(int alpha_layer, int numpad_layer, int kana_layer, int sym_layer) {
  _ALPHA = alpha_layer;
  _NUMPAD = numpad_layer;
  _KANA = kana_layer;
  _SYM = sym_layer;
}

static uint8_t *colorsetPtr;
void windmill_init_keycolors(uint8_t* user_colorset) {  
  colorsetPtr = user_colorset;
}

/*
 * RGB Matrix
 */

#define RGBMATRIX_TIMEOUT 10 // in minutes
#define CL_TRANS 0xFF
#define LAYER_SIZE 8 // 最大設定可能なレイヤー数
#define RGB_STARTING_INDEX 10 // 最初の10個は底面のLEDなので、それを除外するための設定

#ifdef RGBLIGHT_ENABLE
// RGBLIGHT_LED_MAP の挙動がおかしい?ので、独自に
// 本来は、ymd40のconfig.hで吸収したいので、ここに書くのは一時的な措置
const uint8_t rgblight_remap_config[] = {
  11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
  23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12,
  35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24,
  50, 49, 48, 47, 46, 44, 42, 40, 39, 38, 37, 36,
  45, 43, 41
};
uint8_t rgblight_remap(uint8_t pos) {
  return rgblight_remap_config[pos];
}
#endif

static uint8_t cached_keycolormap[LAYER_SIZE][MATRIX_ROWS * MATRIX_COLS];
static uint8_t ime_indicators[5][2]; // IME_WIN ... IME_IOS
static uint8_t lang_indicators[5][2]; // JA_ROME ... JA_KANA
static uint16_t idle_timer = 0;
static uint8_t halfmin_counter = 0;
static bool led_initialized = false;
static bool led_on = true;
static bool led_darkmode = false;

void toggle_darkmode(void) {
  led_darkmode = !led_darkmode;

  keyboard_config.led_darkmode = led_darkmode;
  eeconfig_update_user(keyboard_config.raw);
}

void refresh_rgb_matrix_timeout(void) {
  if (led_on == false) {
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
#ifdef RGB_MATRIX_ENABLE
    rgb_matrix_sethsv(HSV_OFF);
    rgb_matrix_set_color_all(RGB_OFF);
#endif
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

void cache_keycolors(void) {
  for (int layer = 0; layer < LAYER_SIZE; ++layer) {
    for (int row = 0; row < MATRIX_ROWS; ++row) {
      for (int col = 0; col < MATRIX_COLS; ++col) {
        uint16_t keycode = keymap_key_to_keycode(layer, (keypos_t){.row = row, .col = col});
        cached_keycolormap[layer][row * 12 + col] = (keycode == _______) ? CL_TRANS : windmill_process_keycolor_user(keycode);

        switch (keycode) {
          case IME_WIN ... IME_IOS:
            ime_indicators[keycode - IME_WIN][0] = layer;
            ime_indicators[keycode - IME_WIN][1] = row * MATRIX_COLS + col;
            break;
          case JA_ROME ... JA_KANA:
            lang_indicators[keycode - JA_ROME][0] = layer;
            lang_indicators[keycode - JA_ROME][1] = row * MATRIX_COLS + col;
            break;
        }
      }
    }
  }
}

/*
 * Kana
 */

enum ime_types {
  _IME_WINDOWS,
  _IME_ANDROID,
  _IME_CHROMEOS,
  _IME_MAC, // not immplemented yet
  _IME_IOS, // not immplemented yet
};
enum lang_types {
  _LANG_JA_ROME,
  _LANG_JA_ROKA,
  _LANG_JA_KANA,
};
static bool _is_kana = false;
static uint8_t _ime_type = _IME_WINDOWS;
static uint8_t _lang_type = _LANG_JA_ROME;

bool is_kana(void) {
  return _is_kana;
}
uint8_t get_ime_type(void) {
  return _ime_type;
}
void set_ime_type(uint8_t ime_type) {
  _ime_type = ime_type;

  keyboard_config.ime_type = ime_type;
  eeconfig_update_user(keyboard_config.raw);
}
uint8_t get_lang_type(void) {
  return _lang_type;
}
void set_lang_type(uint8_t lang_type) {
  _lang_type = lang_type;

  keyboard_config.lang_type = lang_type;
  eeconfig_update_user(keyboard_config.raw);
}

void send_kana(void) {
  uint8_t ime_type = get_ime_type();
  clear_keyboard();
  switch (ime_type) {
    case _IME_WINDOWS:
    case _IME_MAC:
      tap_code(KC_LNG1);
      break;
    case _IME_ANDROID:
      tap_code16(S(KC_SPC));
      break;
    case _IME_CHROMEOS:
      tap_code16(C(KC_SPC));
      break;
  }
}

void send_alpha(void) {
  uint8_t ime_type = get_ime_type();
  clear_keyboard();
  switch (ime_type) {
    case _IME_WINDOWS:
    case _IME_MAC:
      tap_code(KC_LNG2);
      break;
    case _IME_ANDROID:
      tap_code16(S(KC_SPC));
      break;
    case _IME_CHROMEOS:
      tap_code16(C(KC_SPC));
      break;
  }
}

void kana_on(void) {
  layer_move(_ALPHA);
  if (_lang_type != _LANG_JA_ROME) {
    layer_on(_KANA);
    _is_kana = true;
    keyboard_config.is_kana = true;
    eeconfig_update_user(keyboard_config.raw);
  }
  send_kana();
}

void kana_off(void) {
  layer_move(_ALPHA);
  if (_lang_type != _LANG_JA_ROME) {
    _is_kana = false;
    keyboard_config.is_kana = false;
    eeconfig_update_user(keyboard_config.raw);
  }
  send_alpha();
}

// 独自キーコードからの変換
// OS:Windows 11, Layout:English(US), IME:Microsoft IME
uint16_t translate_kana_to_ascii_default(uint16_t keycode) {
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

// 独自キーコードからの変換
// OS:Android, Layout:English(US), IME:Wnn Keyboard Lab
uint16_t translate_kana_to_ascii_android(uint16_t keycode) {
  switch (keycode) {
    case KA_KE:   return KC_ASTR; // け
    case KA_RO:   return KC_BSLS; // ろ
    case KA_XO:   return KC_AMPR; // ぉ
    case KA_XYA:  return KC_QUOT; // ゃ
    case KA_XYU:  return KC_LPRN; // ゅ
    case KA_XYO:  return KC_NO;   // ょ ※打てない
    case KA_DAKU: return KC_GRV;  // ゛
    case KA_HAN:  return KC_LBRC; // ゜
    case KA_CHOU: return KC_PIPE; // ー
  }
  return translate_kana_to_ascii_default(keycode);
}

uint16_t translate_kana_to_ascii(uint16_t keycode) {
  uint8_t ime_type = get_ime_type();
  switch (ime_type) {
    case _IME_ANDROID: return translate_kana_to_ascii_android(keycode);
  }
  return translate_kana_to_ascii_default(keycode);
}

uint16_t translate_special_to_ascii(uint16_t keycode) {
  switch (keycode) {
    case KA_QUES: return KC_QUES; // ?
    case KA_PIPE: return KC_PIPE; // |
    case KA_SLSH: return KC_SLSH; // /
    case KA_BSLS: return KC_BSLS; // ￥
  }
  return KC_NO;
}

/*
 * process_keycord_*
 */

bool process_keycode_kana(uint16_t keycode) {
  uint16_t translated = translate_kana_to_ascii(keycode);
  if (translated == KC_NO) return true;

  bool shifted = (get_mods() & MOD_MASK_SHIFT);
  if (shifted) unregister_mods(MOD_MASK_SHIFT);
  tap_code16(translated);
  if (shifted) register_mods(MOD_MASK_SHIFT);
  return false;
}

bool process_keycode_fn(uint16_t keycode) {
  switch (keycode) {
    case RGB_TOG:
      toggle_darkmode();
      break;
    case KC_LNG1:
      kana_on();
      break;
    case KC_LNG2:
      kana_off();
      break;
    case KC_NUM:
      layer_invert(_NUMPAD);
      break;
    case IME_WIN ... IME_IOS:
      set_ime_type(_IME_WINDOWS + keycode - IME_WIN);
      layer_state_set_kb(layer_state);
      break;
    case JA_ROME ... JA_KANA:
      set_lang_type(_LANG_JA_ROME + keycode - JA_ROME);
      layer_state_set_kb(layer_state);
      break;
    default: 
      return true;
  }
  return false;
}

bool process_keycode_sym(uint16_t keycode) {
  if (is_kana()) {
    if (layer_state_is(_SYM)) {
      send_alpha();
      tap_code16(keycode);
      send_kana();
      return false;
    }
    switch (keycode) {
      case KA_QUES:
      case KA_PIPE:
      case KA_SLSH:
      case KA_BSLS:
        send_alpha();
        tap_code16(translate_special_to_ascii(keycode));
        send_kana();
        return false;
    }
  }

  return true;
}

/*
 * process_record_*
 */

bool process_record_kana(uint16_t keycode, keyrecord_t *record) {
  if (record->event.pressed) {
    if (!process_keycode_kana(keycode)) return false;
  }
  return true;
}

bool process_record_fn(uint16_t keycode, keyrecord_t *record) {
  if (record->event.pressed) {
    if (!process_keycode_fn(keycode)) return false;
  }
  return true;
}

bool process_record_sym(uint16_t keycode, keyrecord_t *record) {
  if (record->event.pressed) {
    if (!process_keycode_sym(keycode)) return false;
  }
  return true;
}

/*
 * Mod Seq
 */

#define MOD_POOL_MAX 8
#define MOD_MASK_NONE 0x00

struct mod_sequence {
  bool is_active;
  keypos_t key;
  uint8_t mod_mask;
  uint8_t layer;
  uint16_t timer;
  int followers;
};

static int mod_sequence_count = 0;
static bool weakmod_alt = false;
static bool weakmod_gui = false;
static uint8_t last_mod_state = 0;
static uint16_t last_layer_state = 0;
static struct mod_sequence mod_pool[MOD_POOL_MAX];
void keep_clean(void) {
  mod_sequence_count = 0;
  for (int i = 0; i < MOD_POOL_MAX; ++i)
    if (mod_pool[i].is_active) {
      if (matrix_is_on(mod_pool[i].key.row, mod_pool[i].key.col)) {
        mod_pool[i].is_active = false;
      } else {
        ++mod_sequence_count;
      }
    }
}
void update_mod_state(void) {
  uint8_t next_mod_state = 0;
  uint8_t mod_to_add = 0;
  uint8_t mod_to_del = 0;
  uint8_t target_mod_state = 0;
  uint16_t next_layer_state = 0;
  uint16_t layer_to_add = 0;
  uint16_t layer_to_del = 0;
  uint16_t target_layer_state = 0;
  bool should_remove_alt = false;
  bool should_remove_gui = false;

  for (int i = 0; i < MOD_POOL_MAX; ++i)
    if (mod_pool[i].is_active) {
      next_mod_state = next_mod_state | mod_pool[i].mod_mask;
      next_layer_state = next_layer_state | ((layer_state_t)1 << mod_pool[i].layer);
    }

  // 前回の状況との差分計算 (mod_seqを使っているもののみ)
  mod_to_del = last_mod_state & (last_mod_state ^ next_mod_state);
  mod_to_add = next_mod_state & (last_mod_state ^ next_mod_state);
  layer_to_del = last_layer_state & (last_layer_state ^ next_layer_state);
  layer_to_add = next_layer_state & (last_layer_state ^ next_layer_state);
  
  should_remove_alt = mod_to_del & MOD_MASK_ALT;
  should_remove_gui = mod_to_del & MOD_MASK_GUI;

  // weakmodを除外
  mod_to_del = mod_to_del ^ (mod_to_del & (MOD_MASK_ALT | MOD_MASK_GUI));
  mod_to_add = mod_to_add ^ (mod_to_add & (MOD_MASK_ALT | MOD_MASK_GUI));

  // 現在の状況との差分計算 (mod_seq以外も含む)
  target_mod_state = get_mods() | mod_to_add;
  target_mod_state = target_mod_state & (target_mod_state ^ mod_to_del);
  target_layer_state = layer_state | layer_to_add;
  target_layer_state = target_layer_state & (target_layer_state ^ layer_to_del);

  // シフト以外の修飾が指定されている場合、_KANAレイヤーをオフにする
  if (is_kana()) {
    target_layer_state = target_layer_state | ((layer_state_t)1 << _KANA);
    if (next_mod_state && next_mod_state != MOD_MASK_SHIFT)
      target_layer_state = target_layer_state ^ ((layer_state_t)1 << _KANA);
  }

  // 適用
  set_mods(target_mod_state);
  layer_state_set(target_layer_state);
  weakmod_alt = next_mod_state & MOD_MASK_ALT;
  weakmod_gui = next_mod_state & MOD_MASK_GUI;
  if (should_remove_alt) unregister_mods(MOD_MASK_ALT);
  if (should_remove_gui) unregister_mods(MOD_MASK_GUI);

  last_mod_state = next_mod_state;
  last_layer_state = next_layer_state;
}
void add_mod_sequence(uint16_t keycode, keyrecord_t *record, uint8_t mod_mask, uint8_t layer_to_activate) {
  // 空いているところに追加
  for (int i = 0; i < MOD_POOL_MAX; ++i)
    if (!mod_pool[i].is_active) {
      mod_pool[i].is_active = true;
      mod_pool[i].key = record->event.key;
      mod_pool[i].mod_mask = mod_mask;
      mod_pool[i].layer = layer_to_activate;
      mod_pool[i].timer = timer_read();
      mod_pool[i].followers = 0;
      break;
    }
  ++mod_sequence_count;
  update_mod_state();
}
void del_mod_sequence(uint16_t keycode, keyrecord_t *record) {
  for (int i = 0; i < MOD_POOL_MAX; ++i)
    if (mod_pool[i].is_active && mod_pool[i].key.row == record->event.key.row && mod_pool[i].key.col == record->event.key.col) {
      mod_pool[i].is_active = false;
      break;
    }
  --mod_sequence_count;
  update_mod_state();
  keep_clean();
}
bool detect_single_tap(keyrecord_t *record) {
  for (int i = 0; i < MOD_POOL_MAX; ++i)
    if (mod_pool[i].is_active && mod_pool[i].key.row == record->event.key.row && mod_pool[i].key.col == record->event.key.col) {
      return !mod_pool[i].followers && timer_elapsed(mod_pool[i].timer) < THIRD_TAPPING_TERM;
    }
  return false;
}
bool detect_quick_tap(void) {
  if (mod_sequence_count != 1) return false;
  if (!mod_pool[0].is_active) return false;
  if (mod_pool[0].followers != 1) return false;
  return timer_elapsed(mod_pool[0].timer) < SECOND_TAPPING_TERM;
  return false;
}
bool process_mod_sequence(uint16_t keycode, keyrecord_t *record) {
  if (record->event.pressed && mod_sequence_count)
    for (int i = 0; i < MOD_POOL_MAX; ++i)
      if (mod_pool[i].is_active) mod_pool[i].followers += 1;
  return true;
}
bool process_weakmod_activation(uint16_t keycode) {
  if (weakmod_alt) register_mods(MOD_MASK_ALT);
  if (weakmod_gui) register_mods(MOD_MASK_GUI);
  return true;
}
bool process_weakmod(uint16_t keycode, keyrecord_t *record) {
  if (record->event.pressed) {
    if (!process_weakmod_activation(keycode)) return false;
  }
  return true;
}

bool windmill_modlayertap(uint16_t keycode, keyrecord_t *record, uint8_t mod_mask, uint8_t layer_to_activate) {
  if (record->event.pressed) {
    // 修飾キー(dual role)が押された
    // 例. 英数 (press) <-- イマココ
    add_mod_sequence(keycode, record, mod_mask, layer_to_activate);
    return false;
  }

  bool single_tap = detect_single_tap(record);
  del_mod_sequence(keycode, record);
  if (single_tap) {
    // 時間内に単独でタップされた
    // 例. 英数 (press)
    //     英数 (release) <-- イマココ
    windmill_tap_code(keycode);
  }
  return false;
}
bool windmill_layertap(uint16_t keycode, keyrecord_t *record, uint8_t layer_to_activate) {
  return windmill_modlayertap(keycode, record, MOD_MASK_NONE, layer_to_activate);
}
bool windmill_modtap(uint16_t keycode, keyrecord_t *record, uint8_t mod_mask) {
  return windmill_modlayertap(keycode, record, mod_mask, 0);
}

/*
 * 拡張版 tap_code
 */

void windmill_tap_code(uint16_t keycode) {
  if (!process_keycode_kana(keycode)) return;
  if (!process_keycode_fn(keycode)) return;
  if (!process_keycode_sym(keycode)) return;
  if (!process_weakmod_activation(keycode)) return;
  tap_code16(keycode);
}

/*
 * Quick Tap
 */

static uint16_t queued_keycode = 0;
static keypos_t queued_key = { .row = 0, .col = 0 };
bool process_quick_tap(uint16_t keycode, keyrecord_t *record) {
  bool pressed = record->event.pressed;

  // 修飾キーが押されたのち、SECOND_TAPPING_TERM時間内であればキーを送出せずqueued_keycodeに入れて待つ
  // 例. み(press)
  //     わ(press) <-- イマココ
  if (pressed) {
    if (detect_quick_tap()) {
      queued_keycode = keycode;
      queued_key = record->event.key;
      return false;
    }
    if (queued_keycode) {
      windmill_tap_code(queued_keycode);
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
    windmill_tap_code(keycode);
    return false;
  }

  // ダブルロールキーがリリースされた場合
  // 例. み(press)
  //     わ(press)
  //     み(release) <-- イマココ
  //     わ(release)
  if (detect_quick_tap()) {
    del_mod_sequence(keycode, record);
    windmill_tap_code(keycode); // 例.「み」
    windmill_tap_code(keymap_key_to_keycode(is_kana() ? _KANA : _ALPHA, queued_key)); // 例.「わ」
    return false;
  }
  windmill_tap_code(queued); // 例.「を」
  del_mod_sequence(keycode, record);
  return false;
}

/*
 * Permanent configs
 */
void load_keyboard_config(void) {
  keyboard_config.raw = eeconfig_read_user();
  if (keyboard_config.is_kana) {
    _is_kana = 1;
  }
  if (keyboard_config.led_darkmode) {
    led_darkmode = 1;
  }
  if (_IME_WINDOWS <= keyboard_config.ime_type && keyboard_config.ime_type <= _IME_IOS) {
    _ime_type = keyboard_config.ime_type;
  }
  if (_LANG_JA_ROME <= keyboard_config.lang_type && keyboard_config.lang_type <= _LANG_JA_KANA) {
    _lang_type = keyboard_config.lang_type;
  }
}

/*
 * QMK callbacks
 */

void keyboard_post_init_kb(void) {
#ifdef CONSOLE_ENABLE
  debug_enable = true;
#endif
  
  load_keyboard_config();
  keyboard_post_init_user();

#ifdef RGB_MATRIX_ENABLE
  rgb_matrix_mode(RGB_MATRIX_NONE);
  rgb_matrix_sethsv(HSV_OFF);
#elif defined(RGBLIGHT_ENABLE)
  rgblight_mode(RGBLIGHT_MODE_STATIC_LIGHT);
  rgblight_sethsv(HSV_OFF);
#endif

  cache_keycolors();
  led_initialized = true;

  layer_move(_ALPHA);
  if (is_kana()) {
    layer_on(_KANA);
  }
}

static uint8_t cached_keycolors[48];
layer_state_t layer_state_set_kb(layer_state_t state) {
  // IMEのモードを示すインジケーターの位置
  const uint8_t ime_type = get_ime_type();
  uint8_t ime_indicator[2] = { ime_indicators[ime_type][0], ime_indicators[ime_type][1] };

  // 言語モードを示すインジケーターの位置
  const uint8_t lang_type = get_lang_type();
  uint8_t lang_indicator[2] = { lang_indicators[lang_type][0], lang_indicators[lang_type][1] };

  // どのレイヤーが有効化されているか
  bool layer_states[LAYER_SIZE];
  for (int layer = 0; layer < LAYER_SIZE; ++layer) {
    layer_states[layer] = layer_state_cmp(state, layer);
  }

  for (int i = 0; i < MATRIX_ROWS * MATRIX_COLS; ++i) {
    for (int layer = LAYER_SIZE - 1; layer >= 0 ; --layer) {
      if (!layer_states[layer]) continue;
      uint8_t keycolor = cached_keycolormap[layer][i];
      if (keycolor == CL_TRANS) continue;
      // インジケーターの場合は色指定を上書き
      if (layer == ime_indicator[0] && i == ime_indicator[1]) keycolor = 0;
      if (layer == lang_indicator[0] && i == lang_indicator[1]) keycolor = 0;
      cached_keycolors[i] = keycolor;
      break;
    }
  }

#ifdef RGBLIGHT_ENABLE
  if (!led_darkmode) {
    for (int i = 0; i < MATRIX_ROWS * MATRIX_COLS; ++i)
      rgblight_setrgb_at(
        *(colorsetPtr + cached_keycolors[i] * 6 + 0),
        *(colorsetPtr + cached_keycolors[i] * 6 + 1),
        *(colorsetPtr + cached_keycolors[i] * 6 + 2),
        rgblight_remap(i)
      );
  } else {
    for (int i = 0; i < MATRIX_ROWS * MATRIX_COLS; ++i)
      rgblight_setrgb_at(
        *(colorsetPtr + cached_keycolors[i] * 6 + 3),
        *(colorsetPtr + cached_keycolors[i] * 6 + 4),
        *(colorsetPtr + cached_keycolors[i] * 6 + 5),
        rgblight_remap(i)
      );
  }
#endif

  return state;
}

#ifdef RGB_MATRIX_ENABLE
void rgb_matrix_indicators_kb(void) {
  if (!led_on || !led_initialized) return;

  if (!led_darkmode) {
    for (int i = 0; i < MATRIX_ROWS * MATRIX_COLS; ++i)
      rgb_matrix_set_color(
        RGB_STARTING_INDEX + i,
        *(colorsetPtr + cached_keycolors[i] * 6 + 0),
        *(colorsetPtr + cached_keycolors[i] * 6 + 1),
        *(colorsetPtr + cached_keycolors[i] * 6 + 2)
      );
    return;
  }

  for (int i = 0; i < MATRIX_ROWS * MATRIX_COLS; ++i)
    rgb_matrix_set_color(
      RGB_STARTING_INDEX + i,
      *(colorsetPtr + cached_keycolors[i] * 6 + 3),
      *(colorsetPtr + cached_keycolors[i] * 6 + 4),
      *(colorsetPtr + cached_keycolors[i] * 6 + 5)
    );
}
#endif

void matrix_scan_kb(void) {
  update_rgb_matrix_timeout();

  matrix_scan_user();
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
  if (!process_rgb_matrix_timeout(keycode, record)) return false;
  if (!process_mod_sequence(keycode, record)) return false;
  if (!process_quick_tap(keycode, record)) return false;
  
  // keymap.c
  if (!process_record_user(keycode, record)) return false;
  
  if (!process_record_kana(keycode, record)) return false;
  if (!process_record_fn(keycode, record)) return false;
  if (!process_record_sym(keycode, record)) return false;
  if (!process_weakmod(keycode, record)) return false;
  return true;
}
