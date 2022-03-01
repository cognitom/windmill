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

#pragma once

#include "quantum.h"

#if defined(KEYBOARD_windmill_technik)
  #include "technik.h"
#elif defined(KEYBOARD_windmill_ymd40)
  #include "ymd40.h"
#endif

#define KC_LNG1 KC_LANG1
#define KC_LNG2 KC_LANG2
#define KC_NUM  0x53

// 各キーの対応は translate_kana_to_ascii() 関数の内容を参照のこと
enum windmill_keycodes {
  // かな
  KA_A = SAFE_RANGE, KA_I, KA_U, KA_E, KA_O,
  KA_KA, KA_KI, KA_KU, KA_KE, KA_KO,
  KA_SA, KA_SHI, KA_SU, KA_SE, KA_SO,
  KA_TA, KA_CHI, KA_TSU, KA_TE, KA_TO,
  KA_NA, KA_NI, KA_NU, KA_NE, KA_NO,
  KA_HA, KA_HI, KA_FU, KA_HE, KA_HO,
  KA_MA, KA_MI, KA_MU, KA_ME, KA_MO,
  KA_YA, KA_YU, KA_YO,
  KA_RA, KA_RI, KA_RU, KA_RE, KA_RO,
  KA_WA, KA_WO, KA_N,
  // 拗音
  KA_XA, KA_XI, KA_XU, KA_XE, KA_XO, KA_XTSU, KA_XYA, KA_XYU, KA_XYO,
  // 記号
  KA_LKAK, KA_RKAK, KA_DAKU, KA_HAN, KA_TEN, KA_MARU, KA_NAKA, KA_CHOU,
  // 半角スペース
  KA_SPC,
  // 半角記号
  KA_QUES, KA_PIPE, KA_SLSH, KA_BSLS,
  // 日本語入力モード
  JA_ROME, JA_ROKA, JA_KANA, // ローマ字入力, ローマ字エミュレーション入力, かな入力
  // IME種別
  IME_WIN, IME_AND, IME_CRM, IME_MAC, IME_IOS,

  WINDMILL_SAFE_RANGE
};

void windmill_init_layers(int alpha_layer, int numpad_layer, int kana_layer, int sym_layer);
void windmill_init_keycolors(uint8_t* user_colorset);
uint8_t windmill_process_keycolor_user(uint16_t keycode);

bool windmill_modlayertap(uint16_t keycode, keyrecord_t *record, uint8_t mod_mask, uint8_t layer_to_activate);
bool windmill_modtap(uint16_t keycode, keyrecord_t *record, uint8_t mod_mask);
bool windmill_layertap(uint16_t keycode, keyrecord_t *record, uint8_t layer_to_activate);

bool is_kana(void);
void send_kana(void);
void send_alpha(void);
void kana_on(void);
void kana_off(void);

void windmill_tap_code(uint16_t keycode);
