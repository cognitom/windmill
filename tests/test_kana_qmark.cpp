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

/* process_kana_qmark() のHIDレポートを検証する (issue #17)。
 *
 * かなレイヤーの「も」(LT(2,KC_M)) をShiftを押しながらタップすると、
 * KC_LNG2 → Shift+KC_SLSH → KC_LNG1 の順で送出し、半角「?」を入力する。
 * キーコードの並びだけでなく、実Shiftがいつ外れていつ戻るかをレポート列で
 * 固定する (issue #18 と同じ理由で、キーコードの一致だけでは修飾の出方の
 * 崩れに気づけない)。要点は2つ。
 *
 * - KC_LNG2/KC_LNG1 にShiftが乗っていないこと。乗っているとIMEがモードを
 *   切り替えず、実機で「・」が出る
 * - 「?」のShiftが、外した実Shiftと同じものであること。左Shiftを自前で
 *   付けると修飾が入れ替わって見え、実機で半角「/」が出る
 *
 * 起動直後のベースレイヤーは英数 (issue #22) なので、かなレイヤー上の
 * 挙動を見るには先に MY_LCTL をダブルタップしてかなへ切り替えておく必要がある。 */

#include <vector>

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_layer.h"
#include "action_util.h"
#include "test_keymap.hpp"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;
using testing::Invoke;

class KanaQmark : public WindmillTest {};

// windmill.c の LT2_IME_WAIT_MS と合わせる
static constexpr uint32_t IME_WAIT_MS = 10;

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

// Shiftなしなら通常どおり KC_M が出る
TEST_F(KanaQmark, without_shift_sends_plain_keycode) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_M));
        EXPECT_EMPTY_REPORT(driver);
    }
    tap_key(key(POS_MO), 120);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* Shift+タップ1回ぶんの期待レポート列。押されているShiftは held で渡す。
 *
 * 英数/かなへの切り替えの間だけShiftを外し、「?」のShiftには外したものを
 * そのまま戻して使う。ここで左Shiftを自前で付けると、ホストからは修飾が
 * 入れ替わって見えて実機ではShiftが取りこぼされる (issue #18 と同じ罠)。 */
static void expect_qmark_reports(TestDriver& driver, uint8_t held) {
    EXPECT_EMPTY_REPORT(driver);           // 実Shiftを外す
    EXPECT_REPORT(driver, (KC_LNG2));      // 英数へ (Shiftが乗っていないこと)
    EXPECT_EMPTY_REPORT(driver);
    EXPECT_REPORT(driver, (held));         // 同じShiftを戻す
    EXPECT_REPORT(driver, (held, KC_SLSH));
    EXPECT_REPORT(driver, (held));
    EXPECT_EMPTY_REPORT(driver);           // また外す
    EXPECT_REPORT(driver, (KC_LNG1));      // かなへ (Shiftが乗っていないこと)
    EXPECT_EMPTY_REPORT(driver);
    EXPECT_REPORT(driver, (held));         // 押しっぱなしの実Shiftを戻す
}

/* Shiftを押しながらのタップは KC_LNG2 -> Shift+KC_SLSH -> KC_LNG1 に化ける。
 * IMEにモード切り替えを届かせるため、その間だけ実Shiftは外れている */
TEST_F(KanaQmark, shifted_tap_sends_halfwidth_question_mark) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto mi = key(POS_MI); // 右親指Shift
    auto mo = key(POS_MO);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // み ホールド確定
        expect_qmark_reports(driver, KC_LEFT_SHIFT);
        EXPECT_EMPTY_REPORT(driver); // み 解放
    }

    mi.press();
    run_one_scan_loop();
    idle_for(150);
    tap_key(mo, 120);
    idle_for(120);
    mi.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* 実機で報告された は→も→も の再現 (PR #32)。「も」以外のキーを打った直後の
 * 1打鍵目だけ半角「/」になっていた。Shiftを入れ替えずに使えば1回目も2回目も
 * 同じレポート列になる。 */
TEST_F(KanaQmark, shifted_tap_is_stable_after_other_key) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto mi = key(POS_MI);
    auto ha = key(POS_HA); // は (KC_F)
    auto mo = key(POS_MO);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));        // み ホールド確定
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_F));  // は
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        expect_qmark_reports(driver, KC_LEFT_SHIFT);   // も 1回目
        expect_qmark_reports(driver, KC_LEFT_SHIFT);   // も 2回目
        EXPECT_EMPTY_REPORT(driver);                   // み 解放
    }

    mi.press();
    run_one_scan_loop();
    idle_for(150);
    tap_key(ha, 120);
    idle_for(120);
    tap_key(mo, 120);
    idle_for(120);
    tap_key(mo, 120);
    idle_for(120);
    mi.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

/* 送出されたレポートを時刻付きで記録する。
 *
 * 「?」のShiftを自前で上げ下げする形と tap_code16(S(KC_SLSH)) に任せる形とでは、
 * ホストへ届くレポート列が完全に一致する。違うのはレポートとレポートの間隔だけ
 * なので、EXPECT_REPORT の突き合わせでは issue #39 の再発 (PR #41) を素通り
 * させてしまった。ここは時刻を見る。 */
struct StampedReport {
    uint8_t  mods;
    uint8_t  key;
    uint32_t time;
};

static void record_reports(TestDriver& driver, std::vector<StampedReport>& log) {
    EXPECT_CALL(driver, send_keyboard_mock(_))
        .Times(AnyNumber())
        .WillRepeatedly(Invoke([&log](report_keyboard_t& report) {
            log.push_back({report.mods, report.keys[0], timer_read32()});
        }));
}

/* Shiftを押すレポートと KC_SLSH のレポートの間にウェイトが入っていること。
 * 英数へ切り替えた直後のIMEは、間を置かずに並んだShiftを取りこぼし、
 * 半角「?」ではなく「/」を出す (PR #32 で報告された症状)。 */
TEST_F(KanaQmark, waits_between_shift_and_slash) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    std::vector<StampedReport> log;
    record_reports(driver, log);

    auto mi = key(POS_MI); // 右親指Shift
    auto mo = key(POS_MO);

    mi.press();
    run_one_scan_loop();
    idle_for(150);
    tap_key(mo, 120);
    idle_for(120);
    mi.release();
    run_one_scan_loop();
    idle_for(120);

    size_t slash = 0;
    for (size_t i = 1; i < log.size(); ++i) {
        if (log[i].key == KC_SLSH) {
            slash = i;
            break;
        }
    }
    ASSERT_GT(slash, 0u) << "KC_SLSH のレポートが出ていない";

    EXPECT_EQ(log[slash].mods, MOD_LSFT) << "「?」にShiftが乗っていない";
    EXPECT_EQ(log[slash - 1].mods, MOD_LSFT) << "KC_SLSH の直前はShiftのみのレポートのはず";
    EXPECT_EQ(log[slash - 1].key, 0);
    EXPECT_GE(log[slash].time - log[slash - 1].time, IME_WAIT_MS)
        << "Shiftを押した直後に KC_SLSH を送るとIMEがShiftを取りこぼす";
}

/* ホールドすれば通常どおりSymレイヤーに乗る。かなベースのままなので、数字は
 * 既存のIMEラップ (is_sym_ime_wrap_target) で KC_LNG2 -> KC_1 -> KC_LNG1 に
 * なる。process_kana_qmark がホールドに割り込んでいないことの確認 */
TEST_F(KanaQmark, hold_still_enters_sym_layer) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto mo  = key(POS_MO);
    auto one = key(0, 1); // Symレイヤーでの数字段

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LNG2));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_1));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LNG1));
        EXPECT_EMPTY_REPORT(driver);
    }

    mo.press();
    run_one_scan_loop();
    idle_for(50);     // タップ確定前に、別キーの押下でホールドへ割り込ませる
    tap_key(one, 60);
    idle_for(60);
    mo.release();
    run_one_scan_loop();
    idle_for(60);
    VERIFY_AND_CLEAR(driver);
}
