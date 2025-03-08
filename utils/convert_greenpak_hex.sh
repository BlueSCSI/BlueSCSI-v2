#!/bin/bash -ex

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

# This script converts .hex file from GreenPAK GP6 design tool
# to a .h file that can be included in code.

INFILE="greenpak/SCSI_Accelerator_SLG46824.hex"
TMPFILE="/tmp/greenpak.bin"
OUTFILE="lib/zuluSCSI_platform_GD32F205/greenpak_fw.h"

objcopy --input-target=ihex --output-target=binary $INFILE $TMPFILE

echo 'const uint8_t g_greenpak_fw[] = {' > $OUTFILE
cat $TMPFILE | xxd -i >> $OUTFILE
echo '};' >> $OUTFILE

