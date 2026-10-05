/* Copyright 2026 Tsutomu Kawamura
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

/* テスト用のキーマップ。firmware/technik/keymaps/default/keymap.c と同じ内容を
 * QMK のテスト基盤 (set_keymap) に載せられる形で持つ。geonix41 / minipeg48 /
 * ymd40 も、キーの並びは同じなのでこれ1枚で足りる。
 *
 * ただし windmill.c は左右の Fn / 親指Shift を matrix の列で見分けるので、
 * 見た目の位置と matrix の列がずれる機種 (geonix41) は、並びが同じでも
 * 同じテストを通るとは限らない (issue #68)。そこでキーマップは見た目の位置で
 * 書いておき、matrix へ載せるときに WINDMILL_TEST_ROW3_COLS で最下段の列を
 * 引き直す。既定は「位置＝列」(technik / ymd40 / minipeg48)。geonix41 の配線で
 * 通すテストは tests/geonix41/ にある。
 *
 * 実機の keymap.c は LAYOUT_ortho_4x12 マクロと PROGMEM に依存していて
 * そのままは使えないため、ここだけ二重管理になる。配置を変えたら両方直すこと。 */

#pragma once

#include "test_fixture.hpp"
#include "test_keymap_key.hpp"

extern "C" {
#include "windmill.h"
}

// clang-format off
static const uint16_t windmill_keymap[LAYER_SIZE][MATRIX_ROWS][MATRIX_COLS] = {

  [LAYER_KANA] = {
    {KC_ESC,  KN_NU,        KN_FU,        KN_A,        KN_U,          KN_E,           KN_O,           KN_YA,         KN_YU,          KN_YO,      KN_WA,   KC_ENT},
    {KC_TAB,  KN_TA,        KN_TE,        KN_I,        KN_SU,         KN_KA,          KN_N,           KN_NA,         KN_NI,          KN_RA,      KN_SE,   KN_DAKU},
    {KC_BSPC, KN_CHI,       KN_TO,        KN_SHI,      KN_HA,         KN_KI,          KN_KU,          KN_MA,         KN_NO,          KN_RI,      KN_RE,   KN_KE},
    {MY_LCTL, LGUI_T(KC_Z), LALT_T(KC_X), LT(3,KC_C),  LT(2,KC_V),    LSFT_T(KC_B),   LSFT_T(KC_N),   LT(2,KC_M),    LT(3,KC_COMMA), KN_RU,      KN_ME,   KN_RO},
  },

  [LAYER_ALPHA] = {
    {KC_TRNS, KC_Q,    KC_W,    KC_E,  KC_R,          KC_T,           KC_Y,           KC_U,          KC_I,         KC_O,        KC_P,         KC_TRNS},
    {KC_TRNS, KC_A,    KC_S,    KC_D,  KC_F,          KC_G,           KC_H,           KC_J,          KC_K,         KC_L,        SY_SCLN_COLN, SY_QUOT_DQUO},
    {KC_TRNS, KC_Z,    KC_X,    KC_C,  KC_V,          KC_B,           KC_N,           KC_M,          SY_COMM_LABK, SY_DOT_RABK, KC_UP,        KC_RGHT},
    {KC_TRNS, KC_LGUI, KC_LALT, MO(3), LT(2,KC_BSLS), LSFT_T(KC_SPC), LSFT_T(KC_SPC), LT(2,KC_SLSH), MO(3),        KC_APP,      KC_LEFT,      KC_DOWN},
  },

  [LAYER_SYM] = {
    {KC_TRNS, KC_1,         KC_2,         KC_3,        KC_4,          KC_5,           KC_6,           KC_7,          KC_8,           KC_9,       KC_0,    KC_TRNS},
    {KC_TRNS, SY_EXLM,      SY_AT,        SY_HASH,     SY_DLR,        SY_PERC,        SY_CIRC,        SY_AMPR,       SY_ASTR,        SY_LPRN,    SY_RPRN, SY_GRV},
    {KC_TRNS, SY_EQL,       SY_PLUS,      SY_MINS,     SY_UNDS,       SY_LBRC,        SY_RBRC,        SY_TILD,       SY_LCBR,        SY_RCBR,    KC_UP,   KC_RGHT},
    {KC_TRNS, KC_TRNS,      KC_TRNS,      KC_TRNS,     KC_TRNS,       SY_PIPE,        SY_QUES,        KC_TRNS,       KC_TRNS,        KC_TRNS,    KC_LEFT, KC_DOWN},
  },

  [LAYER_FN] = {
    {KC_TRNS, KC_F1,        KC_F2,        KC_F3,       KC_F4,         KC_F5,          KC_F6,          KC_F7,         KC_F8,          KC_F9,      KC_F10,  MY_DARK},
    {KC_CAPS, KC_F11,       KC_F12,       KC_NO,       KC_NO,         KC_NO,          KC_NO,          KC_NO,         KC_NO,          KC_NO,      KC_NO,   KC_NO},
    {KC_DEL,  KC_PSCR,      KC_NO,        KC_NO,       KC_NO,         KC_BRID,        KC_BRIU,        KC_MUTE,       KC_VOLD,        KC_VOLU,    KC_UP,   KC_RGHT},
    {MY_IME,  KC_TRNS,      KC_TRNS,      KC_TRNS,     KC_TRNS,       KC_TRNS,        KC_TRNS,        KC_TRNS,       KC_TRNS,        KC_TRNS,    KC_LEFT, KC_DOWN},
  },

  [LAYER_CONF] = {
    {KC_NO,   KC_NO,        KC_NO,        KC_NO,       KC_NO,         KC_NO,          KC_NO,          KC_NO,         KC_NO,          KC_NO,      KC_NO,   QK_BOOT},
    {KC_NO,   MY_WIN,       MY_ANDR,      KC_NO,       KC_NO,         KC_NO,          KC_NO,          KC_NO,         KC_NO,          KC_NO,      KC_NO,   KC_NO},
    {KC_NO,   MY_JIS,       MY_US,        KC_NO,       KC_NO,         KC_NO,          KC_NO,          KC_NO,         KC_NO,          KC_NO,      KC_NO,   KC_NO},
    {KC_NO,   KC_NO,        KC_NO,        KC_TRNS,     KC_NO,         KC_NO,          KC_NO,          KC_NO,         KC_TRNS,        KC_NO,      KC_NO,   KC_NO},
  },
};

/* 最下段の、見た目の位置ごとの matrix の列。各機種の keyboard.json の
 * "layouts" の "matrix" と揃えること。最下段以外は全機種で位置＝列 */
#ifndef WINDMILL_TEST_ROW3_COLS
#    define WINDMILL_TEST_ROW3_COLS {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}
#endif
static const uint8_t windmill_row3_cols[MATRIX_COLS] = WINDMILL_TEST_ROW3_COLS;
// clang-format on

static inline uint8_t windmill_matrix_col(uint8_t row, uint8_t col) {
    return row == 3 ? windmill_row3_cols[col] : col;
}

/* かなレイヤー上の位置 (見た目の行と位置。matrix の列ではない)。コメントの文字は JISかな入力での出力 */
#define POS_LCTL 3, 0  // 言語切替 (英数⇔かな)
#define POS_KO 3, 5    // こ  左親指Shift
#define POS_MI 3, 6    // み  右親指Shift
#define POS_NO 2, 8    // の  KN_NO  Shift時 S(KC_COMM) = 、
#define POS_RA 1, 9    // ら  KN_RA  Shift時 S(KC_LBRC)  (US のとき。JIS は S(KC_RBRC))
#define POS_SU 1, 4    // す  KN_SU  Shift時 KC_BSLS  (Shiftを外す必要がある。JIS は KC_NUHS)
#define POS_NA 1, 7    // な  KN_NA  Shift時 KC_MINS = ほ (Shiftを外す必要がある)
#define POS_HA 2, 4    // は  KN_HA  英数レイヤーでは "f"
#define POS_MO 3, 7    // も  LT(2,KC_M)   Shift+タップで半角?
#define POS_TSU 3, 1   // つ  LGUI_T(KC_Z) 英数レイヤーでは KC_LGUI
#define POS_SA 3, 2    // さ  LALT_T(KC_X) 英数レイヤーでは KC_LALT
#define POS_NU 0, 1    // ぬ  KN_NU        英数レイヤーでは "q"
#define POS_RI 2, 9    // り  KN_RI        英数レイヤーでは "."
#define POS_FN_L 3, 3  // そ  LT(3,KC_C)     英数レイヤーでは MO(3)
#define POS_FN_R 3, 8  // ね  LT(3,KC_COMMA) 英数レイヤーでは MO(3)

/* 設定レイヤー上の位置。左右のFnを両方ホールドしている間だけ出る (issue #62) */
#define POS_WIN 1, 1   // MY_WIN   Fnレイヤーでは KC_F11
#define POS_ANDR 1, 2  // MY_ANDR  Fnレイヤーでは KC_F12
#define POS_JIS 2, 1   // MY_JIS   Fnレイヤーでは KC_PSCR
#define POS_US 2, 2    // MY_US    Fnレイヤーでは KC_NO

class WindmillTest : public TestFixture {
   public:
    void set_windmill_keymap() {
        for (uint8_t layer = 0; layer < LAYER_SIZE; ++layer)
            for (uint8_t row = 0; row < MATRIX_ROWS; ++row)
                for (uint8_t col = 0; col < MATRIX_COLS; ++col)
                    add_key(KeymapKey(layer, windmill_matrix_col(row, col), row, windmill_keymap[layer][row][col]));

        /* 起動直後と同じ英数から始める。QMKのテスト基盤は keyboard_init() を
         * テストスイートごとに一度しか呼ばず、default_layer_state をテストごとには
         * 戻さない。MY_LCTL はトグル (issue #53) なので、前のテストがかなで
         * 終わっていると、同じ1回タップでも行き先が逆になってしまう */
        default_layer_set((layer_state_t)1 << LAYER_ALPHA);
    }

    KeymapKey key(uint8_t row, uint8_t col) {
        return KeymapKey(LAYER_KANA, windmill_matrix_col(row, col), row, windmill_keymap[LAYER_KANA][row][col]);
    }

    // 左右のFnを両方ホールドして、設定レイヤー上のキーを1回叩く
    void tap_on_conf(uint8_t row, uint8_t col) {
        auto fn_l = key(POS_FN_L);
        auto fn_r = key(POS_FN_R);
        fn_l.press();
        run_one_scan_loop();
        fn_r.press();
        run_one_scan_loop();
        idle_for(250); // TAPPING_TERM 超え。ホールド確定
        tap_key(key(row, col), 50);
        idle_for(50);
        fn_r.release();
        run_one_scan_loop();
        fn_l.release();
        run_one_scan_loop();
        idle_for(120);
    }

    // 押しっぱなしのキーが無い、落ち着いた状態にする
    void settle() {
        idle_for(TAPPING_TERM * 2);
    }

    static constexpr unsigned IME_WAIT_MS = 10; // windmill.c の IME_WAIT_MS
};

/* 接続先の配列を US にして走らせるテスト向け。
 *
 * 配列の既定は JIS (issue #74) なので、何もしなければテストは JIS の出力を見る。
 * US の出力を固定しているスイートはこちらを継ぐ。設定はEEPROMに残り、スイートが
 * 変わると初期化されるので、テストごとに選び直しても害は無い。
 * 設定レイヤーのキーはレポートを出さないので、期待値を置く前に呼んでよい */
class WindmillUsTest : public WindmillTest {
   public:
    void set_windmill_keymap() {
        WindmillTest::set_windmill_keymap();
        tap_on_conf(POS_US);
        settle();
    }
};
