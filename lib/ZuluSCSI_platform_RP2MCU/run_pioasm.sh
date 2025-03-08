#!/bin/bash

# This script regenerates the .pio.h files from .pio
pioasm sdio_RP2MCU.pio sdio_RP2MCU.pio.h
pioasm scsi_accel_target_RP2MCU.pio scsi_accel_target_RP2MCU.pio.h
pioasm scsi_accel_host_RP2MCU.pio scsi_accel_host_RP2MCU.pio.h
