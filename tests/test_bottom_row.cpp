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

/* かなレイヤー最下段のタップ・ホールド (issue #85) のレポート列を検証する。
 *
 * テスト名の先頭の英字は issue #85 の「テストで固定するケース」の記号。押し方は
 * 「押してからの ms」で書く。結果のコメントは IME が出す文字で、Shift 面はまだ
 * 無いので「を」「ぁ」は Shift+キーのレポートとして届く。
 *
 * 判定の境界 (150ms と 250ms のちょうど) と、待っている間はレポートが出ず、
 * 待っていたキーが押した順に出ることもここで見る。 */

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

static constexpr bool DN = true;
static constexpr bool UP = false;

// 押し方の1手。row, col はかなレイヤー上の位置 (POS_*)、at は最初の手からの ms
struct Step {
    uint8_t  row;
    uint8_t  col;
    bool     down;
    uint16_t at;
};

class BottomRow : public WindmillTest {
   public:
    // 起動直後の英数から MY_LCTL 1回タップでかなへ切り替える。レポートの中身は問わない
    void switch_to_kana(TestDriver& driver) {
        EXPECT_ANY_REPORT(driver).Times(AnyNumber());
        tap_key(key(POS_LCTL), 120);
        settle();
        VERIFY_AND_CLEAR(driver);
    }

    /* 押し方のとおりにキーを押し離しする。各手は指定の時刻のスキャンで処理される。
     * 最後の手のあとは、待っていたものが全て出るまで回す */
    void play(std::initializer_list<Step> steps) {
        const uint16_t start = timer_read();
        for (const auto& step : steps) {
            while (uint16_t(timer_read() - start) < step.at) run_one_scan_loop();
            auto k = key(step.row, step.col);
            if (step.down) {
                k.press();
            } else {
                k.release();
            }
        }
        run_one_scan_loop();
        idle_for(300);
    }

    // 最初の手からの経過時間 (ms)。play() の start と揃えるため、押す直前に控える
    uint16_t since(uint16_t start) {
        return timer_read() - start;
    }
};

/* 記号レイヤーの記号は、かなベースでは英数へ切り替えて送る (is_sym_ime_wrap_target) */
static void expect_sym_wrapped(TestDriver& driver, uint8_t keycode) {
    EXPECT_REPORT(driver, (KC_LNG2));
    EXPECT_EMPTY_REPORT(driver);
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // 記号の weak Shift
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT, keycode));
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
    EXPECT_EMPTY_REPORT(driver);
    EXPECT_REPORT(driver, (KC_LNG1));
    EXPECT_EMPTY_REPORT(driver);
}

/* 親指の組の半角スペースが、X (こ) を押したまま決まったときのレポート列。
 * X はホールド (左Shift) になり、スペースはその Shift を外して送ってから戻す */
static void expect_space_while_x_held(TestDriver& driver) {
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // こ はホールド
    EXPECT_EMPTY_REPORT(driver);            // Shift を外してから
    EXPECT_REPORT(driver, (KC_SPACE));
    EXPECT_EMPTY_REPORT(driver);
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // 押したままの こ の Shift を戻す
}

/*
 * issue #85 のケース
 */

// a: こ↓0 み↓100 わ↓200 わ↑230 み↑240 こ↑250 → みを
TEST_F(BottomRow, a_shift_then_partner_tap_then_key) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));       // わ の押下で こ がホールド
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_N)); // み (Shift+み も み)
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_0)); // を
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);                  // こ 解放
    }
    play({{POS_KO, DN, 0}, {POS_MI, DN, 100}, {POS_WA, DN, 200}, {POS_WA, UP, 230}, {POS_MI, UP, 240}, {POS_KO, UP, 250}});
    VERIFY_AND_CLEAR(driver);
}

// b: こ↓0 み↓20 わ↓40 わ↑60 み↑70 こ↑80 → こみわ
TEST_F(BottomRow, b_quick_roll_keeps_press_order) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_B)); // こ
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_N)); // み
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_0)); // わ
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_KO, DN, 0}, {POS_MI, DN, 20}, {POS_WA, DN, 40}, {POS_WA, UP, 60}, {POS_MI, UP, 70}, {POS_KO, UP, 80}});
    VERIFY_AND_CLEAR(driver);
}

// c: こ↓0 わ↓100 わ↑130 こ↑160 → を
TEST_F(BottomRow, c_key_within_150ms_waits_then_shifted) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // 150ms で こ がホールド
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_0));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_KO, DN, 0}, {POS_WA, DN, 100}, {POS_WA, UP, 130}, {POS_KO, UP, 160}});
    VERIFY_AND_CLEAR(driver);
}

// d: こ↓0 わ↓200 わ↑230 こ↑260 → を
TEST_F(BottomRow, d_key_after_150ms_holds_at_once) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_0));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_KO, DN, 0}, {POS_WA, DN, 200}, {POS_WA, UP, 230}, {POS_KO, UP, 260}});
    VERIFY_AND_CLEAR(driver);
}

// e: こ↓0 こ↑200 → こ
TEST_F(BottomRow, e_release_before_250ms_is_tap) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_B));
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_KO, DN, 0}, {POS_KO, UP, 200}});
    VERIFY_AND_CLEAR(driver);
}

// f: こ↓0 こ↑300 → 何も出ない (左Shift の押し離しだけ)
TEST_F(BottomRow, f_long_press_is_shift_only) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_KO, DN, 0}, {POS_KO, UP, 300}});
    VERIFY_AND_CLEAR(driver);
}

// g: こ↓0 み↓200 み↑230 こ↑260 → 半角スペース
TEST_F(BottomRow, g_partner_tap_is_space) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        expect_space_while_x_held(driver); // み を離した 230ms で決まる
        EXPECT_EMPTY_REPORT(driver); // こ 解放
    }
    play({{POS_KO, DN, 0}, {POS_MI, DN, 200}, {POS_MI, UP, 230}, {POS_KO, UP, 260}});
    VERIFY_AND_CLEAR(driver);
}

// h: こ↓0 あ↓200 あ↑230 み↓300 こ↑350 わ↓400 わ↑430 み↑470 → ぁを
TEST_F(BottomRow, h_partner_after_shift_was_used_holds_by_key) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_3)); // ぁ
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        // み が決まるまで、こ の解放も わ も待つ。150ms で み がホールド
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_RIGHT_SHIFT));
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT)); // こ 解放
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT, KC_0)); // を
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_KO, DN, 0}, {POS_A, DN, 200}, {POS_A, UP, 230}, {POS_MI, DN, 300}, {POS_KO, UP, 350}, {POS_WA, DN, 400}, {POS_WA, UP, 430}, {POS_MI, UP, 470}});
    VERIFY_AND_CLEAR(driver);
}

// i: こ↓0 あ↓200 あ↑230 み↓300 こ↑350 わ↓500 わ↑530 み↑550 → ぁを
TEST_F(BottomRow, i_partner_after_shift_was_used_holds_at_once) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_3)); // ぁ
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_RIGHT_SHIFT)); // わ の押下で み がホールド
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));                // こ 解放
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT, KC_0));          // を
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_KO, DN, 0}, {POS_A, DN, 200}, {POS_A, UP, 230}, {POS_MI, DN, 300}, {POS_KO, UP, 350}, {POS_WA, DN, 500}, {POS_WA, UP, 530}, {POS_MI, UP, 550}});
    VERIFY_AND_CLEAR(driver);
}

// j: ひ↓0 も↓100 た↓200 た↑230 も↑240 ひ↑250 → も!
TEST_F(BottomRow, j_sym_hold_then_partner_tap_then_key) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_M)); // も。記号レイヤーの同じ位置は透過
        EXPECT_EMPTY_REPORT(driver);
        expect_sym_wrapped(driver, KC_1); // !
    }
    play({{POS_HI, DN, 0}, {POS_MO, DN, 100}, {POS_TA, DN, 200}, {POS_TA, UP, 230}, {POS_MO, UP, 240}, {POS_HI, UP, 250}});
    VERIFY_AND_CLEAR(driver);
}

// k: ひ↓0 も↓300 も↑350 ひ↑400 → も
TEST_F(BottomRow, k_partner_tap_on_sym_layer) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_M));
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_HI, DN, 0}, {POS_MO, DN, 300}, {POS_MO, UP, 350}, {POS_HI, UP, 400}});
    VERIFY_AND_CLEAR(driver);
}

// l: そ↓0 ね↓100 ぬ↓200 ぬ↑230 ね↑240 そ↑250 → ねF1
TEST_F(BottomRow, l_fn_hold_then_partner_tap_then_key) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_COMMA)); // ね
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_F1));
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_FN_L, DN, 0}, {POS_FN_R, DN, 100}, {POS_NU, DN, 200}, {POS_NU, UP, 230}, {POS_FN_R, UP, 240}, {POS_FN_L, UP, 250}});
    VERIFY_AND_CLEAR(driver);
}

// n: つ↓0 つ↑300 → Win の押し離し
TEST_F(BottomRow, n_long_press_is_gui_only) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_TSU, DN, 0}, {POS_TSU, UP, 300}});
    VERIFY_AND_CLEAR(driver);
}

// o: さ↓0 さ↑300 → Alt の押し離し
TEST_F(BottomRow, o_long_press_is_alt_only) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_ALT));
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_SA, DN, 0}, {POS_SA, UP, 300}});
    VERIFY_AND_CLEAR(driver);
}

/* 親指の組で半角スペースになるもの (p 〜 s、u)。こ を離した後に組が決まったときで、
 * こ が 250ms に達していれば左Shiftが一度出て、下ろしてからスペースを送る */
static void expect_space_after_shift(TestDriver& driver) {
    InSequence s;
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
    EXPECT_EMPTY_REPORT(driver);
    EXPECT_REPORT(driver, (KC_SPACE));
    EXPECT_EMPTY_REPORT(driver);
}

static void expect_space(TestDriver& driver) {
    InSequence s;
    EXPECT_REPORT(driver, (KC_SPACE));
    EXPECT_EMPTY_REPORT(driver);
}

// p: こ↓0 み↓200 こ↑300 み↑400 → 半角スペース
TEST_F(BottomRow, p_space_x_released_first_after_250ms) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    expect_space_after_shift(driver);
    play({{POS_KO, DN, 0}, {POS_MI, DN, 200}, {POS_KO, UP, 300}, {POS_MI, UP, 400}});
    VERIFY_AND_CLEAR(driver);
}

// q: こ↓0 み↓200 み↑300 こ↑400 → 半角スペース
TEST_F(BottomRow, q_space_y_released_first_after_250ms) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        expect_space_while_x_held(driver); // み を離した 300ms で決まる
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_KO, DN, 0}, {POS_MI, DN, 200}, {POS_MI, UP, 300}, {POS_KO, UP, 400}});
    VERIFY_AND_CLEAR(driver);
}

// r: こ↓0 み↓100 こ↑200 み↑300 → 半角スペース
TEST_F(BottomRow, r_space_x_released_in_150_to_250ms) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    expect_space(driver); // こ はホールドにならないまま組が決まる
    play({{POS_KO, DN, 0}, {POS_MI, DN, 100}, {POS_KO, UP, 200}, {POS_MI, UP, 300}});
    VERIFY_AND_CLEAR(driver);
}

// s: こ↓0 み↓100 み↑200 こ↑300 → 半角スペース
TEST_F(BottomRow, s_space_y_released_first) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        expect_space_while_x_held(driver); // み を離した 200ms で決まる
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_KO, DN, 0}, {POS_MI, DN, 100}, {POS_MI, UP, 200}, {POS_KO, UP, 300}});
    VERIFY_AND_CLEAR(driver);
}

// t: こ↓0 み↓100 こ↑130 み↑200 → こみ
TEST_F(BottomRow, t_x_released_before_150ms_is_tap) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_B));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_N));
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_KO, DN, 0}, {POS_MI, DN, 100}, {POS_KO, UP, 130}, {POS_MI, UP, 200}});
    VERIFY_AND_CLEAR(driver);
}

// u: こ↓0 み↓100 み↑130 こ↑200 → 半角スペース
TEST_F(BottomRow, u_space_both_short) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    uint16_t start = 0, at_space = 0;
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver).WillOnce(InvokeWithoutArgs([&] { at_space = since(start); }));
        EXPECT_REPORT(driver, (KC_SPACE));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver); // こ 解放
    }
    start = timer_read();
    play({{POS_KO, DN, 0}, {POS_MI, DN, 100}, {POS_MI, UP, 130}, {POS_KO, UP, 200}});
    VERIFY_AND_CLEAR(driver);
    EXPECT_EQ(at_space, 150); // み の解放ではなく、こ が 150ms に達した時点で決まる
    VERIFY_AND_CLEAR(driver);
}

// v: こ↓0 み↓100 こ↑130 み↑400 → こ
TEST_F(BottomRow, v_x_tap_then_y_hold) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_B));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT)); // み は 250ms でホールド
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_KO, DN, 0}, {POS_MI, DN, 100}, {POS_KO, UP, 130}, {POS_MI, UP, 400}});
    VERIFY_AND_CLEAR(driver);
}

/* w: こ↓0 み↓100 こ↑160 み↑400 → こ
 * こ は 150〜250ms で離したので、み の結果が出るまで出力を待つ */
TEST_F(BottomRow, w_x_waits_for_y_then_tap) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    uint16_t start = 0, at_tap = 0;
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_B)).WillOnce(InvokeWithoutArgs([&] { at_tap = since(start); }));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }
    start = timer_read();
    play({{POS_KO, DN, 0}, {POS_MI, DN, 100}, {POS_KO, UP, 160}, {POS_MI, UP, 400}});
    VERIFY_AND_CLEAR(driver);
    EXPECT_EQ(at_tap, 350); // み が 250ms に達した時点
}

// x: こ↓0 み↓100 こ↑300 み↑400 → 何も出ない
TEST_F(BottomRow, x_both_hold) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));                 // こ 250ms
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_RIGHT_SHIFT)); // み 250ms
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));                // 待っていた こ の解放
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_KO, DN, 0}, {POS_MI, DN, 100}, {POS_KO, UP, 300}, {POS_MI, UP, 400}});
    VERIFY_AND_CLEAR(driver);
}

/* y: こ↓0 あ↓200 あ↑230 み↓300 こ↑350 み↑400 → ぁみ
 * み を離した時点で こ はシフトとしてかかっているので組にならず、み は通常のタップ */
TEST_F(BottomRow, y_partner_tap_after_shift_was_used_is_tap) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_3)); // ぁ
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_N)); // み。こ の解放は み が決まるまで待っていた
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver); // こ 解放
    }
    play({{POS_KO, DN, 0}, {POS_A, DN, 200}, {POS_A, UP, 230}, {POS_MI, DN, 300}, {POS_KO, UP, 350}, {POS_MI, UP, 400}});
    VERIFY_AND_CLEAR(driver);
}

// z: ひ↓0 こ↓200 こ↑230 ひ↑260 → |
TEST_F(BottomRow, z_later_tap_promotes_earlier_to_hold) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        expect_sym_wrapped(driver, KC_BACKSLASH); // こ の位置は記号レイヤーで「|」
    }
    play({{POS_HI, DN, 0}, {POS_KO, DN, 200}, {POS_KO, UP, 230}, {POS_HI, UP, 260}});
    VERIFY_AND_CLEAR(driver);
}

// aa: こ↓0 み↓200 み↑230 み↓300 み↑330 こ↑400 → 半角スペース2つ
TEST_F(BottomRow, aa_each_partner_tap_is_space) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // 1つ目 (230ms)。こ はホールド
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_SPACE));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver); // 2つ目 (330ms)
        EXPECT_REPORT(driver, (KC_SPACE));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver); // こ 解放
    }
    play({{POS_KO, DN, 0}, {POS_MI, DN, 200}, {POS_MI, UP, 230}, {POS_MI, DN, 300}, {POS_MI, UP, 330}, {POS_KO, UP, 400}});
    VERIFY_AND_CLEAR(driver);
}

/* ab: こ↓0 み↓200 み↑230 わ↓300 わ↑330 こ↑400 → 半角スペース、を
 * 決まったスペースは、後から こ がシフトとしてかかっても取り消さない */
TEST_F(BottomRow, ab_space_stays_when_shift_used_later) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        expect_space_while_x_held(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_0)); // を
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver); // こ 解放
    }
    play({{POS_KO, DN, 0}, {POS_MI, DN, 200}, {POS_MI, UP, 230}, {POS_WA, DN, 300}, {POS_WA, UP, 330}, {POS_KO, UP, 400}});
    VERIFY_AND_CLEAR(driver);
}

/* m: そ↓0 ね↓50 た↓300 た↑330 ね↑340 そ↑350 → 設定レイヤーの た (MY_WIN)
 * 設定はEEPROMに残るので、スイートを分けて Android から Windows へ戻ることで見る */
class BottomRowConf : public BottomRow {};

TEST_F(BottomRowConf, m_both_fn_hold_reach_conf_layer) {
    TestDriver driver;
    set_windmill_keymap();

    EXPECT_NO_REPORT(driver);
    tap_on_conf(POS_ANDR);
    settle();
    VERIFY_AND_CLEAR(driver);

    switch_to_kana(driver);

    EXPECT_NO_REPORT(driver); // 「そ」「ね」も「た」も出ない
    play({{POS_FN_L, DN, 0}, {POS_FN_R, DN, 50}, {POS_TA, DN, 300}, {POS_TA, UP, 330}, {POS_FN_R, UP, 340}, {POS_FN_L, UP, 350}});
    VERIFY_AND_CLEAR(driver);

    // Windows に戻っていれば、言語切替は Ctrl+Space ではなく KC_LNG2 (かな → 英数)
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LNG2));
        EXPECT_EMPTY_REPORT(driver);
    }
    tap_key(key(POS_LCTL), 120);
    settle();
    VERIFY_AND_CLEAR(driver);
}

/*
 * 判定の境界
 */

// 149ms で離せばタップ、150ms ちょうどならホールド (後から別のキーが押されている)
TEST_F(BottomRow, interrupt_boundary_149ms_is_tap) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_B)); // こ
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_0)); // わ
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_KO, DN, 0}, {POS_WA, DN, 100}, {POS_WA, UP, 120}, {POS_KO, UP, 149}});
    VERIFY_AND_CLEAR(driver);
}

TEST_F(BottomRow, interrupt_boundary_150ms_is_hold) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_0)); // を
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_KO, DN, 0}, {POS_WA, DN, 100}, {POS_WA, UP, 120}, {POS_KO, UP, 150}});
    VERIFY_AND_CLEAR(driver);
}

// 150ms ちょうどに押したキーは、待たずにホールドを決める
TEST_F(BottomRow, key_at_150ms_holds_at_once) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    uint16_t start = 0, at_hold = 0, at_key = 0;
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)).WillOnce(InvokeWithoutArgs([&] { at_hold = since(start); }));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_0)).WillOnce(InvokeWithoutArgs([&] { at_key = since(start); }));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }
    start = timer_read();
    play({{POS_KO, DN, 0}, {POS_WA, DN, 150}, {POS_WA, UP, 180}, {POS_KO, UP, 200}});
    VERIFY_AND_CLEAR(driver);
    EXPECT_EQ(at_hold, 150);
    EXPECT_EQ(at_key, 150);
}

// 249ms で離せばタップ、250ms ちょうどならホールド (単独の長押し)
TEST_F(BottomRow, hold_boundary_249ms_is_tap) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_B));
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_KO, DN, 0}, {POS_KO, UP, 249}});
    VERIFY_AND_CLEAR(driver);
}

TEST_F(BottomRow, hold_boundary_250ms_is_hold) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_KO, DN, 0}, {POS_KO, UP, 250}});
    VERIFY_AND_CLEAR(driver);
}

/* 親指の組の X (先に押したほう) は、150ms 以上押していなければ組にならない。
 * 149ms ならそれぞれのタップ、150ms ちょうどなら半角スペース */
TEST_F(BottomRow, thumb_pair_boundary_149ms_is_two_taps) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_B));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_N));
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_KO, DN, 0}, {POS_MI, DN, 50}, {POS_MI, UP, 100}, {POS_KO, UP, 149}});
    VERIFY_AND_CLEAR(driver);
}

TEST_F(BottomRow, thumb_pair_boundary_150ms_is_space) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        expect_space_while_x_held(driver); // こ が 150ms に達したスキャンで決まり、同じスキャンで離れる
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_KO, DN, 0}, {POS_MI, DN, 50}, {POS_MI, UP, 100}, {POS_KO, UP, 150}});
    VERIFY_AND_CLEAR(driver);
}

/* 親指の組の Y (後から押したほう) は、250ms 未満で離せば半角スペース。
 * 250ms ちょうどならホールドで、先に離れて待っていた X はタップ */
TEST_F(BottomRow, thumb_pair_y_249ms_is_space) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    expect_space(driver);
    play({{POS_KO, DN, 0}, {POS_MI, DN, 100}, {POS_KO, UP, 200}, {POS_MI, UP, 349}});
    VERIFY_AND_CLEAR(driver);
}

TEST_F(BottomRow, thumb_pair_y_250ms_is_hold) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_B));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_KO, DN, 0}, {POS_MI, DN, 100}, {POS_KO, UP, 200}, {POS_MI, UP, 350}});
    VERIFY_AND_CLEAR(driver);
}

/*
 * 待っている間と、待っていたキーの出方
 */

/* 未判定のキーを押してから 150ms 未満に押したキーは、そのキーが決まるまで
 * レポートを出さない。こ が 150ms でホールドに決まった時点で、まとめて出る */
TEST_F(BottomRow, no_report_while_waiting) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto ko = key(POS_KO);
    auto wa = key(POS_WA);

    EXPECT_NO_REPORT(driver);
    ko.press();
    run_one_scan_loop();
    idle_for(99);
    wa.press(); // 100ms
    run_one_scan_loop();
    idle_for(29);
    wa.release(); // 130ms
    run_one_scan_loop();
    idle_for(19); // 149ms のスキャンまで回す
    VERIFY_AND_CLEAR(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_0));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
    }
    run_one_scan_loop(); // 150ms
    VERIFY_AND_CLEAR(driver);

    EXPECT_EMPTY_REPORT(driver);
    ko.release();
    run_one_scan_loop();
    idle_for(50);
    VERIFY_AND_CLEAR(driver);
}

/* 待っていたキーは押した順に、押し離しの重なりも保って出る。
 * こ を 150ms 未満で離したのでタップ。こ → あ → わ の順 */
TEST_F(BottomRow, waiting_keys_keep_press_order) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_B)); // こ
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_3)); // あ
        EXPECT_REPORT(driver, (KC_3, KC_0)); // わ (あ を押したまま)
        EXPECT_REPORT(driver, (KC_0));
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_KO, DN, 0}, {POS_A, DN, 20}, {POS_WA, DN, 40}, {POS_A, UP, 60}, {POS_WA, UP, 80}, {POS_KO, UP, 100}});
    VERIFY_AND_CLEAR(driver);
}

/* 最下段のキーの押下では、先に押された未判定のキーを判定しない。
 * ひ を押してから 200ms で こ を押しても ひ はホールドにならず、250ms 前に離せば
 * タップになる。こ も続けてタップで、押した順に ひ → こ */
TEST_F(BottomRow, bottom_press_does_not_decide_earlier) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_V)); // ひ
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_B)); // こ
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_HI, DN, 0}, {POS_KO, DN, 200}, {POS_HI, UP, 240}, {POS_KO, UP, 260}});
    VERIFY_AND_CLEAR(driver);
}

// 最下段のキーはリピートしない。押しっぱなしでもタップは押下と解放の1回きり
TEST_F(BottomRow, tap_does_not_stay_down) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    uint16_t start = 0, at_down = 0, at_up = 0;
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_B)).WillOnce(InvokeWithoutArgs([&] { at_down = since(start); }));
        EXPECT_EMPTY_REPORT(driver).WillOnce(InvokeWithoutArgs([&] { at_up = since(start); }));
    }
    start = timer_read();
    play({{POS_KO, DN, 0}, {POS_KO, UP, 200}});
    VERIFY_AND_CLEAR(driver);
    EXPECT_EQ(at_down, 200); // 離して初めてタップに決まる
    EXPECT_EQ(at_up, 200);   // 押下と解放は同じスキャンで続けて出る
}
