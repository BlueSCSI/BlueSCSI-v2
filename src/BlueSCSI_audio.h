/** 
 * ZuluSCSI™ - Copyright (c) 2022-2025 Rabbit Hole Computing™
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
#include <CUEParser.h>
#include "BlueSCSI_disk.h"
#include "BlueSCSI_audio_math.h"

/*
 * Starting volume level for audio output, with 0 being muted and 255 being
 * max volume. SCSI-2 says this should be 25% of maximum by default, MMC-1
 * says 100%. Testing shows this tends to be obnoxious at high volumes, so
 * go with SCSI-2.
 *
 * This implementation uses the high byte for output port 1 and the low byte
 * for port 0. The two values are averaged to determine final volume level.
 */
#define DEFAULT_VOLUME_LEVEL 0x3F
#define DEFAULT_VOLUME_LEVEL_2CH DEFAULT_VOLUME_LEVEL << 8 | DEFAULT_VOLUME_LEVEL

/*
 * Defines the 'enable' masks for the two audio output ports of each device.
 * If this mask is matched with audio_get_channel() the relevant port will
 * have audio output to it, otherwise it will be muted, regardless of the
 * volume level.
 */
#define AUDIO_CHANNEL_ENABLE_MASK 0x0201

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

#ifdef ENABLE_AUDIO_OUTPUT
/**
 * Begins audio playback for a file.
 *
 * \param owner     The SCSI ID that initiated this playback operation.
 * \param img       Pointer to the image container that can load PCM samples.
 * \param trackinfo Track metadata for the track the host wants to play.
 *                  SPDIF uses it to compute byte offsets and cache it for
 *                  subsequent READ SUB-CHANNEL / seek lookups. I2S may
 *                  ignore it and walk its own cue parser.
 * \param lba       Absolute CD LBA where playback should begin.
 * \param length    Length of the play in CD sectors. Zero means "seek to
 *                  lba but don't actually start audio output" — used by
 *                  the SEEK command path to update position reporting.
 * \param swap      If false, little-endian sample order, otherwise big-endian.
 * \return          True if successful, false otherwise.
 */
bool audio_play(uint8_t owner, image_config_t* img, const CUETrackInfo *trackinfo,
                uint32_t lba, uint32_t length, bool swap);

#if defined(ENABLE_AUDIO_OUTPUT_I2S) && (defined(BLUESCSI_ULTRA) || defined(BLUESCSI_ULTRA_WIDE))
/**
 * Begins audio playback for a file using the track and index.
 *
 * \param owner         The SCSI ID that initiated this playback operation.
 * \param img           Pointer to the image container that can load PCM samples to play.
 * \param start_track   Starting track
 * \param start_index   Starting index
 * \param end_track     LBA offset within file where playback will begin, inclusive.
 * \param end_index     LBA length of play .
 * \return       True if successful, false otherwise.
 */

bool audio_play_track_index(uint8_t owner, image_config_t* img, uint8_t start_track, uint8_t start_index, uint8_t end_track, uint8_t end_index);
#endif
#endif // ENABLE_AUDIO_OUTPUT


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
 * \param id     The SCSI ID to stop audio playback on. If id == 0xFF stop audio on all devices
 */
void audio_stop(uint8_t id = 0xFF);

/**
 * Provides SCSI 'audio status code' for the given target. Depending on the
 * code this operation may produce side-effects, see the enum for details.
 *
 * \param id    The SCSI ID to provide status codes for.
 * \return      The matching audio status code.
 */
audio_status_code audio_get_status_code(uint8_t id);

/**
 * Gets the current volume level for a target. This is a pair of 8-bit values
 * ranging from 0-255 that are averaged together to determine the final output
 * level, where 0 is muted and 255 is maximum volume. The high byte corresponds
 * to 0x0E channel 1 and the low byte to 0x0E channel 0. See the spec's mode
 * page documentation for more details.
 *
 * \param id    SCSI ID to provide volume for.
 * \return      The matching volume level.
 */
uint16_t audio_get_volume(uint8_t id);

/**
 * Sets the volume level for a target, as above. See 0x0E mode page for more.
 *
 * \param id    SCSI ID to set volume for.
 * \param vol   The new volume level.
 */
void audio_set_volume(uint8_t id, uint16_t vol);

/**
 * Gets the 0x0E channel information for both audio ports. The high byte
 * corresponds to port 1 and the low byte to port 0. If the bits defined in
 * AUDIO_CHANNEL_ENABLE_MASK are not set for the respective ports, that
 * output will be muted, regardless of volume set.
 *
 * \param id    SCSI ID to provide channel information for.
 * \return      The channel information.
 */
uint16_t audio_get_channel(uint8_t id);

/**
 * Sets the 0x0E channel information for a target, as above. See 0x0E mode
 * page for more.
 *
 * \param id    SCSI ID to set channel information for.
 * \param chn   The new channel information.
 */
void audio_set_channel(uint8_t id, uint16_t chn);

/**
 * Gets the byte position in the audio image
 *
 * \return byte position in the audio image
*/
uint64_t audio_get_file_position();

/**
 * Gets the absolute CD LBA currently being played.
 *
 * Accounts for the current track's file_offset, data_start and any unstored
 * PREGAP so multi-track BINs and per-track BINs both report correct CD
 * addresses in READ SUB-CHANNEL responses.
 *
 * \return absolute CD LBA, or a best-effort value if no track is cached
 */
uint32_t audio_get_lba_position();

/**
 * Sets the playback position without actually playing audio. Used by the
 * zero-length PLAY AUDIO seek path and by SEEK commands on optical devices.
 *
 * \param id        SCSI ID.
 * \param trackinfo Track metadata for the target LBA. Required for SPDIF to
 *                  translate LBA to a BIN byte offset; may be null on
 *                  backends that walk their own cue parser.
 * \param lba       Absolute CD LBA to seek to.
*/
void audio_set_file_position(uint8_t id, const CUETrackInfo *trackinfo, uint32_t lba);
