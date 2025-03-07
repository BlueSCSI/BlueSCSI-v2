/** 
 * Copyright (C) 2023 saybur
 * 
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

#pragma once
#ifdef ENABLE_AUDIO_OUTPUT_SPDIF

#include <stdint.h>

// audio subsystem DMA channels
#define SOUND_DMA_CHA 6
#define SOUND_DMA_CHB 7

// size of the two audio sample buffers, in bytes
// these must be divisible by 1024
// #define AUDIO_BUFFER_SIZE 8192 // ~46.44ms
#define AUDIO_BUFFER_SIZE 4096 // reduce memory usage

/**
 * Handler for DMA interrupts
 *
 * This is called from scsi_dma_irq() in scsi_accel_rp2040.cpp. That is
 * obviously a silly way to handle things. However, using
 * irq_add_shared_handler() causes a lockup, likely due to pico-sdk issue #724
 * fixed in 1.3.1. Current builds use pico-sdk 1.3.0 and are affected by
 * the bug. To work around the problem the above exclusive handler
 * delegates to this function if its normal mask is not matched.
 */
void audio_dma_irq();

/**
 * Indicates if the audio subsystem is actively streaming, including if it is
 * sending silent data during sample stall events.
 *
 * \return true if audio streaming is active, false otherwise.
 */
bool audio_is_active();

/**
 * Initializes the audio subsystem. Should be called only once, toward the end
 * of platform_late_init().
 */
void audio_setup();

/**
 * Called from platform_poll() to fill sample buffer(s) if needed.
 */
void audio_poll();

#endif // ENABLE_AUDIO_OUTPUT_SPDIF
