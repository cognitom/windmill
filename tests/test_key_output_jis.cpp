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

/* 接続先の配列が JIS のときの出力を、位置ごとに総当たりで固定する (issue #75)。
 * US のときは test_key_output.cpp。
 *
 * 期待値は「US のときと同じ文字が JIS 配列で出るキー」。出力表 (key_outputs[]) の
 * JIS の列を写したのではなく、JIS 配列の刻印から引いて書いてある。キーコードの
 * 名前は US 配列での刻印なので、JIS 配列で何のキーかをここにまとめておく。
 *
 *   KC_MINS  - =  (ほ)      KC_EQL   ^ ~  (へ)      KC_INT3  ¥ |  (ー)
 *   KC_LBRC  @ `  (゛)      KC_RBRC  [ {  (゜ 「)
 *   KC_SCLN  ; +  (れ)      KC_QUOT  : *  (け)      KC_NUHS  ] }  (む 」)
 *   KC_INT1  \ _  (ろ)
 *   数字の Shift 側は  ! " # $ % & ' ( )  で、0 には無い
 *
 * US と Shift の有無が逆になる文字は、Shift を付け外しして送る。
 * - Shift 無し → Shift 付き (' = など): キーの前後に Shift の上げ下げが1本ずつ付く
 * - Shift 付き → Shift 無し (@ ^ :): 押されている Shift を外して送り、戻す
 *
 * 配列の既定は JIS なので、ここのスイートは配列を選ばずにそのまま走らせる。 */

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

class KeyOutputJis : public WindmillTest {
   public:
    void set_windmill_keymap() {
        WindmillTest::set_windmill_keymap();
        ASSERT_EQ(windmill_host_layout(), WINDMILL_LAYOUT_JIS);
    }
};

/* 対象OSはEEPROMに残るので、Android向けのテストはスイートを分ける
 * (test_android_mods.cpp と同じ) */
class KeyOutputJisAndroid : public KeyOutputJis {};

// 英数から MY_LCTL 1回タップでかなへ切り替える。レポートの中身は問わない
static void switch_to_kana(WindmillTest* f, TestDriver& driver) {
    EXPECT_ANY_REPORT(driver).Times(AnyNumber());

    f->tap_key(f->key(POS_LCTL), 120);
    f->settle();

    VERIFY_AND_CLEAR(driver);
}

#define POS_SYM 3, 4         // ホールドで記号レイヤー。英数では LT(2,KC_BSLS)、かなでは LT(2,KC_V) (ひ)
#define POS_ALPHA_SHIFT 3, 6 // 英数レイヤーの親指Shift。LSFT_T(KC_SPC)

// ホストへ送るキー。shift は Shift 付きで送るか
struct Out {
    uint8_t keycode;
    bool    shift;
};

struct JisKey {
    uint8_t row;
    uint8_t col;
    Out     plain;   // Shift無しで打ったとき
    Out     shifted; // Shiftを押しながら打ったとき
};

// Shift無しで1回叩いたときのレポート。Shift付きのキーは修飾の上げ下げが前後に1本ずつ付く
static void expect_plain_tap(TestDriver& driver, const Out& o) {
    if (o.shift) {
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, o.keycode));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    } else {
        EXPECT_REPORT(driver, (o.keycode));
        EXPECT_EMPTY_REPORT(driver);
    }
}

/* Shiftを押したまま1回叩いたときのレポート。Shift付きのキーは押されている Shift を
 * そのまま使う。Shift無しのキーは、Shift を外して送り、戻す */
static void expect_shifted_tap(TestDriver& driver, const Out& o) {
    if (o.shift) {
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, o.keycode));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
    } else {
        EXPECT_EMPTY_REPORT(driver); // Shiftを外す
        EXPECT_REPORT(driver, (o.keycode));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // Shiftを戻す
    }
}

// Shift無しで順に叩く
template <size_t N>
static void expect_plain(WindmillTest* f, TestDriver& driver, const JisKey (&keys)[N]) {
    {
        InSequence s;
        for (const auto& k : keys) {
            expect_plain_tap(driver, k.plain);
        }
    }

    for (const auto& k : keys) {
        f->tap_key(f->key(k.row, k.col), 50);
        f->idle_for(50);
    }
    VERIFY_AND_CLEAR(driver);
}

// 親指Shift (shift_row, shift_col) をホールドしたまま順に叩く
template <size_t N>
static void expect_shifted(WindmillTest* f, TestDriver& driver, const JisKey (&keys)[N], uint8_t shift_row, uint8_t shift_col) {
    auto shift = f->key(shift_row, shift_col);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // ホールド確定
        for (const auto& k : keys) {
            expect_shifted_tap(driver, k.shifted);
        }
        EXPECT_EMPTY_REPORT(driver); // 解放
    }

    shift.press();
    f->run_one_scan_loop();
    f->idle_for(250); // TAPPING_TERM 超え。ホールド確定
    for (const auto& k : keys) {
        f->tap_key(f->key(k.row, k.col), 50);
        f->idle_for(50);
    }
    shift.release();
    f->run_one_scan_loop();
    f->idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/*
 * かなレイヤー
 */

// clang-format off
/* JISかな配列の刻印どおり。US では別のキーのShiftへ逃がされている「へ」「ー」が
 * 単独のキーになり、「む」「ろ」「」の位置も変わる。
 * Shift時に出し分けないかなは、押されている Shift ごとそのまま出る (ぁ、を など) */
static const JisKey jis_kana_keys[] = {
    // ぬ ふ あ う え お や ゆ よ わ
    {0, 1, {KC_1, false}, {KC_1, true}}, {0, 2, {KC_2, false}, {KC_2, true}}, {0, 3, {KC_3, false}, {KC_3, true}},
    {0, 4, {KC_4, false}, {KC_4, true}}, {0, 5, {KC_5, false}, {KC_5, true}}, {0, 6, {KC_6, false}, {KC_6, true}},
    {0, 7, {KC_7, false}, {KC_7, true}}, {0, 8, {KC_8, false}, {KC_8, true}}, {0, 9, {KC_9, false}, {KC_9, true}},
    {0, 10, {KC_0, false}, {KC_0, true}},
    // た て い す か ん な に ら せ ゛
    {1, 1, {KC_Q, false}, {KC_Q, true}},
    {1, 2, {KC_W, false}, {KC_EQL, false}},     // へ  (US は Shift 付き)
    {1, 3, {KC_E, false}, {KC_E, true}},
    {1, 4, {KC_R, false}, {KC_NUHS, false}},    // む  (US は KC_BSLS)
    {1, 5, {KC_T, false}, {KC_T, true}},
    {1, 6, {KC_Y, false}, {KC_Y, true}},
    {1, 7, {KC_U, false}, {KC_MINS, false}},    // ほ
    {1, 8, {KC_I, false}, {KC_I, true}},
    {1, 9, {KC_O, false}, {KC_RBRC, true}},     // 「  (US は KC_LBRC)
    {1, 10, {KC_P, false}, {KC_NUHS, true}},    // 」  (US は KC_RBRC)
    {1, 11, {KC_LBRC, false}, {KC_RBRC, false}}, // ゛ ゜
    // ち と し は き く ま の り れ け
    {2, 1, {KC_A, false}, {KC_Z, true}},        // っ
    {2, 2, {KC_S, false}, {KC_S, true}}, {2, 3, {KC_D, false}, {KC_D, true}}, {2, 4, {KC_F, false}, {KC_F, true}},
    {2, 5, {KC_G, false}, {KC_G, true}}, {2, 6, {KC_H, false}, {KC_H, true}}, {2, 7, {KC_J, false}, {KC_J, true}},
    {2, 8, {KC_K, false}, {KC_COMM, true}},     // 、
    {2, 9, {KC_L, false}, {KC_DOT, true}},      // 。
    {2, 10, {KC_SCLN, false}, {KC_SLSH, true}}, // ・
    {2, 11, {KC_QUOT, false}, {KC_INT3, false}}, // ー  (US は Shift+KC_MINS)
    // る め ろ
    {3, 9, {KC_DOT, false}, {KC_DOT, true}}, {3, 10, {KC_SLSH, false}, {KC_SLSH, true}},
    {3, 11, {KC_INT1, false}, {KC_INT1, true}}, // ろ  (US は KC_GRV。JIS では半角/全角になる)
};

/* 対象OSが Android のとき。US では「」の位置が Windows と違うが、その位置は
 * JISかな配列の刻印どおりなので、JIS では Windows と同じになる */
static const JisKey jis_kana_keys_android[] = {
    {1, 9, {KC_O, false}, {KC_RBRC, true}},     // 「
    {1, 10, {KC_P, false}, {KC_NUHS, true}},    // 」
    {1, 2, {KC_W, false}, {KC_EQL, false}},     // へ
    {1, 4, {KC_R, false}, {KC_NUHS, false}},    // む
    {2, 11, {KC_QUOT, false}, {KC_INT3, false}}, // ー
    {3, 11, {KC_INT1, false}, {KC_INT1, true}}, // ろ
};
// clang-format on

TEST_F(KeyOutputJis, kana_keys_without_shift) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    expect_plain(this, driver, jis_kana_keys);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA); // 次のテストへかなを持ち越さない
}

TEST_F(KeyOutputJis, kana_keys_with_thumb_shift) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    expect_shifted(this, driver, jis_kana_keys, POS_MI);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

TEST_F(KeyOutputJisAndroid, kana_keys_are_same_as_windows) {
    TestDriver driver;
    set_windmill_keymap();

    EXPECT_NO_REPORT(driver);
    tap_on_conf(POS_ANDR);
    settle();
    VERIFY_AND_CLEAR(driver);

    switch_to_kana(this, driver);

    expect_plain(this, driver, jis_kana_keys_android);
    expect_shifted(this, driver, jis_kana_keys_android, POS_MI);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

/* 親指Shiftのホールドが確定した直後の1打鍵目。US では Shift 付きだった「ー」は、
 * JIS では Shift を外して送る側になった。ホールド確定 → Shift外し → キー送出 が
 * 同じスキャンに固まると、IMEが Shift の上げ下げを追い切れない (issue #36)。
 * レポート列に加えて送出の間隔も見る */
TEST_F(KeyOutputJis, kana_unshifted_pair_waits_for_ime_on_first_keypress) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto mi = key(POS_MI);
    auto ke = key(2, 11); // け。Shift時 ー
    auto te = key(1, 2);  // て。Shift時 へ

    uint16_t at_hold = 0, at_drop = 0, at_key = 0;

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)).WillOnce(InvokeWithoutArgs([&] { at_hold = timer_read(); }));
        EXPECT_EMPTY_REPORT(driver).WillOnce(InvokeWithoutArgs([&] { at_drop = timer_read(); }));
        EXPECT_REPORT(driver, (KC_INT3)).WillOnce(InvokeWithoutArgs([&] { at_key = timer_read(); }));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // Shiftを戻す
        EXPECT_EMPTY_REPORT(driver);            // へ
        EXPECT_REPORT(driver, (KC_EQL));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver); // み 解放
    }

    mi.press();
    run_one_scan_loop();
    idle_for(150); // TAPPING_TERM 未満。ホールドは け の押下で確定する
    tap_key(ke, 120);
    idle_for(120);
    tap_key(te, 120);
    idle_for(120);
    mi.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);

    EXPECT_GE(uint16_t(at_drop - at_hold), uint16_t(IME_WAIT_MS));
    EXPECT_GE(uint16_t(at_key - at_drop), uint16_t(IME_WAIT_MS));

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

/*
 * 英数レイヤーの記号
 */

// clang-format off
static const JisKey jis_alpha_sym_keys[] = {
    {1, 10, {KC_SCLN, false}, {KC_QUOT, false}}, // ; :   「:」は単独のキー
    {1, 11, {KC_7, true}, {KC_2, true}},         // ' "   どちらも数字の Shift 側
    {2, 8, {KC_COMM, false}, {KC_COMM, true}},   // , <
    {2, 9, {KC_DOT, false}, {KC_DOT, true}},     // . >
    // LT() のタップ側。独自キーコードを置けないので、JIS のときだけ横取りしている
    {3, 4, {KC_INT1, false}, {KC_INT3, true}},   // \ |   「\」は ろ のキー、「|」は ¥ の Shift
    {3, 7, {KC_SLSH, false}, {KC_SLSH, true}},   // / ?
};
// clang-format on

TEST_F(KeyOutputJis, alpha_symbols_without_shift) {
    TestDriver driver;
    set_windmill_keymap();

    expect_plain(this, driver, jis_alpha_sym_keys);
}

TEST_F(KeyOutputJis, alpha_symbols_with_thumb_shift) {
    TestDriver driver;
    set_windmill_keymap();

    expect_shifted(this, driver, jis_alpha_sym_keys, POS_ALPHA_SHIFT);
}

/* 親指Shiftのホールドが確定した直後の1打鍵目に「:」を打つ。Shift を外して送る
 * 経路なので、かなの process_shift_pair() と同じウェイトが要る (issue #36)。
 * 続けて打つ「"」は押されている Shift をそのまま使い、修飾の付け外しを挟まない */
TEST_F(KeyOutputJis, alpha_colon_waits_on_first_keypress_after_thumb_shift) {
    TestDriver driver;
    set_windmill_keymap();

    auto shift = key(POS_ALPHA_SHIFT);
    auto coln  = key(1, 10);
    auto dquo  = key(1, 11);

    uint16_t at_hold = 0, at_drop = 0, at_key = 0;

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)).WillOnce(InvokeWithoutArgs([&] { at_hold = timer_read(); }));
        EXPECT_EMPTY_REPORT(driver).WillOnce(InvokeWithoutArgs([&] { at_drop = timer_read(); }));
        EXPECT_REPORT(driver, (KC_QUOT)).WillOnce(InvokeWithoutArgs([&] { at_key = timer_read(); }));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // Shiftを戻す
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_2));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver); // 親指Shift 解放
    }

    shift.press();
    run_one_scan_loop();
    idle_for(150); // TAPPING_TERM 未満。ホールドは : の押下で確定する
    tap_key(coln, 120);
    idle_for(120);
    tap_key(dquo, 120);
    idle_for(120);
    shift.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);

    EXPECT_GE(uint16_t(at_drop - at_hold), uint16_t(IME_WAIT_MS));
    EXPECT_GE(uint16_t(at_key - at_drop), uint16_t(IME_WAIT_MS));
}

// 親指Shiftのホールドが確定した直後の1打鍵目が「"」。修飾の入れ替えが挟まらない
TEST_F(KeyOutputJis, alpha_dquote_keeps_held_shift_on_first_keypress) {
    TestDriver driver;
    set_windmill_keymap();

    auto shift = key(POS_ALPHA_SHIFT);
    auto dquo  = key(1, 11);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_2));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }

    shift.press();
    run_one_scan_loop();
    idle_for(150);
    tap_key(dquo, 120);
    idle_for(120);
    shift.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* 押している間は押しっぱなし (キーリピートが効く)。Shift 付きで送る「'」も同じ。
 * 重ねて押した次のキーに「'」の Shift が漏れない */
TEST_F(KeyOutputJis, alpha_quote_stays_down_while_held) {
    TestDriver driver;
    set_windmill_keymap();

    auto quot = key(1, 11);
    auto scln = key(1, 10);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_7));
    }
    quot.press();
    run_one_scan_loop();
    idle_for(600); // リピートが始まる程度に押し続ける
    VERIFY_AND_CLEAR(driver);

    EXPECT_REPORT(driver, (KC_7, KC_SCLN)); // ; 押下。Shiftが落ちている
    scln.press();
    run_one_scan_loop();
    idle_for(50);
    VERIFY_AND_CLEAR(driver);

    EXPECT_REPORT(driver, (KC_SCLN));
    quot.release();
    run_one_scan_loop();
    idle_for(50);
    VERIFY_AND_CLEAR(driver);

    EXPECT_EMPTY_REPORT(driver);
    scln.release();
    run_one_scan_loop();
    idle_for(50);
    VERIFY_AND_CLEAR(driver);
}

/* 「"」(押されている Shift + 2) を押したまま、先に Shift を離す。解放では押下で
 * 送ったキー (2) を離す。解放の時点の Shift で引き直すと「'」(Shift+7) を離そうと
 * して、ホストに 2 が押されたまま残る */
TEST_F(KeyOutputJis, alpha_key_released_after_shift_is_not_stuck) {
    TestDriver driver;
    set_windmill_keymap();

    auto shift = key(POS_ALPHA_SHIFT);
    auto dquo  = key(1, 11);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_2));
        EXPECT_REPORT(driver, (KC_2)); // 親指Shift 解放
        EXPECT_EMPTY_REPORT(driver);   // " 解放
    }

    shift.press();
    run_one_scan_loop();
    idle_for(250);
    dquo.press();
    run_one_scan_loop();
    idle_for(50);
    shift.release();
    run_one_scan_loop();
    idle_for(50);
    dquo.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/*
 * 記号レイヤー
 */

// clang-format off
/* Shift時の文字も US と揃える。US で Shift を押しながら打つと、数字は数字キーの
 * Shift 側の記号 (! @ # $ % ^ & * ( )) に、「=」「-」「[」「]」「`」は
 * 「+」「_」「{」「}」「~」になる。元から Shift 付きのキーは同じ文字のまま */
static const JisKey jis_sym_keys[] = {
    // 1 2 3 4 5 6 7 8 9 0。Shift時: ! @ # $ % ^ & * ( )
    {0, 1, {KC_1, false}, {KC_1, true}},
    {0, 2, {KC_2, false}, {KC_LBRC, false}},    // @  単独のキー
    {0, 3, {KC_3, false}, {KC_3, true}},
    {0, 4, {KC_4, false}, {KC_4, true}},
    {0, 5, {KC_5, false}, {KC_5, true}},
    {0, 6, {KC_6, false}, {KC_EQL, false}},     // ^  単独のキー
    {0, 7, {KC_7, false}, {KC_6, true}},        // &
    {0, 8, {KC_8, false}, {KC_QUOT, true}},     // *
    {0, 9, {KC_9, false}, {KC_8, true}},        // (
    {0, 10, {KC_0, false}, {KC_9, true}},       // )
    // ! @ # $ % ^ & * ( )
    {1, 1, {KC_1, true}, {KC_1, true}},
    {1, 2, {KC_LBRC, false}, {KC_LBRC, false}}, // @  Shift を押していても @ (Shift を外して送る)
    {1, 3, {KC_3, true}, {KC_3, true}},
    {1, 4, {KC_4, true}, {KC_4, true}},
    {1, 5, {KC_5, true}, {KC_5, true}},
    {1, 6, {KC_EQL, false}, {KC_EQL, false}},   // ^  同上
    {1, 7, {KC_6, true}, {KC_6, true}},
    {1, 8, {KC_QUOT, true}, {KC_QUOT, true}},
    {1, 9, {KC_8, true}, {KC_8, true}},
    {1, 10, {KC_9, true}, {KC_9, true}},
    {1, 11, {KC_LBRC, true}, {KC_EQL, true}},   // ` ~
    // = + - _ [ ] ~ { }
    {2, 1, {KC_MINS, true}, {KC_SCLN, true}},   // = +
    {2, 2, {KC_SCLN, true}, {KC_SCLN, true}},   // +
    {2, 3, {KC_MINS, false}, {KC_INT1, true}},  // - _
    {2, 4, {KC_INT1, true}, {KC_INT1, true}},   // _
    {2, 5, {KC_RBRC, false}, {KC_RBRC, true}},  // [ {
    {2, 6, {KC_NUHS, false}, {KC_NUHS, true}},  // ] }
    {2, 7, {KC_EQL, true}, {KC_EQL, true}},     // ~
    {2, 8, {KC_RBRC, true}, {KC_RBRC, true}},   // {
    {2, 9, {KC_NUHS, true}, {KC_NUHS, true}},   // }
    // |
    {3, 5, {KC_INT3, true}, {KC_INT3, true}},
};

/* 「?」は親指Shiftと同じ位置にあるので、Shift を押しながらは打てない。Shift無しだけ見る */
static const JisKey jis_sym_ques[] = {
    {3, 6, {KC_SLSH, true}, {KC_SLSH, true}},
};
// clang-format on

// Sym をホールドしたまま順に叩く
template <size_t N>
static void tap_keys_on_sym(WindmillTest* f, const JisKey (&keys)[N]) {
    auto sym = f->key(POS_SYM);

    sym.press();
    f->run_one_scan_loop();
    f->idle_for(250); // TAPPING_TERM 超え。ホールド確定
    for (const auto& k : keys) {
        f->tap_key(f->key(k.row, k.col), 50);
        f->idle_for(50);
    }
    sym.release();
    f->run_one_scan_loop();
    f->idle_for(120);
}

TEST_F(KeyOutputJis, sym_layer_on_alpha) {
    TestDriver driver;
    set_windmill_keymap();

    {
        InSequence s;
        for (const auto& k : jis_sym_keys) {
            expect_plain_tap(driver, k.plain);
        }
        expect_plain_tap(driver, jis_sym_ques[0].plain);
    }

    tap_keys_on_sym(this, jis_sym_keys);
    tap_keys_on_sym(this, jis_sym_ques);
    VERIFY_AND_CLEAR(driver);
}

/* 親指Shiftをホールドしてから Sym をホールドして叩く (順が逆だと、親指Shiftの
 * 位置は記号レイヤーの「?」になる) */
TEST_F(KeyOutputJis, sym_layer_on_alpha_with_thumb_shift) {
    TestDriver driver;
    set_windmill_keymap();

    auto shift = key(POS_ALPHA_SHIFT);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        for (const auto& k : jis_sym_keys) {
            expect_shifted_tap(driver, k.shifted);
        }
        EXPECT_EMPTY_REPORT(driver);
    }

    shift.press();
    run_one_scan_loop();
    idle_for(250);
    tap_keys_on_sym(this, jis_sym_keys);
    shift.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* 親指Shift → Sym → 「@」を間を置かずに押す。親指Shiftのホールドは Sym の押下で、
 * Sym のホールドは「@」の押下で確定する。Shift を外して送る経路なので、外す前後に
 * ウェイトが入っていること */
TEST_F(KeyOutputJis, sym_at_waits_right_after_thumb_shift) {
    TestDriver driver;
    set_windmill_keymap();

    auto shift = key(POS_ALPHA_SHIFT);
    auto sym   = key(POS_SYM);
    auto at    = key(1, 2);

    uint16_t at_at = 0, at_drop = 0, at_key = 0;

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver).WillOnce(InvokeWithoutArgs([&] { at_drop = timer_read(); }));
        EXPECT_REPORT(driver, (KC_LBRC)).WillOnce(InvokeWithoutArgs([&] { at_key = timer_read(); }));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }

    shift.press();
    run_one_scan_loop();
    sym.press();
    run_one_scan_loop();
    at_at = timer_read();
    at.press();
    run_one_scan_loop();
    idle_for(50);
    at.release();
    run_one_scan_loop();
    idle_for(50);
    sym.release();
    run_one_scan_loop();
    shift.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);

    EXPECT_GE(uint16_t(at_drop - at_at), uint16_t(IME_WAIT_MS));
    EXPECT_GE(uint16_t(at_key - at_drop), uint16_t(IME_WAIT_MS));
}

// かなベースでは、1打鍵ごとに英数へ切り替えてから送り、かなへ戻す
TEST_F(KeyOutputJis, sym_layer_on_kana_wraps_with_ime_switch) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    {
        InSequence s;
        for (const auto& k : jis_sym_keys) {
            EXPECT_REPORT(driver, (KC_LNG2));
            EXPECT_EMPTY_REPORT(driver);
            expect_plain_tap(driver, k.plain);
            EXPECT_REPORT(driver, (KC_LNG1));
            EXPECT_EMPTY_REPORT(driver);
        }
    }

    tap_keys_on_sym(this, jis_sym_keys);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

/* 記号を続けて打つと、前のキーを離す前に次のキーを押すことがある。
 * 「=」(Shift+-) を押したまま「-」を押しても、「-」にShiftが乗って「=」に
 * 化けてはいけない */
TEST_F(KeyOutputJis, sym_roll_over_does_not_leak_shift) {
    TestDriver driver;
    set_windmill_keymap();

    auto sym  = key(POS_SYM);
    auto eql  = key(2, 1); // =
    auto lbrc = key(2, 5); // [

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_MINS)); // = 押下
        EXPECT_REPORT(driver, (KC_MINS, KC_RBRC));       // [ 押下。Shiftが落ちている
        EXPECT_REPORT(driver, (KC_RBRC));                // = 解放
        EXPECT_EMPTY_REPORT(driver);                     // [ 解放
    }

    sym.press();
    run_one_scan_loop();
    idle_for(250);
    eql.press();
    run_one_scan_loop();
    idle_for(30);
    lbrc.press();
    run_one_scan_loop();
    idle_for(30);
    eql.release();
    run_one_scan_loop();
    idle_for(30);
    lbrc.release();
    run_one_scan_loop();
    idle_for(30);
    sym.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/*
 * LT() のタップ側
 */

/* 「\」のキーは、ホールドすれば JIS でも記号レイヤーへ移る (上の記号レイヤーの
 * テストが通っていること自体がその確認)。ここでは、タップを横取りしていても
 * ホールドの途中で何も送らないことを見る */
TEST_F(KeyOutputJis, backslash_key_hold_sends_nothing) {
    TestDriver driver;
    set_windmill_keymap();

    auto sym = key(POS_SYM);

    EXPECT_NO_REPORT(driver);
    sym.press();
    run_one_scan_loop();
    idle_for(250);
    sym.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}
