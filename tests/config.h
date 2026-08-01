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

#pragma once

/* 実機と同じ 4x12 にする。test_common.h の既定は 4x10 で、そちらは
 * patches/qmk-test-harness.patch で #ifndef 化してあるので先勝ちになる */
#define MATRIX_ROWS 4
#define MATRIX_COLS 12

#include "test_common.h"

// 各機種の keyboard.json / config.h と揃えること
#define TAPPING_TERM 200
#define HOLD_ON_OTHER_KEY_PRESS
