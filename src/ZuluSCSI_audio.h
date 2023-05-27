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

#pragma once

#include <stdint.h>
#include "ImageBackingStore.h"

/*
 * Starting volume level for audio output, with 0 being muted and 255 being
 * max volume. SCSI-2 says this should be 25% of maximum by default, MMC-1
 * says 100%. Testing shows this tends to be obnoxious at high volumes, so
 * go with SCSI-2.
 */
#define DEFAULT_VOLUME_LEVEL 63

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
 * Indicates whether there is an active playback event for a given target.
 *
 * Note: this does not consider pause/resume events: even if audio is paused
 * this will indicate playback is in progress.
 *
 * \param owner  The SCSI ID to check.
 * \return       True if playback in progress, false if playback idle.
 */
bool audio_is_playing(uint8_t id);

/**
 * Begins audio playback for a file.
 *
 * \param owner  The SCSI ID that initiated this playback operation.
 * \param img    Pointer to the image containing PCM samples to play.
 * \param start  Byte offset within file where playback will begin, inclusive.
 * \param end    Byte offset within file where playback will end, exclusive.
 * \param swap   If false, little-endian sample order, otherwise big-endian.
 * \return       True if successful, false otherwise.
 */
bool audio_play(uint8_t owner, ImageBackingStore* img, uint64_t start, uint64_t end, bool swap);

/**
 * Pauses audio playback. This may be delayed slightly to allow sample buffers
 * to purge.
 *
 * \param id     The SCSI ID to pause audio playback on.
 * \param pause  If true, pause, otherwise resume.
 * \return       True if operation changed audio output, false if no change.
 */
bool audio_set_paused(uint8_t id, bool pause);

/**
 * Stops audio playback.
 *
 * \param id     The SCSI ID to stop audio playback on.
 */
void audio_stop(uint8_t id);

/**
 * Provides SCSI 'audio status code' for the given target. Depending on the
 * code this operation may produce side-effects, see the enum for details.
 *
 * \param id    The SCSI ID to provide status codes for.
 * \return      The matching audio status code.
 */
audio_status_code audio_get_status_code(uint8_t id);

/**
 * Gets the current volume level for a target. This is a range from 0-255,
 * with 0 being muted and 255 being max volume. See 0x0E mode page for more.
 *
 * \param id    SCSI ID to provide volume for.
 * \return      The matching volume level.
 */
uint8_t audio_get_volume(uint8_t id);

/**
 * Sets the volume level for a target. This is a range from 0-255, with 0
 * being muted and 255 being max volume. See 0x0E mode page for more.
 *
 * \param id    SCSI ID to set volume for.
 * \param vol   The new volume level.
 */
void audio_set_volume(uint8_t id, uint8_t vol);
