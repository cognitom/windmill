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

/* Android での Win (Meta) / Alt の扱い (issue #57)。
 *
 * Android は Win と Alt を押し離しの順序で特別扱いするので、ここはキーコードでは
 * なくレポート列そのものが仕様になる。固定したいのは次の2つ。
 *
 * - Win と Alt を両方含むレポートが1本も出ないこと。重なった状態でどちらかを
 *   離すと Caps Lock が切り替わる
 * - Win+. を Alt+. へ読み替える間、Win を「単独で押して離した」形にならないこと。
 *   Android はそれをアプリ一覧の呼び出しと見る。Win を離す前と押し直した後に、
 *   右Ctrlの空打ちが1回ずつ挟まっていることで見る
 *
 * 接続先が Windows のときに何も変わらないことも併せて見る。Win+Alt の重ね押しの
 * ほうは test_hold_layer.cpp (gui_and_alt_overlap_keeps_alpha) が持っている。 */

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_layer.h"
#include "action_util.h"
#include "test_keymap.hpp"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;

class WinMods : public WindmillTest {};

/* is_android はEEPROMに残るので、Android向けのテストはスイートを分ける
 * (test_lang_toggle.cpp と同じ) */
class AndroidMods : public WindmillTest {};

// 設定レイヤーの MY_ANDR でAndroid向けにする。レポートは出ない
static void select_android(WindmillTest* f, TestDriver& driver) {
    EXPECT_NO_REPORT(driver);
    f->tap_on_conf(POS_ANDR);
    f->settle();
    VERIFY_AND_CLEAR(driver);
}

// 英数から MY_LCTL 1回タップでかなへ切り替える。レポートの中身は問わない
static void switch_to_kana(WindmillTest* f, TestDriver& driver) {
    EXPECT_ANY_REPORT(driver).Times(AnyNumber());

    f->tap_key(f->key(POS_LCTL), 120);
    f->settle();

    VERIFY_AND_CLEAR(driver);
}

// Win+. を Alt+. に読み替えた1回ぶん。前後は Win だけが押されている状態
static void expect_gui_dot_as_alt_dot(TestDriver& driver) {
    EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_RIGHT_CTRL)); // 空打ち。Win単独の扱いを取り消す
    EXPECT_REPORT(driver, (KC_LEFT_GUI));
    EXPECT_EMPTY_REPORT(driver);                         // Win を外す
    EXPECT_REPORT(driver, (KC_LEFT_ALT));
    EXPECT_REPORT(driver, (KC_LEFT_ALT, KC_DOT));        // Alt+.
    EXPECT_REPORT(driver, (KC_LEFT_ALT));
    EXPECT_EMPTY_REPORT(driver);
    EXPECT_REPORT(driver, (KC_LEFT_GUI));                // Win を押し直す
    EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_RIGHT_CTRL)); // もう一度空打ち
    EXPECT_REPORT(driver, (KC_LEFT_GUI));
}

// Windows では Win+. はそのまま
TEST_F(WinMods, gui_dot_is_sent_as_is) {
    TestDriver driver;
    set_windmill_keymap();

    auto gui = key(POS_TSU); // 英数レイヤーでは KC_LGUI
    auto dot = key(POS_RI);  // 英数レイヤーでは KC_DOT

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_DOT));
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_EMPTY_REPORT(driver);
    }

    gui.press();
    run_one_scan_loop();
    tap_key(dot, 50);
    idle_for(50);
    gui.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* Win を押している間の Alt は出さない。Alt を離しても何も出ず、
 * Win のショートカットはそのまま打てる */
TEST_F(AndroidMods, alt_after_gui_is_not_sent) {
    TestDriver driver;
    set_windmill_keymap();
    select_android(this, driver);

    auto gui = key(POS_TSU);
    auto alt = key(POS_SA); // 英数レイヤーでは KC_LALT
    auto q   = key(POS_NU); // 英数レイヤーでは KC_Q

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_Q)); // Alt は乗らない
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_EMPTY_REPORT(driver);
    }

    gui.press();
    run_one_scan_loop();
    alt.press();
    run_one_scan_loop();
    idle_for(50);
    tap_key(q, 50);
    idle_for(50);
    alt.release();
    run_one_scan_loop();
    idle_for(50);
    gui.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* 逆順も同じ。先に押した Alt だけが出る。
 * 先に押したほうを離しても、握りつぶした Win が遅れて出ることはない */
TEST_F(AndroidMods, gui_after_alt_is_not_sent) {
    TestDriver driver;
    set_windmill_keymap();
    select_android(this, driver);

    auto gui = key(POS_TSU);
    auto alt = key(POS_SA);
    auto q   = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_ALT));
        EXPECT_EMPTY_REPORT(driver);   // Alt を離す。Win は出てこない
        EXPECT_REPORT(driver, (KC_Q)); // Win を押したままでも素の Q
        EXPECT_EMPTY_REPORT(driver);
    }

    alt.press();
    run_one_scan_loop();
    gui.press();
    run_one_scan_loop();
    idle_for(50);
    alt.release();
    run_one_scan_loop();
    idle_for(50);
    tap_key(q, 50);
    idle_for(50);
    gui.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

// 握りつぶしは押している間だけ。離せば次からは普通に出る
TEST_F(AndroidMods, blocked_mod_works_again_after_release) {
    TestDriver driver;
    set_windmill_keymap();
    select_android(this, driver);

    auto gui = key(POS_TSU);
    auto alt = key(POS_SA);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_ALT)); // 単独で押し直した Alt
        EXPECT_EMPTY_REPORT(driver);
    }

    gui.press();
    run_one_scan_loop();
    alt.press();
    run_one_scan_loop();
    idle_for(50);
    alt.release();
    run_one_scan_loop();
    gui.release();
    run_one_scan_loop();
    idle_for(120);

    alt.press();
    run_one_scan_loop();
    idle_for(50);
    alt.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* かなレイヤーで「つ」「さ」を続けて打つ。issue #57 の元になった打ち方。
 *
 * 「さ」の押下で「つ」のホールドが確定し (HOLD_ON_OTHER_KEY_PRESS)、「さ」は
 * 上がった英数レイヤーの素の KC_LALT として解決される。それを握りつぶす。
 * 握りつぶした Alt はホストへ出ないので、英数レイヤーを上げた数に入れてしまうと
 * 下ろす機会が無くなる。離したあとにかなへ戻っていることを「ぬ」で見る */
TEST_F(AndroidMods, kana_roll_over_gui_and_alt_sends_gui_only) {
    TestDriver driver;
    set_windmill_keymap();
    select_android(this, driver);
    switch_to_kana(this, driver);

    auto tsu = key(POS_TSU); // つ  LGUI_T(KC_Z)
    auto sa  = key(POS_SA);  // さ  LALT_T(KC_X)
    auto nu  = key(POS_NU);  // ぬ  かな = KC_1 / 英数 = KC_Q

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_GUI)); // さ の押下でホールド確定。Alt は出ない
        EXPECT_EMPTY_REPORT(driver);          // つ 解放
        EXPECT_REPORT(driver, (KC_1));        // さ を押したままでも、かなへ戻っている
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_1));        // さ を離したあとも
        EXPECT_EMPTY_REPORT(driver);
    }

    tsu.press();
    run_one_scan_loop();
    sa.press();
    run_one_scan_loop();
    idle_for(50);
    tsu.release();
    run_one_scan_loop();
    idle_for(50);
    tap_key(nu, 50);
    idle_for(50);
    sa.release();
    run_one_scan_loop();
    idle_for(120);
    tap_key(nu, 50);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA); // 次のテストへかなを持ち越さない
}

/* Win+. は Alt+. として出る。Win を押したまま続けて打てば、そのたびに出る。
 * 読み替えのあとも Win は押されたままなので、他のショートカットへ続けられる */
TEST_F(AndroidMods, gui_dot_is_sent_as_alt_dot) {
    TestDriver driver;
    set_windmill_keymap();
    select_android(this, driver);

    auto gui = key(POS_TSU);
    auto dot = key(POS_RI);
    auto q   = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        expect_gui_dot_as_alt_dot(driver);
        expect_gui_dot_as_alt_dot(driver);
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_Q));
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_EMPTY_REPORT(driver);
    }

    gui.press();
    run_one_scan_loop();
    idle_for(50);
    tap_key(dot, 50); // 離すときは何も出ない
    idle_for(50);
    tap_key(dot, 50);
    idle_for(50);
    tap_key(q, 50);
    idle_for(50);
    gui.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* かなレイヤーでも同じ。「つ」のホールドで英数レイヤーが上がり、「り」の位置が
 * ピリオドになる。Win をいったん外しても英数レイヤーは下がらず (続く「ぬ」の
 * 位置が KC_Q)、「つ」を離せばかなへ戻る */
TEST_F(AndroidMods, kana_gui_dot_is_sent_as_alt_dot) {
    TestDriver driver;
    set_windmill_keymap();
    select_android(this, driver);
    switch_to_kana(this, driver);

    auto tsu = key(POS_TSU);
    auto ri  = key(POS_RI); // り  MY_L。英数レイヤーでは KC_DOT
    auto nu  = key(POS_NU);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_GUI)); // り の押下でホールド確定
        expect_gui_dot_as_alt_dot(driver);
        EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_Q)); // 英数レイヤーのまま
        EXPECT_REPORT(driver, (KC_LEFT_GUI));
        EXPECT_EMPTY_REPORT(driver);                // つ 解放
        EXPECT_REPORT(driver, (KC_1));              // かなへ戻る
        EXPECT_EMPTY_REPORT(driver);
    }

    tsu.press();
    run_one_scan_loop();
    ri.press(); // TAPPING_TERM を待たずホールドが確定する
    run_one_scan_loop();
    idle_for(50);
    ri.release();
    run_one_scan_loop();
    idle_for(50);
    tap_key(nu, 50);
    idle_for(50);
    tsu.release();
    run_one_scan_loop();
    idle_for(120);
    tap_key(nu, 50);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA); // 次のテストへかなを持ち越さない
}

// Win を押していなければ、Android でもピリオドはそのまま。Alt+. も素通し
TEST_F(AndroidMods, dot_without_gui_is_sent_as_is) {
    TestDriver driver;
    set_windmill_keymap();
    select_android(this, driver);

    auto alt = key(POS_SA);
    auto dot = key(POS_RI);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_DOT));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_ALT));
        EXPECT_REPORT(driver, (KC_LEFT_ALT, KC_DOT));
        EXPECT_REPORT(driver, (KC_LEFT_ALT));
        EXPECT_EMPTY_REPORT(driver);
    }

    tap_key(dot, 50);
    idle_for(50);
    alt.press();
    run_one_scan_loop();
    tap_key(dot, 50);
    idle_for(50);
    alt.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}
