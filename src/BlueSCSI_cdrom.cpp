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
/*   CDROM positioning information        */
/******************************************/

typedef struct {
    uint32_t last_lba = 0;
} mechanism_status_t;
static mechanism_status_t mechanism_status[8];

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
    0x44, //  1: toc length, LSB
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
    0x79, // 34: LEADOUT position BCD
    0x59, // 35: leadout PSEC BCD
    0x74, // 36: leadout PFRAME BCD
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
    0x00, // 46: PSEC
    0x00, // 47: PFRAME
    // b0
    0x01, // 48: session number
    0x54, // 49: ADR/Control
    0x00, // 50: TNO
    0xB1, // 51: POINT
    0x79, // 52: Min BCD
    0x59, // 53: Sec BCD
    0x74, // 54: Frame BCD
    0x00, // 55: Zero
    0x79, // 56: PMIN BCD
    0x59, // 57: PSEC BCD
    0x74, // 58: PFRAME BCD
    // c0
    0x01, // 59: session number
    0x54, // 60: ADR/Control
    0x00, // 61: TNO
    0xC0, // 62: POINT
    0x00, // 63: Min
    0x00, // 64: Sec
    0x00, // 65: Frame
    0x00, // 66: Zero
    0x00, // 67: PMIN
    0x00, // 68: PSEC
    0x00  // 69: PFRAME
};

static uint8_t SimpleHeader[] =
{
    0x01, // 2048byte user data, L-EC in 288 byte aux field.
    0x00, // reserved
    0x00, // reserved
    0x00, // reserved
    0x00,0x00,0x00,0x00 // Track start sector (LBA or MSF)
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

// Convert logical block address to CD-ROM time in formatted TOC format
static void LBA2MSF(uint32_t LBA, uint8_t* MSF)
{
    MSF[0] = 0; // reserved.
    MSF[3] = LBA % 75; // Frames
    uint32_t rem = LBA / 75;

    MSF[2] = rem % 60; // Seconds
    MSF[1] = rem / 60; // Minutes
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
            LBA2MSF(capacity, scsiDev.data + 8);
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

        uint32_t capacity = getScsiCapacity(
            scsiDev.target->cfg->sdSectorStart,
            scsiDev.target->liveCfg.bytesPerSector,
            scsiDev.target->cfg->scsiSectors);

        // Replace start of leadout track
        if (MSF)
        {
            LBA2MSF(capacity, scsiDev.data + 0x10);
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

static void doReadSessionInfoSimple(uint8_t session, uint16_t allocationLength)
{
    uint32_t len = sizeof(SessionTOC);
    memcpy(scsiDev.data, SessionTOC, len);

    if (len > allocationLength)
    {
        len = allocationLength;
    }
    scsiDev.dataLen = len;
    scsiDev.phase = DATA_IN;
}

static inline uint8_t
fromBCD(uint8_t val)
{
    return ((val >> 4) * 10) + (val & 0xF);
}

static void doReadFullTOCSimple(int convertBCD, uint8_t session, uint16_t allocationLength)
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

        if (convertBCD)
        {
            int descriptor = 4;
            while (descriptor < len)
            {
                int i;
                for (i = 0; i < 7; ++i)
                {
                    scsiDev.data[descriptor + i] =
                        fromBCD(scsiDev.data[descriptor + 4 + i]);
                }
                descriptor += 11;
            }

        }

        if (len > allocationLength)
        {
            len = allocationLength;
        }
        scsiDev.dataLen = len;
        scsiDev.phase = DATA_IN;
    }
}

void doReadHeaderSimple(bool MSF, uint32_t lba, uint16_t allocationLength)
{
    uint32_t len = sizeof(SimpleHeader);
    memcpy(scsiDev.data, SimpleHeader, len);
    if (len > allocationLength)
    {
        len = allocationLength;
    }
    scsiDev.dataLen = len;
    scsiDev.phase = DATA_IN;
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
        LBA2MSF(track->data_start, &dest[4]);
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
    int lasttrack = -1;
    const CUETrackInfo *trackinfo;
    while ((trackinfo = parser.next_track()) != NULL)
    {
        if (firsttrack < 0) firsttrack = trackinfo->track_number;
        lasttrack = trackinfo->track_number;

        if (track == 0 || track == trackinfo->track_number)
        {
            formatTrackInfo(trackinfo, &trackdata[8 * trackcount], MSF);
            trackcount += 1;
        }
    }

    // Format lead-out track info
    if (track == 0 || track == 0xAA)
    {
        CUETrackInfo leadout = {};
        leadout.track_number = 0xAA;
        leadout.track_mode = CUETrack_MODE1_2048;
        leadout.data_start = img.scsiSectors;
        formatTrackInfo(&leadout, &trackdata[8 * trackcount], MSF);
        trackcount += 1;
    }

    // Format response header
    uint16_t toc_length = 2 + trackcount * 8;
    scsiDev.data[0] = toc_length >> 8;
    scsiDev.data[1] = toc_length & 0xFF;
    scsiDev.data[2] = firsttrack;
    scsiDev.data[3] = lasttrack;

    if (trackcount == 0)
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

static void doReadSessionInfo(uint8_t session, uint16_t allocationLength)
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    CUEParser parser;
    if (!loadCueSheet(img, parser))
    {
        // No CUE sheet, use hardcoded data
        return doReadSessionInfoSimple(session, allocationLength);
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

// Convert logical block address to CD-ROM time in the raw TOC format
static void LBA2MSFRaw(uint32_t LBA, uint8_t* MSF)
{
    MSF[2] = LBA % 75; // Frames
    uint32_t rem = LBA / 75;

    MSF[1] = rem % 60; // Seconds
    MSF[0] = rem / 60; // Minutes
}

// Convert logical block address to CD-ROM time in binary coded decimal format
static void LBA2MSFBCD(uint32_t LBA, uint8_t* MSF)
{
    uint8_t fra = LBA % 75;
    uint32_t rem = LBA / 75;
    uint8_t sec = rem % 60;
    uint8_t min = rem / 60;

    MSF[0] = ((min / 10) << 4) | (min % 10);
    MSF[1] = ((sec / 10) << 4) | (sec % 10);
    MSF[2] = ((fra / 10) << 4) | (fra % 10);
}

// Format track info read from cue sheet into the format used by ReadFullTOC command.
// Refer to T10/1545-D MMC-4 Revision 5a, "Response Format 0010b: Raw TOC"
static void formatRawTrackInfo(const CUETrackInfo *track, uint8_t *dest)
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

    if (track->pregap_start > 0)
    {
        LBA2MSFRaw(track->pregap_start, &dest[4]);
    }
    else
    {
        LBA2MSFRaw(track->data_start, &dest[4]);
    }

    dest[7] = 0; // HOUR

    LBA2MSFBCD(track->data_start, &dest[8]);
}

static void doReadFullTOC(int convertBCD, uint8_t session, uint16_t allocationLength)
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    CUEParser parser;
    if (!loadCueSheet(img, parser))
    {
        // No CUE sheet, use hardcoded data
        return doReadFullTOCSimple(convertBCD, session, allocationLength);
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
    int lasttrack = -1;
    const CUETrackInfo *trackinfo;
    while ((trackinfo = parser.next_track()) != NULL)
    {
        if (firsttrack < 0) firsttrack = trackinfo->track_number;
        lasttrack = trackinfo->track_number;

        formatRawTrackInfo(trackinfo, &scsiDev.data[len]);
        trackcount += 1;
        len += 11;
    }

    // First and last track numbers
    scsiDev.data[12] = firsttrack;
    scsiDev.data[23] = lasttrack;

    // Leadout track position
    LBA2MSFBCD(img.scsiSectors, &scsiDev.data[34]);

    // Append recordable disc records b0 and c0 indicating non-recordable disc
    memcpy(scsiDev.data + len, &FullTOC[48], 22);
    len += 22;

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
void doReadHeader(bool MSF, uint32_t lba, uint16_t allocationLength)
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;

#if ENABLE_AUDIO_OUTPUT
    // terminate audio playback if active on this target (Annex C)
    audio_stop(img.scsiId & 7);
#endif

    CUEParser parser;
    if (!loadCueSheet(img, parser))
    {
        // No CUE sheet, use hardcoded data
        return doReadHeaderSimple(MSF, lba, allocationLength);
    }

    // Take the hardcoded header as base
    uint32_t len = sizeof(SimpleHeader);
    memcpy(scsiDev.data, SimpleHeader, len);

    // Search the track with the requested LBA
    CUETrackInfo trackinfo = {};
    getTrackFromLBA(parser, lba, &trackinfo);

    // Track mode (audio / data)
    if (trackinfo.track_mode == CUETrack_AUDIO)
    {
        scsiDev.data[0] = 0;
    }

    // Track start
    if (MSF)
    {
        LBA2MSF(trackinfo.data_start, &scsiDev.data[4]);
    }
    else
    {
        scsiDev.data[4] = (trackinfo.data_start >> 24) & 0xFF;
        scsiDev.data[5] = (trackinfo.data_start >> 16) & 0xFF;
        scsiDev.data[6] = (trackinfo.data_start >>  8) & 0xFF;
        scsiDev.data[7] = (trackinfo.data_start >>  0) & 0xFF;
    }

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

// Reinsert any ejected CDROMs on reboot
void cdromReinsertFirstImage(image_config_t &img)
{
    if (img.image_index > 0)
    {
        // Multiple images for this drive, force restart from first one
        debuglog("---- Restarting from first CD-ROM image");
        img.image_index = 9;
        cdromSwitchNextImage(img);
    }
    else if (img.ejected)
    {
        // Reinsert the single image
        debuglog("---- Closing CD-ROM tray");
        img.ejected = false;
        img.cdrom_events = 2; // New media
    }
}

// Check if we have multiple CD-ROM images to cycle when drive is ejected.
bool cdromSwitchNextImage(image_config_t &img)
{
    // Check if we have a next image to load, so that drive is closed next time the host asks.
    img.image_index++;
    char filename[MAX_FILE_PATH];
    int target_idx = img.scsiId & 7;
    if (!scsiDiskGetImageNameFromConfig(img, filename, sizeof(filename)))
    {
        img.image_index = 0;
        scsiDiskGetImageNameFromConfig(img, filename, sizeof(filename));
    }

#ifdef ENABLE_AUDIO_OUTPUT
    // if in progress for this device, terminate audio playback immediately (Annex C)
    audio_stop(target_idx);
    // Reset position tracking for the new image
    audio_get_status_code(target_idx); // trash audio status code
    audio_clear_bytes_read(target_idx);
#endif
    mechanism_status[target_idx].last_lba = 0;

    if (filename[0] != '\0')
    {
        log("Switching to next CD-ROM image for ", target_idx, ": ", filename);
        img.file.close();
        bool status = scsiDiskOpenHDDImage(target_idx, filename, target_idx, 0, 2048);

        if (status)
        {
            img.ejected = false;
            img.cdrom_events = 2; // New media
            return true;
        }
    }

    return false;
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

        if (img.ejected)
        {
            // We are now reporting to host that the drive is open.
            // Simulate a "close" for next time the host polls.
            cdromSwitchNextImage(img);
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
    uint8_t target = img.scsiId & 7;

#ifdef ENABLE_AUDIO_OUTPUT
    if (status) {
        if (current_only) {
            *status = audio_is_playing(target) ? 1 : 0;
        } else {
            *status = (uint8_t) audio_get_status_code(target);
        }
    }
    if (current_lba) *current_lba = mechanism_status[target].last_lba
            + audio_get_bytes_read(target) / 2352;
#else
    if (status) *status = 0; // audio status code for 'unsupported/invalid' and not-playing indicator
    if (current_lba) *current_lba = mechanism_status[target].last_lba;
#endif
}

static void doPlayAudio(uint32_t lba, uint32_t length)
{
#ifdef ENABLE_AUDIO_OUTPUT
    debuglog("------ CD-ROM Play Audio request at ", lba, " for ", length, " sectors");
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    uint8_t target_id = img.scsiId & 7;

    // Per Annex C terminate playback immediately if already in progress on
    // the current target. Non-current targets may also get their audio
    // interrupted later due to hardware limitations
    audio_stop(img.scsiId & 7);

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
            lba = mechanism_status[target_id].last_lba + audio_get_bytes_read(target_id) / 2352;
        }

        // --- TODO --- determine proper track offset, software I tested with had a tendency
        // to ask for offsets that seem to hint at 2048 here, not the 2352 you'd assume.
        // Might be due to a mode page reporting something unexpected? Needs investigation.
        uint64_t offset = trackinfo.file_offset + 2048 * (lba - trackinfo.data_start);
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
        char filename[MAX_FILE_PATH];
        if (!img.file.name(filename, sizeof(filename)))
        {
            // No underlying file available?
            log("---- No filename for SCSI ID ", target_id);
            scsiDev.status = CHECK_CONDITION;
            scsiDev.target->sense.code = ILLEGAL_REQUEST;
            scsiDev.target->sense.asc = 0x0000;
            scsiDev.phase = STATUS;
            return;
        }
        if (!audio_play(target_id, filename, offset, offset + length * 2352, false))
        {
            // Underlying data/media error? Fake a disk scratch, which should
            // be a condition most CD-DA players are expecting
            scsiDev.status = CHECK_CONDITION;
            scsiDev.target->sense.code = MEDIUM_ERROR;
            scsiDev.target->sense.asc = 0x1106; // CIRC UNRECOVERED ERROR
            scsiDev.phase = STATUS;
            return;
        }
        mechanism_status[target_id].last_lba = lba;
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
    uint8_t target_id = img.scsiId & 7;

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
                     uint8_t main_channel, uint8_t sub_channel)
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;

#if ENABLE_AUDIO_OUTPUT
    // terminate audio playback if active on this target (Annex C)
    audio_stop(img.scsiId & 7);
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
            LBA2MSFBCD(lba + idx, buf);
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
            *buf++ = (trackinfo.track_mode == CUETrack_AUDIO ? 0x10 : 0x14); // Control & ADR
            *buf++ = trackinfo.track_number;
            *buf++ = (lba + idx >= trackinfo.data_start) ? 1 : 0; // Index number (0 = pregap)
            LBA2MSFRaw(lba + idx, buf); buf += 3;
            *buf++ = 0;
            LBA2MSFRaw(lba + idx, buf); buf += 3;
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

        int len = 12;
        *buf++ = 0;  // Subchannel data length (MSB)
        *buf++ = len; // Subchannel data length (LSB)
        *buf++ = 0x01; // Subchannel data format
        *buf++ = (trackinfo.track_mode == CUETrack_AUDIO ? 0x10 : 0x14);
        *buf++ = trackinfo.track_number;
        *buf++ = (lba >= trackinfo.data_start) ? 1 : 0; // Index number (0 = pregap)
        *buf++ = (lba >> 24) & 0xFF; // Absolute block address
        *buf++ = (lba >> 16) & 0xFF;
        *buf++ = (lba >>  8) & 0xFF;
        *buf++ = (lba >>  0) & 0xFF;

        uint32_t relpos = (uint32_t)((int32_t)lba - (int32_t)trackinfo.data_start);
        *buf++ = (relpos >> 24) & 0xFF; // Track relative position (may be negative)
        *buf++ = (relpos >> 16) & 0xFF;
        *buf++ = (relpos >>  8) & 0xFF;
        *buf++ = (relpos >>  0) & 0xFF;

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
        // terminate audio playback if active on this target (Annex C)
        audio_stop(img.scsiId & 7);
#endif
        if ((scsiDev.cdb[4] & 2))
        {
            // CD-ROM load & eject
            int start = scsiDev.cdb[4] & 1;
            if (start)
            {
                debuglog("------ CDROM close tray");
                img.ejected = false;
                img.cdrom_events = 2; // New media
            }
            else
            {
                debuglog("------ CDROM open tray");
                img.ejected = true;
                img.cdrom_events = 3; // Media removal
            }
        }
        else
        {
            // flow through to disk handler
            commandHandled = 0;
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

        // Reject MMC commands for now, otherwise the TOC data format
        // won't be understood.
        // The "format" field is reserved for SCSI-2
        uint8_t format = scsiDev.cdb[2] & 0x0F;
        switch (format)
        {
            case 0: doReadTOC(MSF, track, allocationLength); break; // SCSI-2
            case 1: doReadSessionInfo(MSF, allocationLength); break; // MMC2
            case 2: doReadFullTOC(0, track, allocationLength); break; // MMC2
            case 3: doReadFullTOC(1, track, allocationLength); break; // MMC2
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
    else if (command == 0x51)
    {
        uint16_t allocationLength =
            (((uint32_t) scsiDev.cdb[7]) << 8) +
            scsiDev.cdb[8];
        doReadDiscInformation(allocationLength);
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
        uint32_t start = (scsiDev.cdb[3] * 60 + scsiDev.cdb[4]) * 75 + scsiDev.cdb[5];
        uint32_t end   = (scsiDev.cdb[6] * 60 + scsiDev.cdb[7]) * 75 + scsiDev.cdb[8];

        doPlayAudio(start, end - start);
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

        doReadCD(lba, blocks, sector_type, main_channel, sub_channel);
    }
    else if (command == 0xB9)
    {
        // ReadCD MSF
        uint8_t sector_type = (scsiDev.cdb[1] >> 2) & 7;
        uint32_t start = (scsiDev.cdb[3] * 60 + scsiDev.cdb[4]) * 75 + scsiDev.cdb[5];
        uint32_t end   = (scsiDev.cdb[6] * 60 + scsiDev.cdb[7]) * 75 + scsiDev.cdb[8];
        uint8_t main_channel = scsiDev.cdb[9];
        uint8_t sub_channel = scsiDev.cdb[10];

        doReadCD(start, end - start, sector_type, main_channel, sub_channel);
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

        doReadCD(lba, blocks, 0, 0x10, 0);
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

        doReadCD(lba, blocks, 0, 0x10, 0);
    }
    else
    {
        commandHandled = 0;
    }

    return commandHandled;
}
