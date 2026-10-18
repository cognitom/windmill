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

/* かなレイヤー最下段のタップ・ホールドの判定 (issue #83)。
 *
 * 判定の規則は windmill.c の「かなレイヤー最下段のタップ・ホールド」にある。
 * ここでは issue に挙げた押し方 (ケース a〜o) をそのままレポート列で固定し、
 * 150ms と 250ms の前後で結果が切り替わること、待たせている間はレポートが
 * 出ないこと、待っていたキーが押した順に出ることを見る。
 *
 * 押し方は「押してからの時刻」の並びで書く (Timeline)。キーの押し離しは、
 * 書いた時刻のスキャンで処理される。結果の文字は IME が出すもので、Shift+わ は
 * 「を」、Shift+あ は「ぁ」になる (この issue ではまだ Shift 面が無いので、
 * Shift+キーのレポートとして届く)。 */

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_layer.h"
#include "action_util.h"
#include "test_keymap.hpp"

#include <vector>

using testing::_;
using testing::AnyNumber;
using testing::InSequence;
using testing::InvokeWithoutArgs;

class TapHold : public WindmillTest {
   public:
    // 起動直後の英数から MY_LCTL 1回タップでかなへ切り替える。レポートの中身は問わない
    void switch_to_kana(TestDriver& driver) {
        EXPECT_ANY_REPORT(driver).Times(AnyNumber());
        tap_key(key(POS_LCTL), 120);
        settle();
        VERIFY_AND_CLEAR(driver);
    }

    void TearDown() override {
        default_layer_set((layer_state_t)1 << LAYER_ALPHA); // 次のテストへかなを持ち越さない
        WindmillTest::TearDown();
    }
};

// 設定 (MY_WIN / MY_ANDR) はEEPROMに残るので、ケース m はスイートを分ける
class TapHoldConf : public TapHold {};

/* 押し方の並び。at は最初の押下からの時刻 (ms)。
 * run() は書いた時刻のスキャンで押し離しが処理されるように進める。
 *
 * 時刻はタイマーの絶対値で合わせる。IME向けのウェイト (wait_ms) はテストでも
 * タイマーを進めるので、経過を idle_for() の積み上げで数えるとずれていく */
struct Timeline {
    struct Step {
        unsigned  at;
        KeymapKey key;
        bool      pressed;
    };

    WindmillTest*     f;
    std::vector<Step> steps;
    unsigned          now   = 0;
    uint16_t          start = timer_read();

    Timeline& down(unsigned at, KeymapKey key) {
        steps.push_back({at, key, true});
        return *this;
    }
    Timeline& up(unsigned at, KeymapKey key) {
        steps.push_back({at, key, false});
        return *this;
    }

    // until の時刻の手前まで進める (until のスキャンはまだ走らない)
    void run_until(unsigned until) {
        for (auto& step : steps) {
            if (step.at >= until) break;
            advance_to(step.at);
            step.pressed ? step.key.press() : step.key.release();
        }
        advance_to(until);
    }

    // 残りを全て流して、落ち着くまで待つ
    void run() {
        for (auto& step : steps) {
            if (step.at < now) continue;
            advance_to(step.at);
            step.pressed ? step.key.press() : step.key.release();
        }
        f->run_one_scan_loop();
        f->settle();
    }

    void advance_to(unsigned at) {
        // 1スキャンずつ進める。途中のスキャンでウェイトが走ると、その分だけ早く着く
        while (uint16_t(timer_read() - start) < at) f->run_one_scan_loop();
        now = at;
    }
};

/*
 * issue #83 のケース a〜o
 */

// a: こ↓0 み↓100 わ↓200 わ↑230 み↑240 こ↑250 → をみ
TEST_F(TapHold, a_ko_mi_wa_gives_wo_mi) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto ko = key(POS_KO), mi = key(POS_MI), wa = key(POS_WA);
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));       // 150ms で こ がホールド (後から み が押されている)
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_0)); // わ は待たない (み の相方 こ がホールド中) → を
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_N)); // み は こ が決まる前に押したのでスペースではない → み
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver); // こ 解放
    }
    Timeline{this}.down(0, ko).down(100, mi).down(200, wa).up(230, wa).up(240, mi).up(250, ko).run();
    VERIFY_AND_CLEAR(driver);
}

// b: こ↓0 み↓20 わ↓40 わ↑60 み↑70 こ↑80 → こみわ
TEST_F(TapHold, b_quick_roll_gives_ko_mi_wa) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto ko = key(POS_KO), mi = key(POS_MI), wa = key(POS_WA);
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_B)); // こ
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_N)); // み
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_0)); // わ
        EXPECT_EMPTY_REPORT(driver);
    }
    Timeline{this}.down(0, ko).down(20, mi).down(40, wa).up(60, wa).up(70, mi).up(80, ko).run();
    VERIFY_AND_CLEAR(driver);
}

// c: こ↓0 わ↓100 わ↑130 こ↑160 → を
TEST_F(TapHold, c_wa_within_150ms_waits_and_gets_shift) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto ko = key(POS_KO), wa = key(POS_WA);
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_0)); // 待っていた わ の押し離し
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }
    Timeline{this}.down(0, ko).down(100, wa).up(130, wa).up(160, ko).run();
    VERIFY_AND_CLEAR(driver);
}

// d: こ↓0 わ↓200 わ↑230 こ↑260 → を
TEST_F(TapHold, d_wa_after_150ms_decides_hold) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto ko = key(POS_KO), wa = key(POS_WA);
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // わ の押下で こ がホールドに決まる
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_0));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }
    Timeline{this}.down(0, ko).down(200, wa).up(230, wa).up(260, ko).run();
    VERIFY_AND_CLEAR(driver);
}

// e: こ↓0 こ↑200 → こ
TEST_F(TapHold, e_release_before_250ms_is_tap) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto ko = key(POS_KO);
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_B));
        EXPECT_EMPTY_REPORT(driver);
    }
    Timeline{this}.down(0, ko).up(200, ko).run();
    VERIFY_AND_CLEAR(driver);
}

// f: こ↓0 こ↑300 → 何も出ない (左Shift の押し離しだけ)
TEST_F(TapHold, f_long_press_is_shift_only) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto ko = key(POS_KO);
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }
    Timeline{this}.down(0, ko).up(300, ko).run();
    VERIFY_AND_CLEAR(driver);
}

// g: こ↓0 み↓200 み↑230 こ↑260 → 半角スペース
TEST_F(TapHold, g_mi_during_ko_shift_gives_space) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto ko = key(POS_KO), mi = key(POS_MI);
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // み の押下の次のスキャンで こ がホールドに決まる
        EXPECT_EMPTY_REPORT(driver);            // Shiftを外す
        EXPECT_REPORT(driver, (KC_SPACE));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // Shiftを戻す
        EXPECT_EMPTY_REPORT(driver);            // こ 解放
    }
    Timeline{this}.down(0, ko).down(200, mi).up(230, mi).up(260, ko).run();
    VERIFY_AND_CLEAR(driver);
}

// h: こ↓0 あ↓200 あ↑230 み↓300 こ↑350 わ↓400 わ↑430 み↑470 → ぁを
TEST_F(TapHold, h_handover_mi_decided_by_time) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto ko = key(POS_KO), mi = key(POS_MI), a = key(POS_A), wa = key(POS_WA);
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));        // あ の押下で こ がホールドに決まる
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_3));  // ぁ
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);                   // こ 解放
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));       // 450ms (み から 150ms、後から わ) で み がホールド
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT, KC_0)); // 待っていた わ → を
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);                   // み 解放
    }
    Timeline{this}.down(0, ko).down(200, a).up(230, a).down(300, mi).up(350, ko).down(400, wa).up(430, wa).up(470, mi).run();
    VERIFY_AND_CLEAR(driver);
}

// i: こ↓0 あ↓200 あ↑230 み↓300 こ↑350 わ↓500 わ↑530 み↑550 → ぁを
TEST_F(TapHold, i_handover_mi_decided_by_press) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto ko = key(POS_KO), mi = key(POS_MI), a = key(POS_A), wa = key(POS_WA);
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_3));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT)); // わ の押下で み がホールドに決まる
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT, KC_0));
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }
    Timeline{this}.down(0, ko).down(200, a).up(230, a).down(300, mi).up(350, ko).down(500, wa).up(530, wa).up(550, mi).run();
    VERIFY_AND_CLEAR(driver);
}

// j: ひ↓0 も↓100 た↓200 た↑230 も↑240 ひ↑250 → !も
TEST_F(TapHold, j_hi_mo_ta_gives_exlm_mo) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto hi = key(POS_HI), mo = key(POS_MO), ta = key(POS_TA);
    {
        InSequence s;
        // ひ が 150ms でホールド。た は記号レイヤーの「!」で、かなベースなので英数を挟んで送る
        EXPECT_REPORT(driver, (KC_LNG2));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_1));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LNG1));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_M)); // も は記号レイヤーでも透過なので、も のまま
        EXPECT_EMPTY_REPORT(driver);
    }
    Timeline{this}.down(0, hi).down(100, mo).down(200, ta).up(230, ta).up(240, mo).up(250, hi).run();
    VERIFY_AND_CLEAR(driver);
}

// k: ひ↓0 も↓300 も↑350 ひ↑400 → も
TEST_F(TapHold, k_mo_tap_under_sym_layer) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto hi = key(POS_HI), mo = key(POS_MO);
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_M));
        EXPECT_EMPTY_REPORT(driver);
    }
    Timeline{this}.down(0, hi).down(300, mo).up(350, mo).up(400, hi).run();
    VERIFY_AND_CLEAR(driver);
}

// l: そ↓0 ね↓100 ぬ↓200 ぬ↑230 ね↑240 そ↑250 → F1ね
TEST_F(TapHold, l_so_ne_nu_gives_f1_ne) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto so = key(POS_FN_L), ne = key(POS_FN_R), nu = key(POS_NU);
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_F1)); // そ が 150ms でホールド。ぬ は Fnレイヤーで解決
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_COMMA)); // ね
        EXPECT_EMPTY_REPORT(driver);
    }
    Timeline{this}.down(0, so).down(100, ne).down(200, nu).up(230, nu).up(240, ne).up(250, so).run();
    VERIFY_AND_CLEAR(driver);
}

// m: そ↓0 ね↓50 た↓300 た↑330 ね↑340 そ↑350 → 設定レイヤーの た (MY_WIN)
TEST_F(TapHoldConf, m_so_ne_ta_reaches_conf_layer) {
    TestDriver driver;
    set_windmill_keymap();

    // 先に Android にしておき、MY_WIN で Windows へ戻ることで設定レイヤーに届いたと見る
    EXPECT_NO_REPORT(driver);
    tap_on_conf(POS_ANDR);
    VERIFY_AND_CLEAR(driver);
    switch_to_kana(driver);

    auto so = key(POS_FN_L), ne = key(POS_FN_R), ta = key(POS_TA);

    EXPECT_NO_REPORT(driver); // 設定レイヤーの MY_WIN はレポートを出さない。そ / ね も出ない
    Timeline{this}.down(0, so).down(50, ne).down(300, ta).up(330, ta).up(340, ne).up(350, so).run();
    VERIFY_AND_CLEAR(driver);

    // 英数から言語切替をタップすると、Windows なら KC_LNG1 (Android なら Ctrl+Space)
    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LNG1));
        EXPECT_EMPTY_REPORT(driver);
    }
    tap_key(key(POS_LCTL), 120);
    settle();
    VERIFY_AND_CLEAR(driver);
}

// n: つ↓0 つ↑300 → Win の押し離し
TEST_F(TapHold, n_tsu_long_press_is_gui_only) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto tsu = key(POS_TSU);
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_EMPTY_REPORT(driver);
    }
    Timeline{this}.down(0, tsu).up(300, tsu).run();
    VERIFY_AND_CLEAR(driver);
}

// o: さ↓0 さ↑300 → Alt の押し離し
TEST_F(TapHold, o_sa_long_press_is_alt_only) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto sa = key(POS_SA);
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_ALT));
        EXPECT_EMPTY_REPORT(driver);
    }
    Timeline{this}.down(0, sa).up(300, sa).run();
    VERIFY_AND_CLEAR(driver);
}

/*
 * 150ms と 250ms の境界
 */

// 単独なら 249ms で離せばタップ
TEST_F(TapHold, release_at_249ms_is_tap) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto ko = key(POS_KO);
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_B));
        EXPECT_EMPTY_REPORT(driver);
    }
    Timeline{this}.down(0, ko).up(249, ko).run();
    VERIFY_AND_CLEAR(driver);
}

// 250ms ちょうどはホールド
TEST_F(TapHold, release_at_250ms_is_hold) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto ko = key(POS_KO);
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }
    Timeline{this}.down(0, ko).up(250, ko).run();
    VERIFY_AND_CLEAR(driver);
}

// 後から別のキーが押されていても、149ms で離せばタップ。待っていた わ はその後に出る
TEST_F(TapHold, release_at_149ms_after_other_key_is_tap) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto ko = key(POS_KO), wa = key(POS_WA);
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_B)); // こ
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_0)); // わ。Shift は乗らない
        EXPECT_EMPTY_REPORT(driver);
    }
    Timeline{this}.down(0, ko).down(50, wa).up(100, wa).up(149, ko).run();
    VERIFY_AND_CLEAR(driver);
}

// 150ms ちょうどは「150ms 以上」。後から別のキーが押されていればホールド
TEST_F(TapHold, release_at_150ms_after_other_key_is_hold) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto ko = key(POS_KO), wa = key(POS_WA);
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_0)); // を
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }
    Timeline{this}.down(0, ko).down(50, wa).up(100, wa).up(150, ko).run();
    VERIFY_AND_CLEAR(driver);
}

/* 149ms で押したキーは待たされ、150ms でホールドが決まってから出る。
 * 150ms ちょうどに押したキーは待たずにその場でホールドを決める。どちらも「を」だが、
 * 出る時刻が違う */
TEST_F(TapHold, press_at_149ms_waits_until_150ms) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto ko = key(POS_KO), wa = key(POS_WA);
    Timeline tl{this};
    tl.down(0, ko).down(149, wa).up(180, wa).up(200, ko);

    uint16_t start = tl.start, at_shift = 0, at_wa = 0;
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)).WillOnce(InvokeWithoutArgs([&] { at_shift = timer_read(); }));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_0)).WillOnce(InvokeWithoutArgs([&] { at_wa = timer_read(); }));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }
    tl.run();
    VERIFY_AND_CLEAR(driver);

    EXPECT_EQ(uint16_t(at_shift - start), 150);
    EXPECT_EQ(uint16_t(at_wa - start), 150);
}

TEST_F(TapHold, press_at_150ms_decides_hold_on_the_spot) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto ko = key(POS_KO), wa = key(POS_WA);
    Timeline tl{this};
    tl.down(0, ko).down(150, wa).up(180, wa).up(200, ko);

    // 150ms の手前までは何も出ない
    EXPECT_NO_REPORT(driver);
    uint16_t start = tl.start;
    tl.run_until(150);
    VERIFY_AND_CLEAR(driver);

    uint16_t at_wa = 0;
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_0)).WillOnce(InvokeWithoutArgs([&] { at_wa = timer_read(); }));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }
    tl.run();
    VERIFY_AND_CLEAR(driver);

    EXPECT_EQ(uint16_t(at_wa - start), 150);
}

/*
 * 待たせている間と、押した順
 */

// 待っている間はレポートが出ない。わ を押してから こ が決まる 150ms まで、何も届かない
TEST_F(TapHold, no_report_while_waiting) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto ko = key(POS_KO), wa = key(POS_WA);
    Timeline tl{this};
    tl.down(0, ko).down(100, wa).up(120, wa).up(200, ko);

    EXPECT_NO_REPORT(driver);
    tl.run_until(150); // わ は押して離したが、こ が未判定なので出ない
    VERIFY_AND_CLEAR(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_0));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }
    tl.run();
    VERIFY_AND_CLEAR(driver);
}

// 待っていたキーは押した順に出る。最下段のキーのタップ (み) もその順に並ぶ
TEST_F(TapHold, waiting_keys_keep_press_order) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto ko = key(POS_KO), mi = key(POS_MI), a = key(POS_A), wa = key(POS_WA);
    Timeline tl{this};
    tl.down(0, ko).down(20, a).up(30, a).down(40, mi).up(50, mi).down(60, wa).up(70, wa).up(100, ko);

    EXPECT_NO_REPORT(driver);
    tl.run_until(100); // こ を離すまでは何も出ない
    VERIFY_AND_CLEAR(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_B)); // こ
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_3)); // あ
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_N)); // み
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_0)); // わ
        EXPECT_EMPTY_REPORT(driver);
    }
    tl.run();
    VERIFY_AND_CLEAR(driver);
}

/* 相方がホールド中なら待たない。こ の Shift 中に み を押してすぐ わ を押しても、
 * わ はその場で出る (み の判定を待たない) */
TEST_F(TapHold, no_wait_while_partner_holds) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto ko = key(POS_KO), mi = key(POS_MI), wa = key(POS_WA);
    Timeline tl{this};
    tl.down(0, ko).down(300, mi).down(320, wa).up(340, wa).up(360, mi).up(400, ko);

    uint16_t start = tl.start, at_wa = 0;
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // 250ms で こ がホールド
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_0)).WillOnce(InvokeWithoutArgs([&] { at_wa = timer_read(); }));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        // み は こ の Shift 中に押したが、その後に わ を押したので何も出さない
        EXPECT_EMPTY_REPORT(driver); // こ 解放
    }
    tl.run();
    VERIFY_AND_CLEAR(driver);

    EXPECT_EQ(uint16_t(at_wa - start), 320);
}

/* 相方でないキーのホールドでは待たなくならない。そ (Fn) のホールド中に み を押して
 * すぐ わ を押すと、わ は み が決まる (み から 150ms) まで待ち、み の右Shift と
 * Fnレイヤーの上で解決される。Fnレイヤーの わ の位置は F10 */
TEST_F(TapHold, non_partner_hold_still_waits) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(driver);

    auto so = key(POS_FN_L), mi = key(POS_MI), wa = key(POS_WA);
    Timeline tl{this};
    tl.down(0, so).down(300, mi).down(320, wa).up(340, wa).up(500, mi).up(520, so);

    uint16_t start = tl.start, at_wa = 0;
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT)); // 450ms で み がホールド (後から わ が押されている)
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT, KC_F10)).WillOnce(InvokeWithoutArgs([&] { at_wa = timer_read(); }));
        EXPECT_REPORT(driver, (KC_RIGHT_SHIFT));
        EXPECT_EMPTY_REPORT(driver); // み 解放
    }
    tl.run();
    VERIFY_AND_CLEAR(driver);

    EXPECT_EQ(uint16_t(at_wa - start), 450);
}
