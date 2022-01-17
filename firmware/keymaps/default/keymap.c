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
    _ALPHA,
    _KANA,
    _KANA_SHIFTED,
    _SYM,
    _FN,
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

  [_ALPHA] = LAYOUT_ortho_4x12(
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

  [_KANA_SHIFTED] = LAYOUT_ortho_4x12(
    _______, _______, _______, KA_XA,   KA_XU,   KA_XE,   KA_XO,   KA_XYA,  KA_XYU,  KA_XYO,  KA_WO,   _______,
    _______, _______, KA_HE,   KA_XI,   KA_MU,   _______, _______, KA_HO,   _______, KA_LKAK, KA_RKAK, KA_HAN,
    _______, KA_XTSU, _______, _______, _______, _______, _______, _______, KA_TEN,  KA_MARU, KA_NAKA, KA_CHOU,
    _______, KA_XTSU, _______, _______, _______, KA_SPC,  KA_SPC,  _______, KA_TEN,  KA_MARU, KA_NAKA, KA_CHOU
  ),


  [_SYM] = LAYOUT_ortho_4x12(
    _______, KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    _______,
    _______, KC_EXLM, KC_AT,   KC_HASH, KC_DLR,  KC_PERC, KC_CIRC, KC_AMPR, KC_ASTR, KC_LPRN, KC_RPRN, KC_GRV,
    _______, KC_EQL,  KC_PLUS, KC_MINS, KC_UNDS, KC_LBRC, KC_RBRC, KC_TILD, KC_LCBR, KC_RCBR, _______, _______,
    _______, _______, _______, _______, _______, KC_PIPE, KC_QUES, _______, _______, _______, _______, _______
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


/*
 * Utility methods
 */

// 一時的に修飾キーを外した状態で、tap_codeする
void tap_code_wo_mod(uint16_t keycode, uint8_t mod_mask) {
  uint8_t mod_state = get_mods();
  del_mods(mod_mask);
  tap_code(keycode);
  set_mods(mod_state);
}

// かな入力時のみ必要な処理
bool process_kana(uint16_t keycode, keyrecord_t *record) {
  uint8_t mod_state = get_mods();
  bool ctrled = (mod_state & MOD_MASK_CTRL);
  bool pressed = record->event.pressed;

  if (!is_kana()) return true;

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
          layer_on(_KANA_SHIFTED);
          return false;
        }
        return process_kana_keycode(keycode, record);
      }
      if (!is_mod_seq_first(keycode, record)) return false;

      layer_off(_KANA_SHIFTED);
      unregister_mods(MOD_MASK_SHIFT);
      if (get_mod_follower() == 0 && is_mod_pressed_within(TAPPING_TERM)) {
        // 時間内に単独でタップされた
        // 例. み(press)
        //     み(release) <-- イマココ
        (void)process_kana_keycode(keycode, record);
      }
      stop_mod_sequence();
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
  
  if (pressed) {
    if (!process_kana_keycode(keycode, record)) return false;
  }

  return true;
}

// 英字入力時のみ必要な処理
bool process_alpha(uint16_t keycode, keyrecord_t *record) {
  bool pressed = record->event.pressed;

  if (is_kana()) return true;
  
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
      stop_mod_sequence();
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
          return false;
        }
        return true;
      }
      if (!is_mod_seq_first(keycode, record)) return true;

      layer_off(_SYM);
      if (get_mod_follower() == 0 && is_mod_pressed_within(TAPPING_TERM)) {
        // 時間内に単独でタップされた
        // 例. / (press)
        //     / (release) <-- イマココ
        tap_code(keycode);
      }
      stop_mod_sequence();
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
        if (process_kana_keycode(queued_keycode, record)) tap_code16(queued_keycode);
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
    if (process_kana_keycode(keycode, record)) tap_code16(keycode);
    return false;
  }

  // ダブルロールキーがリリースされた場合
  switch (keycode) {
    // Shift (かな配列の場合)
    case KA_KO:
    case KA_MI:
      // 例. み(press)
      //     わ(press)
      //     み(release) <-- イマココ
      //     わ(release)
      if (is_mod_pressed_within(STICKY_TERM)) {
        unregister_mods(MOD_MASK_SHIFT);
        (void)process_kana_keycode(keycode, record); // 例.「み」
        uint16_t translated = keymap_key_to_keycode(_KANA, queued_key);
        (void)process_kana_keycode(translated, record); // 例.「わ」
        return true;
      }
      (void)process_kana_keycode(queued, record); // 例.「を」
      return true;

    // Shift (英字配列の場合)
    case KC_SPC:
      if (is_mod_pressed_within(STICKY_TERM)) {
        unregister_mods(MOD_MASK_SHIFT);
        tap_code(keycode);
        tap_code(queued);
        return true;
      }
      register_code(queued);
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
        tap_code16(keymap_key_to_keycode(_ALPHA, queued_key)); // 例. "p"
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
          tap_code(keymap_key_to_keycode(_ALPHA, queued_key)); // 例. ";"
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
  init_windmill_layers(_ALPHA, _KANA, _KANA_SHIFTED, _SYM, _FN);
}

void rgb_matrix_indicators_user(void) {
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
      stop_mod_sequence();
      return false;

    case RGB_TOG:
      if (pressed) toggle_darkmode();
      return false;
  }

  return true;
}
