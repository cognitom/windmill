/*
Copyright 2021 Tsutomu Kawamura

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "config_common.h"

/* USB Device descriptor parameter */
#define VENDOR_ID    0x4273 
#define PRODUCT_ID   0x0079
#define DEVICE_VER   0x0001
#define MANUFACTURER Boardsource
#define PRODUCT      Technik-O

/* key matrix size */
#define MATRIX_ROWS 4
#define MATRIX_COLS 12

/* Keyboard Matrix Assignments */
#define MATRIX_ROW_PINS {B0, B1, B2, B3}
#define MATRIX_COL_PINS {B5, B6, B7, F5, C7, D0, D1, D2, D3, D4, D5, D6}
#define UNUSED_PINS
#define DIODE_DIRECTION COL2ROW

#define RGB_DI_PIN C6
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 120
#define RGB_DISABLE_WHEN_USB_SUSPENDED true // turns off RGB Matrix while the PC is sleeping
#ifdef RGBLIGHT_ENABLE
#define RGBLED_NUM 10 // Number of LEDs
#endif

#define DRIVER_LED_TOTAL 58

/* Debounce reduces chatter (unintended double-presses) - set 0 if debouncing is not needed */
#define DEBOUNCE 5

/* Mechanical locking support. Use KC_LCAP, KC_LNUM or KC_LSCR instead in keymap */
#define LOCKING_SUPPORT_ENABLE
/* Locking resynchronize hack */
#define LOCKING_RESYNC_ENABLE

/* disable these deprecated features by default */
#define NO_ACTION_MACRO
#define NO_ACTION_FUNCTION

/* Dual-role keys setting; see https://docs.qmk.fm/#/tap_hold?id=permissive-hold */
#define TAPPING_TERM 250
#define PERMISSIVE_HOLD
