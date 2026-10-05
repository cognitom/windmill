/* Copyright 2023 Finalkey
 * Copyright 2023 LiWenLiu <https://github.com/LiuLiuQMK>
 * Copyright 2026 Tsutomu Kawamura
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

/* geonix41 の機種固有部分。キーの処理は全て windmill.c 側にあるので、
 * ここに残っているのはベンダーライブラリ (rdr_lib) との配線だけ。 */

#include "windmill.h"
#include "../../../lib/rdr_lib/rdr_common.h"

/* rdr_lib は QK_KB_0 から自前のキーコード (Custom_Keycodes) を並べている。
 * windmill の独自キーコードは QK_USER_0 から (windmill.h 参照) なので離れているが、
 * rdr_lib 側の enum が伸びて届いてしまったときに検知する番兵。
 * 重なるとブロブが自分のキーコードとして windmill のキーを食ってしまう。 */
_Static_assert((int)QMK_KB_BLE3_PAIR < (int)MY_WIN,
               "windmill の独自キーコードが rdr_lib の Custom_Keycodes とぶつかっている");

/* ブロブは VIA / RAW HID ありきでビルドされているので、それらを無効にした
 * この構成では未定義参照になる。同等に振る舞うスタブで埋める。 */
#if !defined(VIA_ENABLE)
#    include "keymap_introspection.h"

/* BLEペアリングコード等の自動入力用。静的キーマップのレイヤー0から引く。
 *
 * ブロブは引いたキーコードの下位8bitをそのまま register_code() へ渡す。
 * レイヤー0(かな)のキーはほとんどが独自キーコード (KN_*) で、下位8bitだけ
 * 読まれると無関係なキーに化ける。ホストへ送るキーコードに直してから返す */
uint16_t dynamic_keymap_get_keycode(uint8_t layer, uint8_t row, uint8_t column) {
    if (layer >= keymap_layer_count()) {
        return KC_NO;
    }
    return windmill_output_keycode(keymap_key_to_keycode(layer, (keypos_t){.row = row, .col = column}));
}
#endif

#if !defined(RAW_ENABLE)
// ベンダーツール向けの raw HID 送信。送り先がないので何もしない
void raw_hid_send(uint8_t *data, uint8_t length) {
    (void)data;
    (void)length;
}
#endif

/*
 * キースキャン
 */

void matrix_io_delay(void) {}
void matrix_output_select_delay(void) {}
void matrix_output_unselect_delay(uint8_t line, bool key_pressed) {}

/*
 * LED
 */

/* キー下のLEDの位置。キーの並び (行×列) に対応する。
 * 最下段だけ物理配置の都合で 42 が 2 番目に入る。48〜76 はアンダーグローで、
 * windmill からは触らずベンダー実装に任せる。 */
const uint8_t lighting_map[MATRIX_ROWS * MATRIX_COLS] = {
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11,
    12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
    24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
    36, 42, 37, 38, 39, 40, 41, 43, 44, 45, 46, 47
};

/* RGB Matrix の物理配置。ベンダーのLEDドライバが参照する */
led_config_t g_led_config = { {
    {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11 },
    { 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23 },
    { 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35 },
    { 36, 42, 37, 38, 39, 40, 41, 43, 44, 45, 46, 47 }
}, {
    // キー下
    {   0, 10}, {  20, 10}, {  40, 10}, {  60, 10}, {  80, 10}, { 100, 10},
    { 120, 10}, { 140, 10}, { 160, 10}, { 180, 10}, { 200, 10}, { 224, 10},
    {   0, 20}, {  20, 20}, {  40, 20}, {  60, 20}, {  80, 20}, { 100, 20},
    { 120, 20}, { 140, 20}, { 160, 20}, { 180, 20}, { 200, 20}, { 224, 20},
    {   0, 30}, {  20, 30}, {  40, 30}, {  60, 30}, {  80, 30}, { 100, 30},
    { 120, 30}, { 140, 30}, { 160, 30}, { 180, 30}, { 200, 30}, { 224, 30},
    {   0, 40}, {  20, 40}, {  40, 40}, {  60, 40}, {  80, 40}, { 100, 40},
    { 120, 40}, { 140, 40}, { 160, 40}, { 180, 40}, { 200, 40}, { 224, 40},
    // アンダーグロー
    { 255, 65}, { 255, 65}, { 255, 65}, { 255, 65}, { 255, 65}, { 255, 65},
    { 255, 65}, { 255, 65}, { 255, 65}, { 255, 65}, { 255, 65}, { 255, 65},
    { 255, 65}, { 255, 65}, { 255, 65}, { 255, 65}, { 255, 65}, { 255, 65},
    { 255, 65}, { 255, 65}, { 255, 65}, { 255, 65}, { 255, 65}, { 255, 65},
    { 255, 65}, { 255, 65}, { 255, 65}, { 255, 65}, { 255, 65}
}, {
    // キー下
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    // アンダーグロー
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
} };

/*
 * 電源・スリープ
 */

static bool usb_enum_done = false;

void notify_usb_device_state_change_user(struct usb_device_state usb_device_state) {
    if (Keyboard_Info.Key_Mode == QMK_USB_MODE) {
        if (usb_device_state.configure_state == USB_DEVICE_STATE_CONFIGURED) {
            if (!usb_enum_done) { // 最初の CONFIGURED でだけ実行
                Usb_If_Ok       = true;
                Usb_If_Ok_Led   = true;
                Usb_If_Ok_Delay = 0;
                usb_enum_done   = true;
            }
        } else {
            Usb_If_Ok     = false;
            Usb_If_Ok_Led = false;
            usb_enum_done = false; // 切断でリセット
        }
    } else {
        Usb_If_Ok     = false;
        Usb_If_Ok_Led = false;
    }
}

void housekeeping_task_kb(void) {
    User_Keyboard_Reset();
    housekeeping_task_user();
}

void board_init(void) {
    User_Keyboard_Init();
}

/*
 * windmill の機種フック
 */

void windmill_board_post_init(void) {
    User_Keyboard_Post_Init();
}

void windmill_board_led_begin(void) {
    // ベンダー実装 (電池残量・ペアリング表示など) を先に描かせる。
    // windmill の配色はこのあと上から塗られる
    User_Led_Show();
}

/* 接続先。MY_WIN / MY_ANDR の設定はこの番号ごとに覚える (issue #58)。
 * 接続先の切り替えはブロブが Keyboard_Info に書くので、それをそのまま読む。
 * BLE はチャンネル (1〜3) で分ける。ペアリング先が別の機器になるため */
uint8_t windmill_board_host(void) {
    switch (Keyboard_Info.Key_Mode) {
        case QMK_BLE_MODE:
            switch (Keyboard_Info.Ble_Channel) {
                case QMK_BLE_CHANNEL_1:
                    return WINDMILL_HOST_BLE1;
                case QMK_BLE_CHANNEL_2:
                    return WINDMILL_HOST_BLE2;
                case QMK_BLE_CHANNEL_3:
                    return WINDMILL_HOST_BLE3;
            }
            break;
        case QMK_2P4G_MODE:
            return WINDMILL_HOST_2P4G;
    }
    return WINDMILL_HOST_USB;
}

/* スリープ抑止。独自キーコードのように windmill が途中で消費するキーでも効かせたいので、
 * process_record ではなく pre_process 側で行う */
void windmill_board_pre_process_record(uint16_t keycode, keyrecord_t *record) {
    Usb_Change_Mode_Delay  = 0;
    Usb_Change_Mode_Wakeup = false;
}

bool windmill_board_process_record(uint16_t keycode, keyrecord_t *record) {
    // 無線モード切替など、ブロブ側のキーコードはここで処理される。
    // windmill の独自キーコードは来ない。記号とかなのキーは、ホストへ送る素のキーコードに
    // 直されて来る (windmill.c の process_held_output)。かなのうちShift時に別のかなを
    // 出すものだけは来ない
    return Key_Value_Dispose(keycode, record);
}
