// SCSI subroutines using hand-optimized assembler

#pragma once

#include <stdint.h>

/*!< Peripheral base address in the bit-band region for a cortex M4 */
#define PERIPH_BB_BASE        ((uint32_t)0x42000000)         

void scsi_accel_asm_send(const uint32_t *buf, uint32_t num_words, volatile int *resetFlag);
void scsi_accel_asm_recv(uint32_t *buf, uint32_t num_words, volatile int *resetFlag);