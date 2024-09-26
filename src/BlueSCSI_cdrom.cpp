/* Advanced CD-ROM drive emulation.
 * Adds a few capabilities on top of the SCSI2SD CD-ROM emulation:
 *
 * - bin/cue support for support of multiple tracks
 * - on the fly image switching
 *
 * SCSI2SD V6 - Copyright (C) 2014 Michael McMaster <michael@codesrc.com>
 * ZuluSCSI™ - Copyright (c) 2023 Rabbit Hole Computing™
 *
 * This file is licensed under the GPL version 3 or any later version. 
 * It is derived from cdrom.c in SCSI2SD V6
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
 */


#include <string.h>
#include "BlueSCSI_cdrom.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_config.h"
#include "BlueSCSI_cdrom.h"
#include <CUEParser.h>
#include <assert.h>
#ifdef ENABLE_AUDIO_OUTPUT
#include "BlueSCSI_audio.h"
#endif

extern "C" {
#include <scsi.h>
}

/******************************************/
/* Basic TOC generation without cue sheet */
/******************************************/

static const uint8_t SimpleTOC[] =
{
    0x00, // toc length, MSB
    0x12, // toc length, LSB
    0x01, // First track number
    0x01, // Last track number,
    // TRACK 1 Descriptor
    0x00, // reserved
    0x14, // Q sub-channel encodes current position, Digital track
    0x01, // Track 1,
    0x00, // Reserved
    0x00,0x00,0x00,0x00, // Track start sector (LBA)
    0x00, // reserved
    0x14, // Q sub-channel encodes current position, Digital track
    0xAA, // Leadout Track
    0x00, // Reserved
    0x00,0x00,0x00,0x00, // Track start sector (LBA)
};

static const uint8_t LeadoutTOC[] =
{
    0x00, // toc length, MSB
    0x0A, // toc length, LSB
    0x01, // First track number
    0x01, // Last track number,
    0x00, // reserved
    0x14, // Q sub-channel encodes current position, Digital track
    0xAA, // Leadout Track
    0x00, // Reserved
    0x00,0x00,0x00,0x00, // Track start sector (LBA)
};

static const uint8_t SessionTOC[] =
{
    0x00, // toc length, MSB
    0x0A, // toc length, LSB
    0x01, // First session number
    0x01, // Last session number,
    // TRACK 1 Descriptor
    0x00, // reserved
    0x14, // Q sub-channel encodes current position, Digital track
    0x01, // First track number in last complete session
    0x00, // Reserved
    0x00,0x00,0x00,0x00 // LBA of first track in last session
};


static const uint8_t FullTOC[] =
{
    0x00, //  0: toc length, MSB
    0x2E, //  1: toc length, LSB
    0x01, //  2: First session number
    0x01, //  3: Last session number,
    // A0 Descriptor
    0x01, //  4: session number
    0x14, //  5: ADR/Control
    0x00, //  6: TNO
    0xA0, //  7: POINT
    0x00, //  8: Min
    0x00, //  9: Sec
    0x00, // 10: Frame
    0x00, // 11: Zero
    0x01, // 12: First Track number.
    0x00, // 13: Disc type 00 = Mode 1
    0x00, // 14: PFRAME
    // A1
    0x01, // 15: session number
    0x14, // 16: ADR/Control
    0x00, // 17: TNO
    0xA1, // 18: POINT
    0x00, // 19: Min
    0x00, // 20: Sec
    0x00, // 21: Frame
    0x00, // 22: Zero
    0x01, // 23: Last Track number
    0x00, // 24: PSEC
    0x00, // 25: PFRAME
    // A2
    0x01, // 26: session number
    0x14, // 27: ADR/Control
    0x00, // 28: TNO
    0xA2, // 29: POINT
    0x00, // 30: Min
    0x00, // 31: Sec
    0x00, // 32: Frame
    0x00, // 33: Zero
    0x00, // 34: LEADOUT position
    0x00, // 35: leadout PSEC
    0x00, // 36: leadout PFRAME
    // TRACK 1 Descriptor
    0x01, // 37: session number
    0x14, // 38: ADR/Control
    0x00, // 39: TNO
    0x01, // 40: Point
    0x00, // 41: Min
    0x00, // 42: Sec
    0x00, // 43: Frame
    0x00, // 44: Zero
    0x00, // 45: PMIN
    0x02, // 46: PSEC
    0x00, // 47: PFRAME
};

static const uint8_t DiscInformation[] =
{
    0x00,   //  0: disc info length, MSB
    0x20,   //  1: disc info length, LSB
    0x0E,   //  2: disc status (finalized, single session non-rewritable)
    0x01,   //  3: first track number
    0x01,   //  4: number of sessions (LSB)
    0x01,   //  5: first track in last session (LSB)
    0x01,   //  6: last track in last session (LSB)
    0x00,   //  7: format status (0x00 = non-rewritable, no barcode, no disc id)
    0x00,   //  8: disc type (0x00 = CD-ROM)
    0x00,   //  9: number of sessions (MSB)
    0x00,   // 10: first track in last session (MSB)
    0x00,   // 11: last track in last session (MSB)
    0x00,   // 12: disc ID (MSB)
    0x00,   // 13: .
    0x00,   // 14: .
    0x00,   // 15: disc ID (LSB)
    0x00,   // 16: last session lead-in start (MSB)
    0x00,   // 17: .
    0x00,   // 18: .
    0x00,   // 19: last session lead-in start (LSB)
    0x00,   // 20: last possible lead-out start (MSB)
    0x00,   // 21: .
    0x00,   // 22: .
    0x00,   // 23: last possible lead-out start (LSB)
    0x00,   // 24: disc bar code (MSB)
    0x00,   // 25: .
    0x00,   // 26: .
    0x00,   // 27: .
    0x00,   // 28: .
    0x00,   // 29: .
    0x00,   // 30: .
    0x00,   // 31: disc bar code (LSB)
    0x00,   // 32: disc application code
    0x00,   // 33: number of opc tables
};

static const uint8_t TrackInformation[] =
{
    0x00,   //  0: data length, MSB
    0x1A,   //  1: data length, LSB
    0x01,   //  2: track number
    0x01,   //  3: session number
    0x00,   //  4: reserved
    0x04,   //  5: track mode and flags
    0x8F,   //  6: data mode and flags
    0x00,   //  7: nwa_v
    0x00,   //  8: track start address (MSB)
    0x00,   //  9: .
    0x00,   // 10: .
    0x00,   // 11: track start address (LSB)
    0xFF,   // 12: next writable address (MSB)
    0xFF,   // 13: .
    0xFF,   // 14: .
    0xFF,   // 15: next writable address (LSB)
    0x00,   // 16: free blocks (MSB)
    0x00,   // 17: .
    0x00,   // 18: .
    0x00,   // 19: free blocks (LSB)
    0x00,   // 20: fixed packet size (MSB)
    0x00,   // 21: .
    0x00,   // 22: .
    0x00,   // 23: fixed packet size (LSB)
    0x00,   // 24: track size (MSB)
    0x00,   // 25: .
    0x00,   // 26: .
    0x00,   // 27: track size (LSB)
};

// Convert logical block address to CD-ROM time
static void LBA2MSF(int32_t LBA, uint8_t* MSF, bool relative)
{
    if (!relative) {
        LBA += 150;
    }
    uint32_t ulba = LBA;
    if (LBA < 0) {
        ulba = LBA * -1;
    }

    MSF[2] = ulba % 75; // Frames
    uint32_t rem = ulba / 75;

    MSF[1] = rem % 60; // Seconds
    MSF[0] = rem / 60; // Minutes
}

// Convert logical block address to CD-ROM time in binary coded decimal format
static void LBA2MSFBCD(int32_t LBA, uint8_t* MSF, bool relative)
{
    LBA2MSF(LBA, MSF, relative);
    MSF[0] = ((MSF[0] / 10) << 4) | (MSF[0] % 10);
    MSF[1] = ((MSF[1] / 10) << 4) | (MSF[1] % 10);
    MSF[2] = ((MSF[2] / 10) << 4) | (MSF[2] % 10);
}

// Convert CD-ROM time to logical block address
static int32_t MSF2LBA(uint8_t m, uint8_t s, uint8_t f, bool relative)
{
    int32_t lba = (m * 60 + s) * 75 + f;
    if (!relative) lba -= 150;
    return lba;
}

// Gets the LBA position of the lead-out for the current image
static uint32_t getLeadOutLBA(const CUETrackInfo* lasttrack)
{
    if (lasttrack != nullptr && lasttrack->track_number != 0)
    {
        image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
        uint32_t lastTrackBlocks = (img.file.size() - lasttrack->file_offset)
                / lasttrack->sector_length;
        return lasttrack->data_start + lastTrackBlocks;
    }
    else
    {
        return 1;
    }
}

static void doReadTOCSimple(bool MSF, uint8_t track, uint16_t allocationLength)
{
    if (track == 0xAA)
    {
        // 0xAA requests only lead-out track information (reports capacity)
        uint32_t len = sizeof(LeadoutTOC);
        memcpy(scsiDev.data, LeadoutTOC, len);

        uint32_t capacity = getScsiCapacity(
            scsiDev.target->cfg->sdSectorStart,
            scsiDev.target->liveCfg.bytesPerSector,
            scsiDev.target->cfg->scsiSectors);

        // Replace start of leadout track
        if (MSF)
        {
            scsiDev.data[8] = 0;
            LBA2MSF(capacity, scsiDev.data + 9, false);
        }
        else
        {
            scsiDev.data[8] = capacity >> 24;
            scsiDev.data[9] = capacity >> 16;
            scsiDev.data[10] = capacity >> 8;
            scsiDev.data[11] = capacity;
        }

        if (len > allocationLength)
        {
            len = allocationLength;
        }
        scsiDev.dataLen = len;
        scsiDev.phase = DATA_IN;
    }
    else if (track <= 1)
    {
        // We only support track 1.
        // track 0 means "return all tracks"
        uint32_t len = sizeof(SimpleTOC);
        memcpy(scsiDev.data, SimpleTOC, len);

        if (MSF)
        {
            scsiDev.data[0x0A] = 0x02;
        }

        uint32_t capacity = getScsiCapacity(
            scsiDev.target->cfg->sdSectorStart,
            scsiDev.target->liveCfg.bytesPerSector,
            scsiDev.target->cfg->scsiSectors);

        // Replace start of leadout track
        if (MSF)
        {
            scsiDev.data[0x10] = 0;
            LBA2MSF(capacity, scsiDev.data + 0x11, false);
        }
        else
        {
            scsiDev.data[0x10] = capacity >> 24;
            scsiDev.data[0x11] = capacity >> 16;
            scsiDev.data[0x12] = capacity >> 8;
            scsiDev.data[0x13] = capacity;
        }

        if (len > allocationLength)
        {
            len = allocationLength;
        }
        scsiDev.dataLen = len;
        scsiDev.phase = DATA_IN;
    }
    else
    {
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
        scsiDev.phase = STATUS;
    }
}

static void doReadSessionInfoSimple(bool msf, uint16_t allocationLength)
{
    uint32_t len = sizeof(SessionTOC);
    memcpy(scsiDev.data, SessionTOC, len);

    if (msf)
    {
        scsiDev.data[0x0A] = 0x02;
    }

    if (len > allocationLength)
    {
        len = allocationLength;
    }
    scsiDev.dataLen = len;
    scsiDev.phase = DATA_IN;
}

static void doReadFullTOCSimple(uint8_t session, uint16_t allocationLength, bool useBCD)
{
    // We only support session 1.
    if (session > 1)
    {
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
        scsiDev.phase = STATUS;
    }
    else
    {
        uint32_t len = sizeof(FullTOC);
        memcpy(scsiDev.data, FullTOC, len);

        // update leadout position
        // consistent 2048-byte blocks makes this easier than bin/cue version
        uint32_t capacity = getScsiCapacity(
            scsiDev.target->cfg->sdSectorStart,
            scsiDev.target->liveCfg.bytesPerSector,
            scsiDev.target->cfg->scsiSectors);
        if (useBCD) {
            LBA2MSFBCD(capacity, &scsiDev.data[34], false);
        } else {
            LBA2MSF(capacity, &scsiDev.data[34], false);
        }

        if (len > allocationLength)
        {
            len = allocationLength;
        }
        scsiDev.dataLen = len;
        scsiDev.phase = DATA_IN;
    }
}

void doReadDiscInformationSimple(uint16_t allocationLength)
{
    uint32_t len = sizeof(DiscInformation);
    memcpy(scsiDev.data, DiscInformation, len);
    if (len > allocationLength)
    {
        len = allocationLength;
    }
    scsiDev.dataLen = len;
    scsiDev.phase = DATA_IN;
}

void doReadTrackInformationSimple(bool track, uint32_t lba, uint16_t allocationLength)
{
    uint32_t len = sizeof(TrackInformation);
    memcpy(scsiDev.data, TrackInformation, len);

    uint32_t capacity = getScsiCapacity(
            scsiDev.target->cfg->sdSectorStart,
            scsiDev.target->liveCfg.bytesPerSector,
            scsiDev.target->cfg->scsiSectors);
    if (!track && lba >= capacity)
    {
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
        scsiDev.phase = STATUS;
    }
    else
    {
        // update track size
        scsiDev.data[24] = capacity >> 24;
        scsiDev.data[25] = capacity >> 16;
        scsiDev.data[26] = capacity >> 8;
        scsiDev.data[27] = capacity;

        if (len > allocationLength)
        {
            len = allocationLength;
        }
        scsiDev.dataLen = len;
        scsiDev.phase = DATA_IN;
    }
}

/*********************************/
/* TOC generation from cue sheet */
/*********************************/

// Fetch track info based on LBA
static void getTrackFromLBA(CUEParser &parser, uint32_t lba, CUETrackInfo *result)
{
    // Track info in case we have no .cue file
    result->file_mode = CUEFile_BINARY;
    result->track_mode = CUETrack_MODE1_2048;
    result->sector_length = 2048;
    result->track_number = 1;

    const CUETrackInfo *tmptrack;
    while ((tmptrack = parser.next_track()) != NULL)
    {
        if (tmptrack->track_start <= lba)
        {
            *result = *tmptrack;
        }
        else
        {
            break;
        }
    }
}

// Format track info read from cue sheet into the format used by ReadTOC command.
// Refer to T10/1545-D MMC-4 Revision 5a, "Response Format 0000b: Formatted TOC"
static void formatTrackInfo(const CUETrackInfo *track, uint8_t *dest, bool use_MSF_time)
{
    uint8_t control_adr = 0x14; // Digital track

    if (track->track_mode == CUETrack_AUDIO)
    {
        control_adr = 0x10; // Audio track
    }

    dest[0] = 0; // Reserved
    dest[1] = control_adr;
    dest[2] = track->track_number;
    dest[3] = 0; // Reserved

    if (use_MSF_time)
    {
        // Time in minute-second-frame format
        dest[4] = 0;
        LBA2MSF(track->data_start, &dest[5], false);
    }
    else
    {
        // Time as logical block address
        dest[4] = (track->data_start >> 24) & 0xFF;
        dest[5] = (track->data_start >> 16) & 0xFF;
        dest[6] = (track->data_start >>  8) & 0xFF;
        dest[7] = (track->data_start >>  0) & 0xFF;
    }
}

// Load data from CUE sheet for the given device,
// using the second half of scsiDev.data buffer for temporary storage.
// Returns false if no cue sheet or it could not be opened.
static bool loadCueSheet(image_config_t &img, CUEParser &parser)
{
    if (!img.cuesheetfile.isOpen())
    {
        return false;
    }

    // Use second half of scsiDev.data as the buffer for cue sheet text
    size_t halfbufsize = sizeof(scsiDev.data) / 2;
    char *cuebuf = (char*)&scsiDev.data[halfbufsize];
    img.cuesheetfile.seek(0);
    int len = img.cuesheetfile.read(cuebuf, halfbufsize);

    if (len <= 0)
    {
        return false;
    }

    cuebuf[len] = '\0';
    parser = CUEParser(cuebuf);
    return true;
}

static void doReadTOC(bool MSF, uint8_t track, uint16_t allocationLength)
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    CUEParser parser;
    if (!loadCueSheet(img, parser))
    {
        // No CUE sheet, use hardcoded data
        return doReadTOCSimple(MSF, track, allocationLength);
    }

    // Format track info
    uint8_t *trackdata = &scsiDev.data[4];
    int trackcount = 0;
    int firsttrack = -1;
    CUETrackInfo lasttrack = {0};
    const CUETrackInfo *trackinfo;
    while ((trackinfo = parser.next_track()) != NULL)
    {
        if (firsttrack < 0) firsttrack = trackinfo->track_number;
        lasttrack = *trackinfo;

        if (track <= trackinfo->track_number)
        {
            formatTrackInfo(trackinfo, &trackdata[8 * trackcount], MSF);
            trackcount += 1;
        }
    }

    // Format lead-out track info
    CUETrackInfo leadout = {};
    leadout.track_number = 0xAA;
    leadout.track_mode = (lasttrack.track_number != 0) ? lasttrack.track_mode : CUETrack_MODE1_2048;
    leadout.data_start = getLeadOutLBA(&lasttrack);
    formatTrackInfo(&leadout, &trackdata[8 * trackcount], MSF);
    trackcount += 1;

    // Format response header
    uint16_t toc_length = 2 + trackcount * 8;
    scsiDev.data[0] = toc_length >> 8;
    scsiDev.data[1] = toc_length & 0xFF;
    scsiDev.data[2] = firsttrack;
    scsiDev.data[3] = lasttrack.track_number;

    if (track != 0xAA && trackcount < 2)
    {
        // Unknown track requested
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
        scsiDev.phase = STATUS;
    }
    else
    {
        uint32_t len = 2 + toc_length;

        if (len > allocationLength)
        {
            len = allocationLength;
        }

        scsiDev.dataLen = len;
        scsiDev.phase = DATA_IN;
    }
}

static void doReadSessionInfo(bool msf, uint16_t allocationLength)
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    CUEParser parser;
    if (!loadCueSheet(img, parser))
    {
        // No CUE sheet, use hardcoded data
        return doReadSessionInfoSimple(msf, allocationLength);
    }

    uint32_t len = sizeof(SessionTOC);
    memcpy(scsiDev.data, SessionTOC, len);

    // Replace first track info in the session table
    // based on data from CUE sheet.
    const CUETrackInfo *trackinfo = parser.next_track();
    if (trackinfo)
    {
        formatTrackInfo(trackinfo, &scsiDev.data[4], false);
    }

    if (len > allocationLength)
    {
        len = allocationLength;
    }
    scsiDev.dataLen = len;
    scsiDev.phase = DATA_IN;
}

// Format track info read from cue sheet into the format used by ReadFullTOC command.
// Refer to T10/1545-D MMC-4 Revision 5a, "Response Format 0010b: Raw TOC"
static void formatRawTrackInfo(const CUETrackInfo *track, uint8_t *dest, bool useBCD)
{
    uint8_t control_adr = 0x14; // Digital track

    if (track->track_mode == CUETrack_AUDIO)
    {
        control_adr = 0x10; // Audio track
    }

    dest[0] = 0x01; // Session always 1
    dest[1] = control_adr;
    dest[2] = 0x00; // "TNO", always 0?
    dest[3] = track->track_number; // "POINT", contains track number
    // Next three are ATIME. The spec doesn't directly address how these
    // should be reported in the TOC, just giving a description of Q-channel
    // data from Red Book/ECMA-130. On all disks tested so far these are
    // given as 00/00/00.
    dest[4] = 0x00;
    dest[5] = 0x00;
    dest[6] = 0x00;
    dest[7] = 0; // HOUR

    if (useBCD) {
        LBA2MSFBCD(track->data_start, &dest[8], false);
    } else {
        LBA2MSF(track->data_start, &dest[8], false);
    }
}

static void doReadFullTOC(uint8_t session, uint16_t allocationLength, bool useBCD)
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    CUEParser parser;
    if (!loadCueSheet(img, parser))
    {
        // No CUE sheet, use hardcoded data
        return doReadFullTOCSimple(session, allocationLength, useBCD);
    }

    // We only support session 1.
    if (session > 1)
    {
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
        scsiDev.phase = STATUS;
        return;
    }

    // Take the beginning of the hardcoded TOC as base
    uint32_t len = 4 + 11 * 3; // Header, A0, A1, A2
    memcpy(scsiDev.data, FullTOC, len);

    // Add track descriptors
    int trackcount = 0;
    int firsttrack = -1;
    CUETrackInfo lasttrack = {0};
    const CUETrackInfo *trackinfo;
    while ((trackinfo = parser.next_track()) != NULL)
    {
        if (firsttrack < 0)
        {
            firsttrack = trackinfo->track_number;
            if (trackinfo->track_mode == CUETrack_AUDIO)
            {
                scsiDev.data[5] = 0x10;
            }
        }
        lasttrack = *trackinfo;

        formatRawTrackInfo(trackinfo, &scsiDev.data[len], useBCD);
        trackcount += 1;
        len += 11;
    }

    // First and last track numbers
    scsiDev.data[12] = firsttrack;
    if (lasttrack.track_number != 0)
    {
        scsiDev.data[23] = lasttrack.track_number;
        if (lasttrack.track_mode == CUETrack_AUDIO)
        {
            scsiDev.data[16] = 0x10;
            scsiDev.data[27] = 0x10;
        }
    }

    // Leadout track position
    if (useBCD) {
        LBA2MSFBCD(getLeadOutLBA(&lasttrack), &scsiDev.data[34], false);
    } else {
        LBA2MSF(getLeadOutLBA(&lasttrack), &scsiDev.data[34], false);
    }

    // Correct the record length in header
    uint16_t toclen = len - 2;
    scsiDev.data[0] = toclen >> 8;
    scsiDev.data[1] = toclen & 0xFF;

    if (len > allocationLength)
    {
        len = allocationLength;
    }
    scsiDev.dataLen = len;
    scsiDev.phase = DATA_IN;
}

// SCSI-3 MMC Read Header command, seems to be deprecated in later standards.
// Refer to ANSI X3.304-1997
// The spec is vague, but based on experimentation with a Matshita drive this
// command should return the sector header absolute time (see ECMA-130 21).
// Given 2048-byte block sizes this effectively is 1:1 with the provided LBA.
void doReadHeader(bool MSF, uint32_t lba, uint16_t allocationLength)
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;

#if ENABLE_AUDIO_OUTPUT
    // terminate audio playback if active on this target (Annex C)
    audio_stop(img.scsiId & S2S_CFG_TARGET_ID_BITS);
#endif

    uint8_t mode = 1;
    CUEParser parser;
    if (loadCueSheet(img, parser))
    {
        // Search the track with the requested LBA
        CUETrackInfo trackinfo = {};
        getTrackFromLBA(parser, lba, &trackinfo);

        // Track mode (audio / data)
        if (trackinfo.track_mode == CUETrack_AUDIO)
        {
            scsiDev.data[0] = 0;
        }
    }

    scsiDev.data[0] = mode;
    scsiDev.data[1] = 0; // reserved
    scsiDev.data[2] = 0; // reserved
    scsiDev.data[3] = 0; // reserved

    // Track start
    if (MSF)
    {
        scsiDev.data[4] = 0;
        LBA2MSF(lba, &scsiDev.data[5], false);
    }
    else
    {
        scsiDev.data[4] = (lba >> 24) & 0xFF;
        scsiDev.data[5] = (lba >> 16) & 0xFF;
        scsiDev.data[6] = (lba >>  8) & 0xFF;
        scsiDev.data[7] = (lba >>  0) & 0xFF;
    }

    uint8_t len = 8;
    if (len > allocationLength)
    {
        len = allocationLength;
    }
    scsiDev.dataLen = len;
    scsiDev.phase = DATA_IN;
}

void doReadDiscInformation(uint16_t allocationLength)
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    CUEParser parser;
    if (!loadCueSheet(img, parser))
    {
        // No CUE sheet, use hardcoded data
        return doReadDiscInformationSimple(allocationLength);
    }

    // Take the hardcoded header as base
    uint32_t len = sizeof(DiscInformation);
    memcpy(scsiDev.data, DiscInformation, len);

    // Find first and last track number
    int firsttrack = -1;
    int lasttrack = -1;
    const CUETrackInfo *trackinfo;
    while ((trackinfo = parser.next_track()) != NULL)
    {
        if (firsttrack < 0) firsttrack = trackinfo->track_number;
        lasttrack = trackinfo->track_number;
    }

    scsiDev.data[3] = firsttrack;
    scsiDev.data[5] = firsttrack;
    scsiDev.data[6] = lasttrack;

    if (len > allocationLength)
    {
        len = allocationLength;
    }
    scsiDev.dataLen = len;
    scsiDev.phase = DATA_IN;
}

void doReadTrackInformation(bool track, uint32_t lba, uint16_t allocationLength)
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    CUEParser parser;
    if (!loadCueSheet(img, parser))
    {
        // No CUE sheet, use hardcoded data
        return doReadTrackInformationSimple(track, lba, allocationLength);
    }

    // Take the hardcoded header as base
    uint32_t len = sizeof(TrackInformation);
    memcpy(scsiDev.data, TrackInformation, len);

    // Step through the tracks until the one requested is found
    // Result will be placed in mtrack for later use if found
    bool trackfound = false;
    uint32_t tracklen = 0;
    CUETrackInfo mtrack = {0};
    const CUETrackInfo *trackinfo;
    while ((trackinfo = parser.next_track()) != NULL)
    {
        if (mtrack.track_number != 0) // skip 1st track, just store later
        {
            if ((track && lba == mtrack.track_number)
                || (!track && lba < trackinfo->data_start))
            {
                trackfound = true;
                tracklen = trackinfo->data_start - mtrack.data_start;
                break;
            }
        }
        mtrack = *trackinfo;
    }
    // try the last track as a final attempt if no match found beforehand
    if (!trackfound)
    {
        uint32_t lastLba = getLeadOutLBA(&mtrack);
        if ((track && lba == mtrack.track_number)
            || (!track && lba < lastLba))
        {
            trackfound = true;
            tracklen = lastLba - mtrack.data_start;
        }
    }

    // bail out if no match found
    if (!trackfound)
    {
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
        scsiDev.phase = STATUS;
        return;
    }

    // rewrite relevant bytes, starting with track number
    scsiDev.data[3] = mtrack.track_number;

    // track mode
    if (mtrack.track_mode == CUETrack_AUDIO)
    {
        scsiDev.data[5] = 0x00;
    }

    // track start
    uint32_t start = mtrack.data_start;
    scsiDev.data[8] = start >> 24;
    scsiDev.data[9] = start >> 16;
    scsiDev.data[10] = start >> 8;
    scsiDev.data[11] = start;

    // track size
    scsiDev.data[24] = tracklen >> 24;
    scsiDev.data[25] = tracklen >> 16;
    scsiDev.data[26] = tracklen >> 8;
    scsiDev.data[27] = tracklen;

    debuglog("------ Reporting track ", mtrack.track_number, ", start ", start,
            ", length ", tracklen);
    if (len > allocationLength)
    {
        len = allocationLength;
    }
    scsiDev.dataLen = len;
    scsiDev.phase = DATA_IN;
}

void doGetConfiguration(uint8_t rt, uint16_t startFeature, uint16_t allocationLength)
{
    // rt = 0 is all features, rt = 1 is current features,
    // rt = 2 only startFeature, others reserved
    if (rt > 2)
    {
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
        scsiDev.phase = STATUS;
        return;
    }

    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;

    // write feature header
    uint32_t len = 8; // length bytes set at end of call
    scsiDev.data[4] = 0; // reserved
    scsiDev.data[5] = 0; // reserved
    if (!img.ejected)
    {
        // disk in drive, current profile is CD-ROM
        scsiDev.data[6] = 0x00;
        scsiDev.data[7] = 0x08;
    }
    else
    {
        // no disk, report no current profile
        scsiDev.data[6] = 0;
        scsiDev.data[7] = 0;
    }

    // profile list (0)
    if ((rt == 2 && 0 == startFeature)
        || (rt == 1 && startFeature <= 0)
        || (rt == 0 && startFeature <= 0))
    {
        scsiDev.data[len++] = 0x00;
        scsiDev.data[len++] = 0x00;
        scsiDev.data[len++] = 0x03; // ver 0, persist=1,current=1
        scsiDev.data[len++] = 8; // 2 more
        // CD-ROM profile
        scsiDev.data[len++] = 0x00;
        scsiDev.data[len++] = 0x08;
        scsiDev.data[len++] = (img.ejected) ? 0x00 : 0x01;
        scsiDev.data[len++] = 0;
        // removable disk profile
        scsiDev.data[len++] = 0x00;
        scsiDev.data[len++] = 0x02;
        scsiDev.data[len++] = 0x00;
        scsiDev.data[len++] = 0;
    }

    // core feature (1)
    if ((rt == 2 && startFeature == 1)
        || (rt == 1 && startFeature <= 1)
        || (rt == 0 && startFeature <= 1))
    {
        scsiDev.data[len++] = 0x00;
        scsiDev.data[len++] = 0x01;
        scsiDev.data[len++] = 0x0B; // ver 2, persist=1,current=1
        scsiDev.data[len++] = 8;
        // physical interface standard (SCSI)
        scsiDev.data[len++] = 0x00;
        scsiDev.data[len++] = 0x00;
        scsiDev.data[len++] = 0x00;
        scsiDev.data[len++] = 0x01;
        scsiDev.data[len++] = 0x03; // support INQ2 and DBE
        scsiDev.data[len++] = 0;
        scsiDev.data[len++] = 0;
        scsiDev.data[len++] = 0;
    }

    // morphing feature (2)
    if ((rt == 2 && startFeature == 2)
        || (rt == 1 && startFeature <= 2)
        || (rt == 0 && startFeature <= 2))
    {
        scsiDev.data[len++] = 0x00;
        scsiDev.data[len++] = 0x02;
        scsiDev.data[len++] = 0x07; // ver 1, persist=1,current=1
        scsiDev.data[len++] = 4;
        scsiDev.data[len++] = 0x02; // OCEvent=1,async=0
        scsiDev.data[len++] = 0;
        scsiDev.data[len++] = 0;
        scsiDev.data[len++] = 0;
    }

    // removable medium feature (3)
    if ((rt == 2 && startFeature == 3)
        || (rt == 1 && startFeature <= 3)
        || (rt == 0 && startFeature <= 3))
    {
        scsiDev.data[len++] = 0x00;
        scsiDev.data[len++] = 0x03;
        scsiDev.data[len++] = 0x03; // ver 0, persist=1,current=1
        scsiDev.data[len++] = 4;
        scsiDev.data[len++] = 0x28; // matches 0x2A mode page version
        scsiDev.data[len++] = 0;
        scsiDev.data[len++] = 0;
        scsiDev.data[len++] = 0;
    }

    // random readable feature (0x10, 16)
    if ((rt == 2 && startFeature == 16)
        || (rt == 1 && startFeature <= 16 && !img.ejected)
        || (rt == 0 && startFeature <= 16))
    {
        scsiDev.data[len++] = 0x00;
        scsiDev.data[len++] = 0x10;
        // ver 0, persist=0,current=drive state
        scsiDev.data[len++] = (img.ejected) ? 0x00 : 0x01;
        scsiDev.data[len++] = 8;
        scsiDev.data[len++] = 0x00; // 2048 (MSB)
        scsiDev.data[len++] = 0x00; // .
        scsiDev.data[len++] = 0x08; // .
        scsiDev.data[len++] = 0x00; // 2048 (LSB)
        scsiDev.data[len++] = 0x00;
        // one block min when disk in drive only
        scsiDev.data[len++] = (img.ejected) ? 0x00 : 0x01;
        scsiDev.data[len++] = 0x00; // no support for PP error correction (TODO?)
        scsiDev.data[len++] = 0;
    }

    // multi-read feature (0x1D, 29)
    if ((rt == 2 && startFeature == 29)
        || (rt == 1 && startFeature <= 29 && !img.ejected)
        || (rt == 0 && startFeature <= 29))
    {
        scsiDev.data[len++] = 0x00;
        scsiDev.data[len++] = 0x1D;
        // ver 0, persist=0,current=drive state
        scsiDev.data[len++] = (img.ejected) ? 0x00 : 0x01;
        scsiDev.data[len++] = 0;
    }

    // CD read feature (0x1E, 30)
    if ((rt == 2 && startFeature == 30)
        || (rt == 1 && startFeature <= 30 && !img.ejected)
        || (rt == 0 && startFeature <= 30))
    {
        scsiDev.data[len++] = 0x00;
        scsiDev.data[len++] = 0x1E;
        // ver 2, persist=0,current=drive state
        scsiDev.data[len++] = (img.ejected) ? 0x08 : 0x09;
        scsiDev.data[len++] = 4;
        scsiDev.data[len++] = 0x00; // dap=0,c2=0,cd-text=0
        scsiDev.data[len++] = 0;
        scsiDev.data[len++] = 0;
        scsiDev.data[len++] = 0;
    }

#ifdef ENABLE_AUDIO_OUTPUT
    // CD audio feature (0x103, 259)
    if ((rt == 2 && startFeature == 259)
        || (rt == 1 && startFeature <= 259 && !img.ejected)
        || (rt == 0 && startFeature <= 259))
    {
        scsiDev.data[len++] = 0x01;
        scsiDev.data[len++] = 0x03;
        // ver 1, persist=0,current=drive state
        scsiDev.data[len++] = (img.ejected) ? 0x04 : 0x05;
        scsiDev.data[len++] = 4;
        scsiDev.data[len++] = 0x03; // scan=0,scm=1,sv=1
        scsiDev.data[len++] = 0;
        scsiDev.data[len++] = 0x01; // 256 volume levels
        scsiDev.data[len++] = 0x00; // .
    }
#endif

    // finally, rewrite data length to match
    uint32_t dlen = len - 8;
    scsiDev.data[0] = dlen >> 24;
    scsiDev.data[1] = dlen >> 16;
    scsiDev.data[2] = dlen >> 8;
    scsiDev.data[3] = dlen;

    if (len > allocationLength)
    {
        len = allocationLength;
    }
    scsiDev.dataLen = len;
    scsiDev.phase = DATA_IN;
}

/****************************************/
/* CUE sheet check at image load time   */
/****************************************/

bool cdromValidateCueSheet(image_config_t &img)
{
    CUEParser parser;
    if (!loadCueSheet(img, parser))
    {
        return false;
    }

    const CUETrackInfo *trackinfo;
    int trackcount = 0;
    while ((trackinfo = parser.next_track()) != NULL)
    {
        trackcount++;

        if (trackinfo->track_mode != CUETrack_AUDIO &&
            trackinfo->track_mode != CUETrack_MODE1_2048 &&
            trackinfo->track_mode != CUETrack_MODE1_2352)
        {
            log("---- Warning: track ", trackinfo->track_number, " has unsupported mode ", (int)trackinfo->track_mode);
        }

        if (trackinfo->file_mode != CUEFile_BINARY)
        {
            log("---- Unsupported CUE data file mode ", (int)trackinfo->file_mode);
        }
    }

    if (trackcount == 0)
    {
        log("---- Opened cue sheet but no valid tracks found");
        return false;
    }

    log("---- Cue sheet loaded with ", (int)trackcount, " tracks");
    return true;
}

/**************************************/
/* Ejection and image switching logic */
/**************************************/

// Close CD-ROM tray and note media change event
void cdromCloseTray(image_config_t &img)
{
    if (img.ejected)
    {
        uint8_t target = img.scsiId & S2S_CFG_TARGET_ID_BITS;
        debuglog("------ CDROM close tray on ID ", (int)target);
        img.ejected = false;
        img.cdrom_events = 2; // New media

        if (scsiDev.boardCfg.flags & S2S_CFG_ENABLE_UNIT_ATTENTION)
        {
            debuglog("------ Posting UNIT ATTENTION after medium change");
            scsiDev.targets[target].unitAttention = NOT_READY_TO_READY_TRANSITION_MEDIUM_MAY_HAVE_CHANGED;
        }
    }
}

// Eject CD-ROM tray if closed, close if open
// Switch image on ejection.
void cdromPerformEject(image_config_t &img)
{
    uint8_t target = img.scsiId & S2S_CFG_TARGET_ID_BITS;
#if ENABLE_AUDIO_OUTPUT
    // terminate audio playback if active on this target (MMC-1 Annex C)
    audio_stop(target);
#endif
    if (!img.ejected)
    {
        debuglog("------ CDROM open tray on ID ", (int)target);
        img.ejected = true;
        img.cdrom_events = 3; // Media removal
        switchNextImage(img); // Switch media for next time
    }
    else
    {
        cdromCloseTray(img);
    }
}

// Reinsert any ejected CD-ROMs on reboot
void cdromReinsertFirstImage(image_config_t &img)
{
    if (img.image_index > 0)
    {
        // Multiple images for this drive, force restart from first one
        uint8_t target = img.scsiId & S2S_CFG_TARGET_ID_BITS;
        debuglog("---- Restarting from first CD-ROM image for ID ", (int)target);
        img.image_index = -1;
        img.current_image[0] = '\0';
        switchNextImage(img);
    }
    else if (img.ejected)
    {
        // Reinsert the single image
        cdromCloseTray(img);
    }
}

static void doGetEventStatusNotification(bool immed)
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;

    if (!immed)
    {
        // Asynchronous notification not supported
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
        scsiDev.phase = STATUS;
    }
    else if (img.cdrom_events)
    {
        scsiDev.data[0] = 0;
        scsiDev.data[1] = 6; // EventDataLength
        scsiDev.data[2] = 0x04; // Media status events
        scsiDev.data[3] = 0x04; // Supported events
        scsiDev.data[4] = img.cdrom_events;
        scsiDev.data[5] = 0x01; // Power status
        scsiDev.data[6] = 0; // Start slot
        scsiDev.data[7] = 0; // End slot
        scsiDev.dataLen = 8;
        scsiDev.phase = DATA_IN;
        img.cdrom_events = 0;

        if (img.ejected && img.reinsert_after_eject)
        {
            // We are now reporting to host that the drive is open.
            // Simulate a "close" for next time the host polls.
            cdromCloseTray(img);
        }
    }
    else
    {
        scsiDev.data[0] = 0;
        scsiDev.data[1] = 2; // EventDataLength
        scsiDev.data[2] = 0x00; // Media status events
        scsiDev.data[3] = 0x04; // Supported events
        scsiDev.dataLen = 4;
        scsiDev.phase = DATA_IN;
    }
}

/**************************************/
/* CD-ROM audio playback              */
/**************************************/

void cdromGetAudioPlaybackStatus(uint8_t *status, uint32_t *current_lba, bool current_only)
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;

#ifdef ENABLE_AUDIO_OUTPUT
    if (status) {
        uint8_t target = img.scsiId & S2S_CFG_TARGET_ID_BITS;
        if (current_only) {
            *status = audio_is_playing(target) ? 1 : 0;
        } else {
            *status = (uint8_t) audio_get_status_code(target);
        }
    }
#else
    if (status) *status = 0; // audio status code for 'unsupported/invalid' and not-playing indicator
#endif
    if (current_lba)
    {
        if (img.file.isOpen()) {
            *current_lba = img.file.position() / 2352;
        } else {
            *current_lba = 0;
        }
    }
}

static void doPlayAudio(uint32_t lba, uint32_t length)
{
#ifdef ENABLE_AUDIO_OUTPUT
    debuglog("------ CD-ROM Play Audio request at ", lba, " for ", length, " sectors");
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    uint8_t target_id = img.scsiId & S2S_CFG_TARGET_ID_BITS;

    // Per Annex C terminate playback immediately if already in progress on
    // the current target. Non-current targets may also get their audio
    // interrupted later due to hardware limitations
    audio_stop(target_id);

    // if transfer length is zero no audio playback happens.
    // don't treat as an error per SCSI-2; handle via short-circuit
    if (length == 0)
    {
        scsiDev.status = 0;
        scsiDev.phase = STATUS;
        return;
    }

    // if actual playback is requested perform steps to verify prior to playback
    CUEParser parser;
    if (loadCueSheet(img, parser))
    {
        CUETrackInfo trackinfo = {};
        getTrackFromLBA(parser, lba, &trackinfo);

        if (lba == 0xFFFFFFFF)
        {
            // request to start playback from 'current position'
            lba = img.file.position() / 2352;
        }

        uint64_t offset = trackinfo.file_offset
                + trackinfo.sector_length * (lba - trackinfo.track_start);
        debuglog("------ Play audio CD: ", (int)length, " sectors starting at ", (int)lba,
           ", track number ", trackinfo.track_number, ", data offset in file ", (int)offset);

        if (trackinfo.track_mode != CUETrack_AUDIO)
        {
            debuglog("---- Host tried audio playback on track type ", (int)trackinfo.track_mode);
            scsiDev.status = CHECK_CONDITION;
            scsiDev.target->sense.code = ILLEGAL_REQUEST;
            scsiDev.target->sense.asc = 0x6400; // ILLEGAL MODE FOR THIS TRACK
            scsiDev.phase = STATUS;
            return;
        }

        // playback request appears to be sane, so perform it
        // see earlier note for context on the block length below
        if (!audio_play(target_id, &(img.file), offset,
                offset + length * trackinfo.sector_length, false))
        {
            // Underlying data/media error? Fake a disk scratch, which should
            // be a condition most CD-DA players are expecting
            scsiDev.status = CHECK_CONDITION;
            scsiDev.target->sense.code = MEDIUM_ERROR;
            scsiDev.target->sense.asc = 0x1106; // CIRC UNRECOVERED ERROR
            scsiDev.phase = STATUS;
            return;
        }
        scsiDev.status = 0;
        scsiDev.phase = STATUS;
    }
    else
    {
        // virtual drive supports audio, just not with this disk image
        debuglog("---- Request to play audio on non-audio image");
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = 0x6400; // ILLEGAL MODE FOR THIS TRACK
        scsiDev.phase = STATUS;
    }
#else
    debuglog("---- Target does not support audio playback");
    // per SCSI-2, targets not supporting audio respond to zero-length
    // PLAY AUDIO commands with ILLEGAL REQUEST; this seems to be a check
    // performed by at least some audio playback software
    scsiDev.status = CHECK_CONDITION;
    scsiDev.target->sense.code = ILLEGAL_REQUEST;
    scsiDev.target->sense.asc = 0x0000; // NO ADDITIONAL SENSE INFORMATION
    scsiDev.phase = STATUS;
#endif
}

static void doPauseResumeAudio(bool resume)
{
#ifdef ENABLE_AUDIO_OUTPUT
    log("------ CD-ROM ", resume ? "resume" : "pause", " audio playback");
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    uint8_t target_id = img.scsiId & S2S_CFG_TARGET_ID_BITS;

    if (audio_is_playing(target_id))
    {
        audio_set_paused(target_id, !resume);
        scsiDev.status = 0;
        scsiDev.phase = STATUS;
    }
    else
    {
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = 0x2C00; // COMMAND SEQUENCE ERROR
        scsiDev.phase = STATUS;
    }
#else
    debuglog("---- Target does not support audio pausing");
    scsiDev.status = CHECK_CONDITION;
    scsiDev.target->sense.code = ILLEGAL_REQUEST; // assumed from PLAY AUDIO(10)
    scsiDev.target->sense.asc = 0x0000; // NO ADDITIONAL SENSE INFORMATION
    scsiDev.phase = STATUS;
#endif
}

static void doStopAudio()
{
    debuglog("------ CD-ROM Stop Audio request");
#ifdef ENABLE_AUDIO_OUTPUT
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    uint8_t target_id = img.scsiId & S2S_CFG_TARGET_ID_BITS;
    audio_stop(target_id);
#endif
}

static void doMechanismStatus(uint16_t allocation_length)
{
    uint8_t *buf = scsiDev.data;

    uint8_t status;
    uint32_t lba;
    cdromGetAudioPlaybackStatus(&status, &lba, true);

    *buf++ = 0x00; // No fault state
    *buf++ = (status) ? 0x20 : 0x00; // Currently playing?
    *buf++ = (lba >> 16) & 0xFF;
    *buf++ = (lba >> 8) & 0xFF;
    *buf++ = (lba >> 0) & 0xFF;
    *buf++ = 0; // No CD changer
    *buf++ = 0;
    *buf++ = 0;

    int len = 8;
    if (len > allocation_length) len = allocation_length;

    scsiDev.dataLen = len;
    scsiDev.phase = DATA_IN;
}

/*******************************************/
/* CD-ROM data reading in low level format */
/*******************************************/

static void doReadCD(uint32_t lba, uint32_t length, uint8_t sector_type,
                     uint8_t main_channel, uint8_t sub_channel, bool data_only)
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;

#if ENABLE_AUDIO_OUTPUT
    // terminate audio playback if active on this target (Annex C)
    audio_stop(img.scsiId & S2S_CFG_TARGET_ID_BITS);
#endif

    CUEParser parser;
    if (!loadCueSheet(img, parser)
        && (sector_type == 0 || sector_type == 2)
        && main_channel == 0x10 && sub_channel == 0)
    {
        // Simple case, return sector data directly
        scsiDiskStartRead(lba, length);
        return;
    }

    // Search the track with the requested LBA
    // Supplies dummy data if no cue sheet is active.
    CUETrackInfo trackinfo = {};
    getTrackFromLBA(parser, lba, &trackinfo);

    // Figure out the data offset in the file
    uint64_t offset = trackinfo.file_offset + trackinfo.sector_length * (lba - trackinfo.data_start);
    debuglog("------ Read CD: ", (int)length, " sectors starting at ", (int)lba,
           ", track number ", trackinfo.track_number, ", sector size ", (int)trackinfo.sector_length,
           ", main channel ", main_channel, ", sub channel ", sub_channel,
           ", data offset in file ", (int)offset);

    // Ensure read is not out of range of the image
    uint64_t readend = offset + trackinfo.sector_length * length;
    if (readend > img.file.size())
    {
        log("WARNING: Host attempted CD read at sector ", lba, "+", length,
              ", exceeding image size ", img.file.size());
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
        scsiDev.phase = STATUS;
        return;
    }

    // Verify sector type
    if (sector_type != 0)
    {
        bool sector_type_ok = false;
        if (sector_type == 1 && trackinfo.track_mode == CUETrack_AUDIO)
        {
            sector_type_ok = true;
        }
        else if (sector_type == 2 && trackinfo.track_mode == CUETrack_MODE1_2048)
        {
            sector_type_ok = true;
        }

        if (!sector_type_ok)
        {
            debuglog("---- Failed sector type check, host requested ", (int)sector_type, " CUE file has ", (int)trackinfo.track_mode);
            scsiDev.status = CHECK_CONDITION;
            scsiDev.target->sense.code = ILLEGAL_REQUEST;
            scsiDev.target->sense.asc = 0x6400; // ILLEGAL MODE FOR THIS TRACK
            scsiDev.phase = STATUS;
            return;
        }
    }

    // Select fields to transfer
    // Refer to table 351 in T10/1545-D MMC-4 Revision 5a
    // Only the mandatory cases are supported.
    int sector_length = 0;
    int skip_begin = 0;
    bool add_fake_headers = false;

    if (main_channel == 0)
    {
        // No actual data requested, just sector type check or subchannel
        sector_length = 0;
    }
    else if (trackinfo.track_mode == CUETrack_AUDIO)
    {
        // Transfer whole 2352 byte audio sectors from file to host
        sector_length = 2352;
    }
    else if (trackinfo.track_mode == CUETrack_MODE1_2048 && main_channel == 0x10)
    {
        // Transfer whole 2048 byte data sectors from file to host
        sector_length = 2048;
    }
    else if (trackinfo.track_mode == CUETrack_MODE1_2048 && (main_channel & 0xB8) == 0xB8)
    {
        // Transfer 2048 bytes of data from file and fake the headers
        sector_length = 2048;
        add_fake_headers = true;
        debuglog("------ Host requested ECC data but image file lacks it, replacing with zeros");
    }
    else if (trackinfo.track_mode == CUETrack_MODE1_2352 && main_channel == 0x10)
    {
        // Transfer the 2048 byte payload of data sector to host.
        sector_length = 2048;
        skip_begin = 16;
    }
    else if (trackinfo.track_mode == CUETrack_MODE1_2352 && (main_channel & 0xB8) == 0xB8)
    {
        // Transfer whole 2352 byte data sector with ECC to host
        sector_length = 2352;
    }
    else
    {
        debuglog("---- Unsupported channel request for track type ", (int)trackinfo.track_mode);
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = 0x6400; // ILLEGAL MODE FOR THIS TRACK
        scsiDev.phase = STATUS;
        return;
    }

    if (data_only && sector_length != 2048)
    {
        debuglog("------ Host tried to read non-data sector with standard READ command");
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = 0x6400; // ILLEGAL MODE FOR THIS TRACK
        scsiDev.phase = STATUS;
        return;
    }

    bool field_q_subchannel = false;
    if (sub_channel == 2)
    {
        // Include position information in Q subchannel
        field_q_subchannel = true;
    }
    else if (sub_channel != 0)
    {
        debuglog("---- Unsupported subchannel request");
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
        scsiDev.phase = STATUS;
        return;
    }

    scsiDev.phase = DATA_IN;
    scsiDev.dataLen = 0;
    scsiDev.dataPtr = 0;
    scsiEnterPhase(DATA_IN);

    // Use two buffers alternately for formatting sector data
    uint32_t result_length = sector_length + (field_q_subchannel ? 16 : 0) + (add_fake_headers ? 304 : 0);
    uint8_t *buf0 = scsiDev.data;
    uint8_t *buf1 = scsiDev.data + result_length;

    // Format the sectors for transfer
    for (uint32_t idx = 0; idx < length; idx++)
    {
        platform_poll();
        diskEjectButtonUpdate(false);

        img.file.seek(offset + idx * trackinfo.sector_length + skip_begin);

        // Verify that previous write using this buffer has finished
        uint8_t *buf = ((idx & 1) ? buf1 : buf0);
        uint8_t *bufstart = buf;
        uint32_t start = millis();
        while (!scsiIsWriteFinished(buf + result_length - 1) && !scsiDev.resetFlag)
        {
            if ((uint32_t)(millis() - start) > 5000)
            {
                log("doReadCD() timeout waiting for previous to finish");
                scsiDev.resetFlag = 1;
            }
            platform_poll();
            diskEjectButtonUpdate(false);
        }
        if (scsiDev.resetFlag) break;

        if (add_fake_headers)
        {
            // 12-byte data sector sync pattern
            *buf++ = 0x00;
            for (int i = 0; i < 10; i++)
            {
                *buf++ = 0xFF;
            }
            *buf++ = 0x00;

            // 4-byte data sector header
            LBA2MSFBCD(lba + idx, buf, false);
            buf += 3;
            *buf++ = 0x01; // Mode 1
        }

        if (sector_length > 0)
        {
            // User data
            img.file.read(buf, sector_length);
            buf += sector_length;
        }

        if (add_fake_headers)
        {
            // 288 bytes of ECC
            memset(buf, 0, 288);
            buf += 288;
        }

        if (field_q_subchannel)
        {
            // Formatted Q subchannel data
            // Refer to table 354 in T10/1545-D MMC-4 Revision 5a
            // and ECMA-130 22.3.3
            *buf++ = (trackinfo.track_mode == CUETrack_AUDIO ? 0x10 : 0x14); // Control & ADR
            *buf++ = trackinfo.track_number;
            *buf++ = (lba + idx >= trackinfo.data_start) ? 1 : 0; // Index number (0 = pregap)
            int32_t rel = (int32_t)(lba + idx) - (int32_t)trackinfo.data_start;
            LBA2MSF(rel, buf, true); buf += 3;
            *buf++ = 0;
            LBA2MSF(lba + idx, buf, false); buf += 3;
            *buf++ = 0; *buf++ = 0; // CRC (optional)
            *buf++ = 0; *buf++ = 0; *buf++ = 0; // (pad)
            *buf++ = 0; // No P subchannel
        }

        assert(buf == bufstart + result_length);
        scsiStartWrite(bufstart, result_length);
    }

    scsiFinishWrite();

    scsiDev.status = 0;
    scsiDev.phase = STATUS;
}

static void doReadSubchannel(bool time, bool subq, uint8_t parameter, uint8_t track_number, uint16_t allocation_length)
{
    uint8_t *buf = scsiDev.data;

    if (parameter == 0x01)
    {
        uint8_t audiostatus;
        uint32_t lba;
        cdromGetAudioPlaybackStatus(&audiostatus, &lba, false);
        debuglog("------ Get audio playback position: status ", (int)audiostatus, " lba ", (int)lba);

        // Fetch current track info
        image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
        CUEParser parser;
        CUETrackInfo trackinfo = {};
        loadCueSheet(img, parser);
        getTrackFromLBA(parser, lba, &trackinfo);

        // Request sub channel data at current playback position
        *buf++ = 0; // Reserved
        *buf++ = audiostatus;

        int len;
        if (subq)
        {
            len = 12;
            *buf++ = 0;  // Subchannel data length (MSB)
            *buf++ = len; // Subchannel data length (LSB)
            *buf++ = 0x01; // Subchannel data format
            *buf++ = (trackinfo.track_mode == CUETrack_AUDIO ? 0x10 : 0x14);
            *buf++ = trackinfo.track_number;
            *buf++ = (lba >= trackinfo.data_start) ? 1 : 0; // Index number (0 = pregap)
            if (time)
            {
                *buf++ = 0;
                LBA2MSF(lba, buf, false);
                debuglog("------ ABS M ", *buf, " S ", *(buf+1), " F ", *(buf+2));
                buf += 3;
            }
            else
            {
                *buf++ = (lba >> 24) & 0xFF; // Absolute block address
                *buf++ = (lba >> 16) & 0xFF;
                *buf++ = (lba >>  8) & 0xFF;
                *buf++ = (lba >>  0) & 0xFF;
            }

            int32_t relpos = (int32_t)lba - (int32_t)trackinfo.data_start;
            if (time)
            {
                *buf++ = 0;
                LBA2MSF(relpos, buf, true);
                debuglog("------ REL M ", *buf, " S ", *(buf+1), " F ", *(buf+2));
                buf += 3;
            }
            else
            {
                uint32_t urelpos = relpos;
                *buf++ = (urelpos >> 24) & 0xFF; // Track relative position (may be negative)
                *buf++ = (urelpos >> 16) & 0xFF;
                *buf++ = (urelpos >>  8) & 0xFF;
                *buf++ = (urelpos >>  0) & 0xFF;
            }
        }
        else
        {
            len = 0;
            *buf++ = 0;
            *buf++ = 0;
        }
        len += 4;

        if (len > allocation_length) len = allocation_length;
        scsiDev.dataLen = len;
        scsiDev.phase = DATA_IN;
    }
    else
    {
        debuglog("---- Unsupported subchannel request");
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
        scsiDev.phase = STATUS;
        return;
    }

}

static bool doReadCapacity(uint32_t lba, uint8_t pmi)
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;

    CUEParser parser;
    if (!loadCueSheet(img, parser))
    {
        // basic image, let the disk handler resolve
        return false;
    }

    // find the last track on the disk
    CUETrackInfo lasttrack = {0};
    const CUETrackInfo *trackinfo;
    while ((trackinfo = parser.next_track()) != NULL)
    {
        lasttrack = *trackinfo;
    }

    uint32_t capacity = 0;
    if (lasttrack.track_number != 0)
    {
        capacity = getLeadOutLBA(&lasttrack);
        capacity--; // shift to last addressable LBA
        if (pmi && lba && lba > capacity)
        {
            // MMC technically specifies that PMI should be zero, but SCSI-2 allows this
            // potentially consider treating either out-of-bounds or PMI set as an error
            // for now just ignore this
        }
        debuglog("----- Reporting capacity as ", capacity);
    }
    else
    {
        log("WARNING: unable to find capacity, no cue file found for ID ", img.scsiId);
    }

    scsiDev.data[0] = capacity >> 24;
    scsiDev.data[1] = capacity >> 16;
    scsiDev.data[2] = capacity >> 8;
    scsiDev.data[3] = capacity;
    scsiDev.data[4] = 0;
    scsiDev.data[5] = 0;
    scsiDev.data[6] = 0x08; // rest of code assumes 2048 here
    scsiDev.data[7] = 0x00;
    scsiDev.dataLen = 8;
    scsiDev.phase = DATA_IN;
    return true;
}

/**************************************/
/* CD-ROM command dispatching         */
/**************************************/

// Handle direct-access scsi device commands
extern "C" int scsiCDRomCommand()
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    int commandHandled = 1;

    uint8_t command = scsiDev.cdb[0];
    if (command == 0x1B)
    {
#if ENABLE_AUDIO_OUTPUT
        // terminate audio playback if active on this target (MMC-1 Annex C)
        audio_stop(img.scsiId & S2S_CFG_TARGET_ID_BITS);
#endif
        if ((scsiDev.cdb[4] & 2))
        {
            // CD-ROM load & eject
            int start = scsiDev.cdb[4] & 1;
            if (start)
            {
                cdromCloseTray(img);
            }
            else
            {
                // Eject and switch image
                cdromPerformEject(img);
            }
        }
    }
    else if (command == 0x25)
    {
        // READ CAPACITY
        uint8_t reladdr = scsiDev.cdb[1] & 1;
        uint32_t lba = (((uint32_t) scsiDev.cdb[2]) << 24) +
            (((uint32_t) scsiDev.cdb[3]) << 16) +
            (((uint32_t) scsiDev.cdb[4]) << 8) +
            scsiDev.cdb[5];
        uint8_t pmi = scsiDev.cdb[8] & 1;

        // allow PMI as long as LBA is specified, this is permitted in SCSI-2
        // we don't link commands, do not allow RELADDR
        if ((!pmi && lba != 0) || reladdr)
        {
            scsiDev.status = CHECK_CONDITION;
            scsiDev.target->sense.code = ILLEGAL_REQUEST;
            scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
            scsiDev.phase = STATUS;
        }
        else
        {
            if (!doReadCapacity(lba, pmi))
            {
                // allow disk handler to resolve this one
                commandHandled = 0;
            }
        }
    }
    else if (command == 0x43)
    {
        // CD-ROM Read TOC
        bool MSF = (scsiDev.cdb[1] & 0x02);
        uint8_t track = scsiDev.cdb[6];
        uint16_t allocationLength =
            (((uint32_t) scsiDev.cdb[7]) << 8) +
            scsiDev.cdb[8];

        // The "format" field is reserved for SCSI-2
        uint8_t format = scsiDev.cdb[2] & 0x0F;

        // Matshita SCSI-2 drives appear to use the high 2 bits of the CDB
        // control byte to switch on session info (0x40) and full toc (0x80)
        // responses that are very similar to the standard formats described
        // in MMC-1. These vendor flags must have been pretty common because
        // even a modern SATA drive (ASUS DRW-24B1ST j) responds to them
        // (though it always replies in hex rather than bcd)
        //
        // The session information page is identical to MMC. The full TOC page
        // is identical _except_ it returns addresses in bcd rather than hex.
        bool useBCD = false;
        if (format == 0 && scsiDev.cdb[9] == 0x80)
        {
            format = 2;
            useBCD = true;
        }
        else if (format == 0 && scsiDev.cdb[9] == 0x40)
        {
            format = 1;
        }

        switch (format)
        {
            case 0: doReadTOC(MSF, track, allocationLength); break; // SCSI-2
            case 1: doReadSessionInfo(MSF, allocationLength); break; // MMC2
            case 2: doReadFullTOC(track, allocationLength, useBCD); break; // MMC2
            default:
            {
                scsiDev.status = CHECK_CONDITION;
                scsiDev.target->sense.code = ILLEGAL_REQUEST;
                scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
                scsiDev.phase = STATUS;
            }
        }
    }
    else if (command == 0x44)
    {
        // CD-ROM Read Header
        bool MSF = (scsiDev.cdb[1] & 0x02);
        uint32_t lba = 0; // IGNORED for now
        uint16_t allocationLength =
            (((uint32_t) scsiDev.cdb[7]) << 8) +
            scsiDev.cdb[8];
        doReadHeader(MSF, lba, allocationLength);
    }
    else if (command == 0x46)
    {
        // GET CONFIGURATION
        uint8_t rt = (scsiDev.cdb[1] & 0x03);
        uint16_t startFeature =
            (((uint16_t) scsiDev.cdb[2]) << 8) +
            scsiDev.cdb[3];
        uint16_t allocationLength =
            (((uint32_t) scsiDev.cdb[7]) << 8) +
            scsiDev.cdb[8];
        doGetConfiguration(rt, startFeature, allocationLength);
    }
    else if (command == 0x51)
    {
        uint16_t allocationLength =
            (((uint32_t) scsiDev.cdb[7]) << 8) +
            scsiDev.cdb[8];
        doReadDiscInformation(allocationLength);
    }
    else if (command == 0x52)
    {
        // READ TRACK INFORMATION
        bool track = (scsiDev.cdb[1] & 0x01);
        uint32_t lba = (((uint32_t) scsiDev.cdb[2]) << 24) +
            (((uint32_t) scsiDev.cdb[3]) << 16) +
            (((uint32_t) scsiDev.cdb[4]) << 8) +
            scsiDev.cdb[5];
        uint16_t allocationLength =
            (((uint32_t) scsiDev.cdb[7]) << 8) +
            scsiDev.cdb[8];
        doReadTrackInformation(track, lba, allocationLength);
    }
    else if (command == 0x4A)
    {
        // Get event status notifications (media change notifications)
        bool immed = scsiDev.cdb[1] & 1;
        doGetEventStatusNotification(immed);
    }
    else if (command == 0x45)
    {
        // PLAY AUDIO (10)
        uint32_t lba =
            (((uint32_t) scsiDev.cdb[2]) << 24) +
            (((uint32_t) scsiDev.cdb[3]) << 16) +
            (((uint32_t) scsiDev.cdb[4]) << 8) +
            scsiDev.cdb[5];
        uint32_t blocks =
            (((uint32_t) scsiDev.cdb[7]) << 8) +
            scsiDev.cdb[8];

        doPlayAudio(lba, blocks);
    }
    else if (command == 0xA5)
    {
        // PLAY AUDIO (12)
        uint32_t lba =
            (((uint32_t) scsiDev.cdb[2]) << 24) +
            (((uint32_t) scsiDev.cdb[3]) << 16) +
            (((uint32_t) scsiDev.cdb[4]) << 8) +
            scsiDev.cdb[5];
        uint32_t blocks =
            (((uint32_t) scsiDev.cdb[6]) << 24) +
            (((uint32_t) scsiDev.cdb[7]) << 16) +
            (((uint32_t) scsiDev.cdb[8]) << 8) +
            scsiDev.cdb[9];

        doPlayAudio(lba, blocks);
    }
    else if (command == 0x47)
    {
        // PLAY AUDIO (MSF)
        uint32_t start = MSF2LBA(scsiDev.cdb[3], scsiDev.cdb[4], scsiDev.cdb[5], false);
        uint32_t end   = MSF2LBA(scsiDev.cdb[6], scsiDev.cdb[7], scsiDev.cdb[8], false);

        uint32_t lba = start;
        if (scsiDev.cdb[3] == 0xFF
                && scsiDev.cdb[4] == 0xFF
                && scsiDev.cdb[5] == 0xFF)
        {
            // request to start playback from 'current position'
            image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
            lba = img.file.position() / 2352;
        }

        uint32_t length = end - lba;
        doPlayAudio(lba, length);
    }
    else if (command == 0x4B)
    {
        // PAUSE/RESUME AUDIO
        doPauseResumeAudio(scsiDev.cdb[8] & 1);
    }
    else if (command == 0xBD)
    {
        // Mechanism status
        uint16_t allocationLength = (((uint32_t) scsiDev.cdb[8]) << 8) + scsiDev.cdb[9];
        doMechanismStatus(allocationLength);
    }
    else if (command == 0xBB)
    {
        // Set CD speed (just ignored)
        scsiDev.status = 0;
        scsiDev.phase = STATUS;
    }
    else if (command == 0xBE)
    {
        // ReadCD (in low level format)
        uint8_t sector_type = (scsiDev.cdb[1] >> 2) & 7;
        uint32_t lba =
            (((uint32_t) scsiDev.cdb[2]) << 24) +
            (((uint32_t) scsiDev.cdb[3]) << 16) +
            (((uint32_t) scsiDev.cdb[4]) << 8) +
            scsiDev.cdb[5];
        uint32_t blocks =
            (((uint32_t) scsiDev.cdb[6]) << 16) +
            (((uint32_t) scsiDev.cdb[7]) << 8) +
            (((uint32_t) scsiDev.cdb[8]));
        uint8_t main_channel = scsiDev.cdb[9];
        uint8_t sub_channel = scsiDev.cdb[10];

        doReadCD(lba, blocks, sector_type, main_channel, sub_channel, false);
    }
    else if (command == 0xB9)
    {
        // ReadCD MSF
        uint8_t sector_type = (scsiDev.cdb[1] >> 2) & 7;
        uint32_t start = MSF2LBA(scsiDev.cdb[3], scsiDev.cdb[4], scsiDev.cdb[5], false);
        uint32_t end   = MSF2LBA(scsiDev.cdb[6], scsiDev.cdb[7], scsiDev.cdb[8], false);
        uint8_t main_channel = scsiDev.cdb[9];
        uint8_t sub_channel = scsiDev.cdb[10];

        doReadCD(start, end - start, sector_type, main_channel, sub_channel, false);
    }
    else if (command == 0x42)
    {
        // Read subchannel data
        bool time = (scsiDev.cdb[1] & 0x02);
        bool subq = (scsiDev.cdb[2] & 0x40);
        uint8_t parameter = scsiDev.cdb[3];
        uint8_t track_number = scsiDev.cdb[6];
        uint16_t allocationLength = (((uint32_t) scsiDev.cdb[7]) << 8) + scsiDev.cdb[8];

        doReadSubchannel(time, subq, parameter, track_number, allocationLength);
    }
    else if (command == 0x08)
    {
        // READ(6) for CDs (may need sector translation for cue file handling)
        uint32_t lba =
            (((uint32_t) scsiDev.cdb[1] & 0x1F) << 16) +
            (((uint32_t) scsiDev.cdb[2]) << 8) +
            scsiDev.cdb[3];
        uint32_t blocks = scsiDev.cdb[4];
        if (blocks == 0) blocks = 256;

        doReadCD(lba, blocks, 0, 0x10, 0, true);
    }
    else if (command == 0x28)
    {
        // READ(10) for CDs (may need sector translation for cue file handling)
        uint32_t lba =
            (((uint32_t) scsiDev.cdb[2]) << 24) +
            (((uint32_t) scsiDev.cdb[3]) << 16) +
            (((uint32_t) scsiDev.cdb[4]) << 8) +
            scsiDev.cdb[5];
        uint32_t blocks =
            (((uint32_t) scsiDev.cdb[7]) << 8) +
            scsiDev.cdb[8];

        doReadCD(lba, blocks, 0, 0x10, 0, true);
    }
    else if (command == 0xA8)
    {
        // READ(12) for CDs (may need sector translation for cue file handling)
        uint32_t lba =
            (((uint32_t) scsiDev.cdb[2]) << 24) +
            (((uint32_t) scsiDev.cdb[3]) << 16) +
            (((uint32_t) scsiDev.cdb[4]) << 8) +
            scsiDev.cdb[5];
        uint32_t blocks =
            (((uint32_t) scsiDev.cdb[6]) << 24) +
            (((uint32_t) scsiDev.cdb[7]) << 16) +
            (((uint32_t) scsiDev.cdb[8]) << 8) +
            scsiDev.cdb[9];

        doReadCD(lba, blocks, 0, 0x10, 0, true);
    }
    else if (command == 0x4E)
    {
        // STOP PLAY/SCAN
        doStopAudio();
        scsiDev.status = 0;
        scsiDev.phase = STATUS;
    }
    else if (command == 0x01)
    {
        // REZERO UNIT
        // AppleCD Audio Player uses this as a nonstandard
        // "stop audio playback" command
        doStopAudio();
        scsiDev.status = 0;
        scsiDev.phase = STATUS;
    }
    else if (command == 0x0B || command == 0x2B)
    {
        // SEEK
        // implement Annex C termination requirement and pass to disk handler
        doStopAudio();
        // this may need more specific handling, the Win9x player appears to
        // expect a pickup move to the given LBA
        commandHandled = 0;
    }
    else if (scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_APPLE
            && command == 0xCD)
    {
        // vendor-specific command issued by the AppleCD Audio Player in
        // response to fast-forward or rewind commands. Might be seek,
        // might be reposition. Exact MSF value below is unknown.
        //
        // Byte 0: 0xCD
        // Byte 1: 0x10 for rewind, 0x00 for fast-forward
        // Byte 2: 0x00
        // Byte 3: 'M' in hex
        // Byte 4: 'S' in hex
        // Byte 5: 'F' in hex
        commandHandled = 0;
    }
    else
    {
        commandHandled = 0;
    }

    return commandHandled;
}
