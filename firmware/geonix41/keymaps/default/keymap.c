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
#include "../../../lib/rdr_lib/rdr_common.h"

/* かな (LAYER_KANA) と英数 (LAYER_ALPHA) がベースレイヤーで、MY_LCTL のタップで
 * 交互に切り替わる。英数で透過のキーはかなへ落ちる。
 *
 * かなの Shift 面 (LAYER_KANA_SHIFT) は、こ / み のホールド中だけ有効になる
 * (issue #82)。透過のキーは、Shift を打ち消して下のかなを出す。最下段は同じ位置の
 * かなのキーと同じホールドの役割を持ち、タップで出るものだけが変わる。
 *
 * MD_USB / MD_BLE1-3 / MD_24G は rdr_lib の無線モード切替キー (geonix41 専用)。
 *
 * 設定 (LAYER_CONF) は左右のFnを両方ホールドしている間だけ有効になる (issue #62)。
 * 配置は全機種で共通。Fn の位置は透過のままにしておくこと
 * (windmill.c の set_fn 参照)。 */
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

  [LAYER_KANA] = LAYOUT_ortho_4x12(
    KC_ESC,  KN_NU,        KN_FU,        KN_A,        KN_U,          KN_E,           KN_O,           KN_YA,         KN_YU,          KN_YO,      KN_WA,   KC_ENT,
    KC_TAB,  KN_TA,        KN_TE,        KN_I,        KN_SU,         KN_KA,          KN_N,           KN_NA,         KN_NI,          KN_RA,      KN_SE,   KN_DAKU,
    KC_BSPC, KN_CHI,       KN_TO,        KN_SHI,      KN_HA,         KN_KI,          KN_KU,          KN_MA,         KN_NO,          KN_RI,      KN_RE,   KN_KE,
    MY_LCTL, KB_TSU,       KB_SA,        KB_SO,       KB_HI,         KB_KO,          KB_MI,          KB_MO,         KB_NE,          KN_RU,      KN_ME,   KN_RO
  ),

  [LAYER_KANA_SHIFT] = LAYOUT_ortho_4x12(
    _______, _______, _______, KN_XA,   KN_XU,   KN_XE,   KN_XO,   KN_XYA,  KN_XYU,    KN_XYO,    KN_WO,       _______,
    _______, _______, KN_HE,   KN_XI,   KN_MU,   _______, _______, KN_HO,   _______,   KN_LKAKKO, KN_RKAKKO,   KN_HANDAKU,
    _______, KN_XTSU, _______, _______, _______, _______, _______, _______, KN_TOUTEN, KN_KUTEN,  KN_NAKAGURO, KN_CHOON,
    _______, KB_XTSU, _______, _______, _______, KB_SPC,  KB_SPC,  KB_QUES, KB_TOUTEN, KN_KUTEN,  KN_NAKAGURO, _______
  ),

  [LAYER_ALPHA] = LAYOUT_ortho_4x12(
    _______, KC_Q,    KC_W,    KC_E,         KC_R,                  KC_T,           KC_Y,           KC_U,                  KC_I,         KC_O,   KC_P,    _______,
    _______, KC_A,    KC_S,    KC_D,         KC_F,                  KC_G,           KC_H,           KC_J,                  KC_K,         KC_L,   KC_SCLN, KC_QUOT,
    _______, KC_Z,    KC_X,    KC_C,         KC_V,                  KC_B,           KC_N,           KC_M,                  KC_COMM,      KC_DOT, KC_UP,   KC_RGHT,
    _______, KC_LGUI, KC_LALT, MO(LAYER_FN), LT(LAYER_SYM,KC_BSLS), LSFT_T(KC_SPC), LSFT_T(KC_SPC), LT(LAYER_SYM,KC_SLSH), MO(LAYER_FN), KC_APP, KC_LEFT, KC_DOWN
  ),

  [LAYER_SYM] = LAYOUT_ortho_4x12(
    _______, SY_1,         SY_2,         SY_3,        SY_4,          SY_5,           SY_6,           SY_7,          SY_8,           SY_9,       SY_0,    _______,
    _______, SY_EXLM,      SY_AT,        SY_HASH,     SY_DLR,        SY_PERC,        SY_CIRC,        SY_AMPR,       SY_ASTR,        SY_LPRN,    SY_RPRN, SY_GRV,
    _______, SY_EQL,       SY_PLUS,      SY_MINS,     SY_UNDS,       SY_LBRC,        SY_RBRC,        SY_TILD,       SY_LCBR,        SY_RCBR,    KC_UP,   KC_RGHT,
    _______, _______,      _______,      _______,     _______,       SY_PIPE,        SY_QUES,        _______,       _______,        _______,    KC_LEFT, KC_DOWN
  ),

  [LAYER_FN] = LAYOUT_ortho_4x12(
    _______, KC_F1,        KC_F2,        KC_F3,       KC_F4,         KC_F5,          KC_F6,          KC_F7,         KC_F8,          KC_F9,   KC_F10,  MY_DARK,
    KC_CAPS, KC_F11,       KC_F12,       KC_NO,       KC_NO,         KC_NO,          KC_NO,          KC_NO,         KC_NO,          KC_NO,   KC_NO,   KC_NO,
    KC_DEL,  KC_PSCR,      KC_NO,        KC_NO,       KC_NO,         KC_BRID,        KC_BRIU,        KC_MUTE,       KC_VOLD,        KC_VOLU, KC_UP,   KC_RGHT,
    MY_IME,  _______,      _______,      _______,     _______,       _______,        _______,        _______,       _______,        _______, KC_LEFT, KC_DOWN
  ),

  [LAYER_CONF] = LAYOUT_ortho_4x12(
    MD_USB,  MD_BLE1,      MD_BLE2,      MD_BLE3,     MD_24G,        KC_NO,          KC_NO,          KC_NO,         KC_NO,          KC_NO,   KC_NO,   QK_BOOT,
    KC_NO,   MY_WIN,       MY_ANDR,      KC_NO,       KC_NO,         KC_NO,          KC_NO,          KC_NO,         KC_NO,          KC_NO,   KC_NO,   KC_NO,
    KC_NO,   MY_JIS,       MY_US,        KC_NO,       KC_NO,         KC_NO,          KC_NO,          KC_NO,         KC_NO,          KC_NO,   KC_NO,   KC_NO,
    KC_NO,   KC_NO,        KC_NO,        _______,     KC_NO,         KC_NO,          KC_NO,          KC_NO,         _______,        KC_NO,   KC_NO,   KC_NO
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
  [CL_CONFIG]  = {0x00, 0x33, 0x55,  0x00, 0x03, 0x05},
  [CL_BASE]    = {0x07, 0x07, 0x05,  0x00, 0x00, 0x00},
  [CL_SPECIAL] = {0x1c, 0x11, 0x00,  0x04, 0x03, 0x00},
  [CL_SYMBOL]  = {0x11, 0x22, 0x22,  0x02, 0x04, 0x03},
  [CL_NUMBER]  = {0x66, 0x66, 0x44,  0x06, 0x06, 0x04},
  [CL_BRACKET] = {0x22, 0x33, 0x00,  0x02, 0x03, 0x00},
  [CL_FUNC]    = {0x66, 0x66, 0x22,  0x06, 0x06, 0x02},
  [CL_MEDIA]   = {0x00, 0x33, 0x55,  0x00, 0x03, 0x05},
};

/* dual-role キーはタップ側のキーコードに展開されてから渡される。
 * 数字・記号・かなのキーと、かなレイヤー最下段のキーは独自キーコード
 * (SY_* / KN_* / KB_*) のまま渡される。 */
uint8_t windmill_process_keycolor_user(uint8_t layer, uint16_t keycode) {
  /* かなレイヤーと Shift 面は、文字が打てるキーを全て CL_BASE にする。最下段 (KB_*) も
   * タップで文字を出すので同じ色にする。 */
  if (layer == LAYER_KANA || layer == LAYER_KANA_SHIFT) {
    switch (keycode) {
      case KC_ENT ... KC_TAB: // Enter, Esc, BSpc, Tab
      case MY_LCTL:           // 英数/かな
        return CL_SPECIAL;
    }
    return CL_BASE;
  }

  switch (keycode) {
    case MY_WIN: case MY_ANDR: case MY_JIS: case MY_US: case MY_DARK: case QK_BOOT:
    case MD_USB: case MD_BLE1: case MD_BLE2: case MD_BLE3: case MD_24G:
      return CL_CONFIG;
    case KC_ENT ... KC_TAB: case KC_CAPS: case KC_DEL: case KC_RIGHT ... KC_UP:
    case KC_APP: case KC_INT1 ... KC_LNG2: case KC_LCTL ... KC_RGUI:
    case MY_LCTL: case MY_IME: case QK_MOMENTARY ... QK_MOMENTARY_MAX:
      return CL_SPECIAL;
    case KC_BSLS: case KC_SLSH: // LT(LAYER_SYM,KC_BSLS) / LT(LAYER_SYM,KC_SLSH) のタップ側
    case KC_SCLN: case KC_QUOT: case KC_COMM: case KC_DOT:
    case SY_EXLM: case SY_AT: case SY_HASH: case SY_DLR: case SY_PERC: case SY_CIRC:
    case SY_AMPR: case SY_ASTR: case SY_GRV: case SY_EQL: case SY_PLUS: case SY_MINS:
    case SY_UNDS: case SY_TILD: case SY_PIPE: case SY_QUES:
      return CL_SYMBOL;
    case SY_1 ... SY_0:
      return CL_NUMBER;
    case SY_LPRN: case SY_RPRN: case SY_LBRC: case SY_RBRC: case SY_LCBR: case SY_RCBR:
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
