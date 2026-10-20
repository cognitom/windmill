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

/* Ctrl (MY_LCTL) のタップ・ホールド (issue #95)。判定はかなレイヤー最下段と同じ
 * (issue #85) で、タップは言語切替、ホールドは Ctrl。押し方は test_bottom_row.cpp と
 * 同じく「押してからの ms」で書く。
 *
 * どちらに決まったかは、後に打つ「ぬ」の位置で見る。英数では KC_Q、かなでは KC_1。
 * タップならベースレイヤーが入れ替わり、ホールドなら Ctrl の下で英数レイヤーになる。
 *
 * かなベースで Ctrl / Win / Alt を重ねて押したとき、どれかを離しても、残りを
 * 押している間は英数レイヤーのままであることもここで見る。 */

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

class CtrlTapHold : public WindmillTest {
   public:
    // 起動直後の英数から MY_LCTL 1回タップでかなへ切り替える。レポートの中身は問わない
    void switch_to_kana(TestDriver& driver) {
        EXPECT_ANY_REPORT(driver).Times(AnyNumber());
        tap_key(key(POS_LCTL), 120);
        settle();
        VERIFY_AND_CLEAR(driver);
    }

    // test_bottom_row.cpp の BottomRow::play() と同じ
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

    uint16_t since(uint16_t start) {
        return timer_read() - start;
    }
};

/*
 * 判定の境界。英数ベースから押す
 */

// 149ms で離せばタップ。待っていた ぬ は切り替えた後のかなで出る
TEST_F(CtrlTapHold, interrupt_boundary_149ms_is_tap) {
    TestDriver driver;
    set_windmill_keymap();

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LNG1)); // かなへ
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_1)); // ぬ
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_LCTL, DN, 0}, {POS_NU, DN, 100}, {POS_NU, UP, 120}, {POS_LCTL, UP, 149}});
    VERIFY_AND_CLEAR(driver);
}

// 150ms ちょうどならホールド (後から別のキーが押されている)。言語は切り替えない
TEST_F(CtrlTapHold, interrupt_boundary_150ms_is_hold) {
    TestDriver driver;
    set_windmill_keymap();

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_CTRL));
        EXPECT_REPORT(driver, (KC_LEFT_CTRL, KC_Q)); // Ctrl+Q
        EXPECT_REPORT(driver, (KC_LEFT_CTRL));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_Q)); // 英数のまま
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_LCTL, DN, 0}, {POS_NU, DN, 100}, {POS_NU, UP, 120}, {POS_LCTL, UP, 150}, {POS_NU, DN, 300}, {POS_NU, UP, 320}});
    VERIFY_AND_CLEAR(driver);
}

// 150ms ちょうどに押したキーは、待たずにホールドを決める
TEST_F(CtrlTapHold, key_at_150ms_holds_at_once) {
    TestDriver driver;
    set_windmill_keymap();

    uint16_t start = 0, at_hold = 0, at_key = 0;
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_CTRL)).WillOnce(InvokeWithoutArgs([&] { at_hold = since(start); }));
        EXPECT_REPORT(driver, (KC_LEFT_CTRL, KC_Q)).WillOnce(InvokeWithoutArgs([&] { at_key = since(start); }));
        EXPECT_REPORT(driver, (KC_LEFT_CTRL));
        EXPECT_EMPTY_REPORT(driver);
    }
    start = timer_read();
    play({{POS_LCTL, DN, 0}, {POS_NU, DN, 150}, {POS_NU, UP, 180}, {POS_LCTL, UP, 200}});
    VERIFY_AND_CLEAR(driver);
    EXPECT_EQ(at_hold, 150);
    EXPECT_EQ(at_key, 150);
}

// 249ms で離せばタップ (単独の長押し)
TEST_F(CtrlTapHold, hold_boundary_249ms_is_tap) {
    TestDriver driver;
    set_windmill_keymap();

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LNG1));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_1)); // かなへ移っている
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_LCTL, DN, 0}, {POS_LCTL, UP, 249}, {POS_NU, DN, 400}, {POS_NU, UP, 420}});
    VERIFY_AND_CLEAR(driver);
}

// 250ms ちょうどならホールド。離しても言語は切り替えない
TEST_F(CtrlTapHold, hold_boundary_250ms_is_hold) {
    TestDriver driver;
    set_windmill_keymap();

    uint16_t start = 0, at_hold = 0;
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_CTRL)).WillOnce(InvokeWithoutArgs([&] { at_hold = since(start); }));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_Q)); // 英数のまま
        EXPECT_EMPTY_REPORT(driver);
    }
    start = timer_read();
    play({{POS_LCTL, DN, 0}, {POS_LCTL, UP, 250}, {POS_NU, DN, 400}, {POS_NU, UP, 420}});
    VERIFY_AND_CLEAR(driver);
    EXPECT_EQ(at_hold, 250);
}

/* 素早く2回押して、2回目を押し続ければ Ctrl。1回目のタップでかなへ切り替わり、
 * 2回目のホールドの下では英数レイヤーになる。QMK の mod-tap ではタップの繰り返しに
 * なって Ctrl を取れなかった */
TEST_F(CtrlTapHold, quick_second_press_holds) {
    TestDriver driver;
    set_windmill_keymap();

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LNG1)); // 1回目のタップ。かなへ
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_CTRL)); // 2回目が 250ms でホールド
        EXPECT_REPORT(driver, (KC_LEFT_CTRL, KC_Q));
        EXPECT_REPORT(driver, (KC_LEFT_CTRL));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_1)); // Ctrl を離せばかな
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_LCTL, DN, 0},
          {POS_LCTL, UP, 50},
          {POS_LCTL, DN, 100},
          {POS_NU, DN, 400},
          {POS_NU, UP, 420},
          {POS_LCTL, UP, 500},
          {POS_NU, DN, 600},
          {POS_NU, UP, 620}});
    VERIFY_AND_CLEAR(driver);
}

/*
 * 最下段のキーとの組み合わせ。かなベースから押す
 */

// つ のタップと Ctrl のタップを重ねても、押した順に出る
TEST_F(CtrlTapHold, rolled_taps_keep_press_order) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_Z)); // つ
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LNG2)); // Ctrl のタップ。英数へ
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_Q)); // 英数へ移っている
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_TSU, DN, 0}, {POS_LCTL, DN, 50}, {POS_TSU, UP, 80}, {POS_LCTL, UP, 100}, {POS_NU, DN, 300}, {POS_NU, UP, 320}});
    VERIFY_AND_CLEAR(driver);
}

/* つ が未判定の間に押した Ctrl も、押した順に効く。ぬ の押下で、150ms を過ぎた つ と
 * Ctrl がどちらもホールドに決まり、Win、Ctrl の順に押される */
TEST_F(CtrlTapHold, ctrl_pressed_while_bottom_undecided_keeps_order) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_LEFT_CTRL));
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_LEFT_CTRL, KC_Q)); // 英数レイヤーで出る
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_LEFT_CTRL));
        EXPECT_REPORT(driver, (KC_LEFT_GUI)); // Ctrl 解放
        EXPECT_EMPTY_REPORT(driver);          // つ 解放
        EXPECT_REPORT(driver, (KC_1));        // かなへ戻る
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_TSU, DN, 0},
          {POS_LCTL, DN, 50},
          {POS_NU, DN, 200},
          {POS_NU, UP, 220},
          {POS_LCTL, UP, 240},
          {POS_TSU, UP, 260},
          {POS_NU, DN, 400},
          {POS_NU, UP, 420}});
    VERIFY_AND_CLEAR(driver);
}

/*
 * 修飾のホールド中の英数レイヤー。かなベースから押す
 */

/* Ctrl → Win → Alt の順に重ねて押し、押した順に離す。どれかを離しても、残りを
 * 押している間は英数レイヤーのまま。Ctrl のホールド中に押した つ / さ は、英数
 * レイヤーの素の KC_LGUI / KC_LALT になる */
TEST_F(CtrlTapHold, ctrl_gui_alt_overlap_keeps_alpha) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_CTRL));                           // 250ms でホールド
        EXPECT_REPORT(driver, (KC_LEFT_CTRL, KC_LEFT_GUI));              // つ = 素の KC_LGUI
        EXPECT_REPORT(driver, (KC_LEFT_CTRL, KC_LEFT_GUI, KC_LEFT_ALT)); // さ = 素の KC_LALT
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_LEFT_ALT));               // Ctrl 解放
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_LEFT_ALT, KC_Q));         // 英数のまま
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_LEFT_ALT));
        EXPECT_REPORT(driver, (KC_LEFT_ALT)); // つ 解放
        EXPECT_REPORT(driver, (KC_LEFT_ALT, KC_Q)); // 英数のまま
        EXPECT_REPORT(driver, (KC_LEFT_ALT));
        EXPECT_EMPTY_REPORT(driver);   // さ 解放
        EXPECT_REPORT(driver, (KC_1)); // かなへ戻る
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_LCTL, DN, 0},
          {POS_TSU, DN, 300},
          {POS_SA, DN, 350},
          {POS_LCTL, UP, 400},
          {POS_NU, DN, 450},
          {POS_NU, UP, 470},
          {POS_TSU, UP, 500},
          {POS_NU, DN, 550},
          {POS_NU, UP, 570},
          {POS_SA, UP, 600},
          {POS_NU, DN, 650},
          {POS_NU, UP, 670}});
    VERIFY_AND_CLEAR(driver);
}

/* Win → Ctrl の順に押し、Win を先に離す。英数レイヤーが上がっていても Ctrl の位置は
 * 透過で MY_LCTL のままなので、Ctrl も最下段と同じ判定でホールドになる */
TEST_F(CtrlTapHold, gui_then_ctrl_release_gui_keeps_alpha) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_GUI));               // つ 250ms でホールド
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_LEFT_CTRL)); // Ctrl 250ms でホールド
        EXPECT_REPORT(driver, (KC_LEFT_CTRL));              // つ 解放
        EXPECT_REPORT(driver, (KC_LEFT_CTRL, KC_Q));        // 英数のまま
        EXPECT_REPORT(driver, (KC_LEFT_CTRL));
        EXPECT_EMPTY_REPORT(driver);   // Ctrl 解放
        EXPECT_REPORT(driver, (KC_1)); // かなへ戻る
        EXPECT_EMPTY_REPORT(driver);
    }
    play({{POS_TSU, DN, 0},
          {POS_LCTL, DN, 300},
          {POS_TSU, UP, 600},
          {POS_NU, DN, 650},
          {POS_NU, UP, 670},
          {POS_LCTL, UP, 700},
          {POS_NU, DN, 800},
          {POS_NU, UP, 820}});
    VERIFY_AND_CLEAR(driver);
}
