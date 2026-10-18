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


/* 記号とかなのキーの出力を、位置ごとに総当たりで固定する (issue #73、issue #79)。
 *
 * これらのキーは独自キーコードで、ホストへ何を送るかは windmill.c の出力表
 * (key_outputs[]) が決める。表の行を1つ書き漏らしても、そのキーが何も出さなく
 * なるだけでコンパイルは通る。だから「この位置を打ったらこのレポート」を
 * 全キーぶん並べておく。
 *
 * 出力は1キー1つで、押した瞬間に1回だけ送る。押しっぱなしにしてもリピートしない。
 * 押されている Shift は打ち消し、出力に Shift が要るときだけ足す (send_output)。
 *
 * 出力表の基準は JIS だが、配列の切り替えはまだ無効で、常に US の値を引く
 * (issue #75 で有効にする)。ここの期待値は US 配列のもので、独自キーコードにする前の
 * キーマップ (KC_1 や S(KC_1) を直接置いていたころ) と同じ文字になる。 */

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_layer.h"
#include "action_util.h"
#include "test_keymap.hpp"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;

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

#define POS_SYM 3, 4 // ホールドで記号レイヤー。英数では LT(LAYER_SYM,KC_BSLS)、かなでは ひ
#define POS_ALPHA_SHIFT 3, 6 // 英数レイヤーの親指Shift。LSFT_T(KC_SPC)

// 押されている Shift に対しての出し方
enum ShiftKind {
    PLAIN, // Shift の要らないキー。Shift を押していたら外して送る
    SHIFT, // Shift の要るキー。Shift を押していたらそのまま、無ければ足して送る
};

struct Key {
    uint8_t   row;
    uint8_t   col;
    uint8_t   keycode;
    ShiftKind kind;
};

// Shift を押していないときの、キー1つぶんのレポート
static void expect_unshifted(TestDriver& driver, const Key& k) {
    if (k.kind == SHIFT) {
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, k.keycode));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    } else {
        EXPECT_REPORT(driver, (k.keycode));
        EXPECT_EMPTY_REPORT(driver);
    }
}

// 左Shift を押しているときの、キー1つぶんのレポート。前後は Shift だけが押されている
static void expect_shifted(TestDriver& driver, const Key& k) {
    if (k.kind == SHIFT) {
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, k.keycode));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
    } else {
        EXPECT_EMPTY_REPORT(driver); // Shiftを外す
        EXPECT_REPORT(driver, (k.keycode));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // Shiftを戻す
    }
}

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
    // つ さ そ ひ こ み も ね る め ろ。最下段の8つは、他のキーを押さずに離せばタップ
    {3, 1, KC_Z, PLAIN}, {3, 2, KC_X, PLAIN}, {3, 3, KC_C, PLAIN}, {3, 4, KC_V, PLAIN}, {3, 5, KC_B, PLAIN},
    {3, 6, KC_N, PLAIN}, {3, 7, KC_M, PLAIN}, {3, 8, KC_COMM, PLAIN}, {3, 9, KC_DOT, PLAIN}, {3, 10, KC_SLSH, PLAIN},
    {3, 11, KC_GRV, PLAIN},
};

/* 右の親指Shift (み) をホールドしているときの、Shift 面の出力。み 自身の位置と、
 * 半角スペースの こ、IMEを切り替える「?」の も は別のテストで見る
 * (test_thumb_shift.cpp、test_kana_qmark.cpp)。Shift 面で透過のキーは、下のかなを
 * Shift を外して出す */
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
    {2, 8, KC_COMM, SHIFT}, {2, 9, KC_DOT, SHIFT}, {2, 10, KC_SLSH, SHIFT}, {2, 11, KC_MINS, SHIFT},
    // っ さ そ ひ 、 。 ・ ろ
    {3, 1, KC_Z, SHIFT},
    {3, 2, KC_X, PLAIN}, {3, 3, KC_C, PLAIN}, {3, 4, KC_V, PLAIN},
    {3, 8, KC_COMM, SHIFT}, {3, 9, KC_DOT, SHIFT}, {3, 10, KC_SLSH, SHIFT},
    {3, 11, KC_GRV, PLAIN},
};

// 対象OSが Android のときだけ変わるもの。「」の出し方がIMEで違う
static const Key kana_shift_keys_android[] = {
    {1, 9, KC_RBRC, SHIFT},  // 「
    {1, 10, KC_BSLS, SHIFT}, // 」
};
// clang-format on

// 右の親指Shift (み) をホールドしたまま順に叩く
template <size_t N>
static void expect_kana_shifted(WindmillTest* f, TestDriver& driver, const Key (&keys)[N]) {
    auto mi = f->key(POS_MI);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // 1つ目のキーの押下で み のホールド確定
        for (const auto& k : keys) {
            expect_shifted(driver, k);
        }
        EXPECT_EMPTY_REPORT(driver); // み 解放
    }

    mi.press();
    f->run_one_scan_loop();
    f->idle_for(250); // KANA_HOLD_TERM 超え
    tap_all(f, keys);
    mi.release();
    f->run_one_scan_loop();
    f->idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

TEST_F(KeyOutput, kana_keys_without_shift) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    {
        InSequence s;
        for (const auto& k : kana_keys) {
            expect_unshifted(driver, k);
        }
    }
    tap_all(this, kana_keys);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA); // 次のテストへかなを持ち越さない
}

TEST_F(KeyOutput, kana_shift_face) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    expect_kana_shifted(this, driver, kana_shift_keys);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

/* 押した瞬間に送り切る。押し続けてもリピートせず、離しても何も出ない。
 * 押したまま別のキーを押せば、そちらも単独で出る */
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
        EXPECT_REPORT(driver, (KC_F));
        EXPECT_EMPTY_REPORT(driver);
    }

    nu.press();
    run_one_scan_loop();
    idle_for(600); // リピートが始まる程度に押し続ける
    ha.press();
    run_one_scan_loop();
    idle_for(50);
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

    expect_kana_shifted(this, driver, kana_shift_keys_android);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
    select_os(this, driver, POS_WIN); // EEPROMに残るので戻しておく
}

/*
 * 英数レイヤーの記号
 */

/* 英数レイヤーは素のキーコードのままで、Shift もリピートも QMK に任せる (issue #79)。
 * Shift時の文字 (: " < >) は、押されているShiftがそのまま乗って出る */

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
            expect_unshifted(driver, k);
        }
    }
    tap_all(this, alpha_sym_keys);
    VERIFY_AND_CLEAR(driver);
}

TEST_F(KeyOutput, alpha_symbols_with_thumb_shift) {
    TestDriver driver;
    set_windmill_keymap();

    auto shift = key(POS_ALPHA_SHIFT);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        for (const auto& k : alpha_sym_keys) {
            EXPECT_REPORT(driver, (KC_LEFT_SHIFT, k.keycode));
            EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
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

// 英数レイヤーのキーは押している間は押しっぱなし (ホスト側のキーリピートが効く)
TEST_F(KeyOutput, alpha_key_stays_down_while_held) {
    TestDriver driver;
    set_windmill_keymap();

    auto dot = key(2, 9);

    EXPECT_REPORT(driver, (KC_DOT));
    dot.press();
    run_one_scan_loop();
    idle_for(600);
    VERIFY_AND_CLEAR(driver);

    EXPECT_EMPTY_REPORT(driver);
    dot.release();
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

// 最下段の | ? は親指Shiftと同じ位置なので、Shift と重ねるテストでは打てない
static constexpr size_t SYM_KEYS_ABOVE_THUMBS = (sizeof(sym_keys) / sizeof(sym_keys[0])) - 2;

// Sym をホールドしたまま sym_keys の先頭 count 個を順に叩く
static void tap_sym_keys(WindmillTest* f, size_t count = (sizeof(sym_keys) / sizeof(sym_keys[0]))) {
    auto sym = f->key(POS_SYM);

    sym.press();
    f->run_one_scan_loop();
    f->idle_for(250); // TAPPING_TERM / KANA_HOLD_TERM 超え。1つ目のキーでホールド確定
    for (size_t i = 0; i < count; ++i) {
        f->tap_key(f->key(sym_keys[i].row, sym_keys[i].col), 50);
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
        for (const auto& k : sym_keys) {
            expect_unshifted(driver, k);
        }
    }

    tap_sym_keys(this);
    VERIFY_AND_CLEAR(driver);
}

/* 記号レイヤーでは Shift が効かない (issue #79)。Shift を押していても同じ文字が出る。
 * Shift の要らない文字は Shift を外して送る */
TEST_F(KeyOutput, sym_layer_ignores_shift) {
    TestDriver driver;
    set_windmill_keymap();

    auto shift = key(POS_ALPHA_SHIFT);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // 英数の親指Shift
        for (size_t i = 0; i < SYM_KEYS_ABOVE_THUMBS; ++i) {
            expect_shifted(driver, sym_keys[i]);
        }
        EXPECT_EMPTY_REPORT(driver); // 親指Shift 解放
    }

    shift.press();
    run_one_scan_loop();
    idle_for(250);
    tap_sym_keys(this, SYM_KEYS_ABOVE_THUMBS);
    shift.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

// かなベースでは、1打鍵ごとに英数へ切り替えてから送り、かなへ戻す
TEST_F(KeyOutput, sym_layer_on_kana_wraps_with_ime_switch) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    {
        InSequence s;
        for (const auto& k : sym_keys) {
            EXPECT_REPORT(driver, (KC_LNG2));
            EXPECT_EMPTY_REPORT(driver);
            expect_unshifted(driver, k);
            EXPECT_REPORT(driver, (KC_LNG1));
            EXPECT_EMPTY_REPORT(driver);
        }
    }

    tap_sym_keys(this);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

/* かなベースで Shift と Sym を重ねると、英数/かなの切り替えの間は Shift を外す。
 * KC_LNG2 / KC_LNG1 に Shift が乗るとIMEがモードを切り替えない。Shift の要る文字は
 * 外した実Shiftを戻して使う */
TEST_F(KeyOutput, sym_layer_on_kana_with_shift_drops_shift_for_ime_switch) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto mi   = key(POS_MI);
    auto sym  = key(POS_SYM); // ひ。Shift 面でも透過なので Sym になる
    auto one  = key(0, 1);    // 1
    auto exlm = key(1, 1);    // !

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // み のホールド確定
        // 1
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LNG2));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_1));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LNG1));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        // !
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LNG2));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_1));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LNG1));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver); // み 解放
    }

    mi.press();
    run_one_scan_loop();
    idle_for(250);
    sym.press(); // み のホールド確定
    run_one_scan_loop();
    idle_for(250);
    tap_key(one, 50); // ひ のホールド確定
    idle_for(50);
    tap_key(exlm, 50);
    idle_for(50);
    sym.release();
    run_one_scan_loop();
    idle_for(50);
    mi.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

/* 記号を続けて打つと、前のキーを離す前に次のキーを押すことがある。
 * 「!」(Shift+1) を押したまま「=」を押しても、「=」にShiftが乗って「+」に
 * 化けてはいけない。どちらも押下で送り切るので、前のキーの Shift は残らない */
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
        EXPECT_REPORT(driver, (KC_EQL)); // = 押下。Shiftは乗らない
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
