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


/* 親指Shift (かなレイヤーの こ / み のホールド) のレポート列を検証する (issue #37)。
 *
 * 左右の親指Shiftはどちらも同じ左Shiftなので、片方を押したままもう片方を
 * 押して離す「持ち替え」(ハンドオーバー) でShiftが途切れないことを確かめる。
 * QMKの修飾はビットマスクで参照カウントを持たないため、素のままだと先に
 * 離したほうの unregister_mods() でShiftが落ちてしまう。
 *
 * こ / み は、押してから KANA_HOLD_TERM 以上たって別のキーを押したときに
 * ホールドになる (issue #79)。持ち替えで後から押したほうも同じで、次のキーを
 * 押すまではホールドにならない。
 *
 * Shift を確かめるキーは「の」。Shift 面では「、」(Shift+,) なので、Shift が
 * 乗っていればそのまま出る。「は」のように Shift 面で透過のキーは、Shift を外して
 * 「は」を出すので確かめに使えない。 */

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_layer.h"
#include "action_util.h"
#include "test_keymap.hpp"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;
using testing::InvokeWithoutArgs;

class ThumbShift : public WindmillTest {};

// 起動直後の英数から MY_LCTL 1回タップでかなへ切り替える。レポートの中身は問わない
static void switch_to_kana(WindmillTest* f, TestDriver& driver) {
    EXPECT_ANY_REPORT(driver).Times(AnyNumber());

    f->tap_key(f->key(POS_LCTL), 120);
    f->settle();

    VERIFY_AND_CLEAR(driver);
}

// Shift を押したまま「の」を打ったときの「、」
static void expect_touten(TestDriver& driver) {
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_COMMA));
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
}

/* 先に押したほうをホールドにしてから、もう片方を押してホールドにし、先のほうを離す。
 * first / second の順で持ち替える */
static void handover(WindmillTest* f, TestDriver& driver, KeymapKey first, KeymapKey second) {
    auto no = f->key(POS_NO);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // second の押下で first のホールド確定
        expect_touten(driver);                  // この「の」で second もホールド確定
        expect_touten(driver);                  // 持ち替え後も Shift が乗る
        EXPECT_EMPTY_REPORT(driver);            // second 解放
    }

    first.press();
    f->run_one_scan_loop();
    f->idle_for(250);
    second.press();
    f->run_one_scan_loop();
    f->idle_for(250);
    f->tap_key(no, 120);
    f->idle_for(120);
    first.release(); // second はまだ押されている
    f->run_one_scan_loop();
    f->idle_for(120);
    f->tap_key(no, 120);
    f->idle_for(120);
    second.release();
    f->run_one_scan_loop();
    f->idle_for(120);
    VERIFY_AND_CLEAR(driver);
}

TEST_F(ThumbShift, handover_left_to_right_keeps_shift) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    handover(this, driver, key(POS_KO), key(POS_MI));

    default_layer_set((layer_state_t)1 << LAYER_ALPHA); // 次のテストへかなを持ち越さない
}

TEST_F(ThumbShift, handover_right_to_left_keeps_shift) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    handover(this, driver, key(POS_MI), key(POS_KO));

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

/* 後から押したほうがまだホールドになっていないうちに先のほうを離すと、Shift は
 * いったん離れる。ホールドかどうかは次のキーの押下で決まるので、次のキーには
 * Shift が戻って乗る */
TEST_F(ThumbShift, handover_before_next_key_shifts_next_key) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto ko = key(POS_KO);
    auto mi = key(POS_MI);
    auto no = key(POS_NO);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // み の押下で こ のホールド確定
        EXPECT_EMPTY_REPORT(driver);            // こ 解放。み はまだホールドではない
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // 「の」の押下で み のホールド確定
        expect_touten(driver);
        EXPECT_EMPTY_REPORT(driver); // み 解放
    }

    ko.press();
    run_one_scan_loop();
    idle_for(250);
    mi.press();
    run_one_scan_loop();
    idle_for(250);
    ko.release();
    run_one_scan_loop();
    idle_for(120);
    tap_key(no, 120);
    idle_for(120);
    mi.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

/* 片方の親指Shiftをホールドしたまま、もう片方をタップすると半角スペース。
 * Shift は外して送り、外したレポートを1本独立させる (issue #36)。
 * 外す前後にウェイトが挟まっていることも見る */
TEST_F(ThumbShift, space_drops_shift_in_its_own_report) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto ko = key(POS_KO); // 左親指Shift
    auto mi = key(POS_MI); // 右親指Shift

    uint16_t at_hold = 0, at_drop = 0, at_space = 0;

    {
        InSequence s;
        // こ ホールド確定 → Shiftを外す → スペース。この3本の間隔を控える
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)).WillOnce(InvokeWithoutArgs([&] { at_hold = timer_read(); }));
        EXPECT_EMPTY_REPORT(driver).WillOnce(InvokeWithoutArgs([&] { at_drop = timer_read(); }));
        EXPECT_REPORT(driver, (KC_SPACE)).WillOnce(InvokeWithoutArgs([&] { at_space = timer_read(); }));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));  // Shiftを戻す (ホストへ届くこと)
        EXPECT_EMPTY_REPORT(driver);             // 2打鍵目。レポート列は1打鍵目と同じ
        EXPECT_REPORT(driver, (KC_SPACE));
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        EXPECT_EMPTY_REPORT(driver);             // こ 解放
    }

    ko.press();
    run_one_scan_loop();
    idle_for(250);
    mi.press(); // こ のホールド確定
    run_one_scan_loop();
    mi.release(); // 他のキーを押さずに離したのでタップ = スペース
    run_one_scan_loop();
    idle_for(120);
    tap_key(mi, 120); // 2打鍵目
    idle_for(120);
    ko.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);

    // ホールド確定 → Shift外し → スペース が同じスキャンに固まっていないこと
    EXPECT_GE(uint16_t(at_drop - at_hold), uint16_t(IME_WAIT_MS));
    EXPECT_GE(uint16_t(at_space - at_drop), uint16_t(IME_WAIT_MS));

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

/* Shift 開始後に他のキーを押していたら、もう片方のタップは何も出さない (誤入力ガード) */
TEST_F(ThumbShift, space_is_suppressed_after_other_key) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto ko = key(POS_KO);
    auto mi = key(POS_MI);
    auto no = key(POS_NO);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT)); // 「の」の押下で こ のホールド確定
        expect_touten(driver);
        EXPECT_EMPTY_REPORT(driver); // こ 解放。み のタップは何も出さない
    }

    ko.press();
    run_one_scan_loop();
    idle_for(250);
    tap_key(no, 120);
    idle_for(120);
    tap_key(mi, 120);
    idle_for(120);
    ko.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

/* こ と み をほぼ同時に押すと、先の こ は KANA_HOLD_TERM 未満で次のキーが来たので
 * タップになる。「こ」「み」と出て、Shift もスペースも出ない */
TEST_F(ThumbShift, quick_ko_mi_are_two_taps) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto ko = key(POS_KO);
    auto mi = key(POS_MI);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_B)); // こ
        EXPECT_EMPTY_REPORT(driver);
        EXPECT_REPORT(driver, (KC_N)); // み
        EXPECT_EMPTY_REPORT(driver);
    }

    ko.press();
    run_one_scan_loop();
    mi.press();
    run_one_scan_loop();
    mi.release();
    run_one_scan_loop();
    ko.release();
    run_one_scan_loop();
    idle_for(120);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}

// 両方離せばShiftも落ちる。持ち替えの後始末が残っていないことの確認
TEST_F(ThumbShift, both_released_clears_shift) {
    TestDriver driver;
    set_windmill_keymap();
    switch_to_kana(this, driver);

    auto ko = key(POS_KO);
    auto mi = key(POS_MI);
    auto no = key(POS_NO);

    {
        InSequence s;
        EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
        expect_touten(driver);
        EXPECT_EMPTY_REPORT(driver);   // 両方離れた
        EXPECT_REPORT(driver, (KC_K)); // Shiftは残っていない。「の」が出る
        EXPECT_EMPTY_REPORT(driver);
    }

    ko.press();
    run_one_scan_loop();
    idle_for(250);
    mi.press();
    run_one_scan_loop();
    idle_for(250);
    tap_key(no, 120); // 両方ともホールド
    idle_for(120);
    ko.release();
    run_one_scan_loop();
    idle_for(120);
    mi.release();
    run_one_scan_loop();
    idle_for(120);
    tap_key(no, 120);
    idle_for(120);
    VERIFY_AND_CLEAR(driver);

    default_layer_set((layer_state_t)1 << LAYER_ALPHA);
}
