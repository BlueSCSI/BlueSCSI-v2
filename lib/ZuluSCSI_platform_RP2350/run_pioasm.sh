#!/bin/bash

# This script regenerates the .pio.h files from .pio

pioasm sdio.RP2350.pio sdio.RP2350.pio.h
pioasm sdio.Pico_2.pio sdio.Pico_2.h
pioasm sdio_BS2.pio sdio_BS2.pio.h

pioasm scsi_accel_target_RP2350.pio scsi_accel_target_RP2350.pio.h
pioasm scsi_accel_target_BS2.pio scsi_accel_target_BS2.pio.h
pioasm scsi_accel_target_Pico_2.pio scsi_accel_target_Pico_2.pio.h

pioasm scsi_accel_host_RP2350.pio scsi_accel_host_RP2350.pio.h
pioasm scsi_accel_host_Pico_2.pio scsi_accel_host_Pico_2.pio.h
