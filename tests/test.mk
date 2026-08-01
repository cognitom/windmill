# Copyright 2026 Tsutomu Kawamura
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# firmware/ は /qmk_firmware/keyboards/windmill へ、tests/ は
# /qmk_firmware/tests/windmill へマウントされる (scripts/test.sh 参照)。
# テスト対象は windmill.c 本体そのもので、コピーは持たない。
SRC   += keyboards/windmill/windmill.c
VPATH += $(TOP_DIR)/keyboards/windmill

# Bootmagic/Command は実機の挙動に関係ないので落としておく
COMMAND_ENABLE = no
