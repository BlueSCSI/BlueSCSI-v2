#!/bin/bash

# ZuluSCSI™ - Copyright (c) 2022-2025 Rabbit Hole Computing™
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


# This script runs GDB with openocd and stlink to
# allow debugging and seeing the SWO log output in realtime.

killall orbuculum
killall orbcat
rm -f swo.log

arm-none-eabi-gdb \
       -iex 'target extended | openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "gdb_port pipe"' \
       -iex 'mon reset_config srst_only' \
       -iex 'mon halt' \
       -iex 'mon tpiu config internal swo.log uart false 38400000 2000000' \
       -iex 'shell bash -m -c "orbuculum -f swo.log &"' \
       -iex 'shell bash -m -c "orbcat -c 0,%c &"' \
       .pio/build/ZuluSCSIv1_1/firmware.elf
