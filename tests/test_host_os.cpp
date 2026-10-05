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

/* MY_WIN / MY_ANDR の設定を接続先ごとに覚える (issue #58)。
 *
 * 無線機 (geonix41) は USB / BLE1〜3 / 2.4G を切り替えて使うので、接続先ごとに
 * 相手のOSが違う。見たいのは次の2つ。
 *
 * - ある接続先でOSを変えても、他の接続先の設定は変わらないこと
 * - 接続先を戻したら、そこで選んだOSのキーがまた出ること
 *
 * 接続先は機種フック windmill_board_host() が返す。windmill.c の既定は weak で
 * 常に有線なので、ここで強いほうを定義して差し替える。このファイル以外の
 * テストでは test_host が有線のままなので、既定と同じに振る舞う。
 *
 * OSの違いは言語切替 (MY_LCTL) のタップで見る。英数からのタップで、Windows は
 * KC_LNG1、Android は Ctrl+Space を送る (test_lang_toggle.cpp 参照)。 */

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_layer.h"
#include "action_util.h"
#include "eeconfig.h"
#include "test_keymap.hpp"

using testing::_;
using testing::InSequence;

// test_host_layout.cpp も同じ差し替えで接続先を切り替えるので、static にしない
uint8_t test_host = WINDMILL_HOST_USB;

extern "C" uint8_t windmill_board_host(void) {
    return test_host;
}

extern "C" void keyboard_post_init_kb(void);

/* 設定はEEPROMに残るので、テストごとに有線から始めるだけでは足りない。
 * QMKのテスト基盤はスイートの頭でEEPROMを初期化する (SetUpTestCase) ので、
 * 前提の違うテストはスイートを分ける */
class HostOs : public WindmillTest {
   public:
    HostOs() {
        test_host = WINDMILL_HOST_USB;
    }
    ~HostOs() {
        test_host = WINDMILL_HOST_USB; // 他のスイートへ持ち越さない
    }
};
class HostOsEeprom : public HostOs {};
class HostOsOutOfRange : public HostOs {};
class HostOsWinBack : public HostOs {};
class HostOsLegacy : public HostOs {};

// 設定レイヤーの MY_WIN / MY_ANDR でOSを選ぶ。レポートは出ない
static void select_os(WindmillTest* f, TestDriver& driver, uint8_t row, uint8_t col) {
    EXPECT_NO_REPORT(driver);
    f->tap_on_conf(row, col);
    f->settle();
    VERIFY_AND_CLEAR(driver);
}

/* 英数から言語切替を1回タップして、送られたキーでOSを確かめる。
 * タップのたびにベースレイヤーが反転するので、毎回英数へ戻してから叩く */
static void expect_lang_toggle(WindmillTest* f, TestDriver& driver, bool android) {
    default_layer_set((layer_state_t)1 << LAYER_ALPHA); // 英数から始める
    auto lctl = f->key(POS_LCTL);

    {
        InSequence s;
        if (android) {
            EXPECT_REPORT(driver, (KC_LEFT_CTRL));
            EXPECT_REPORT(driver, (KC_LEFT_CTRL, KC_SPC));
            EXPECT_REPORT(driver, (KC_LEFT_CTRL));
        } else {
            EXPECT_REPORT(driver, (KC_LNG1));
        }
        EXPECT_EMPTY_REPORT(driver);
    }

    f->tap_key(lctl, 120);
    f->settle();
    VERIFY_AND_CLEAR(driver);
}

/* BLE1 で Android にしても、USB / BLE2 / BLE3 / 2.4G は Windows のまま。
 * BLE1 へ戻ると Android に戻る */
TEST_F(HostOs, android_is_remembered_per_host) {
    TestDriver driver;
    set_windmill_keymap();

    test_host = WINDMILL_HOST_BLE1;
    select_os(this, driver, POS_ANDR);
    expect_lang_toggle(this, driver, true);

    for (uint8_t host : {WINDMILL_HOST_USB, WINDMILL_HOST_BLE2, WINDMILL_HOST_BLE3, WINDMILL_HOST_2P4G}) {
        test_host = host;
        expect_lang_toggle(this, driver, false);
    }

    test_host = WINDMILL_HOST_BLE1;
    expect_lang_toggle(this, driver, true);
}

/* 接続先ごとに2bitずつ並ぶ。4種ぶん確保してあるのは Mac / iOS を足すため。
 * 他の設定 (LEDの明るさ、旧形式のビット) と重ならないことも見る */
TEST_F(HostOsEeprom, holds_two_bits_per_host) {
    TestDriver driver;
    set_windmill_keymap();

    test_host = WINDMILL_HOST_2P4G;
    select_os(this, driver, POS_ANDR);

    // bit0: 旧形式のAndroid、bit1: LEDの明るさ、bit2〜: 接続先ごとのOS
    EXPECT_EQ(eeconfig_read_kb(), 1u << (2 + WINDMILL_HOST_2P4G * 2));
}

/* 範囲外の接続先は有線として扱い、隣の設定を壊さない */
TEST_F(HostOsOutOfRange, falls_back_to_usb) {
    TestDriver driver;
    set_windmill_keymap();

    test_host = WINDMILL_HOST_SIZE;
    select_os(this, driver, POS_ANDR);

    test_host = WINDMILL_HOST_USB;
    expect_lang_toggle(this, driver, true);
    test_host = WINDMILL_HOST_2P4G; // 最後の接続先の隣 (はみ出した先) も無事
    expect_lang_toggle(this, driver, false);
}

/* 全ての接続先を Android にしてから、BLE2 だけ Windows へ戻す */
TEST_F(HostOsWinBack, win_on_one_host_keeps_others) {
    TestDriver driver;
    set_windmill_keymap();

    for (uint8_t host = 0; host < WINDMILL_HOST_SIZE; ++host) {
        test_host = host;
        select_os(this, driver, POS_ANDR);
    }

    test_host = WINDMILL_HOST_BLE2;
    select_os(this, driver, POS_WIN);
    expect_lang_toggle(this, driver, false);

    for (uint8_t host : {WINDMILL_HOST_USB, WINDMILL_HOST_BLE1, WINDMILL_HOST_BLE3, WINDMILL_HOST_2P4G}) {
        test_host = host;
        expect_lang_toggle(this, driver, true);
    }
}

/* 以前のファームウェアで Android にしていた (bit0 が立っている) なら、
 * 起動時に全ての接続先を Android として引き継ぎ、bit0 は下ろす。
 * LEDの明るさ (bit1) はそのまま残る */
TEST_F(HostOsLegacy, legacy_android_migrates_to_all_hosts) {
    TestDriver driver;

    eeconfig_update_kb(0b11); // 旧形式の Android + LED暗
    keyboard_post_init_kb();  // 起動時の読み込みをやり直す
    set_windmill_keymap();

    EXPECT_EQ(eeconfig_read_kb(), 0b10u | (0b0101010101u << 2));

    for (uint8_t host = 0; host < WINDMILL_HOST_SIZE; ++host) {
        test_host = host;
        expect_lang_toggle(this, driver, true);
    }
}
