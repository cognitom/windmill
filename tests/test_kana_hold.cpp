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


/* かなレイヤー最下段 (つ さ そ ひ こ み も ね) のタップとホールドの判定 (issue #79)。
 *
 * - 判定は別のキーの押下時。押されてから KANA_HOLD_TERM (150ms) 以上ならホールド、
 *   未満ならタップ。タップならその場でかなが出て、別のキーはかなのまま解決される
 * - 別のキーを押さずに離したらタップ。長押ししてもタップ
 * - ホールドなら、別のキーは上げたレイヤーで解決される
 *
 * 境界の前後は ±3ms で見る。QMK のキーイベントの時刻は最下位ビットを立てて
 * (timer_read() | 1) 記録されるので、テストのスキャン数と時刻の差が1ずれうる。 */

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_layer.h"
#include "action_util.h"
#include "test_keymap.hpp"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;

class KanaHold : public WindmillTest {};

// 起動直後の英数から MY_LCTL 1回タップでかなへ切り替える。レポートの中身は問わない
static void switch_to_kana(WindmillTest* f, TestDriver& driver) {
    EXPECT_ANY_REPORT(driver).Times(AnyNumber());

    f->tap_key(f->key(POS_LCTL), 120);
    f->settle();

    VERIFY_AND_CLEAR(driver);
}

/* hold を押して、押してから約 elapsed ms 後に「ぬ」を叩き、hold を離す。
 * press() の直後のスキャンと、ぬ を押すスキャンとで 1ms ぶん進むので差し引く */
static void press_then_nu(WindmillTest* f, KeymapKey hold, unsigned elapsed) {
    auto nu = f->key(POS_NU);

    hold.press();
    f->run_one_scan_loop();
    f->idle_for(elapsed - 1);
    nu.press();
    f->run_one_scan_loop();
    f->idle_for(50);
    nu.release();
    f->run_one_scan_loop();
    f->idle_for(50);
    hold.release();
    f->run_one_scan_loop();
    f->idle_for(120);
}

static constexpr unsigned BEFORE_TERM = WindmillTest::KANA_HOLD_TERM - 3;
static constexpr unsigned AFTER_TERM  = WindmillTest::KANA_HOLD_TERM + 3;

// 境界の手前ではタップ。つ が先に出て、ぬ はかなのまま
static void expect_tap_then_nu(WindmillTest* f, TestDriver& driver, uint8_t row, uint8_t col, uint8_t tap) {
    {
        InSequence s;
        EXPECT_REPORT(driver, (tap));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_1)); // ぬ
        EXPECT_EMPTY_REPORT(driver);
    }
    press_then_nu(f, f->key(row, col), BEFORE_TERM);
    VERIFY_AND_CLEAR(driver);
}

// つ: Win。境界を越えると、ぬ は英数レイヤーの q になり Win が乗る
TEST_F(KanaHold, tsu_switches_at_hold_term) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    expect_tap_then_nu(this, driver, POS_TSU, KC_Z);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_Q));
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_EMPTY_REPORT(driver);
    }
    press_then_nu(this, key(POS_TSU), AFTER_TERM);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA); // 次のテストへかなを持ち越さない
}

// ひ: Sym。境界を越えると、ぬ は記号レイヤーの 1 になる (かなベースなので英数へ切り替えて送る)
TEST_F(KanaHold, hi_switches_at_hold_term) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    expect_tap_then_nu(this, driver, POS_HI, KC_V);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LNG2));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_1));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LNG1));
        EXPECT_EMPTY_REPORT(driver);
    }
    press_then_nu(this, key(POS_HI), AFTER_TERM);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

/* こ: Shift と Shift 面。境界を越えると Shift が乗る。ぬ は Shift 面で透過なので、
 * Shift を外して「ぬ」を出す */
TEST_F(KanaHold, ko_switches_at_hold_term) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    expect_tap_then_nu(this, driver, POS_KO, KC_B);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_1));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver); // こ 解放
    }
    press_then_nu(this, key(POS_KO), AFTER_TERM);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

// 他のキーを押さずに離せば、どれだけ長く押してもタップ。離した時点で出る
TEST_F(KanaHold, long_press_alone_is_tap) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto tsu = key(POS_TSU);

    EXPECT_NO_REPORT(driver);
    tsu.press();
    run_one_scan_loop();
    idle_for(1000);
    VERIFY_AND_CLEAR(driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_Z));
        EXPECT_EMPTY_REPORT(driver);
    }
    tsu.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

/* 左右の Sym (ひ / も) も参照カウントで持つ。先に離したほうで記号レイヤーが
 * 落ちない */
TEST_F(KanaHold, sym_handover_keeps_sym_layer) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto hi = key(POS_HI);
    auto mo = key(POS_MO);
    auto at = key(1, 2); // 記号レイヤー = @ / かな = て

    // かなベースなので英数へ切り替えて送る
    auto expect_at = [&]() {
        EXPECT_REPORT(driver, (KC_LNG2));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_2));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LNG1));
        EXPECT_EMPTY_REPORT(driver);
    };
    {
        InSequence s;
        expect_at();               // ひ も ともにホールド
        expect_at();               // ひ を離しても記号レイヤー
        EXPECT_REPORT(driver, (KC_W)); // 両方離せばかな
        EXPECT_EMPTY_REPORT(driver);
    }

    hi.press();
    run_one_scan_loop();
    idle_for(200);
    mo.press(); // ひ のホールド確定
    run_one_scan_loop();
    idle_for(200);
    tap_key(at, 50); // も のホールド確定
    idle_for(50);
    hi.release();
    run_one_scan_loop();
    idle_for(50);
    tap_key(at, 50);
    idle_for(50);
    mo.release();
    run_one_scan_loop();
    idle_for(50);
    tap_key(at, 50);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

/* Shift 面の最下段も、かなレイヤーと同じホールドの役割を持つ。タップで出るものだけが
 * 変わる。こ を Shift にしたまま、つ の位置をタップすれば「っ」、ホールドすれば Win */
TEST_F(KanaHold, shift_face_bottom_row_keeps_hold_role) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto ko  = key(POS_KO);
    auto tsu = key(POS_TSU);
    auto nu  = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // つ の押下で こ のホールド確定
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_Z)); // っ
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_LEFT_GUI)); // ぬ の押下で つ のホールド確定
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_LEFT_GUI, KC_Q)); // 英数レイヤーの q
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_LEFT_GUI));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // つ 解放
        EXPECT_EMPTY_REPORT(driver);            // こ 解放
    }

    ko.press();
    run_one_scan_loop();
    idle_for(200);
    tap_key(tsu, 50); // 他のキーを押さずに離したのでタップ
    idle_for(50);
    tsu.press();
    run_one_scan_loop();
    idle_for(200);
    tap_key(nu, 50);
    idle_for(50);
    tsu.release();
    run_one_scan_loop();
    idle_for(50);
    ko.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}
