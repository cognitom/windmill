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
  _ALPHA_SHIFTED,
  _NUMPAD,
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
    KC_LNG2, KC_LGUI, KC_LALT, KC_NUM,  KC_BSLS, KC_SPC,  KC_SPC,  KC_SLSH, KC_LNG1, KC_APP,  KC_LEFT, KC_DOWN
  ),

  [_ALPHA_SHIFTED] = LAYOUT_ortho_4x12(
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, KC_COLN, KC_DQUO,
    _______, _______, _______, _______, _______, _______, _______, _______, KC_LT,   KC_GT,   _______, _______,
    _______, _______, _______, _______, KC_PIPE, _______, _______, KC_QUES, _______, _______, _______, _______
  ),

  [_NUMPAD] = LAYOUT_ortho_4x12(
    _______, _______, _______, _______, _______, _______, _______, KC_7,    KC_8,    KC_9,    _______, _______,
    _______, _______, _______, _______, _______, _______, _______, KC_4,    KC_5,    KC_6,    _______, _______,
    _______, _______, _______, _______, _______, _______, _______, KC_1,    KC_2,    KC_3,    _______, _______,
    _______, _______, _______, _______, _______, _______, _______, KC_0,    _______, _______, _______, _______
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
    _______, KA_XTSU, _______, _______, _______, KA_BSLS, KA_SLSH, _______, KA_TEN,  KA_MARU, KA_NAKA, KA_CHOU,
    _______, KA_XTSU, _______, _______, KA_PIPE, KA_SPC,  KA_SPC,  KA_QUES, _______, _______, _______, _______
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
 * Color settings
 */

enum keycolors {
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
  //             (R,    G,    B   ) (H,    S,    V   )
  [CL_BASE]    = {0x07, 0x07, 0x05,  0x00, 0x00, 0x00},
  [CL_SPECIAL] = {0x1c, 0x11, 0x00,  0x04, 0x03, 0x00},
  [CL_SYMBOL]  = {0x11, 0x22, 0x22,  0x02, 0x04, 0x03},
  [CL_NUMBER]  = {0x66, 0x66, 0x44,  0x06, 0x06, 0x04},
  [CL_BRACKET] = {0x22, 0x33, 0x00,  0x02, 0x03, 0x00},
  [CL_FUNC]    = {0x66, 0x66, 0x44,  0x06, 0x06, 0x04},
  [CL_MEDIA]   = {0x00, 0x33, 0x55,  0x00, 0x03, 0x05},
};

uint8_t windmill_process_keycolor_user(uint16_t keycode) {
  switch (keycode) {
    case KC_ENT ... KC_TAB: case KC_DEL: case KC_RGHT ... KC_NUM:
    case KC_APP: case KC_INT1 ... KC_LNG2: case KC_LCTL ... KC_RGUI:
    case RGB_TOG: case RESET:
      return CL_SPECIAL;
    case KC_MINS ... KC_EQL: case KC_BSLS ... KC_SLSH:
    case KC_EXLM ... KC_ASTR: case KC_UNDS ... KC_PLUS:
    case KC_PIPE ... KC_TILD: case KC_QUES:
    case KA_DAKU ... KA_CHOU: case KA_QUES:
      return CL_SYMBOL;
    case KC_1 ... KC_0:
      return CL_NUMBER;
    case KC_LT ... KC_GT: case KC_LPRN ... KC_RPRN:
    case KC_LBRC ... KC_RBRC: case KC_LCBR ... KC_RCBR:
    case KA_LKAK ... KA_RKAK:
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
  windmill_init_layers(_ALPHA, _NUMPAD, _KANA, _SYM);
  windmill_init_keycolors((uint8_t*)colorset);
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
  bool pressed = record->event.pressed;
  bool ctrled = (get_mods() & MOD_MASK_CTRL);
  
  switch (keycode) {
    // Shift
    case KC_SPC:
      return windmill_modlayertap(keycode, record, MOD_MASK_SHIFT, _ALPHA_SHIFTED);
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
    case KC_NUM:
    case KA_SO:
    case KA_NE:
      return windmill_layertap(keycode, record, _FN);
    // Alt
    case KA_SA:
      return windmill_modtap(keycode, record, MOD_MASK_ALT);
    // GUI
    case KA_TSU:
      return windmill_modtap(keycode, record, MOD_MASK_GUI);
    // Ctrl
    case KC_LNG2:
      return windmill_modtap(keycode, record, MOD_MASK_CTRL);
    // Others
    case KC_UP:
    case KC_DOWN:
    case KC_LEFT:
    case KC_RIGHT:
      if (is_kana() && ctrled && pressed) {
        unregister_mods(MOD_MASK_CTRL);
        tap_code(keycode);
        register_mods(MOD_MASK_CTRL);
        return false;
      }
      break;
  }

  return true;
}
