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

/* 接続先ごとのOS設定 (MY_WIN / MY_ANDR) のHIDレポートを検証する (issue #58)。
 *
 * 内部状態は覗かず、設定が効く2箇所を打鍵して出力で判定する。
 *
 * - 「ら」(MY_O) をShift付きで叩く。Windows/デスクトップ向けなら「{」=
 *   S(KC_LBRC)、Android向けなら S(KC_RBRC)
 * - Ctrl (MY_LCTL) のタップでの言語切替 (issue #53)。Windows向けなら
 *   KC_LNG1 / KC_LNG2、Android向けなら Ctrl+Space
 *
 * EEPROMはスイートの頭で初期化される (SetUpTestCase) が、テストの間では
 * 残る。同じスイートの前のテストが付けた設定を引きずらないよう、
 * 各テストが自分で見るスロットを明示して設定すること。 */

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_layer.h"
#include "action_util.h"
#include "test_keymap.hpp"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;

#define POS_FN 3, 3   // そ  LT(3,KC_C)  英数レイヤーでは MO(3)
#define POS_WIN 0, 0  // Fnレイヤーの MY_WIN
#define POS_ANDR 0, 3 // Fnレイヤーの MY_ANDR

/* 今つながっている先。windmill.c の weak な既定実装 (常に0) を差し替えて、
 * テストから切り替える。実機では geonix41.c がブロブの接続状態から決める。
 * 番号は geonix41.c の os_slots と同じ並び。 */
enum {
    SLOT_USB = 0,
    SLOT_BLE1,
    SLOT_BLE2,
    SLOT_BLE3,
    SLOT_2P4G,
};
static uint8_t connected_slot = SLOT_USB;

extern "C" uint8_t windmill_board_os_slot(void) {
    return connected_slot;
}

class OsSlot : public WindmillTest {
   public:
    void SetUp() override {
        WindmillTest::SetUp();
        connected_slot = SLOT_USB;
    }
};

// Fnをホールドして、Fnレイヤー上のキーを1回叩く
static void tap_with_fn(WindmillTest* f, uint8_t row, uint8_t col) {
    auto fn = f->key(POS_FN);
    fn.press();
    f->run_one_scan_loop();
    f->idle_for(250); // TAPPING_TERM 超え。ホールド確定
    f->tap_key(f->key(row, col), 50);
    f->idle_for(50);
    fn.release();
    f->run_one_scan_loop();
    f->idle_for(120);
}

// 今つながっている先のOS設定を変える。MY_WIN / MY_ANDR 自体はレポートを出さない
static void select_os(WindmillTest* f, TestDriver& driver, uint8_t row, uint8_t col) {
    EXPECT_NO_REPORT(driver);
    tap_with_fn(f, row, col);
    f->settle();
    VERIFY_AND_CLEAR(driver);
}

/* 起動直後の英数からかなへ切り替える。MY_LCTL のタップはIMEへも送るが、
 * ここで見たいのはそちらではないのでレポートの中身は問わない */
static void switch_to_kana(WindmillTest* f, TestDriver& driver) {
    EXPECT_ANY_REPORT(driver).Times(AnyNumber());
    f->tap_key(f->key(POS_LCTL), 120);
    f->settle();
    VERIFY_AND_CLEAR(driver);
}

// 右親指Shiftをホールドして「ら」(MY_O) を叩く
static void type_ra_with_shift(WindmillTest* f) {
    auto mi = f->key(POS_MI);
    auto ra = f->key(POS_RA);

    mi.press();
    f->run_one_scan_loop();
    f->idle_for(150);
    f->tap_key(ra, 120);
    f->idle_for(120);
    mi.release();
    f->run_one_scan_loop();
    f->idle_for(120);
}

/* 「ら」+Shift のレポート。押されている実Shiftをそのまま使うので、修飾の
 * 付け外しは挟まらない (test_shift_pair.cpp の shifted_pair_reuses_held_shift)。
 * bracket は Windows向けなら KC_LBRC、Android向けなら KC_RBRC */
static void expect_ra(TestDriver& driver, uint8_t bracket) {
    InSequence s;
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT, bracket));
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
    EXPECT_EMPTY_REPORT(driver);
}

/* USB と Bluetooth1 で別々に覚えること。
 *
 * 従来は設定が1つしかなく、キーボードの接続先を切り替えるたびに MY_WIN /
 * MY_ANDR も押し直す必要があった。 */
TEST_F(OsSlot, keeps_setting_per_connection) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    // USB は Android、Bluetooth1 は Windows にする
    connected_slot = SLOT_USB;
    select_os(this, driver, POS_ANDR);
    connected_slot = SLOT_BLE1;
    select_os(this, driver, POS_WIN);

    // Bluetooth1 のまま。USB 側の Android が漏れてこないこと
    expect_ra(driver, KC_LEFT_BRACKET);
    type_ra_with_shift(this);
    VERIFY_AND_CLEAR(driver);

    // USB へ戻すと Android のまま
    connected_slot = SLOT_USB;
    expect_ra(driver, KC_RIGHT_BRACKET);
    type_ra_with_shift(this);
    VERIFY_AND_CLEAR(driver);

    // もう一度 Bluetooth1 へ。行き帰りで上書きされていないこと
    connected_slot = SLOT_BLE1;
    expect_ra(driver, KC_LEFT_BRACKET);
    type_ra_with_shift(this);
    VERIFY_AND_CLEAR(driver);
}

// Bluetooth の3チャンネルも互いに独立していること
TEST_F(OsSlot, bluetooth_channels_are_independent) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    connected_slot = SLOT_BLE2;
    select_os(this, driver, POS_ANDR);
    connected_slot = SLOT_BLE3;
    select_os(this, driver, POS_WIN);

    connected_slot = SLOT_BLE2;
    expect_ra(driver, KC_RIGHT_BRACKET);
    type_ra_with_shift(this);
    VERIFY_AND_CLEAR(driver);

    connected_slot = SLOT_BLE3;
    expect_ra(driver, KC_LEFT_BRACKET);
    type_ra_with_shift(this);
    VERIFY_AND_CLEAR(driver);
}

/* 一度も設定していない接続先は Windows/デスクトップ向け。
 * ここだけは他のテストが触らないスロット (2.4G) を使う。 */
TEST_F(OsSlot, untouched_connection_defaults_to_windows) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    connected_slot = SLOT_2P4G;
    expect_ra(driver, KC_LEFT_BRACKET);
    type_ra_with_shift(this);
    VERIFY_AND_CLEAR(driver);
}

/* 言語切替 (MY_LCTL のタップ) でIMEへ送るキーも接続先ごとに変わること。
 * OS設定は MY_O/MY_P だけでなくこちらにも効く (issue #53)。
 *
 * ベースレイヤーはタップごとに反転する。ここは起動直後の英数から始める。 */
TEST_F(OsSlot, lang_switch_key_follows_connection) {
    TestDriver driver;
    set_windmill_keymap();

    // USB は Windows、Bluetooth1 は Android。前のテストの設定に頼らず明示する
    connected_slot = SLOT_USB;
    select_os(this, driver, POS_WIN);
    connected_slot = SLOT_BLE1;
    select_os(this, driver, POS_ANDR);

    // USB: 英数 → かな なので KC_LNG1 (かな) を直接指定する
    connected_slot = SLOT_USB;
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LNG1));
        EXPECT_EMPTY_REPORT(driver);
    }
    tap_key(key(POS_LCTL), 120);
    settle();
    VERIFY_AND_CLEAR(driver);

    // Bluetooth1: Android なので Ctrl+Space のトグル
    connected_slot = SLOT_BLE1;
    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_CTRL));
        EXPECT_REPORT(driver, (KC_LEFT_CTRL, KC_SPC));
        EXPECT_REPORT(driver, (KC_LEFT_CTRL));
        EXPECT_EMPTY_REPORT(driver);
    }
    tap_key(key(POS_LCTL), 120);
    settle();
    VERIFY_AND_CLEAR(driver);
}
