#!/bin/bash

# This script regenerates the .pio.h files from .pio

pioasm rp2040_sdio.pio rp2040_sdio.pio.h
pioasm rp2040_sdio_BS2.pio rp2040_sdio_BS2.pio.h

pioasm scsi_accel.pio scsi_accel.pio.h
pioasm scsi_accel_BS2.pio scsi_accel_BS2.pio.h
