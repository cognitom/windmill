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

/* MY_JIS / MY_US の設定を接続先ごとに覚える (issue #74)。
 *
 * 出力表にまだ JIS の列が無く、どちらを選んでもホストへ送るキーは変わらない。
 * レポートでは設定を見分けられないので、windmill_host_layout() とEEPROMの値で見る。
 * JIS の列が入ったら、レポートで見るテストは test_key_output.cpp の側へ足す。
 *
 * 接続先は test_host_os.cpp が差し替えている windmill_board_host() で切り替える。 */

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_layer.h"
#include "action_util.h"
#include "eeconfig.h"
#include "test_keymap.hpp"

using testing::_;
using testing::InSequence;

extern uint8_t test_host; // test_host_os.cpp

extern "C" void keyboard_post_init_kb(void);

/* EEPROM上の位置。bit0: 旧形式のAndroid、bit1: LEDの明るさ、bit2〜11: 接続先ごとの
 * OS、bit12〜21: 接続先ごとの配列。リリース後に動かすと、覚えた設定が別の接続先の
 * ものとして読まれる */
#define OS_BIT(host) (2 + (host) * 2)
#define LAYOUT_BIT(host) (12 + (host) * 2)

/* 設定はEEPROMに残るので、前提の違うテストはスイートを分ける
 * (test_host_os.cpp と同じ) */
class HostLayout : public WindmillTest {
   public:
    HostLayout() {
        test_host = WINDMILL_HOST_USB;
    }
    ~HostLayout() {
        test_host = WINDMILL_HOST_USB; // 他のスイートへ持ち越さない
    }
};
class HostLayoutPerHost : public HostLayout {};
class HostLayoutJisBack : public HostLayout {};
class HostLayoutOutOfRange : public HostLayout {};
class HostLayoutReboot : public HostLayout {};
class HostLayoutBootRead : public HostLayout {};
class HostLayoutUpgrade : public HostLayout {};
class HostLayoutReserved : public HostLayout {};
class HostLayoutOutput : public HostLayout {};

// 設定レイヤーの MY_JIS / MY_US で配列を選ぶ。レポートは出ない
static void select_layout(WindmillTest* f, TestDriver& driver, uint8_t row, uint8_t col) {
    EXPECT_NO_REPORT(driver);
    f->tap_on_conf(row, col);
    f->settle();
    VERIFY_AND_CLEAR(driver);
}

static void expect_layouts(std::initializer_list<uint8_t> hosts, uint8_t layout) {
    for (uint8_t host : hosts) {
        test_host = host;
        EXPECT_EQ(windmill_host_layout(), layout) << "host " << (int)host;
    }
}

#define ALL_HOSTS {WINDMILL_HOST_USB, WINDMILL_HOST_BLE1, WINDMILL_HOST_BLE2, WINDMILL_HOST_BLE3, WINDMILL_HOST_2P4G}

/* EEPROMリセット直後は全ての接続先が JIS。既定のまま MY_JIS を押しても何も書かない */
TEST_F(HostLayout, default_is_jis) {
    TestDriver driver;
    set_windmill_keymap();

    EXPECT_EQ(eeconfig_read_kb(), 0u);
    expect_layouts(ALL_HOSTS, WINDMILL_LAYOUT_JIS);

    test_host = WINDMILL_HOST_BLE1;
    select_layout(this, driver, POS_JIS);
    EXPECT_EQ(eeconfig_read_kb(), 0u);
    expect_layouts(ALL_HOSTS, WINDMILL_LAYOUT_JIS);
}

/* BLE1 で US にしても、他の接続先は JIS のまま。保存先は BLE1 の2bitだけで、
 * OSの設定やLEDの明るさのビットには触らない */
TEST_F(HostLayoutPerHost, us_is_remembered_per_host) {
    TestDriver driver;
    set_windmill_keymap();

    test_host = WINDMILL_HOST_BLE1;
    select_layout(this, driver, POS_US);

    EXPECT_EQ(eeconfig_read_kb(), (uint32_t)WINDMILL_LAYOUT_US << LAYOUT_BIT(WINDMILL_HOST_BLE1));
    expect_layouts({WINDMILL_HOST_BLE1}, WINDMILL_LAYOUT_US);
    expect_layouts({WINDMILL_HOST_USB, WINDMILL_HOST_BLE2, WINDMILL_HOST_BLE3, WINDMILL_HOST_2P4G}, WINDMILL_LAYOUT_JIS);

    // OSの設定とは別々に持つ。同じ接続先でOSを変えても配列は動かない
    test_host = WINDMILL_HOST_BLE1;
    select_layout(this, driver, POS_ANDR);
    EXPECT_EQ(eeconfig_read_kb(), ((uint32_t)WINDMILL_LAYOUT_US << LAYOUT_BIT(WINDMILL_HOST_BLE1)) | (1u << OS_BIT(WINDMILL_HOST_BLE1)));
    expect_layouts({WINDMILL_HOST_BLE1}, WINDMILL_LAYOUT_US);
}

/* 全ての接続先を US にしてから、BLE2 だけ JIS へ戻す */
TEST_F(HostLayoutJisBack, jis_on_one_host_keeps_others) {
    TestDriver driver;
    set_windmill_keymap();

    for (uint8_t host = 0; host < WINDMILL_HOST_SIZE; ++host) {
        test_host = host;
        select_layout(this, driver, POS_US);
    }
    expect_layouts(ALL_HOSTS, WINDMILL_LAYOUT_US);

    test_host = WINDMILL_HOST_BLE2;
    select_layout(this, driver, POS_JIS);

    expect_layouts({WINDMILL_HOST_BLE2}, WINDMILL_LAYOUT_JIS);
    expect_layouts({WINDMILL_HOST_USB, WINDMILL_HOST_BLE1, WINDMILL_HOST_BLE3, WINDMILL_HOST_2P4G}, WINDMILL_LAYOUT_US);
}

/* 範囲外の接続先は有線として扱い、隣の設定を壊さない */
TEST_F(HostLayoutOutOfRange, falls_back_to_usb) {
    TestDriver driver;
    set_windmill_keymap();

    test_host = WINDMILL_HOST_SIZE;
    select_layout(this, driver, POS_US);

    EXPECT_EQ(eeconfig_read_kb(), (uint32_t)WINDMILL_LAYOUT_US << LAYOUT_BIT(WINDMILL_HOST_USB));
    expect_layouts({WINDMILL_HOST_USB}, WINDMILL_LAYOUT_US);
    expect_layouts({WINDMILL_HOST_BLE1, WINDMILL_HOST_BLE2, WINDMILL_HOST_BLE3, WINDMILL_HOST_2P4G}, WINDMILL_LAYOUT_JIS);
}

/* キーで選んだ配列は、起動し直しても接続先ごとに残る */
TEST_F(HostLayoutReboot, survives_reboot) {
    TestDriver driver;
    set_windmill_keymap();

    test_host = WINDMILL_HOST_BLE3;
    select_layout(this, driver, POS_US);
    const uint32_t saved = eeconfig_read_kb();

    keyboard_post_init_kb(); // 起動時の読み込みをやり直す

    EXPECT_EQ(eeconfig_read_kb(), saved); // 起動時に書き戻したり移行したりしない
    expect_layouts({WINDMILL_HOST_BLE3}, WINDMILL_LAYOUT_US);
    expect_layouts({WINDMILL_HOST_USB, WINDMILL_HOST_BLE1, WINDMILL_HOST_BLE2, WINDMILL_HOST_2P4G}, WINDMILL_LAYOUT_JIS);
}

/* 起動時の値はEEPROMから読む。上のテストだけだと、RAMに残っていた値を見ている
 * だけでも通ってしまうので、EEPROMを直接書き換えてから起動し直す */
TEST_F(HostLayoutBootRead, boot_reads_eeprom) {
    TestDriver driver;
    set_windmill_keymap();

    test_host = WINDMILL_HOST_BLE1;
    select_layout(this, driver, POS_US);

    eeconfig_update_kb((uint32_t)WINDMILL_LAYOUT_US << LAYOUT_BIT(WINDMILL_HOST_2P4G));
    keyboard_post_init_kb();

    expect_layouts({WINDMILL_HOST_2P4G}, WINDMILL_LAYOUT_US);
    expect_layouts({WINDMILL_HOST_USB, WINDMILL_HOST_BLE1, WINDMILL_HOST_BLE2, WINDMILL_HOST_BLE3}, WINDMILL_LAYOUT_JIS);
}

/* この設定が入る前のファームウェアで使っていた機体は、配列のビットが 0 のまま
 * 起動してくる。OSやLEDの設定がどうなっていても JIS として読め、旧形式の Android の
 * 移行 (issue #58) が配列のビットを汚さない */
TEST_F(HostLayoutUpgrade, eeprom_without_layout_is_jis) {
    TestDriver driver;

    // LED暗 + 全ての接続先が Android
    eeconfig_update_kb(0b10u | (0b0101010101u << 2));
    keyboard_post_init_kb();
    set_windmill_keymap();
    expect_layouts(ALL_HOSTS, WINDMILL_LAYOUT_JIS);

    // 旧形式の Android + LED暗
    eeconfig_update_kb(0b11);
    keyboard_post_init_kb();
    EXPECT_EQ(eeconfig_read_kb(), 0b10u | (0b0101010101u << 2));
    expect_layouts(ALL_HOSTS, WINDMILL_LAYOUT_JIS);
}

/* 2bitのうち使っていない値 (2, 3) が入っていても、既定の JIS として読む */
TEST_F(HostLayoutReserved, unknown_value_is_jis) {
    TestDriver driver;

    eeconfig_update_kb((2u << LAYOUT_BIT(WINDMILL_HOST_USB)) | (3u << LAYOUT_BIT(WINDMILL_HOST_BLE1)));
    keyboard_post_init_kb();
    set_windmill_keymap();

    expect_layouts(ALL_HOSTS, WINDMILL_LAYOUT_JIS);
}

/* JIS の列が入るまでは、どちらを選んでも US の出力のまま。
 * 英数レイヤーの「;」で見る。US 配列では KC_SCLN がそのまま「;」 */
TEST_F(HostLayoutOutput, both_layouts_send_us_keys) {
    TestDriver driver;
    set_windmill_keymap();

    auto scln = key(1, 10); // 英数 = SY_SCLN_COLN

    auto expect_scln = [&]() {
        {
            InSequence s;
            EXPECT_REPORT(driver, (KC_SCLN));
            EXPECT_EMPTY_REPORT(driver);
        }
        tap_key(scln, 50);
        settle();
        VERIFY_AND_CLEAR(driver);
    };

    ASSERT_EQ(windmill_host_layout(), WINDMILL_LAYOUT_JIS);
    expect_scln();

    select_layout(this, driver, POS_US);
    ASSERT_EQ(windmill_host_layout(), WINDMILL_LAYOUT_US);
    expect_scln();

    select_layout(this, driver, POS_JIS);
    ASSERT_EQ(windmill_host_layout(), WINDMILL_LAYOUT_JIS);
    expect_scln();
}
