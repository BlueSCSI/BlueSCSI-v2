#!/bin/bash

arm-none-eabi-gdb \
       -iex 'target extended /dev/ttyACM0' \
       -iex 'mon s' -iex 'att 1' \
       -iex 'set mem inaccessible-by-default off' \
       -iex 'source utils/rp2040_gdb_macros' \
       .pio/build/BlueSCSI_RP2040/firmware.elf
