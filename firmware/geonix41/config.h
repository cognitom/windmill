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
#pragma once

/* rdr_lib が QK_KB_0 から30個の自前キーコード (Custom_Keycodes) を並べているので、
 * windmill の MY_* はその後ろから始める。ズレの検知は geonix41.c の
 * _Static_assert で行っている */
#define WINDMILL_KEYCODE_BASE QK_KB_30

/* Mechanical locking support. Use KC_LCAP, KC_LNUM or KC_LSCR instead in keymap */
#define LOCKING_SUPPORT_ENABLE
/* Locking resynchronize hack */
#define LOCKING_RESYNC_ENABLE

#define MATRIX_UNSELECT_DRIVE_HIGH
#define CORTEX_ENABLE_WFI_IDLE          FALSE

/* Ensure we jump to bootloader if the RESET keycode was pressed */
#define EARLY_INIT_PERFORM_BOOTLOADER_JUMP TRUE

/* デバウンスドライバ側は未定義でも5にフォールバックするので qmk lint は
 * 「デフォルトと同じ」と言ってくるが、消してはいけない。
 * bootmagic.c は BOOTMAGIC_DEBOUNCE を DEBOUNCE*2 で作り、DEBOUNCE が
 * 見えないときだけ 30 に落ちるため、消すと起動時の待ちが 10ms → 30ms に伸びる */
#define DEBOUNCE 5

/* tap-hold は別キーが押された時点でホールド確定。
 * LGUI_T(KC_Z) や LT(2,KC_V) の打鍵感がこれで決まる */
#define HOLD_ON_OTHER_KEY_PRESS

/* rdr_lib のカスタムEEPROMドライバ用。rdr_common.h のバッファサイズにも
 * 使われるので、減らすとブロブ側とズレる */
#define EEPROM_SIZE 1152

/* ベンダーの元の設定には EECONFIG_KB_DATA_SIZE 1 があったが外してある。
 * QMK は EECONFIG_KB_DATA_SIZE が 0 のときだけ eeconfig_read_kb()/update_kb() を
 * 生やすので (quantum/eeconfig.h)、1 のままだと windmill.c の設定保存が通らない。
 * ブロブは QMK の eeconfig を eeconfig_disable() 以外使っておらず、自前のデータは
 * 別のフラッシュ領域 (IAPROM) に置いているので外して問題ない。 */
#define EECONFIG_USER_DATA_SIZE 4

#ifndef NOP_FUDGE
#define NOP_FUDGE 0.4
#endif

/* キー下 48個 (0〜47) + アンダーグロー 29個 (48〜76)。
 * windmill が塗るのは lighting_map で指したキー下だけで、
 * アンダーグローはベンダー実装 (User_Led_Show) に任せる */
#define RGB_MATRIX_LED_COUNT 77
#define RGB_MATRIX_KEYPRESSES
#define RGB_MATRIX_KEYRELEASES
#define RGB_MATRIX_FRAMEBUFFER_EFFECTS
#define RGB_DISABLE_AFTER_TIMEOUT 0
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 144
#define RGB_MATRIX_SPD_STEP 64

/* windmill は post_init で RGB_MATRIX_NONE + HSV_OFF にしてから自前で描くので、
 * ここはその前の初期状態にすぎない。背景を消しておく */
#define RGB_MATRIX_DEFAULT_MODE RGB_MATRIX_SOLID_COLOR
#define RGB_MATRIX_DEFAULT_SAT 0
#define RGB_MATRIX_DEFAULT_VAL 0

#define RGB_MATRIX_SLEEP
