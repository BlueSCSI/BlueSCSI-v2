#!/bin/bash

# This script runs GDB with openocd and stlink to
# allow debugging and seeing the SWO log output in realtime.

killall orbuculum
killall orbcat

arm-none-eabi-gdb \
       -iex 'target extended | openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "gdb_port pipe"' \
       -iex 'mon halt' \
       -iex 'mon tpiu config internal swo.log uart false 38400000 2000000' \
       -iex 'shell bash -m -c "orbuculum -f swo.log &"' \
       -iex 'shell bash -m -c "orbcat -c 0,%c &"' \
       .pio/build/genericGD32F205VC/firmware.elf
