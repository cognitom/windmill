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

#pragma once

#include "quantum.h"
#define LAYOUT_ortho_4x12( \
    K00, K01, K02, K03, K04, K05, K06, K07, K08, K09, K10, K11, \
    K12, K13, K14, K15, K16, K17, K18, K19, K20, K21, K22, K23, \
    K24, K25, K26, K27, K28, K29, K30, K31, K32, K33, K34, K35, \
    K36, K37, K38, K39, K40, K41, K42, K43, K44, K45, K46, K47 \
    ) { \
        {K00, K01, K02, K03, K04, K05, K06, K07, K08, K09, K10, K11}, \
        {K12, K13, K14, K15, K16, K17, K18, K19, K20, K21, K22, K23}, \
        {K24, K25, K26, K27, K28, K29, K30, K31, K32, K33, K34, K35}, \
        {K36, K37, K38, K39, K40, K41, K42, K43, K44, K45, K46, K47} \
    }

// TODO: QMKのバージョンを上げて、以下2行を削除
#define KC_LNG1 KC_LANG1
#define KC_LNG2 KC_LANG2

#define MOD_MASK_NONE 0x00

// 各キーの対応は translate_kana_to_ascii() 関数の内容を参照のこと
enum kana_keycodes {
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

  WINDMILL_SAFE_RANGE
};

// for RGB matrix
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

void init_windmill_layers(int alpha_layer, int kana_layer, int kana_shifted_layer, int sym_layer, int fn_layer);

bool windmill_modlayertap(uint16_t keycode, keyrecord_t *record, uint8_t mod_mask, uint8_t layer_to_activate);
bool windmill_modtap(uint16_t keycode, keyrecord_t *record, uint8_t mod_mask);
bool windmill_layertap(uint16_t keycode, keyrecord_t *record, uint8_t layer_to_activate);

bool is_kana(void);
void send_kana(void);
void send_alpha(void);
void kana_on(void);
void kana_off(void);

uint16_t translate_kana_to_ascii(uint16_t keycode);
void windmill_tap_code(uint16_t keycode);

void toggle_darkmode(void);
