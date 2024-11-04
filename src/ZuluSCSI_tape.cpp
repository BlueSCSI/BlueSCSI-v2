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
#include <ZuluSCSI_platform.h>

extern "C" {
#include <scsi.h>
}

static void doSeek(uint32_t lba)
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;
    uint32_t capacity = img.file.size() / bytesPerSector;

    dbgmsg("------ Locate tape to LBA ", (int)lba);

    if (lba >= capacity)
    {
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
        scsiDev.phase = STATUS;
    }
    else
    {
        delay(10);
        img.tape_pos = lba;

        scsiDev.status = GOOD;
        scsiDev.phase = STATUS;
    }
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
                dbgmsg("------ Host requested variable block max ", (int)length, " bytes, blocksize is ", (int)blocklen);
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
    else if (command == 0x0A)
    {
        // WRITE6
        bool fixed = scsiDev.cdb[1] & 1;

        if (img.quirks == S2S_CFG_QUIRKS_OMTI)
        {
            fixed = true;
        }

        uint32_t length =
            (((uint32_t) scsiDev.cdb[2]) << 16) +
            (((uint32_t) scsiDev.cdb[3]) << 8) +
            scsiDev.cdb[4];

        // Host can request either multiple fixed-length blocks, or a single variable length one.
        // Only single block length is supported currently.
        uint32_t blocklen = scsiDev.target->liveCfg.bytesPerSector;
        uint32_t blocks_to_write = length;
        if (!fixed)
        {
            blocks_to_write = 1;

            if (length != blocklen)
            {
                dbgmsg("------ Host requested variable block ", (int)length, " bytes, blocksize is ", (int)blocklen);
                scsiDev.status = CHECK_CONDITION;
                scsiDev.target->sense.code = ILLEGAL_REQUEST;
                scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
                scsiDev.phase = STATUS;
                return 1;
            }
        }

        if (blocks_to_write > 0)
        {
            scsiDiskStartWrite(img.tape_pos, blocks_to_write);
            img.tape_pos += blocks_to_write;
        }
    }
    else if (command == 0x13)
    {
        // VERIFY
        bool fixed = scsiDev.cdb[1] & 1;

        if (img.quirks == S2S_CFG_QUIRKS_OMTI)
        {
            fixed = true;
        }

        bool byte_compare = scsiDev.cdb[1] & 2;
        uint32_t length =
            (((uint32_t) scsiDev.cdb[2]) << 16) +
            (((uint32_t) scsiDev.cdb[3]) << 8) +
            scsiDev.cdb[4];

        if (!fixed)
        {
            length = 1;
        }

        if (byte_compare)
        {
            dbgmsg("------ Verify with byte compare is not implemented");
            scsiDev.status = CHECK_CONDITION;
            scsiDev.target->sense.code = ILLEGAL_REQUEST;
            scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
            scsiDev.phase = STATUS;
        }
        else
        {
            // Host requests ECC check, report that it passed.
            scsiDev.status = GOOD;
            scsiDev.phase = STATUS;
            img.tape_pos += length;
        }
    }
    else if (command == 0x19)
    {
        // Erase
        // Just a stub implementation, fake erase to end of tape
        img.tape_pos = img.scsiSectors;
    }
    else if (command == 0x01)
    {
        // REWIND
        // Set tape position back to 0.
        img.tape_pos = 0;
    }
    else if (command == 0x05)
    {
        // READ BLOCK LIMITS
        uint32_t blocklen = scsiDev.target->liveCfg.bytesPerSector;
        scsiDev.data[0] = 0; // Reserved
        scsiDev.data[1] = (blocklen >> 16) & 0xFF; // Maximum block length (MSB)
        scsiDev.data[2] = (blocklen >>  8) & 0xFF;
        scsiDev.data[3] = (blocklen >>  0) & 0xFF; // Maximum block length (LSB)
        scsiDev.data[4] = (blocklen >>  8) & 0xFF; // Minimum block length (MSB)
        scsiDev.data[5] = (blocklen >>  8) & 0xFF; // Minimum block length (MSB)
        scsiDev.dataLen = 6;
        scsiDev.phase = DATA_IN;
    }
    else if (command == 0x10)
    {
        // WRITE FILEMARKS
        dbgmsg("------ Filemarks storage not implemented, reporting ok");
        scsiDev.status = GOOD;
        scsiDev.phase = STATUS;
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
    else if (command == 0x2B)
    {
        // Seek/Locate 10
        uint32_t lba =
            (((uint32_t) scsiDev.cdb[3]) << 24) +
            (((uint32_t) scsiDev.cdb[4]) << 16) +
            (((uint32_t) scsiDev.cdb[5]) << 8) +
            scsiDev.cdb[6];

        doSeek(lba);
    }
    else if (command == 0x34)
    {
        // ReadPosition
        uint32_t lba = img.tape_pos;
        scsiDev.data[0] = 0x00;
        if (lba == 0) scsiDev.data[0] |= 0x80;
        if (lba >= img.scsiSectors) scsiDev.data[0] |= 0x40;
        scsiDev.data[1] = 0x00;
        scsiDev.data[2] = 0x00;
        scsiDev.data[3] = 0x00;
        scsiDev.data[4] = (lba >> 24) & 0xFF; // Next block on tape
        scsiDev.data[5] = (lba >> 16) & 0xFF;
        scsiDev.data[6] = (lba >>  8) & 0xFF;
        scsiDev.data[7] = (lba >>  0) & 0xFF;
        scsiDev.data[8] = (lba >> 24) & 0xFF; // Last block in buffer
        scsiDev.data[9] = (lba >> 16) & 0xFF;
        scsiDev.data[10] = (lba >>  8) & 0xFF;
        scsiDev.data[11] = (lba >>  0) & 0xFF;
        scsiDev.data[12] = 0x00;
        scsiDev.data[13] = 0x00;
        scsiDev.data[14] = 0x00;
        scsiDev.data[15] = 0x00;
        scsiDev.data[16] = 0x00;
        scsiDev.data[17] = 0x00;
        scsiDev.data[18] = 0x00;
        scsiDev.data[19] = 0x00;

        scsiDev.phase = DATA_IN;
        scsiDev.dataLen = 20;
    }
    else
    {
        commandHandled = 0;
    }

    return commandHandled;
}