// SCSI subroutines using hand-optimized assembler

#pragma once

#include <stdint.h>

void scsi_accel_asm_send(const uint32_t *buf, uint32_t num_words, volatile int *resetFlag);
void scsi_accel_asm_recv(uint32_t *buf, uint32_t num_words, volatile int *resetFlag);