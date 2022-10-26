// External GreenPAK SLG46824 programmable logic can optionally be used to
// accelerate SCSI communications. This module contains code to load firmware
// to the GreenPAK through I2C.

#pragma once

#include <stdint.h>

bool greenpak_write(uint16_t regaddr, const uint8_t *data, int length);
bool greenpak_read(uint16_t regaddr, uint8_t *data, int length);

bool greenpak_load_firmware();
bool greenpak_is_ready();