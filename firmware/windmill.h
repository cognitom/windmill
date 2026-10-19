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

/* レイヤー番号。keymaps[] の並びと一致させること。
 *
 * かなの Shift 面は、こ / み のホールド中だけ上げる (windmill.c の apply_bottom_hold)。
 * 英数と記号より下に置くのは、Shift を押しながら Ctrl で英数レイヤーを上げたときや、
 * 記号レイヤーを上げたときに、そちらのキーが勝つようにするため */
#define LAYER_KANA       0 // かな。OS側のIMEを「かな入力」にして使う
#define LAYER_KANA_SHIFT 1 // かなの Shift 面
#define LAYER_ALPHA      2 // 英数
#define LAYER_SYM        3 // 数字・記号
#define LAYER_FN         4 // ファンクション・メディア
#define LAYER_CONF       5 // 設定。左右のFnを両方ホールドしている間だけ (set_fn 参照)
#define LAYER_SIZE       6

/* 英数レイヤーの Fn。左右を両方ホールドしている間だけ設定レイヤーへ移す
 * (set_fn 参照、issue #62)。左右とも MO(LAYER_FN) で同じキーコードなので、
 * キーコードでは左右を区別できない。同じ行に並んでいるので matrix の列 (col)
 * だけで見分ける。
 *
 * ここで見る列は keyrecord_t に載ってくる matrix の col で、LAYOUT_ortho_4x12 の
 * 見た目の位置ではない。technik / ymd40 / minipeg48 は両者が一致するので既定値で
 * よいが、geonix41 は最下段の配線が見た目の並びと違う (keyboard.json の "layouts"
 * の "matrix" 参照)。そういう機種は config.h で上書きする (issue #68)。
 * keymaps[] の英数レイヤーの並びと一致させること。かなレイヤーの Fn (そ / ね) は
 * 独自キーコードなので列を見ない */
#define ALPHA_FN MO(LAYER_FN)
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
 * 英数レイヤー以外の文字キー (数字、記号、かな) は全て独自キーコードで、
 * 1キー1出力にする (issue #82)。押した瞬間に送り切ってリピートせず、押されている
 * Shift は打ち消す。ホストへ何を送るかは windmill.c の出力表 (key_outputs[]) が持つ。
 * キーマップには「どの文字のキーか」だけを書き、接続先の配列やOSで変わるキーコードは
 * 表の側で吸収する。英数レイヤーのキーと文字以外のキー (Enter や矢印など) は
 * 素のキーコードのままで、リピートと Shift はQMKに任せる。
 *
 * LT() / MT() のタップ側には独自キーコードを置けない (8bitの基本キーコードしか
 * 入らない)。英数レイヤーの LT(LAYER_SYM,KC_BSLS) / LT(LAYER_SYM,KC_SLSH) は
 * 素のキーコードのまま残してある。かなレイヤー最下段は LT() / MT() をやめて
 * 独自キーコードにし、タップ・ホールドの判定を windmill.c が持つ (issue #85)。
 *
 * KB_TSU 〜 SY_QUES は key_outputs[] のインデックス (keycode - KEY_OUTPUT_FIRST) に
 * 使っている。足すときは表にも行を足すこと (tests/test_key_output.cpp が
 * 全キーの出力を見ている)。 */
enum windmill_keycodes {
    MY_WIN = QK_USER_0, // 言語切替と「」の出し方をWindows/デスクトップ向けに (接続先ごとにEEPROM保存)
    MY_ANDR,            // 同じくAndroid向けに。Win と Alt の扱いも変わる (windmill.c「Android での Win / Alt」参照)
    MY_DARK,            // LEDの明るさ 強/弱 を切り替え (EEPROM保存。LED搭載機のみ)
    MY_IME,             // ホスト側のIMEだけ切り替える (ベースレイヤーは動かさない)
    MY_JIS,             // 接続先のキーボード配列を JIS に (接続先ごとにEEPROM保存。既定)
    MY_US,              // 同じく US に

    /* かなレイヤー最下段。タップでかな、ホールドで修飾かレイヤー
     * (windmill.c「かなレイヤー最下段のタップ・ホールド」参照)。並びは左から順。
     * 足すときは windmill.c の bottom_keys[] にも行を足すこと */
    KB_TSU, // つ  ホールド: Win。ホールド中は英数レイヤー
    KB_SA,  // さ  ホールド: Alt。同上
    KB_SO,  // そ  ホールド: Fn。ね と両方で設定レイヤー
    KB_HI,  // ひ  ホールド: 記号レイヤー
    KB_KO,  // こ  ホールド: 左Shift。ホールド中は Shift 面
    KB_MI,  // み  ホールド: 右Shift。同上
    KB_MO,  // も  ホールド: 記号レイヤー
    KB_NE,  // ね  ホールド: Fn

    /* Shift 面の最下段。ホールドの役割は、同じ位置にあるかなレイヤーのキー (KB_*) と
     * 同じで、タップで出るものだけが変わる。透過の位置 (さ そ ひ) はかなレイヤーの
     * キーがそのまま効く */
    KB_XTSU,   // っ  (つ の位置)
    KB_SPC,    // 空  (こ / み の位置) 親指の組で出す半角スペース。組にならないタップは、
               //     同じ位置のかなレイヤーのキーとして出す (windmill.c の send_bottom_tap)
    KB_QUES,   // ？  (も の位置) 半角。英数に切り替えて送り、かなに戻す
    KB_TOUTEN, // 、  (ね の位置)

    /* かなレイヤー。名前は出るかな */
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
    KN_TE,   // て
    KN_I,    // い
    KN_SU,   // す
    KN_KA,   // か
    KN_N,    // ん
    KN_NA,   // な
    KN_NI,   // に
    KN_RA,   // ら
    KN_SE,   // せ
    KN_DAKU, // ゛
    KN_CHI,  // ち
    KN_TO,   // と
    KN_SHI,  // し
    KN_HA,   // は
    KN_KI,   // き
    KN_KU,   // く
    KN_MA,   // ま
    KN_NO,   // の
    KN_RI,   // り
    KN_RE,   // れ
    KN_KE,   // け
    KN_RU,   // る
    KN_ME,   // め
    KN_RO,   // ろ

    /* かなの Shift 面 */
    KN_XA,      // ぁ
    KN_XU,      // ぅ
    KN_XE,      // ぇ
    KN_XO,      // ぉ
    KN_XYA,     // ゃ
    KN_XYU,     // ゅ
    KN_XYO,     // ょ
    KN_WO,      // を
    KN_HE,      // へ
    KN_XI,      // ぃ
    KN_MU,      // む
    KN_HO,      // ほ
    KN_LKAKKO,  // 「
    KN_RKAKKO,  // 」
    KN_HANDAKU, // ゜
    KN_XTSU,    // っ
    KN_TOUTEN,  // 、
    KN_KUTEN,   // 。
    KN_NAKAGURO, // ・
    KN_CHOON,   // ー

    /* 記号レイヤー。1キー1文字。is_alpha_output() が SY_1 〜 SY_QUES を範囲で
     * 見ているので、記号レイヤーのキーはこの間に足すこと */
    SY_1,    // 1
    SY_2,    // 2
    SY_3,    // 3
    SY_4,    // 4
    SY_5,    // 5
    SY_6,    // 6
    SY_7,    // 7
    SY_8,    // 8
    SY_9,    // 9
    SY_0,    // 0
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

// かなレイヤー最下段のキーの範囲。Shift 面の最下段 (KB_XTSU 〜 KB_TOUTEN) を含む
#define KEY_BOTTOM_FIRST KB_TSU
#define KEY_BOTTOM_LAST  KB_TOUTEN

// 出力表 (windmill.c の key_outputs[]) で引くキーの範囲。最下段のキーはタップで出すものを引く
#define KEY_OUTPUT_FIRST KB_TSU
#define KEY_OUTPUT_LAST  SY_QUES

// 1キー1出力で送る文字キーの範囲 (最下段のキーを除く)
#define KEY_CHAR_FIRST KN_NU
#define KEY_CHAR_LAST  SY_QUES

/* 出力表で引くキー (かなレイヤー最下段のキーを含む) なら、ホストへ送るキーコードを
 * 返す。Shift の要るものは S() 付き。それ以外はそのまま返す。キーマップのキーコードを
 * 素のキーコードとして読みたい機種側の処理 (geonix41 のブロブ) 向け */
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
