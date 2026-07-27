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

#pragma once

#include "quantum.h"

/* LEDの配色処理を持つかどうか。minipeg48 はLED非搭載なので丸ごと外れる */
#if defined(RGB_MATRIX_ENABLE) || defined(RGBLIGHT_ENABLE)
#    define WINDMILL_LED_ENABLE
#endif

/* レイヤー番号。keymaps[] の並びと一致させること */
#define LAYER_KANA  0 // かな。OS側のIMEを「かな入力」にして使う
#define LAYER_ALPHA 1 // 英数
#define LAYER_SYM   2 // 数字・記号
#define LAYER_FN    3 // ファンクション・メディア・設定
#define LAYER_SIZE  4

/* 親指Shift。keymaps[] のレイヤー0で使っているものと一致させること */
#define THUMB_SHIFT_B LSFT_T(KC_B)
#define THUMB_SHIFT_N RSFT_T(KC_N)

/* geonix41/minipeg48 から移植したカスタムキーコード。
 * MY_W 〜 MY_A は my_shift_pairs[] のインデックス (keycode - MY_W) に
 * 使っているので、並び順を変えないこと。 */
enum windmill_keycodes {
    MY_O = QK_KB_0, // Shift時: 「
    MY_P,           // Shift時: 」
    MY_LCTL,        // tap: 英数, double-tap: かな, hold: Ctrl + 英数レイヤー
    MY_W,           // Shift時: +
    MY_R,           // Shift時: バックスラッシュ
    MY_U,           // Shift時: -
    MY_LBRC,        // Shift時: ]
    MY_K,           // Shift時: <
    MY_L,           // Shift時: >
    MY_SCLN,        // Shift時: ?
    MY_QUOT,        // Shift時: _
    MY_A,           // Shift時: Z
    MY_WIN,         // MY_O/MY_PのShift時出力をWindows/デスクトップ向けに (EEPROM保存)
    MY_ANDR,        // MY_O/MY_PのShift時出力をAndroid向けに (EEPROM保存)
    MY_DARK,        // LEDの明るさ 強/弱 を切り替え (EEPROM保存。LED搭載機のみ)
};

#ifdef WINDMILL_LED_ENABLE

/* keymap.c 側で定義する配色テーブルを登録する。
 * colorset は [色][6] = {明るい時のR,G,B, 暗い時のR,G,B} の配列。 */
void windmill_init_keycolors(uint8_t *user_colorset);

/* keymap.c 側で実装する。キーコードを配色カテゴリ (colorset の添字) に分類する。
 * dual-role キーは windmill_base_keycode() でタップ側に展開してから渡される。
 * レイヤー0 (かな) はOSのIMEがかなに変換するのでキーコードから色を決められない。
 * そのため、どのレイヤーの分なのかを layer で渡す。 */
uint8_t windmill_process_keycolor_user(uint8_t layer, uint16_t keycode);

/* MT()/LT() をタップ側のキーコードに展開する。それ以外はそのまま返す。 */
uint16_t windmill_base_keycode(uint16_t keycode);

#endif // WINDMILL_LED_ENABLE
