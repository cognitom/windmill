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

/* geonix41 の配線で windmill.c を通す (issue #68)。
 *
 * 最下段の matrix の列が見た目の並びと違う (firmware/geonix41/keyboard.json の
 * "layouts" 参照)。windmill.c は左右の Fn / 親指Shift をこの列で見分けるので、
 * 親の tests/ (位置＝列) だけでは、列の設定を誤っていても気づけない。
 *
 * テストの中身は親と同じものを使い (*.cpp 参照)、変えるのは配線と列の設定だけ */
#include "../config.h"

// firmware/geonix41/keyboard.json の最下段の "matrix" と揃えること
#define WINDMILL_TEST_ROW3_COLS {0, 2, 3, 4, 5, 6, 1, 7, 8, 9, 10, 11}

// firmware/geonix41/config.h と揃えること
#define FN_L_COL 4
#define FN_R_COL 8
#define ALPHA_THUMB_SHIFT_L_COL 6
#define ALPHA_THUMB_SHIFT_R_COL 1
