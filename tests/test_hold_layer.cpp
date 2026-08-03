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

/* 修飾ホールド中の英数レイヤーを検証する (issue #34)。
 *
 * かなレイヤーで Ctrl / Win(つ) / Alt(さ) をホールドしている間は英数レイヤーへ
 * 移り、離したら必ずかなへ戻ること。レイヤーそのものは覗かず、「ぬ」の位置
 * (かな = KC_1、英数 = KC_Q) を打ってレポートで判定する。
 *
 * 「離したら戻る」ほうが本題。Win/Alt は英数レイヤーでは素の KC_LGUI / KC_LALT
 * なので、レイヤーを上げている最中の解放イベントは mod-tap として届くとは
 * 限らない。キーイベント頼みで layer_off() すると、そこから英数レイヤーへ
 * 貼りついたまま戻れなくなる。
 *
 * 起動直後のベースレイヤーは英数 (issue #22) なので、かなレイヤー上の挙動を
 * 見るには先に MY_LCTL をダブルタップしてかなへ切り替えておく必要がある。 */

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_layer.h"
#include "action_util.h"
#include "test_keymap.hpp"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;

class HoldLayer : public WindmillTest {};

// MY_LCTL ダブルタップでかなへ切り替える。レポートの中身は問わない
static void switch_to_kana(WindmillTest* f, TestDriver& driver) {
    EXPECT_ANY_REPORT(driver).Times(AnyNumber());

    auto lctl = f->key(POS_LCTL);
    for (int i = 0; i < 2; ++i) {
        lctl.press();
        f->run_one_scan_loop();
        f->idle_for(120);
        lctl.release();
        f->run_one_scan_loop();
        if (i == 0) f->idle_for(60); // TD_DTAP_TERM未満
    }
    f->settle();

    VERIFY_AND_CLEAR(driver);
}

/* Win(つ) のホールドで英数レイヤーへ移り、離せばかなへ戻る。
 * 戻るところが issue #34 の再発防止 */
TEST_F(HoldLayer, gui_hold_switches_to_alpha_and_back) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto tsu = key(POS_TSU);
    auto nu  = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_GUI));         // ホールド確定
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_Q));   // ぬ の位置が英数レイヤーの q になる
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_EMPTY_REPORT(driver);                  // つ 解放
        EXPECT_REPORT(driver, (KC_1));                // かなへ戻っている (ぬ)
        EXPECT_EMPTY_REPORT(driver);
    }

    tsu.press();
    run_one_scan_loop();
    idle_for(250); // TAPPING_TERM 超え。ホールド確定
    tap_key(nu, 120);
    idle_for(120);
    tsu.release();
    run_one_scan_loop();
    idle_for(120);
    tap_key(nu, 120);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

// Alt(さ) も同じ
TEST_F(HoldLayer, alt_hold_switches_to_alpha_and_back) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto sa = key(POS_SA);
    auto nu = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_ALT));
        EXPECT_REPORT(driver, (KC_LEFT_ALT, KC_Q));
        EXPECT_REPORT(driver, (KC_LEFT_ALT));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_1));
        EXPECT_EMPTY_REPORT(driver);
    }

    sa.press();
    run_one_scan_loop();
    idle_for(250);
    tap_key(nu, 120);
    idle_for(120);
    sa.release();
    run_one_scan_loop();
    idle_for(120);
    tap_key(nu, 120);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* 別キー割り込みでホールドが確定する経路 (HOLD_ON_OTHER_KEY_PRESS)。
 * TAPPING_TERM を待たずに確定するので、割り込んだキー自身が英数レイヤーで
 * 解決されないと Win+Q が Win+1 になってしまう */
TEST_F(HoldLayer, gui_hold_by_interrupt_resolves_on_alpha) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto tsu = key(POS_TSU);
    auto nu  = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_GUI));       // ぬ の押下でホールド確定
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_Q));
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_1));
        EXPECT_EMPTY_REPORT(driver);
    }

    tsu.press();
    run_one_scan_loop();
    nu.press(); // TAPPING_TERM を待たず、ここでホールドが確定する
    run_one_scan_loop();
    idle_for(120);
    nu.release();
    run_one_scan_loop();
    idle_for(120);
    tsu.release();
    run_one_scan_loop();
    idle_for(120);
    tap_key(nu, 120);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

// タップ (つ / さ) はレイヤーを動かさない
TEST_F(HoldLayer, tap_stays_on_kana) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto tsu = key(POS_TSU);
    auto sa  = key(POS_SA);
    auto nu  = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_Z)); // つ
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_X)); // さ
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_1)); // ぬ。かなのまま
        EXPECT_EMPTY_REPORT(driver);
    }

    tap_key(tsu, 120);
    idle_for(120);
    tap_key(sa, 120);
    idle_for(120);
    tap_key(nu, 120);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* Ctrl と Win の重ね押し。先に離したほうのぶんだけレイヤーが下がり、
 * 残っているほうが押されている限り英数レイヤーは続く。
 *
 * Ctrlが先なので、つ の押下時点では既に英数レイヤーが上がっていて、
 * その位置は素の KC_LGUI として解決される (mod-tapを通らない) */
TEST_F(HoldLayer, ctrl_and_gui_overlap_keeps_alpha) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto lctl = key(POS_LCTL);
    auto tsu  = key(POS_TSU);
    auto nu   = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_CTRL));
        EXPECT_REPORT(driver, (KC_LEFT_CTRL, KC_LEFT_GUI));
        EXPECT_REPORT(driver, (KC_LEFT_GUI));       // Ctrl だけ離す
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_Q)); // Win が残っているので英数のまま
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_EMPTY_REPORT(driver);                // つ 解放
        EXPECT_REPORT(driver, (KC_1));              // かなへ戻っている
        EXPECT_EMPTY_REPORT(driver);
    }

    lctl.press();
    run_one_scan_loop();
    idle_for(250); // Ctrl ホールド確定
    tsu.press();
    run_one_scan_loop();
    idle_for(250);
    lctl.release();
    run_one_scan_loop();
    idle_for(120);
    tap_key(nu, 120);
    idle_for(120);
    tsu.release();
    run_one_scan_loop();
    idle_for(120);
    tap_key(nu, 120);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

// Win と Alt の重ね押しも同じ
TEST_F(HoldLayer, gui_and_alt_overlap_keeps_alpha) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto tsu = key(POS_TSU);
    auto sa  = key(POS_SA);
    auto nu  = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_LEFT_ALT));
        EXPECT_REPORT(driver, (KC_LEFT_ALT));       // Win だけ離す
        EXPECT_REPORT(driver, (KC_LEFT_ALT, KC_Q)); // Alt が残っているので英数のまま
        EXPECT_REPORT(driver, (KC_LEFT_ALT));
        EXPECT_EMPTY_REPORT(driver);                // さ 解放
        EXPECT_REPORT(driver, (KC_1));
        EXPECT_EMPTY_REPORT(driver);
    }

    tsu.press();
    run_one_scan_loop();
    idle_for(250);
    sa.press();
    run_one_scan_loop();
    idle_for(250);
    tsu.release();
    run_one_scan_loop();
    idle_for(120);
    tap_key(nu, 120);
    idle_for(120);
    sa.release();
    run_one_scan_loop();
    idle_for(120);
    tap_key(nu, 120);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* Win をホールドしたあと、MY_LCTL のダブルタップでかなへ戻れること。
 *
 * layer_off() を取りこぼすと layer_state に英数が残り、ベースレイヤーを
 * かなへ切り替えても上から英数で潰されて戻れなくなる (issue #34 の症状) */
TEST_F(HoldLayer, kana_switch_works_after_gui_hold) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto tsu = key(POS_TSU);
    auto nu  = key(POS_NU);

    EXPECT_ANY_REPORT(driver).Times(AnyNumber()); // ホールド中のレポートは問わない
    tsu.press();
    run_one_scan_loop();
    idle_for(250); // ホールド確定
    tsu.release();
    run_one_scan_loop();
    settle();
    VERIFY_AND_CLEAR(driver);

    switch_to_kana(this, driver); // MY_LCTL でかなへ切り替え直す

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_1)); // ぬ
        EXPECT_EMPTY_REPORT(driver);
    }

    tap_key(nu, 120);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}
