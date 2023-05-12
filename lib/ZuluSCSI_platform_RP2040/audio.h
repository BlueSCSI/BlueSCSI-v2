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
#ifdef ENABLE_AUDIO_OUTPUT

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

// audio subsystem DMA channels
#define SOUND_DMA_CHA 6
#define SOUND_DMA_CHB 7

// size of the two audio sample buffers, in bytes
// these must be divisible by 1024
#define AUDIO_BUFFER_SIZE 8192 // ~46.44ms

/*
 * Status codes for audio playback, matching the SCSI 'audio status codes'.
 *
 * The first two are for a live condition and will be returned repeatedly. The
 * following two reflect a historical condition and are only returned once.
 */
enum audio_status_code {
    ASC_PLAYING = 0x11,
    ASC_PAUSED = 0x12,
    ASC_COMPLETED = 0x13,
    ASC_ERRORED = 0x14,
    ASC_NO_STATUS = 0x15
};

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
 * \return true if audio streaming is paused, false otherwise.
 */
bool audio_is_paused();

/**
 * \return the owner value passed to the _play() call, or 0xFF if no owner.
 */
uint8_t audio_get_owner();

/**
 * Initializes the audio subsystem. Should be called only once, toward the end
 * of platform_late_init().
 */
void audio_setup();

/**
 * Called from platform_poll() to fill sample buffer(s) if needed.
 */
void audio_poll();

/**
 * Begins audio playback for a file.
 *
 * \param owner  The SCSI ID that initiated this playback operation.
 * \param file   Path of a file containing PCM samples to play.
 * \param start  Byte offset within file where playback will begin, inclusive.
 * \param end    Byte offset within file where playback will end, exclusive.
 * \param swap   If false, little-endian sample order, otherwise big-endian.
 * \return       True if successful, false otherwise.
 */
bool audio_play(uint8_t owner, const char* file, uint64_t start, uint64_t end, bool swap);

/**
 * Pauses audio playback. This may be delayed slightly to allow sample buffers
 * to purge.
 *
 * \param pause  If true, pause, otherwise resume.
 * \return       True if operation changed audio output, false if no change.
 */
bool audio_set_paused(bool pause);

/**
 * Stops audio playback.
 */
void audio_stop();

/**
 * Provides SCSI 'audio status code' for the given target. Depending on the
 * code this operation may produce side-effects, see the enum for details.
 *
 * \param id    The SCSI ID to provide status codes for.
 * \return      The matching audio status code.
 */
audio_status_code audio_get_status_code(uint8_t id);

/**
 * Provides the number of sample bytes read in during an audio_play() call.
 * This can be combined with an (external) starting offset to determine
 * virtual CD positioning information. This is only an approximation since
 * this tracker is always at the end of the most recently read sample data.
 *
 * This is intentionally not cleared by audio_stop(): audio_play() events will
 * reset this information.
 *
 * \param id    The SCSI ID target to return data for.
 * \return      The number of bytes read in during a playback.
 */
uint32_t audio_get_bytes_read(uint8_t id);

/**
 * Clears the byte counter in the above call. This is insensitive to whether
 * audio playback is occurring but is safe to call in any event.
 *
 * \param id    The SCSI ID target to return data for.
 */
void audio_clear_bytes_read(uint8_t id);

#ifdef __cplusplus
}
#endif

#endif // ENABLE_AUDIO_OUTPUT