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

/* かなレイヤーで修飾キーをホールドしている間の英数レイヤーへの一時切り替え
 * (issue #34)。Ctrl (MY_LCTL) だけだったのを Win(つ) / Alt(さ) にも揃えた。
 *
 * レイヤーの状態を直接覗くのではなく、「ぬ」の位置を打ってレポートで判定する。
 * かなレイヤーでは KC_1、英数レイヤーでは KC_Q なので、どちらで解決されたかが
 * ホストへ出るレポートから分かる。
 *
 * 特に効かせたいのは「離せば必ずかなへ戻る」ほう。自分で英数レイヤーを上げると
 * つ/さ の解放は mod-tap (LGUI_T(KC_Z)) ではなく英数レイヤー側の素の KC_LGUI で
 * 引き直されることがあり、mod-tap のキーコードで解放を待つ実装だと英数レイヤーへ
 * 貼りついたまま戻れなくなる (update_hold_layer 参照)。
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

/* Win(つ) のホールド中は英数レイヤー。離せばかなへ戻る。
 * 戻るほうが issue #34 の修正で壊しやすい (旧実装ではここで貼りついた) */
TEST_F(HoldLayer, gui_hold_switches_to_alpha_and_back) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto tsu = key(POS_TSU); // つ  LGUI_T(KC_Z)
    auto nu  = key(POS_NU);  // ぬ  かな = KC_1 / 英数 = KC_Q

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_GUI));         // ホールド確定
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_Q));   // 英数レイヤーで解決される
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_EMPTY_REPORT(driver);                  // つ 解放
        EXPECT_REPORT(driver, (KC_1));                // かなへ戻っている
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

    auto sa = key(POS_SA); // さ  LALT_T(KC_X)
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
 * 割り込んだキー自身も英数レイヤーで解決される必要があるので、レイヤーを
 * 上げるのは押下の処理中でなければ間に合わない */
TEST_F(HoldLayer, gui_hold_by_interrupt_resolves_on_alpha) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto tsu = key(POS_TSU);
    auto nu  = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_GUI));       // ぬ の押下でホールド確定
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_Q)); // 割り込んだ ぬ も英数
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_EMPTY_REPORT(driver);
    }

    tsu.press();
    run_one_scan_loop();
    nu.press(); // TAPPING_TERM を待たずホールドが確定する
    run_one_scan_loop();
    idle_for(120);
    nu.release();
    run_one_scan_loop();
    idle_for(120);
    tsu.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

// タップ (つ) はレイヤーを動かさない。かなのまま「つ」が出る
TEST_F(HoldLayer, tap_stays_on_kana) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto tsu = key(POS_TSU);
    auto nu  = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_Z)); // つ
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_1)); // ぬ。かなのまま
        EXPECT_EMPTY_REPORT(driver);
    }

    tap_key(tsu, 50); // TAPPING_TERM 未満
    idle_for(120);
    tap_key(nu, 120);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* Ctrl + Win の重ね押し。先に離したほうのぶんだけ下がる。
 *
 * Ctrlが先に押されていると英数レイヤーが既に上がっているので、つ の押下は
 * mod-tap ではなく英数レイヤー側の素の KC_LGUI として解決される。そちらを
 * 数え損ねると、Ctrlを離した時点でかなへ戻ってしまう */
TEST_F(HoldLayer, ctrl_and_gui_overlap_keeps_alpha) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto lctl = key(POS_LCTL);
    auto tsu  = key(POS_TSU);
    auto nu   = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_CTRL));                  // Ctrl ホールド確定
        EXPECT_REPORT(driver, (KC_LEFT_CTRL, KC_LEFT_GUI));     // つ = 素の KC_LGUI
        EXPECT_REPORT(driver, (KC_LEFT_GUI));                   // Ctrl を先に離す
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_Q));             // Winが残るので英数のまま
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_EMPTY_REPORT(driver);                            // つ 解放
        EXPECT_REPORT(driver, (KC_1));                          // かなへ戻る
        EXPECT_EMPTY_REPORT(driver);
    }

    lctl.press();
    run_one_scan_loop();
    idle_for(250); // ホールド確定
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

// Win + Alt の重ね押しも同じ
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
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_LEFT_ALT)); // さ = 素の KC_LALT
        EXPECT_REPORT(driver, (KC_LEFT_ALT));              // つ を先に離す
        EXPECT_REPORT(driver, (KC_LEFT_ALT, KC_Q));        // Altが残るので英数のまま
        EXPECT_REPORT(driver, (KC_LEFT_ALT));
        EXPECT_EMPTY_REPORT(driver);
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

/* Winホールドのあとも MY_LCTL の英数/かな切り替えが効くこと。
 * 英数レイヤーが上がりっぱなしになると、ベースレイヤーをかなへ戻しても
 * 上から潰されて症状が出ない */
TEST_F(HoldLayer, base_layer_switch_works_after_gui_hold) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto lctl = key(POS_LCTL);
    auto tsu  = key(POS_TSU);
    auto nu   = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_GUI)); // Winホールド
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LNG2));     // MY_LCTL 1回タップ → 英数
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_Q));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LNG1));     // ダブルタップ → かな
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_1));        // かなへ戻せている
        EXPECT_EMPTY_REPORT(driver);
    }

    tsu.press();
    run_one_scan_loop();
    idle_for(250);
    tsu.release();
    run_one_scan_loop();
    idle_for(120);

    tap_key(lctl, 50); // 英数へ
    settle();
    tap_key(nu, 120);
    idle_for(120);

    for (int i = 0; i < 2; ++i) { // かなへ
        lctl.press();
        run_one_scan_loop();
        idle_for(50);
        lctl.release();
        run_one_scan_loop();
        if (i == 0) idle_for(60); // TD_DTAP_TERM未満
    }
    settle();
    tap_key(nu, 120);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}
