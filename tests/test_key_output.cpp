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

/* 記号とかなのキーの出力を、位置ごとに総当たりで固定する (issue #73)。
 *
 * これらのキーは独自キーコードで、ホストへ何を送るかは windmill.c の出力表
 * (key_outputs[]) が決める。表の行を1つ書き漏らしても、そのキーが何も出さなく
 * なるだけでコンパイルは通る。だから「この位置を打ったらこのレポート」を
 * 全キーぶん並べておく。
 *
 * ここの期待値は、独自キーコードにする前のキーマップ (KC_1 や S(KC_1) を直接
 * 置いていたころ) の出力をそのまま写したもの。出力表を経由しても、QMKが素の
 * キーコードを処理するのと同じレポート列になることを見ている。押している間は
 * 押しっぱなしになること (キーリピートが効くこと) も含む。
 *
 * ここは接続先の配列が US のときの期待値。JIS のときは test_key_output_jis.cpp
 * (issue #75)。 */

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_layer.h"
#include "action_util.h"
#include "test_keymap.hpp"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;

class KeyOutput : public WindmillUsTest {};

/* 対象OSはEEPROMに残るので、Android向けのテストはスイートを分ける
 * (test_android_mods.cpp と同じ) */
class KeyOutputAndroid : public WindmillUsTest {};

// 英数から MY_LCTL 1回タップでかなへ切り替える。レポートの中身は問わない
static void switch_to_kana(WindmillTest* f, TestDriver& driver) {
    EXPECT_ANY_REPORT(driver).Times(AnyNumber());

    f->tap_key(f->key(POS_LCTL), 120);
    f->settle();

    VERIFY_AND_CLEAR(driver);
}

#define POS_SYM 3, 4 // ホールドで記号レイヤー。英数では LT(2,KC_BSLS)、かなでは LT(2,KC_V) (ひ)
#define POS_ALPHA_SHIFT 3, 6 // 英数レイヤーの親指Shift。LSFT_T(KC_SPC)

/*
 * かなレイヤー
 */

// Shiftを押しながら打ったときの出し方
enum ShiftKind {
    AS_IS, // 出し分けない。押されているShiftごとそのまま出る
    KEEP,  // 別のキーを、押されているShiftを使って出す
    DROP,  // 別のキーを、Shiftを外して出す
};

struct KanaKey {
    uint8_t   row;
    uint8_t   col;
    uint8_t   plain;   // Shift無しで出るキー
    ShiftKind kind;
    uint8_t   shifted; // Shift時に出るキー (AS_IS なら plain と同じ)
};

// clang-format off
static const KanaKey kana_keys[] = {
    // ぬ ふ あ う え お や ゆ よ わ
    {0, 1, KC_1, AS_IS, KC_1}, {0, 2, KC_2, AS_IS, KC_2}, {0, 3, KC_3, AS_IS, KC_3}, {0, 4, KC_4, AS_IS, KC_4},
    {0, 5, KC_5, AS_IS, KC_5}, {0, 6, KC_6, AS_IS, KC_6}, {0, 7, KC_7, AS_IS, KC_7}, {0, 8, KC_8, AS_IS, KC_8},
    {0, 9, KC_9, AS_IS, KC_9}, {0, 10, KC_0, AS_IS, KC_0},
    // た て い す か ん な に ら せ ゛
    {1, 1, KC_Q, AS_IS, KC_Q},
    {1, 2, KC_W, KEEP, KC_EQL},     // へ
    {1, 3, KC_E, AS_IS, KC_E},
    {1, 4, KC_R, DROP, KC_BSLS},    // む
    {1, 5, KC_T, AS_IS, KC_T},
    {1, 6, KC_Y, AS_IS, KC_Y},
    {1, 7, KC_U, DROP, KC_MINS},    // ほ
    {1, 8, KC_I, AS_IS, KC_I},
    {1, 9, KC_O, KEEP, KC_LBRC},    // 「 (Windows)
    {1, 10, KC_P, KEEP, KC_RBRC},   // 」 (Windows)
    {1, 11, KC_LBRC, DROP, KC_RBRC}, // ゜
    // ち と し は き く ま の り れ け
    {2, 1, KC_A, KEEP, KC_Z},       // っ
    {2, 2, KC_S, AS_IS, KC_S}, {2, 3, KC_D, AS_IS, KC_D}, {2, 4, KC_F, AS_IS, KC_F}, {2, 5, KC_G, AS_IS, KC_G},
    {2, 6, KC_H, AS_IS, KC_H}, {2, 7, KC_J, AS_IS, KC_J},
    {2, 8, KC_K, KEEP, KC_COMM},    // 、
    {2, 9, KC_L, KEEP, KC_DOT},     // 。
    {2, 10, KC_SCLN, KEEP, KC_SLSH}, // ・
    {2, 11, KC_QUOT, KEEP, KC_MINS}, // ー
    // る め ろ
    {3, 9, KC_DOT, AS_IS, KC_DOT}, {3, 10, KC_SLSH, AS_IS, KC_SLSH}, {3, 11, KC_GRV, AS_IS, KC_GRV},
};

// 対象OSが Android のときだけ変わるもの。「」の出し方がIMEで違う
static const KanaKey kana_keys_android[] = {
    {1, 9, KC_O, KEEP, KC_RBRC},    // 「
    {1, 10, KC_P, KEEP, KC_BSLS},   // 」
};
// clang-format on

// Shift無しで順に叩く
template <size_t N>
static void expect_kana_plain(WindmillTest* f, TestDriver& driver, const KanaKey (&keys)[N]) {
    {
        InSequence s;
        for (const auto& k : keys) {
            EXPECT_REPORT(driver, (k.plain));
            EXPECT_EMPTY_REPORT(driver);
        }
    }

    for (const auto& k : keys) {
        f->tap_key(f->key(k.row, k.col), 50);
        f->idle_for(50);
    }
    VERIFY_AND_CLEAR(driver);
}

// 右の親指Shift (み) をホールドしたまま順に叩く
template <size_t N>
static void expect_kana_shifted(WindmillTest* f, TestDriver& driver, const KanaKey (&keys)[N]) {
    auto mi = f->key(POS_MI);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // み ホールド確定
        for (const auto& k : keys) {
            if (k.kind == DROP) {
                EXPECT_EMPTY_REPORT(driver); // Shiftを外す
                EXPECT_REPORT(driver, (k.shifted));
                EXPECT_EMPTY_REPORT(driver);
                EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // Shiftを戻す
            } else {
                EXPECT_REPORT(driver, (KC_LEFT_SHIFT, k.shifted));
                EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
            }
        }
        EXPECT_EMPTY_REPORT(driver); // み 解放
    }

    mi.press();
    f->run_one_scan_loop();
    f->idle_for(250); // TAPPING_TERM 超え。ホールド確定
    for (const auto& k : keys) {
        f->tap_key(f->key(k.row, k.col), 50);
        f->idle_for(50);
    }
    mi.release();
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

TEST_F(KeyOutput, kana_keys_with_thumb_shift) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    expect_kana_shifted(this, driver, kana_keys);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

/* 出し分けないキーは、押している間は押しっぱなし。離すまでレポートが空に
 * ならない (ホスト側のキーリピートが効く)。重ねて押せば両方が載る */
TEST_F(KeyOutput, kana_key_stays_down_while_held) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto nu = key(POS_NU);
    auto ha = key(POS_HA);

    EXPECT_REPORT(driver, (KC_1));
    nu.press();
    run_one_scan_loop();
    idle_for(600); // リピートが始まる程度に押し続ける
    VERIFY_AND_CLEAR(driver);

    EXPECT_REPORT(driver, (KC_1, KC_F));
    ha.press();
    run_one_scan_loop();
    idle_for(50);
    VERIFY_AND_CLEAR(driver);

    EXPECT_REPORT(driver, (KC_F));
    nu.release();
    run_one_scan_loop();
    idle_for(50);
    VERIFY_AND_CLEAR(driver);

    EXPECT_EMPTY_REPORT(driver);
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
    expect_kana_shifted(this, driver, kana_keys_android);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
    select_os(this, driver, POS_WIN); // EEPROMに残るので戻しておく
}

/*
 * 英数レイヤーの記号
 */

struct Key {
    uint8_t row;
    uint8_t col;
    uint8_t keycode;
    bool    shift; // Shift付きで送るキーか (S(KC_x))
};

// clang-format off
static const Key alpha_sym_keys[] = {
    {1, 10, KC_SCLN, false}, // ; :
    {1, 11, KC_QUOT, false}, // ' "
    {2, 8, KC_COMM, false},  // , <
    {2, 9, KC_DOT, false},   // . >
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

    for (const auto& k : alpha_sym_keys) {
        tap_key(key(k.row, k.col), 50);
        idle_for(50);
    }
    VERIFY_AND_CLEAR(driver);
}

// Shift時の文字 (: " < >) は、押されているShiftがそのまま乗って出る
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
    for (const auto& k : alpha_sym_keys) {
        tap_key(key(k.row, k.col), 50);
        idle_for(50);
    }
    shift.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/*
 * 記号レイヤー
 */

// clang-format off
static const Key sym_keys[] = {
    // 数字は素のキーコードのまま。かなベースでのIMEの切り替えは記号と同じなので並べておく
    {0, 1, KC_1, false}, {0, 2, KC_2, false}, {0, 3, KC_3, false}, {0, 4, KC_4, false}, {0, 5, KC_5, false},
    {0, 6, KC_6, false}, {0, 7, KC_7, false}, {0, 8, KC_8, false}, {0, 9, KC_9, false}, {0, 10, KC_0, false},
    // ! @ # $ % ^ & * ( ) `
    {1, 1, KC_1, true}, {1, 2, KC_2, true}, {1, 3, KC_3, true}, {1, 4, KC_4, true}, {1, 5, KC_5, true},
    {1, 6, KC_6, true}, {1, 7, KC_7, true}, {1, 8, KC_8, true}, {1, 9, KC_9, true}, {1, 10, KC_0, true},
    {1, 11, KC_GRV, false},
    // = + - _ [ ] ~ { }
    {2, 1, KC_EQL, false}, {2, 2, KC_EQL, true}, {2, 3, KC_MINS, false}, {2, 4, KC_MINS, true},
    {2, 5, KC_LBRC, false}, {2, 6, KC_RBRC, false}, {2, 7, KC_GRV, true}, {2, 8, KC_LBRC, true}, {2, 9, KC_RBRC, true},
    // | ?
    {3, 5, KC_BSLS, true}, {3, 6, KC_SLSH, true},
};
// clang-format on

// キー1つぶんの押して離すレポート。Shift付きのキーは修飾の上げ下げが前後に1本ずつ付く
static void expect_tap(TestDriver& driver, const Key& k) {
    if (k.shift) {
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, k.keycode));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    } else {
        EXPECT_REPORT(driver, (k.keycode));
        EXPECT_EMPTY_REPORT(driver);
    }
}

// Sym をホールドしたまま sym_keys を順に叩く
static void tap_sym_keys(WindmillTest* f) {
    auto sym = f->key(POS_SYM);

    sym.press();
    f->run_one_scan_loop();
    f->idle_for(250); // TAPPING_TERM 超え。ホールド確定
    for (const auto& k : sym_keys) {
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
        for (const auto& k : sym_keys) {
            expect_tap(driver, k);
        }
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
        for (const auto& k : sym_keys) {
            EXPECT_REPORT(driver, (KC_LNG2));
            EXPECT_EMPTY_REPORT(driver);
            expect_tap(driver, k);
            EXPECT_REPORT(driver, (KC_LNG1));
            EXPECT_EMPTY_REPORT(driver);
        }
    }

    tap_sym_keys(this);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

/* 記号を続けて打つと、前のキーを離す前に次のキーを押すことがある。
 * 「!」(Shift+1) を押したまま「=」を押しても、「=」にShiftが乗って「+」に
 * 化けてはいけない。QMKは素のキーコードなら押下のたびに前のキーの weak Shift を
 * 落とすので、出力表を経由するキーも同じにしておく */
TEST_F(KeyOutput, sym_roll_over_does_not_leak_shift) {
    TestDriver driver;
    set_windmill_keymap();

    auto sym  = key(POS_SYM);
    auto exlm = key(1, 1); // !
    auto eql  = key(2, 1); // =

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_1)); // ! 押下
        EXPECT_REPORT(driver, (KC_1, KC_EQL));        // = 押下。Shiftが落ちている
        EXPECT_REPORT(driver, (KC_EQL));              // ! 解放
        EXPECT_EMPTY_REPORT(driver);                  // = 解放
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

/* かなのキーを押したまま Sym をホールドして、それからかなのキーを離す。
 * かなの「ぬ」と記号レイヤーの「1」は別のキーなので、離したぶんは「ぬ」の解放と
 * して届く。
 *
 * 独自キーコードにする前はどちらも同じ KC_1 だったので、記号レイヤーの数字の
 * 解放と見分けられず、IMEの切り替えの後始末として握りつぶされていた。ホストには
 * 「ぬ」が押されたまま残っていた */
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
    idle_for(250); // TAPPING_TERM 超え。ホールド確定
    nu.release();
    run_one_scan_loop();
    idle_for(50);
    sym.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}
