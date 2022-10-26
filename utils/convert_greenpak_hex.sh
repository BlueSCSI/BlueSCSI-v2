#!/bin/bash -ex
# This script converts .hex file from GreenPAK GP6 design tool
# to a .h file that can be included in code.

INFILE="greenpak/SCSI_Accelerator_SLG46824.hex"
TMPFILE="/tmp/greenpak.bin"
OUTFILE="lib/bluescsi_platform_GD32F205/greenpak_fw.h"

objcopy --input-target=ihex --output-target=binary $INFILE $TMPFILE

echo 'const uint8_t g_greenpak_fw[] = {' > $OUTFILE
cat $TMPFILE | xxd -i >> $OUTFILE
echo '};' >> $OUTFILE

