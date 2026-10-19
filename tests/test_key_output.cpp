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

/* 文字キーの出力を、位置ごとに総当たりで固定する (issue #73、issue #82)。
 *
 * 英数レイヤー以外の文字キーは独自キーコードで、ホストへ何を送るかは windmill.c の
 * 出力表 (key_outputs[]) が決める。表の行を1つ書き漏らしても、そのキーが何も出さなく
 * なるだけでコンパイルは通る。だから「この位置を打ったらこのレポート」を、かな・
 * Shift 面・記号レイヤーの全キーぶん並べておく。
 *
 * 文字キーは1キー1出力 (issue #82)。押下で押し離しを送り切り、リピートしない。
 * 押されている Shift は打ち消し、出力に Shift が要るときだけ足す。
 *
 * - Shift が要る文字は、押されている Shift をそのまま使う。押されていなければ左Shiftを
 *   足し、Shift の押下と文字キーの押下は別のレポートになる
 * - Shift が要らない文字は、押されている Shift を外して送り、戻す
 *
 * 期待値の文字は US 配列のもの。配列の切り替え (issue #80) まではどちらを選んでも
 * US になる。英数レイヤーの記号は素のキーコードのままで、QMKの標準どおり Shift が乗る。 */

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_layer.h"
#include "action_util.h"
#include "test_keymap.hpp"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;
using testing::InvokeWithoutArgs;

class KeyOutput : public WindmillTest {};

/* 対象OSはEEPROMに残るので、Android向けのテストはスイートを分ける
 * (test_android_mods.cpp と同じ) */
class KeyOutputAndroid : public WindmillTest {};

// 英数から MY_LCTL 1回タップでかなへ切り替える。レポートの中身は問わない
static void switch_to_kana(WindmillTest* f, TestDriver& driver) {
    EXPECT_ANY_REPORT(driver).Times(AnyNumber());

    f->tap_key(f->key(POS_LCTL), 120);
    f->settle();

    VERIFY_AND_CLEAR(driver);
}

#define POS_SYM 3, 4 // ホールドで記号レイヤー。英数では LT(LAYER_SYM,KC_BSLS)、かなでは KB_HI (ひ)
#define POS_ALPHA_SHIFT 3, 6 // 英数レイヤーの右の親指Shift。RSFT_T(KC_SPC)

// 送り方
enum Kind {
    PLAIN, // Shift の要らない文字
    SHIFT, // Shift の要る文字
    ALPHA, // Shift の要る文字を、英数に切り替えて送る (Shift 面の「？」)
};

struct Key {
    uint8_t row;
    uint8_t col;
    uint8_t keycode;
    Kind    kind;
};

/* 1文字ぶんの期待レポート列。held は押されている Shift (無ければ 0)。
 * Shift を足すときは左Shift */
static void expect_char(TestDriver& driver, const Key& k, uint8_t held) {
    switch (k.kind) {
        case PLAIN:
            if (held) EXPECT_EMPTY_REPORT(driver); // Shift を外す
            EXPECT_REPORT(driver, (k.keycode));
            EXPECT_EMPTY_REPORT(driver);
            if (held) EXPECT_REPORT(driver, (held)); // Shift を戻す
            break;
        case SHIFT:
            if (held) {
                EXPECT_REPORT(driver, (held, k.keycode)); // 押されている Shift をそのまま使う
                EXPECT_REPORT(driver, (held));
            } else {
                EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // Shift を足す
                EXPECT_REPORT(driver, (KC_LEFT_SHIFT, k.keycode));
                EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
                EXPECT_EMPTY_REPORT(driver);
            }
            break;
        case ALPHA: {
            const uint8_t shift = held ? held : KC_LEFT_SHIFT;
            if (held) EXPECT_EMPTY_REPORT(driver); // 切り替えのキーに Shift を乗せない
            EXPECT_REPORT(driver, (KC_LNG2));
            EXPECT_EMPTY_REPORT(driver);
            EXPECT_REPORT(driver, (shift));
            EXPECT_REPORT(driver, (shift, k.keycode));
            EXPECT_REPORT(driver, (shift));
            EXPECT_EMPTY_REPORT(driver);
            EXPECT_REPORT(driver, (KC_LNG1));
            EXPECT_EMPTY_REPORT(driver);
            if (held) EXPECT_REPORT(driver, (held));
            break;
        }
    }
}

// 順に叩く。押しっぱなしの時間は最下段のキーでもタップになる長さ
template <size_t N>
static void tap_all(WindmillTest* f, const Key (&keys)[N]) {
    for (const auto& k : keys) {
        f->tap_key(f->key(k.row, k.col), 50);
        f->idle_for(50);
    }
}

/*
 * かなレイヤー
 */

// clang-format off
static const Key kana_keys[] = {
    // ぬ ふ あ う え お や ゆ よ わ
    {0, 1, KC_1, PLAIN}, {0, 2, KC_2, PLAIN}, {0, 3, KC_3, PLAIN}, {0, 4, KC_4, PLAIN}, {0, 5, KC_5, PLAIN},
    {0, 6, KC_6, PLAIN}, {0, 7, KC_7, PLAIN}, {0, 8, KC_8, PLAIN}, {0, 9, KC_9, PLAIN}, {0, 10, KC_0, PLAIN},
    // た て い す か ん な に ら せ ゛
    {1, 1, KC_Q, PLAIN}, {1, 2, KC_W, PLAIN}, {1, 3, KC_E, PLAIN}, {1, 4, KC_R, PLAIN}, {1, 5, KC_T, PLAIN},
    {1, 6, KC_Y, PLAIN}, {1, 7, KC_U, PLAIN}, {1, 8, KC_I, PLAIN}, {1, 9, KC_O, PLAIN}, {1, 10, KC_P, PLAIN},
    {1, 11, KC_LBRC, PLAIN},
    // ち と し は き く ま の り れ け
    {2, 1, KC_A, PLAIN}, {2, 2, KC_S, PLAIN}, {2, 3, KC_D, PLAIN}, {2, 4, KC_F, PLAIN}, {2, 5, KC_G, PLAIN},
    {2, 6, KC_H, PLAIN}, {2, 7, KC_J, PLAIN}, {2, 8, KC_K, PLAIN}, {2, 9, KC_L, PLAIN}, {2, 10, KC_SCLN, PLAIN},
    {2, 11, KC_QUOT, PLAIN},
    // つ さ そ ひ こ み も ね (タップ) る め ろ
    {3, 1, KC_Z, PLAIN}, {3, 2, KC_X, PLAIN}, {3, 3, KC_C, PLAIN}, {3, 4, KC_V, PLAIN}, {3, 5, KC_B, PLAIN},
    {3, 6, KC_N, PLAIN}, {3, 7, KC_M, PLAIN}, {3, 8, KC_COMM, PLAIN},
    {3, 9, KC_DOT, PLAIN}, {3, 10, KC_SLSH, PLAIN}, {3, 11, KC_GRV, PLAIN},
};

/* Shift 面。こ / み のどちらかをホールドしている間だけ上がる。PLAIN は透過で、
 * 下のかなを Shift を外して出す。こ と み の位置 (空) は親指の組の半角スペースで、
 * test_bottom_row.cpp が見る */
static const Key kana_shift_keys[] = {
    // ぬ ふ ぁ ぅ ぇ ぉ ゃ ゅ ょ を
    {0, 1, KC_1, PLAIN}, {0, 2, KC_2, PLAIN}, {0, 3, KC_3, SHIFT}, {0, 4, KC_4, SHIFT}, {0, 5, KC_5, SHIFT},
    {0, 6, KC_6, SHIFT}, {0, 7, KC_7, SHIFT}, {0, 8, KC_8, SHIFT}, {0, 9, KC_9, SHIFT}, {0, 10, KC_0, SHIFT},
    // た へ ぃ む か ん ほ に 「 」 ゜
    {1, 1, KC_Q, PLAIN},
    {1, 2, KC_EQL, SHIFT},   // へ
    {1, 3, KC_E, SHIFT},     // ぃ
    {1, 4, KC_BSLS, PLAIN},  // む
    {1, 5, KC_T, PLAIN}, {1, 6, KC_Y, PLAIN},
    {1, 7, KC_MINS, PLAIN},  // ほ
    {1, 8, KC_I, PLAIN},
    {1, 9, KC_LBRC, SHIFT},  // 「 (Windows)
    {1, 10, KC_RBRC, SHIFT}, // 」 (Windows)
    {1, 11, KC_RBRC, PLAIN}, // ゜
    // っ と し は き く ま 、 。 ・ ー
    {2, 1, KC_Z, SHIFT},
    {2, 2, KC_S, PLAIN}, {2, 3, KC_D, PLAIN}, {2, 4, KC_F, PLAIN}, {2, 5, KC_G, PLAIN}, {2, 6, KC_H, PLAIN},
    {2, 7, KC_J, PLAIN},
    {2, 8, KC_COMM, SHIFT}, {2, 9, KC_DOT, SHIFT}, {2, 10, KC_SLSH, SHIFT},
    {2, 11, KC_MINS, SHIFT}, // ー
    // っ さ そ ひ (タップ) ？ 、 。 ・ ろ
    {3, 1, KC_Z, SHIFT},
    {3, 2, KC_X, PLAIN}, {3, 3, KC_C, PLAIN}, {3, 4, KC_V, PLAIN},
    {3, 7, KC_SLSH, ALPHA},  // ？ 半角
    {3, 8, KC_COMM, SHIFT}, {3, 9, KC_DOT, SHIFT}, {3, 10, KC_SLSH, SHIFT},
    {3, 11, KC_GRV, PLAIN},
};

// 対象OSが Android のときだけ変わるもの。「」の出し方がIMEで違う
static const Key kana_keys_android[] = {
    {1, 9, KC_O, PLAIN},  // ら
    {1, 10, KC_P, PLAIN}, // せ
};
static const Key kana_shift_keys_android[] = {
    {1, 9, KC_RBRC, SHIFT},  // 「
    {1, 10, KC_BSLS, SHIFT}, // 」
};
// clang-format on

// Shift無しで順に叩く
template <size_t N>
static void expect_kana_plain(WindmillTest* f, TestDriver& driver, const Key (&keys)[N]) {
    {
        InSequence s;
        for (const auto& k : keys) expect_char(driver, k, 0);
    }
    tap_all(f, keys);
    VERIFY_AND_CLEAR(driver);
}

// 親指Shift (こ / み) をホールドしたまま、Shift 面のキーを順に叩く
template <size_t N>
static void expect_kana_shifted(WindmillTest* f, TestDriver& driver, const Key (&keys)[N], uint8_t row, uint8_t col, uint8_t held) {
    auto thumb = f->key(row, col);

    {
        InSequence s;
        EXPECT_REPORT(driver, (held)); // ホールド確定
        for (const auto& k : keys) expect_char(driver, k, held);
        EXPECT_EMPTY_REPORT(driver); // 解放
    }

    thumb.press();
    f->run_one_scan_loop();
    f->idle_for(250); // 250ms 押し続けてホールド確定
    tap_all(f, keys);
    thumb.release();
    f->run_one_scan_loop();
    f->idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

TEST_F(KeyOutput, kana_keys_without_shift) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    expect_kana_plain(this, driver, kana_keys);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA); // 次のテストへかなを持ち越さない
}

TEST_F(KeyOutput, kana_shift_layer_with_right_thumb) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    expect_kana_shifted(this, driver, kana_shift_keys, POS_MI, KC_RIGHT_SHIFT);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

TEST_F(KeyOutput, kana_shift_layer_with_left_thumb) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    expect_kana_shifted(this, driver, kana_shift_keys, POS_KO, KC_LEFT_SHIFT);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

/* 1キー1出力。押した瞬間に押し離しを送り切り、押し続けてもそれ以上は出ない
 * (ホスト側のキーリピートも効かない)。重ねて押しても、それぞれ押した時点で出る */
TEST_F(KeyOutput, kana_key_does_not_repeat) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto nu = key(POS_NU);
    auto ha = key(POS_HA);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_1));
        EXPECT_EMPTY_REPORT(driver);
    }
    nu.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    EXPECT_NO_REPORT(driver);
    idle_for(600); // リピートが始まる程度に押し続ける
    VERIFY_AND_CLEAR(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_F));
        EXPECT_EMPTY_REPORT(driver);
    }
    ha.press();
    run_one_scan_loop();
    idle_for(50);
    VERIFY_AND_CLEAR(driver);

    EXPECT_NO_REPORT(driver); // 離しても何も出ない
    nu.release();
    run_one_scan_loop();
    idle_for(50);
    ha.release();
    run_one_scan_loop();
    idle_for(50);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

// 設定レイヤーでOSを選ぶ。レポートは出ない
static void select_os(WindmillTest* f, TestDriver& driver, uint8_t row, uint8_t col) {
    EXPECT_NO_REPORT(driver);
    f->tap_on_conf(row, col);
    f->settle();
    VERIFY_AND_CLEAR(driver);
}

TEST_F(KeyOutputAndroid, kana_brackets_follow_host_os) {
    TestDriver driver;
    set_windmill_keymap();
    select_os(this, driver, POS_ANDR);
    switch_to_kana(this, driver);

    expect_kana_plain(this, driver, kana_keys_android);
    expect_kana_shifted(this, driver, kana_shift_keys_android, POS_MI, KC_RIGHT_SHIFT);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
    select_os(this, driver, POS_WIN); // EEPROMに残るので戻しておく
}

/*
 * 英数レイヤーの記号。素のキーコードのままで、QMKの標準どおりに振る舞う
 */

// clang-format off
static const Key alpha_sym_keys[] = {
    {1, 10, KC_SCLN, PLAIN}, // ; :
    {1, 11, KC_QUOT, PLAIN}, // ' "
    {2, 8, KC_COMM, PLAIN},  // , <
    {2, 9, KC_DOT, PLAIN},   // . >
};
// clang-format on

TEST_F(KeyOutput, alpha_symbols_without_shift) {
    TestDriver driver;
    set_windmill_keymap();

    {
        InSequence s;
        for (const auto& k : alpha_sym_keys) {
            EXPECT_REPORT(driver, (k.keycode));
            EXPECT_EMPTY_REPORT(driver);
        }
    }
    tap_all(this, alpha_sym_keys);
    VERIFY_AND_CLEAR(driver);
}

/* Shift時の文字 (: " < >) は、押されているShiftがそのまま乗って出る。
 * 右の親指は右Shift (issue #88)。左Shift へ入れ替えるレポートが挟まると、
 * issue #18 と同じく実機で Shift が取りこぼされうるので、列ごと固定する */
TEST_F(KeyOutput, alpha_symbols_with_thumb_shift) {
    TestDriver driver;
    set_windmill_keymap();

    auto shift = key(POS_ALPHA_SHIFT);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        for (const auto& k : alpha_sym_keys) {
            EXPECT_REPORT(driver, (KC_RIGHT_SHIFT, k.keycode));
            EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        }
        EXPECT_EMPTY_REPORT(driver);
    }

    shift.press();
    run_one_scan_loop();
    idle_for(250); // TAPPING_TERM 超え。ホールド確定
    tap_all(this, alpha_sym_keys);
    shift.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

// clang-format off
static const Key alpha_letter_keys[] = {
    {0, 1, KC_Q, PLAIN}, {0, 2, KC_W, PLAIN}, {0, 3, KC_E, PLAIN}, {0, 4, KC_R, PLAIN}, {0, 5, KC_T, PLAIN},
    {0, 6, KC_Y, PLAIN}, {0, 7, KC_U, PLAIN}, {0, 8, KC_I, PLAIN}, {0, 9, KC_O, PLAIN}, {0, 10, KC_P, PLAIN},
    {1, 1, KC_A, PLAIN}, {1, 2, KC_S, PLAIN}, {1, 3, KC_D, PLAIN}, {1, 4, KC_F, PLAIN}, {1, 5, KC_G, PLAIN},
    {1, 6, KC_H, PLAIN}, {1, 7, KC_J, PLAIN}, {1, 8, KC_K, PLAIN}, {1, 9, KC_L, PLAIN},
    {2, 1, KC_Z, PLAIN}, {2, 2, KC_X, PLAIN}, {2, 3, KC_C, PLAIN}, {2, 4, KC_V, PLAIN}, {2, 5, KC_B, PLAIN},
    {2, 6, KC_N, PLAIN}, {2, 7, KC_M, PLAIN},
};
// clang-format on

// 英字も同じ。右の親指の右Shift がそのまま乗って大文字になる (issue #88)
TEST_F(KeyOutput, alpha_letters_with_thumb_shift) {
    TestDriver driver;
    set_windmill_keymap();

    auto shift = key(POS_ALPHA_SHIFT);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        for (const auto& k : alpha_letter_keys) {
            EXPECT_REPORT(driver, (KC_RIGHT_SHIFT, k.keycode));
            EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        }
        EXPECT_EMPTY_REPORT(driver);
    }

    shift.press();
    run_one_scan_loop();
    idle_for(250);
    tap_all(this, alpha_letter_keys);
    shift.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

// 英数レイヤーの記号は、押している間は押しっぱなし (QMKの標準。リピートも効く)
TEST_F(KeyOutput, alpha_symbol_stays_down_while_held) {
    TestDriver driver;
    set_windmill_keymap();

    auto scln = key(1, 10);

    EXPECT_REPORT(driver, (KC_SCLN));
    scln.press();
    run_one_scan_loop();
    idle_for(600);
    VERIFY_AND_CLEAR(driver);

    EXPECT_EMPTY_REPORT(driver);
    scln.release();
    run_one_scan_loop();
    idle_for(50);
    VERIFY_AND_CLEAR(driver);
}

/*
 * 記号レイヤー
 */

// clang-format off
static const Key sym_keys[] = {
    // 1 2 3 4 5 6 7 8 9 0
    {0, 1, KC_1, PLAIN}, {0, 2, KC_2, PLAIN}, {0, 3, KC_3, PLAIN}, {0, 4, KC_4, PLAIN}, {0, 5, KC_5, PLAIN},
    {0, 6, KC_6, PLAIN}, {0, 7, KC_7, PLAIN}, {0, 8, KC_8, PLAIN}, {0, 9, KC_9, PLAIN}, {0, 10, KC_0, PLAIN},
    // ! @ # $ % ^ & * ( ) `
    {1, 1, KC_1, SHIFT}, {1, 2, KC_2, SHIFT}, {1, 3, KC_3, SHIFT}, {1, 4, KC_4, SHIFT}, {1, 5, KC_5, SHIFT},
    {1, 6, KC_6, SHIFT}, {1, 7, KC_7, SHIFT}, {1, 8, KC_8, SHIFT}, {1, 9, KC_9, SHIFT}, {1, 10, KC_0, SHIFT},
    {1, 11, KC_GRV, PLAIN},
    // = + - _ [ ] ~ { }
    {2, 1, KC_EQL, PLAIN}, {2, 2, KC_EQL, SHIFT}, {2, 3, KC_MINS, PLAIN}, {2, 4, KC_MINS, SHIFT},
    {2, 5, KC_LBRC, PLAIN}, {2, 6, KC_RBRC, PLAIN}, {2, 7, KC_GRV, SHIFT}, {2, 8, KC_LBRC, SHIFT}, {2, 9, KC_RBRC, SHIFT},
    // | ?
    {3, 5, KC_BSLS, SHIFT}, {3, 6, KC_SLSH, SHIFT},
};
// clang-format on

/* 記号レイヤーの1文字ぶん。かなベースなら英数に切り替えて送り、かなへ戻す。
 * 押されている Shift は切り替えの前に外し、最後に戻す */
static void expect_sym(TestDriver& driver, const Key& k, uint8_t held, bool kana) {
    if (!kana) {
        expect_char(driver, k, held);
        return;
    }
    if (held) EXPECT_EMPTY_REPORT(driver);
    EXPECT_REPORT(driver, (KC_LNG2));
    EXPECT_EMPTY_REPORT(driver);
    expect_char(driver, k, 0);
    EXPECT_REPORT(driver, (KC_LNG1));
    EXPECT_EMPTY_REPORT(driver);
    if (held) EXPECT_REPORT(driver, (held));
}

// 親指Shift で押さえている位置。記号レイヤーでは「|」「?」だが、押したままなので叩けない
static bool is_held(const Key& k, uint8_t held_col) {
    return k.row == 3 && k.col == held_col;
}

// Sym をホールドしたまま sym_keys を順に叩く。held_col の位置は飛ばす
static void tap_sym_keys(WindmillTest* f, uint8_t held_col = 0) {
    auto sym = f->key(POS_SYM);

    sym.press();
    f->run_one_scan_loop();
    f->idle_for(250); // 英数は TAPPING_TERM、かなは 250ms を超えてホールド確定
    for (const auto& k : sym_keys) {
        if (is_held(k, held_col)) continue;
        f->tap_key(f->key(k.row, k.col), 50);
        f->idle_for(50);
    }
    sym.release();
    f->run_one_scan_loop();
    f->idle_for(120);
}

TEST_F(KeyOutput, sym_layer_on_alpha) {
    TestDriver driver;
    set_windmill_keymap();

    {
        InSequence s;
        for (const auto& k : sym_keys) expect_sym(driver, k, 0, false);
    }
    tap_sym_keys(this);
    VERIFY_AND_CLEAR(driver);
}

// かなベースでは、1打鍵ごとに英数へ切り替えてから送り、かなへ戻す
TEST_F(KeyOutput, sym_layer_on_kana_wraps_with_ime_switch) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    {
        InSequence s;
        for (const auto& k : sym_keys) expect_sym(driver, k, 0, true);
    }
    tap_sym_keys(this);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

/* 記号レイヤーでは Shift が効かない。英数ベースで親指Shift (SandS) を押していても
 * 同じ文字が出る。Shift の要らない数字などは Shift を外して送る */
TEST_F(KeyOutput, sym_layer_on_alpha_ignores_shift) {
    TestDriver driver;
    set_windmill_keymap();

    auto shift = key(POS_ALPHA_SHIFT);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        for (const auto& k : sym_keys) {
            if (!is_held(k, 6)) expect_sym(driver, k, KC_RIGHT_SHIFT, false);
        }
        EXPECT_EMPTY_REPORT(driver);
    }

    shift.press();
    run_one_scan_loop();
    idle_for(250);
    tap_sym_keys(this, 6);
    shift.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* かなベースで こ (左Shift) を押したまま ひ をホールドしても同じ。英数/かなの切り替えの
 * キーに Shift が乗らないよう、切り替えの前に外す */
TEST_F(KeyOutput, sym_layer_on_kana_ignores_shift) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto ko = key(POS_KO);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        for (const auto& k : sym_keys) {
            if (!is_held(k, 5)) expect_sym(driver, k, KC_LEFT_SHIFT, true);
        }
        EXPECT_EMPTY_REPORT(driver);
    }

    ko.press();
    run_one_scan_loop();
    idle_for(250);
    tap_sym_keys(this, 5);
    ko.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

/* 記号を続けて打つと、前のキーを離す前に次のキーを押すことがある。
 * 「!」(Shift+1) は押した時点で送り切っているので、「=」に Shift が漏れて「+」に
 * 化けることは無い */
TEST_F(KeyOutput, sym_roll_over_does_not_leak_shift) {
    TestDriver driver;
    set_windmill_keymap();

    auto sym  = key(POS_SYM);
    auto exlm = key(1, 1); // !
    auto eql  = key(2, 1); // =

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // ! 押下
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_1));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_EQL)); // = 押下。Shift は乗らない
        EXPECT_EMPTY_REPORT(driver);
    }

    sym.press();
    run_one_scan_loop();
    idle_for(250);
    exlm.press();
    run_one_scan_loop();
    idle_for(30);
    eql.press();
    run_one_scan_loop();
    idle_for(30);
    exlm.release();
    run_one_scan_loop();
    idle_for(30);
    eql.release();
    run_one_scan_loop();
    idle_for(30);
    sym.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* issue #87 の再現手順。かなを打ってから、ひ を押したまま こ を3回打つ。
 *
 * Windows 標準の IME は、かなを打った直後だと英数への切り替えが遅れ、Shift を押した
 * 直後のキーで Shift を取りこぼしていた (1打目だけ「|」ではなく「\」)。Shift の押下と
 * 記号キーの押下を別のレポートにし、間に IME_WAIT_MS を挟む。英数への切り替えと
 * Shift の押下の間、Shift の解放とかなへの切り替えの間も同じ */
TEST_F(KeyOutput, sym_after_kana_waits_between_shift_and_key) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto nu   = key(POS_NU);
    auto hi   = key(POS_HI);
    auto pipe = key(POS_KO); // 記号レイヤーでは「|」

    uint16_t at_lng2 = 0, at_shift = 0, at_key = 0, at_unshift = 0, at_lng1 = 0;
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_1)); // ぬ
        EXPECT_EMPTY_REPORT(driver);

        // 1打目。間隔を控える
        EXPECT_REPORT(driver, (KC_LNG2));
        EXPECT_EMPTY_REPORT(driver).WillOnce(InvokeWithoutArgs([&] { at_lng2 = timer_read(); }));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)).WillOnce(InvokeWithoutArgs([&] { at_shift = timer_read(); }));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_BSLS)).WillOnce(InvokeWithoutArgs([&] { at_key = timer_read(); }));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver).WillOnce(InvokeWithoutArgs([&] { at_unshift = timer_read(); }));
        EXPECT_REPORT(driver, (KC_LNG1)).WillOnce(InvokeWithoutArgs([&] { at_lng1 = timer_read(); }));
        EXPECT_EMPTY_REPORT(driver);

        // 2打目と3打目も同じレポート列
        for (int i = 0; i < 2; ++i) expect_sym(driver, {3, 5, KC_BSLS, SHIFT}, 0, true);
    }

    tap_key(nu, 50);
    idle_for(50);
    hi.press();
    run_one_scan_loop();
    idle_for(250); // ひ がホールドに決まる
    for (int i = 0; i < 3; ++i) {
        tap_key(pipe, 50);
        idle_for(50);
    }
    hi.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);

    EXPECT_GE(uint16_t(at_shift - at_lng2), uint16_t(IME_WAIT_MS));
    EXPECT_GE(uint16_t(at_key - at_shift), uint16_t(IME_WAIT_MS));
    EXPECT_GE(uint16_t(at_unshift - at_key), uint16_t(IME_WAIT_MS));
    EXPECT_GE(uint16_t(at_lng1 - at_unshift), uint16_t(IME_WAIT_MS));

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

/* かなのキーを押したまま Sym をホールドして、それからかなのキーを離す。
 * かなの「ぬ」は押した時点で送り切っているので、離したときに何も出ず、ホストに
 * 押しっぱなしのキーも残らない。
 *
 * 独自キーコードにする前は、かなの「ぬ」と記号レイヤーの「1」が同じ KC_1 で、
 * 記号レイヤーの数字の解放と見分けられずに握りつぶされ、「ぬ」が押されたまま
 * 残っていた */
TEST_F(KeyOutput, kana_key_released_under_sym_layer_is_not_stuck) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto nu  = key(POS_NU);
    auto sym = key(POS_SYM);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_1));
        EXPECT_EMPTY_REPORT(driver);
    }

    nu.press();
    run_one_scan_loop();
    idle_for(50);
    sym.press();
    run_one_scan_loop();
    idle_for(250); // 250ms 押し続けてホールド確定
    nu.release();
    run_one_scan_loop();
    idle_for(50);
    sym.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}
