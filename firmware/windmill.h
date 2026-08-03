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

/* 親指Shift。keymaps[] のレイヤー0で使っているものと一致させること。
 *
 * 左右とも左Shiftにする (issue #37)。以前は右を RSFT_T(KC_N) にしていたが、
 * `S(KC_x)` の weak Shift は必ず左Shiftなので、右Shiftを押している間だけ
 * ホストから見て修飾が入れ替わり、その1打鍵だけShiftが効かない罠があった
 * (issue #18、issue #17)。左に揃えれば実Shiftと weak Shift が同じビットになり、
 * 入れ替えのレポートが原理的に発生しない。 */
#define THUMB_SHIFT_B LSFT_T(KC_B)
#define THUMB_SHIFT_N LSFT_T(KC_N)

/* かなレイヤーの「も」。Shiftを押しながらタップすると半角「?」を出す
 * (process_kana_qmark 参照)。keymaps[] のレイヤー0で使っているものと一致させること */
#define KANA_QMARK_KEY LT(2, KC_M)

/* カスタムキーコードの開始位置。通常は QK_KB_0 から。ただし geonix41 のように
 * ベンダーのライブラリが QK_KB_0 から自前のキーコードを並べている機種では、
 * ぶつからないよう後ろにずらす必要があるので、機種の config.h で上書きする。 */
#ifndef WINDMILL_KEYCODE_BASE
#    define WINDMILL_KEYCODE_BASE QK_KB_0
#endif

/* geonix41/minipeg48 から移植したカスタムキーコード。
 * MY_W 〜 MY_A は my_shift_pairs[] のインデックス (keycode - MY_W) に
 * 使っているので、並び順を変えないこと。 */
enum windmill_keycodes {
    MY_O = WINDMILL_KEYCODE_BASE, // Shift時: 「
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

/* 機種固有の割り込み口。windmill.c が QMK の *_kb フックを占有しているので、
 * ベンダーのライブラリを呼ぶ必要がある機種 (geonix41) 向けに weak で開けてある。
 * 実装しない機種では何もしない。 */

// keyboard_post_init_kb() の最後
void windmill_board_post_init(void);

/* すべてのキーイベントの入口 (pre_process_record_kb)。MY_* のように windmill が
 * 途中で消費するキーでも必ず通るので、スリープ抑止などはこちらで行う。 */
void windmill_board_pre_process_record(uint16_t keycode, keyrecord_t *record);

/* process_record_kb() の最後。windmill が消費しなかったキーだけが渡る。
 * false を返すとキーはそこで消費される。 */
bool windmill_board_process_record(uint16_t keycode, keyrecord_t *record);

// LEDを流し込む直前。ベンダー側の描画を先に走らせてから配色を上書きするために使う
void windmill_board_led_begin(void);
