/** 
 * Copyright (C) 2023 saybur
 * ZuluSCSI™ - Copyright (c) 2023-2025 Rabbit Hole Computing™
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


#pragma once
#ifdef ENABLE_AUDIO_OUTPUT_I2S

extern bool g_audio_enabled;
extern bool g_ode_audio_stopped;

// size of the a circular audio sample buffer, in bytes
// these must be divisible by 1024
#define AUDIO_BUFFER_SIZE 16384
// #define AUDIO_BUFFER_SIZE 8192 // ~46.44ms 
// # define AUDIO_BUFFER_SIZE 4096 // reduce memory usage
#define AUDIO_BUFFER_HALF_SIZE AUDIO_BUFFER_SIZE / 2

/**
 * Handler for I2S DMA interrupts
 */
void ODE_IRQHandler();


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
#endif //ENABLE_AUDIO_OUTPUT_I2S