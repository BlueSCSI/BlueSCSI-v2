/* Tape device emulation
 * Will be called by scsi.c from SCSI2SD.
 *
 * ZuluSCSI™ - Copyright (c) 2023 Rabbit Hole Computing™
 * Copyright (c) 2023 Kars de Jong
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

#include "ZuluSCSI_disk.h"
#include "ZuluSCSI_log.h"
#include "ZuluSCSI_config.h"

extern "C" {
#include <scsi.h>
}

extern "C" int scsiTapeCommand()
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    int commandHandled = 1;

    uint8_t command = scsiDev.cdb[0];
    if (command == 0x08)
    {
        // READ6
        bool fixed = scsiDev.cdb[1] & 1;
        bool supress_invalid_length = scsiDev.cdb[1] & 2;

        if (img.quirks == S2S_CFG_QUIRKS_OMTI)
        {
            fixed = true;
        }

        uint32_t length =
            (((uint32_t) scsiDev.cdb[2]) << 16) +
            (((uint32_t) scsiDev.cdb[3]) << 8) +
            scsiDev.cdb[4];

        // Host can request either multiple fixed-length blocks, or a single variable length one.
        // If host requests variable length block, we return one blocklen sized block.
        uint32_t blocklen = scsiDev.target->liveCfg.bytesPerSector;
        uint32_t blocks_to_read = length;
        if (!fixed)
        {
            blocks_to_read = 1;

            bool underlength = (length > blocklen);
            bool overlength = (length < blocklen);
            if (overlength || (underlength && !supress_invalid_length))
            {
                dbgmsg("Host requested variable block max ", (int)length, " bytes, blocksize is ", (int)blocklen);
                scsiDev.status = CHECK_CONDITION;
                scsiDev.target->sense.code = ILLEGAL_REQUEST;
                scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
                scsiDev.phase = STATUS;
                return 1;
            }
        }


        if (blocks_to_read > 0)
        {
            scsiDiskStartRead(img.tape_pos, blocks_to_read);
            img.tape_pos += blocks_to_read;
        }
    }
    else if (command == 0x01)
    {
        // REWIND
        // Set tape position back to 0.
        img.tape_pos = 0;
    }
    else if (command == 0x11)
    {
        // SPACE
        // Set the tape position forward to a specified offset.
        uint8_t code = scsiDev.cdb[1] & 7;
        uint32_t count =
            (((uint32_t) scsiDev.cdb[2]) << 24) +
            (((uint32_t) scsiDev.cdb[3]) << 16) +
            (((uint32_t) scsiDev.cdb[4]) << 8) +
            scsiDev.cdb[5];
        if (code == 0)
        {
            // Blocks.
            uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;
            uint32_t capacity = img.file.size() / bytesPerSector;

            if (count < capacity)
            {
                img.tape_pos = count;
            }
            else
            {
                scsiDev.status = CHECK_CONDITION;
                scsiDev.target->sense.code = BLANK_CHECK;
                scsiDev.target->sense.asc = 0; // END-OF-DATA DETECTED
                scsiDev.phase = STATUS;
            }
        }
        else if (code == 1)
        {
            // Filemarks.
            // For now just indicate end of data
            scsiDev.status = CHECK_CONDITION;
            scsiDev.target->sense.code = BLANK_CHECK;
            scsiDev.target->sense.asc = 0; // END-OF-DATA DETECTED
            scsiDev.phase = STATUS;
        }
        else if (code == 3)
        {
            // End-of-data.
            scsiDev.status = CHECK_CONDITION;
            scsiDev.target->sense.code = BLANK_CHECK;
            scsiDev.target->sense.asc = 0; // END-OF-DATA DETECTED
            scsiDev.phase = STATUS;
        }
    }
    else
    {
        commandHandled = 0;
    }

    return commandHandled;
}