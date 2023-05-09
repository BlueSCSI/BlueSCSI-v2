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
#include "ZuluSCSI_cdrom.h"
#include "ZuluSCSI_log.h"
#include "ZuluSCSI_config.h"
#include <CUEParser.h>

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

static void LBA2MSF(uint32_t LBA, uint8_t* MSF)
{
	MSF[0] = 0; // reserved.
	MSF[3] = LBA % 75; // M
	uint32_t rem = LBA / 75;

	MSF[2] = rem % 60; // S
	MSF[1] = rem / 60;

}

static void doReadTOCSimple(int MSF, uint8_t track, uint16_t allocationLength)
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

static uint8_t SimpleHeader[] =
{
	0x01, // 2048byte user data, L-EC in 288 byte aux field.
	0x00, // reserved
	0x00, // reserved
	0x00, // reserved
	0x00,0x00,0x00,0x00 // Track start sector (LBA or MSF)
};

void doReadHeaderSimple(int MSF, uint32_t lba, uint16_t allocationLength)
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

/*********************************/
/* TOC generation from cue sheet */
/*********************************/

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
    char *cuebuf = (char*)&scsiDev.data[sizeof(scsiDev.data) / 2];
    img.cuesheetfile.seek(0);
    int len = img.cuesheetfile.read(cuebuf, sizeof(cuebuf));

    if (len <= 0)
    {
        return false;
    }

    cuebuf[len] = '\0';
    parser = CUEParser(cuebuf);
    return true;
}

static void doReadTOC(int MSF, uint8_t track, uint16_t allocationLength)
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

static void LBA2MSFRaw(uint32_t LBA, uint8_t* MSF)
{
    MSF[2] = LBA % 75; // M
	uint32_t rem = LBA / 75;

	MSF[1] = rem % 60; // S
	MSF[0] = rem / 60;
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

    LBA2MSFRaw(track->data_start, &dest[8]);
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
    LBA2MSFRaw(img.scsiSectors, &scsiDev.data[34]);

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
void doReadHeader(int MSF, uint32_t lba, uint16_t allocationLength)
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
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
    const CUETrackInfo *trackinfo;
    CUETrackMode trackmode = CUETrack_MODE1_2048;
    uint32_t trackstart = 0;
    while ((trackinfo = parser.next_track()) != NULL)
    {
        if (trackinfo->data_start < lba)
        {
            trackstart = trackinfo->data_start;
            trackmode = trackinfo->track_mode;
        }
    }

    // Track mode (audio / data)
    if (trackmode == CUETrack_AUDIO)
    {
        scsiDev.data[0] = 0;
    }

    // Track start
    if (MSF)
    {
        LBA2MSF(trackstart, &scsiDev.data[4]);
    }
    else
    {
        scsiDev.data[4] = (trackstart >> 24) & 0xFF;
        scsiDev.data[5] = (trackstart >> 16) & 0xFF;
        scsiDev.data[6] = (trackstart >>  8) & 0xFF;
        scsiDev.data[7] = (trackstart >>  0) & 0xFF;
    }

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
            trackinfo->track_mode != CUETrack_MODE1_2048)
        {
            logmsg("---- Warning: track ", trackinfo->track_number, " has unsupported mode ", (int)trackinfo->track_mode);
        }

        if (trackinfo->file_mode != CUEFile_BINARY)
        {
            logmsg("---- Unsupported CUE data file mode ", (int)trackinfo->file_mode);
        }
    }

    if (trackcount == 0)
    {
        logmsg("---- Opened cue sheet but no valid tracks found");
        return false;
    }

    logmsg("---- Cue sheet loaded with ", (int)trackcount, " tracks");
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
        dbgmsg("---- Restarting from first CD-ROM image");
        img.image_index = 9;
        cdromSwitchNextImage(img);
    }
    else if (img.ejected)
    {
        // Reinsert the single image
        dbgmsg("---- Closing CD-ROM tray");
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

    if (filename[0] != '\0')
    {
        logmsg("Switching to next CD-ROM image for ", target_idx, ": ", filename);
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
/* CD-ROM command dispatching         */
/**************************************/

// Handle direct-access scsi device commands
extern "C" int scsiCDRomCommand()
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
	int commandHandled = 1;

	uint8_t command = scsiDev.cdb[0];
    if (command == 0x1B && (scsiDev.cdb[4] & 2))
    {
        // CD-ROM load & eject
        int start = scsiDev.cdb[4] & 1;
        if (start)
        {
            dbgmsg("------ CDROM close tray");
            img.ejected = false;
            img.cdrom_events = 2; // New media
        }
        else
        {
            dbgmsg("------ CDROM open tray");
            img.ejected = true;
            img.cdrom_events = 3; // Media removal
        }
    }
	else if (command == 0x43)
	{
		// CD-ROM Read TOC
		int MSF = scsiDev.cdb[1] & 0x02 ? 1 : 0;
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
		int MSF = scsiDev.cdb[1] & 0x02 ? 1 : 0;
		uint32_t lba = 0; // IGNORED for now
		uint16_t allocationLength =
			(((uint32_t) scsiDev.cdb[7]) << 8) +
			scsiDev.cdb[8];
		doReadHeader(MSF, lba, allocationLength);
	}
    else if (command == 0x4A)
    {
        // Get event status notifications (media change notifications)
        bool immed = scsiDev.cdb[1] & 1;
        doGetEventStatusNotification(immed);
    }
	else
	{
		commandHandled = 0;
	}

	return commandHandled;
}
