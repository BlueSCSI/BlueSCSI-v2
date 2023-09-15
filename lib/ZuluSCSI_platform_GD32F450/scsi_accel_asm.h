/** 
 * ZuluSCSI™ - Copyright (c) 2022 Rabbit Hole Computing™
 * 
 * ZuluSCSI™ firmware is licensed under the GPL version 3 or any later version. 
 * 
 * https://www.gnu.org/licenses/gpl-3.0.html
 * ----
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

// SCSI subroutines using hand-optimized assembler

#pragma once

#include <stdint.h>

/*!< Peripheral base address in the bit-band region for a cortex M4 */
#define PERIPH_BB_BASE        ((uint32_t)0x42000000)    

void scsi_accel_asm_send(const uint32_t *buf, uint32_t num_words, volatile int *resetFlag);
void scsi_accel_asm_recv(uint32_t *buf, uint32_t num_words, volatile int *resetFlag);