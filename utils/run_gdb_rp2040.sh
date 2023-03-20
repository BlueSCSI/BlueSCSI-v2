#!/bin/bash

# ZuluSCSI™ - Copyright (c) 2022 Rabbit Hole Computing™
#
# ZuluSCSI™ file is licensed under the GPL version 3 or any later version. 
#
# https://www.gnu.org/licenses/gpl-3.0.html
# ----
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version. 
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details. 
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

arm-none-eabi-gdb \
       -iex 'target extended /dev/ttyACM0' \
       -iex 'mon s' -iex 'att 1' \
       -iex 'set mem inaccessible-by-default off' \
       -iex 'source utils/rp2040_gdb_macros' \
       .pio/build/ZuluSCSI_RP2040/firmware.elf
