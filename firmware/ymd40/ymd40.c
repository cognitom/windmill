/* Copyright 2021 James Young (@noroadsleft)
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

#include "ymd40.h"


// RGBLIGHT_LED_MAP の挙動がおかしいので、独自にマッピング
const uint8_t lighting_map[48] = {
  11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
  23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12,
  35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24,
  50, 49, 48, 47, 46, 44, 42, 40, 39, 38, 37, 36
};
