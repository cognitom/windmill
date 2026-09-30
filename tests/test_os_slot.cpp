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
 * 判定は内部状態を覗かずに「ら」(MY_O) をShift付きで叩いた出力で行う。
 * Windows/デスクトップ向けなら「{」= S(KC_LBRC)、Android向けなら S(KC_RBRC)
 * になるので、どちらの設定で解決されたかがレポートに出る。 */

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_layer.h"
#include "action_util.h"
#include "test_keymap.hpp"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;

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

    /* ベースレイヤーをかなにする。MY_O は英数レイヤーでは素の KC_O なので、
     * かなを起点にしないと「ら」+Shift でOS設定を見られない。同じ実行の他の
     * テストが残した状態には頼らず、MY_LCTL のダブルタップで明示的に寄せる。 */
    void start_on_kana(TestDriver& driver) {
        EXPECT_ANY_REPORT(driver).Times(AnyNumber());
        tap_lctl(2);
    }

    /* Fn (そ) をホールドして MY_WIN / MY_ANDR を叩き、今の接続先のOS設定を変える。
     * Fn のホールドは MY_* の押下で確定する (HOLD_ON_OTHER_KEY_PRESS)。 */
    void tap_os_key(uint8_t row, uint8_t col) {
        auto fn = key(POS_SO);
        auto os = key(row, col);

        fn.press();
        run_one_scan_loop();
        idle_for(50);
        os.press();
        run_one_scan_loop();
        idle_for(50);
        os.release();
        run_one_scan_loop();
        idle_for(50);
        fn.release();
        run_one_scan_loop();
        settle();
    }

    // 右親指Shiftをホールドして「ら」(MY_O) を叩く
    void type_ra_with_shift() {
        auto mi = key(POS_MI);
        auto ra = key(POS_RA);

        mi.press();
        run_one_scan_loop();
        idle_for(150);
        tap_key(ra, 120);
        idle_for(120);
        mi.release();
        run_one_scan_loop();
        idle_for(120);
    }

    /* 「ら」+Shift のレポートを期待する。押されている実Shiftをそのまま使うので、
     * 修飾の付け外しは挟まらない (test_shift_pair.cpp の shifted_pair_reuses_held_shift)。 */
    void expect_ra_for_windows(TestDriver& driver) {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_LEFT_BRACKET)); // { = S(KC_LBRC)
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }

    void expect_ra_for_android(TestDriver& driver) {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_RIGHT_BRACKET)); // S(KC_RBRC)
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);
    }
};

/* USB と Bluetooth1 で別々に覚えること。
 *
 * 従来は設定が1つしかなく、キーボードの接続先を切り替えるたびに MY_WIN /
 * MY_ANDR も押し直す必要があった。 */
TEST_F(OsSlot, keeps_setting_per_connection) {
    TestDriver driver;
    set_windmill_keymap();
    settle();

    // かなを起点にする。ここと設定キーのレポートは問わない
    start_on_kana(driver);
    connected_slot = SLOT_USB;
    tap_os_key(POS_ANDR);
    connected_slot = SLOT_BLE1;
    tap_os_key(POS_WIN);
    VERIFY_AND_CLEAR(driver);

    // Bluetooth1 のまま。USB 側の Android が漏れてこないこと
    expect_ra_for_windows(driver);
    type_ra_with_shift();
    VERIFY_AND_CLEAR(driver);

    // USB へ戻すと Android のまま
    connected_slot = SLOT_USB;
    expect_ra_for_android(driver);
    type_ra_with_shift();
    VERIFY_AND_CLEAR(driver);

    // もう一度 Bluetooth1 へ。行き帰りで上書きされていないこと
    connected_slot = SLOT_BLE1;
    expect_ra_for_windows(driver);
    type_ra_with_shift();
    VERIFY_AND_CLEAR(driver);
}

// Bluetooth の3チャンネルも互いに独立していること
TEST_F(OsSlot, bluetooth_channels_are_independent) {
    TestDriver driver;
    set_windmill_keymap();
    settle();

    start_on_kana(driver);
    connected_slot = SLOT_BLE2;
    tap_os_key(POS_ANDR);
    connected_slot = SLOT_BLE3;
    tap_os_key(POS_WIN);
    VERIFY_AND_CLEAR(driver);

    connected_slot = SLOT_BLE2;
    expect_ra_for_android(driver);
    type_ra_with_shift();
    VERIFY_AND_CLEAR(driver);

    connected_slot = SLOT_BLE3;
    expect_ra_for_windows(driver);
    type_ra_with_shift();
    VERIFY_AND_CLEAR(driver);
}

/* 一度も設定していない接続先は Windows/デスクトップ向け。
 * ここだけは他のテストが触らないスロット (2.4G) を使う。 */
TEST_F(OsSlot, untouched_connection_defaults_to_windows) {
    TestDriver driver;
    set_windmill_keymap();
    settle();

    start_on_kana(driver);
    VERIFY_AND_CLEAR(driver);

    connected_slot = SLOT_2P4G;
    expect_ra_for_windows(driver);
    type_ra_with_shift();
    VERIFY_AND_CLEAR(driver);
}
