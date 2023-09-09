#!/bin/bash

# This script regenerates the .pio.h files from .pio

pioasm sdio_RP2040.pio sdio_RP2040.pio.h
pioasm sdio_Pico.pio sdio_Pico.pio.h
pioasm sdio_BS2.pio sdio_BS2.pio.h

pioasm scsi_accel_target_RP2040.pio scsi_accel_target_RP2040.pio.h
pioasm scsi_accel_target_BS2.pio scsi_accel_target_BS2.pio.h
pioasm scsi_accel_target_Pico.pio scsi_accel_target_Pico.pio.h

pioasm scsi_accel_host_RP2040.pio scsi_accel_host_RP2040.pio.h
pioasm scsi_accel_host_Pico.pio scsi_accel_host_Pico.pio.h
