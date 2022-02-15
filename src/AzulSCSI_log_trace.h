// SCSI trace logging

#pragma once

#include <stdint.h>

// Called from scsiPhy.cpp
void scsiLogPhaseChange(int new_phase);
void scsiLogDataIn(const uint8_t *buf, uint32_t length);
void scsiLogDataOut(const uint8_t *buf, uint32_t length);