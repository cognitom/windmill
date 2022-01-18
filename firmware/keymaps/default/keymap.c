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
    _______, KA_XTSU, _______, _______, KA_PIPE, KA_SPC,  KA_SPC,  KA_QUES, KA_TEN,  KA_MARU, KA_NAKA, KA_CHOU
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

// 一時的に修飾キーを外した状態で、tap_codeする
void tap_code_wo_mod(uint16_t keycode, uint8_t mod_mask) {
  uint8_t mod_state = get_mods();
  del_mods(mod_mask);
  tap_code(keycode);
  set_mods(mod_state);
}

/*
 * QMK callbacks
 */

void keyboard_post_init_user(void) {
  init_windmill_layers(_ALPHA, _KANA, _KANA_SHIFTED, _SYM, _FN);
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
  bool pressed = record->event.pressed;
  bool ctrled = (get_mods() & MOD_MASK_CTRL);
  
  // その他
  switch (keycode) {
    // Shift
    case KC_SPC:
      return windmill_modtap(keycode, record, MOD_MASK_SHIFT);
    case KA_KO:
    case KA_MI:
      return windmill_modlayertap(keycode, record, MOD_MASK_SHIFT, _KANA_SHIFTED);
    // Sym
    case KC_BSLS:
    case KC_SLSH:
    case KA_HI:
    case KA_MO:
      return windmill_layertap(keycode, record, _SYM);
    // Fn
    case KC_LNG1:
    case KC_LNG2:
    case KA_SO:
    case KA_NE:
      return windmill_layertap(keycode, record, _FN);
    // Alt
    case KA_SA:
      return windmill_modtap(keycode, record, MOD_MASK_ALT);
    // GUI
    case KA_TSU:
      return windmill_modtap(keycode, record, MOD_MASK_GUI);
    // その他
    case KC_LCTL:
      if (is_kana()) {
        // CTRLキーを押した場合かなレイヤーをオフ
        if (pressed) layer_off(_KANA);
        else layer_on(_KANA);
      }
      break;
    case KC_ESC:
      if (is_kana() && pressed) {
        // ESCでかな入力をオフに ※ただし、Ctrl押下中はプレーンなESCを送出 
        if (ctrled) {
          tap_code_wo_mod(keycode, MOD_MASK_CTRL);
          return false;
        }
        kana_off();
        return false;
      }
      break;
    case KC_UP:
    case KC_DOWN:
    case KC_LEFT:
    case KC_RIGHT:
      if (is_kana() && ctrled && pressed) {
        tap_code_wo_mod(keycode, MOD_MASK_CTRL);
        return false;
      }
      break;
  }

  return true;
}
