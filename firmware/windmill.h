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
#define LAYER_FN    3 // ファンクション・メディア
#define LAYER_CONF  4 // 設定。左右のFnを両方ホールドしている間だけ (process_fn 参照)
#define LAYER_SIZE  5

/* 親指Shift。keymaps[] のレイヤー0で使っているものと一致させること。
 *
 * 左右とも左Shiftにする (issue #37)。以前は右を RSFT_T(KC_N) にしていたが、
 * `S(KC_x)` の weak Shift は必ず左Shiftなので、右Shiftを押している間だけ
 * ホストから見て修飾が入れ替わり、その1打鍵だけShiftが効かない罠があった
 * (issue #18、issue #17)。左に揃えれば実Shiftと weak Shift が同じビットになり、
 * 入れ替えのレポートが原理的に発生しない。 */
#define THUMB_SHIFT_B LSFT_T(KC_B)
#define THUMB_SHIFT_N LSFT_T(KC_N)

/* 英数レイヤーの親指Shift (SandS)。左右とも元から LSFT_T(KC_SPC) で同じ
 * キーコードなので、かなレイヤーの THUMB_SHIFT_B/N と違ってタップ側の
 * キーコードでは左右を区別できない。同じ行に並んでいるので列 (col) だけで
 * 見分ける (issue #40)。
 *
 * ここで見る列は keyrecord_t に載ってくる matrix の col で、LAYOUT_ortho_4x12 の
 * 見た目の位置ではない。technik / ymd40 / minipeg48 は両者が一致するので既定値で
 * よいが、geonix41 は最下段の配線が見た目の並びと違う (keyboard.json の "layouts"
 * の "matrix" 参照)。そういう機種は config.h で上書きする (issue #68)。
 * 値は keymaps[] のレイヤー1(英数)で ALPHA_THUMB_SHIFT を置いた位置の matrix の列 */
#define ALPHA_THUMB_SHIFT LSFT_T(KC_SPC)
#ifndef ALPHA_THUMB_SHIFT_L_COL
#    define ALPHA_THUMB_SHIFT_L_COL 5
#endif
#ifndef ALPHA_THUMB_SHIFT_R_COL
#    define ALPHA_THUMB_SHIFT_R_COL 6
#endif

/* かなレイヤーの「も」。Shiftを押しながらタップすると半角「?」を出す
 * (process_kana_qmark 参照)。keymaps[] のレイヤー0で使っているものと一致させること */
#define KANA_QMARK_KEY LT(2, KC_M)

/* かなレイヤーの Win(つ) / Alt(さ)。ホールドしている間は MY_LCTL と同じく
 * 英数レイヤーへ移す (process_kana_mod 参照、issue #34)。
 * keymaps[] のレイヤー0で使っているものと一致させること */
#define KANA_GUI_KEY LGUI_T(KC_Z)
#define KANA_ALT_KEY LALT_T(KC_X)

/* Fn。左右を両方ホールドしている間だけ設定レイヤーへ移す (process_fn 参照、
 * issue #62)。英数レイヤーは左右とも MO(3) で同じキーコードなので、
 * ALPHA_THUMB_SHIFT と同じく matrix の列 (col) で左右を見分ける。列が見た目の
 * 位置と違う機種は config.h で上書きする (ALPHA_THUMB_SHIFT_L_COL 参照)。
 * keymaps[] のレイヤー0(かな)とレイヤー1(英数)の並びと一致させること */
#define KANA_FN_L LT(3, KC_C)
#define KANA_FN_R LT(3, KC_COMMA)
#define ALPHA_FN  MO(3)
#ifndef FN_L_COL
#    define FN_L_COL 3
#endif
#ifndef FN_R_COL
#    define FN_R_COL 8
#endif

/* 言語切替 (タップ) と Ctrl + 英数レイヤー (ホールド)。
 *
 * QMK標準の mod-tap。タップ側を KC_NO にしてあるので QMK はタップで何も送らず、
 * タップとホールドの両方を windmill.c が横取りする (QMKの docs/mod_tap.md
 * 「Changing both tap and hold」と同じ形)。以前はダブルタップ (かな) があった
 * ので自前の状態機械で見分けていたが、トグルにした (issue #53) ことで
 * tapping term の計測も別キー割り込みでのホールド確定も QMK に任せられる。
 *
 * keymaps[] のレイヤー0で使っているものと一致させること */
#define MY_LCTL LCTL_T(KC_NO)

/* 独自キーコード。全機種で QK_USER_0 から並べる。
 *
 * キーボード側のキーコードは QK_KB_0 から置くのがQMKの作法だが、そちらは
 * QK_KB_MAX まで64個しか無い。記号とかなのキーを全て独自キーコードにした
 * (issue #73) ので収まらず、geonix41 ではベンダーのライブラリ (rdr_lib) が
 * QK_KB_0 から30個を使っているぶん、さらに狭い。QK_USER の範囲 (QK_USER_MAX まで
 * 448個) は本来キーマップ用だが、このリポジトリのキーマップは全てここのキーコード
 * だけで書いてあり、SAFE_RANGE から独自に足しているものは無い。
 *
 * 記号とかなのキー (KN_* / SY_*) がホストへ何を送るかは、windmill.c の出力表
 * (key_outputs[]) が持つ。キーマップには「どの文字のキーか」だけを書き、
 * 接続先の配列やOSで変わるキーコードは表の側で吸収する。
 *
 * LT() / MT() のタップ側には独自キーコードを置けない (8bitの基本キーコードしか
 * 入らない)。かなレイヤー最下段の つ さ そ ひ こ み も ね と、英数レイヤーの
 * LT(2,KC_BSLS) / LT(2,KC_SLSH) は素のキーコードのまま残してある。
 *
 * KN_NU 〜 SY_QUES は key_outputs[] のインデックス (keycode - KEY_OUTPUT_FIRST) に
 * 使っている。足すときは表にも行を足すこと (tests/test_key_output.cpp が
 * 全キーの出力を見ている)。 */
enum windmill_keycodes {
    MY_WIN = QK_USER_0, // 言語切替と「」の出し方をWindows/デスクトップ向けに (接続先ごとにEEPROM保存)
    MY_ANDR,            // 同じくAndroid向けに。Win と Alt の扱いも変わる (windmill.c「Android での Win / Alt」参照)
    MY_DARK,            // LEDの明るさ 強/弱 を切り替え (EEPROM保存。LED搭載機のみ)
    MY_IME,             // ホスト側のIMEだけ切り替える (ベースレイヤーは動かさない)
    MY_JIS,             // 接続先のキーボード配列を JIS に (接続先ごとにEEPROM保存。既定)
    MY_US,              // 同じく US に

    /* かなレイヤー。名前はShift無しで出るかな。Shift時に別のかなを出すものは
     * コメントに添えた。それ以外はOSのIMEに任せる (あ → ぁ、わ → を など) */
    KN_NU,   // ぬ
    KN_FU,   // ふ
    KN_A,    // あ
    KN_U,    // う
    KN_E,    // え
    KN_O,    // お
    KN_YA,   // や
    KN_YU,   // ゆ
    KN_YO,   // よ
    KN_WA,   // わ
    KN_TA,   // た
    KN_TE,   // て  Shift時: へ
    KN_I,    // い
    KN_SU,   // す  Shift時: む
    KN_KA,   // か
    KN_N,    // ん
    KN_NA,   // な  Shift時: ほ
    KN_NI,   // に
    KN_RA,   // ら  Shift時: 「
    KN_SE,   // せ  Shift時: 」
    KN_DAKU, // ゛  Shift時: ゜
    KN_CHI,  // ち  Shift時: っ
    KN_TO,   // と
    KN_SHI,  // し
    KN_HA,   // は
    KN_KI,   // き
    KN_KU,   // く
    KN_MA,   // ま
    KN_NO,   // の  Shift時: 、
    KN_RI,   // り  Shift時: 。
    KN_RE,   // れ  Shift時: ・
    KN_KE,   // け  Shift時: ー
    KN_RU,   // る
    KN_ME,   // め
    KN_RO,   // ろ

    /* 英数レイヤーの記号。「Shift無しの文字_Shift時の文字」の組で1つ */
    SY_SCLN_COLN, // ; :
    SY_QUOT_DQUO, // ' "
    SY_COMM_LABK, // , <
    SY_DOT_RABK,  // . >

    /* 記号レイヤー。1キー1文字。is_sym_ime_wrap_target() が SY_EXLM 〜 SY_QUES を
     * 範囲で見ているので、記号レイヤーのキーはこの間に足すこと */
    SY_EXLM, // !
    SY_AT,   // @
    SY_HASH, // #
    SY_DLR,  // $
    SY_PERC, // %
    SY_CIRC, // ^
    SY_AMPR, // &
    SY_ASTR, // *
    SY_LPRN, // (
    SY_RPRN, // )
    SY_GRV,  // `
    SY_EQL,  // =
    SY_PLUS, // +
    SY_MINS, // -
    SY_UNDS, // _
    SY_LBRC, // [
    SY_RBRC, // ]
    SY_TILD, // ~
    SY_LCBR, // {
    SY_RCBR, // }
    SY_PIPE, // |
    SY_QUES, // ?

    WINDMILL_KEYCODE_END, // 番兵。キーマップには置かない
};

// 出力表 (windmill.c の key_outputs[]) で引くキーの範囲
#define KEY_OUTPUT_FIRST KN_NU
#define KEY_OUTPUT_LAST  SY_QUES

/* 出力表で引くキーなら、Shift無しでホストへ送るキーコードを返す。それ以外は
 * そのまま返す。キーマップのキーコードを素のキーコードとして読みたい機種側の
 * 処理 (geonix41 のブロブ) 向け */
uint16_t windmill_output_keycode(uint16_t keycode);

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

/* 接続先 (ホスト) の番号。無線機では接続先ごとに相手のOSが違うので、
 * MY_WIN / MY_ANDR の設定を接続先ごとに覚える (issue #58)。
 * MY_JIS / MY_US の設定も同じ (issue #74)。
 * 番号はEEPROM上の格納位置になるので、並びを変えないこと */
#define WINDMILL_HOST_USB  0 // 有線。無線を持たない機種は常にここ
#define WINDMILL_HOST_BLE1 1
#define WINDMILL_HOST_BLE2 2
#define WINDMILL_HOST_BLE3 3
#define WINDMILL_HOST_2P4G 4
#define WINDMILL_HOST_SIZE 5

/* 接続先のキーボード配列。相手のOSがこのキーボードをどの配列として受け取るか。
 * 値はEEPROMに残るので変えないこと。0 が既定 (windmill.c の LAYOUT_BITS 参照) */
#define WINDMILL_LAYOUT_JIS 0 // 日本語 (JIS)
#define WINDMILL_LAYOUT_US  1 // English (US)

// いま繋がっている接続先の配列 (WINDMILL_LAYOUT_*)
uint8_t windmill_host_layout(void);

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

/* いま繋がっている接続先 (WINDMILL_HOST_*)。OSの設定を引くたびに呼ばれるので、
 * 接続先の切り替えを windmill 側で追いかける必要はない。
 * 実装しない機種では WINDMILL_HOST_USB */
uint8_t windmill_board_host(void);

// LEDを流し込む直前。ベンダー側の描画を先に走らせてから配色を上書きするために使う
void windmill_board_led_begin(void);
