/**
 * Copyright (C) 2025 Eric Helgeson
 *
 * This file is part of BlueSCSI
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

#include <SdFat.h>
#include <minIni.h>
#include "BlueSCSI_config.h"
#include "BlueSCSI_platform.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_settings.h"
#include "BlueSCSI_disk.h"
#include "BlueSCSI_partitions.h"

// GPT Structures
#define GPT_SIGNATURE 0x5452415020494645ULL // "EFI PART"
#define GPT_HEADER_LBA 1

struct GptHeader {
    uint64_t signature;
    uint32_t revision;
    uint32_t headerSize;
    uint32_t headerCrc32;
    uint32_t reserved;
    uint64_t myLba;
    uint64_t alternateLba;
    uint64_t firstUsableLba;
    uint64_t lastUsableLba;
    uint8_t diskGuid[16];
    uint64_t partitionEntryLba;
    uint32_t numberOfPartitionEntries;
    uint32_t sizeOfPartitionEntry;
    uint32_t partitionEntryArrayCrc32;
} __attribute__((packed));

struct GptPartitionEntry {
    uint8_t partitionTypeGuid[16];
    uint8_t uniquePartitionGuid[16];
    uint64_t firstLba;
    uint64_t lastLba;
    uint64_t attributeFlags;
    uint16_t partitionName[36];
} __attribute__((packed));

// MBR Structures
#define PARTITION_ENTRY_SIZE 16
#define MBR_PARTITION_OFFSET 446
#define MBR_NUM_PARTITIONS 4
#define PARTITION_TYPE_GPT 0xEE
#define MBR_HEADER_LBA 0

struct MbrPartitionEntry {
    uint8_t status; // 0x80 = bootable
    uint8_t chsFirst[3];
    uint8_t type; // Partition type
    uint8_t chsLast[3];
    uint32_t lbaFirst; // Start sector (little-endian)
    uint32_t sectors; // Number of sectors (little-endian)
} __attribute__((packed));

bool checkAndConfigureGPTPartitions() {
    logmsg("--- Detected GPT, scanning for partitions...");

    // Read GPT Header at LBA 1
    if (!SD.card()->readSector(GPT_HEADER_LBA, scsiDev.data)) {
        logmsg("Error reading GPT header!");
        return false;
    }

    const auto *gptHeader = reinterpret_cast<GptHeader *>(scsiDev.data);

    // Verify GPT signature
    if (gptHeader->signature != GPT_SIGNATURE) {
        logmsg("Invalid GPT signature.");
        return false;
    }

    logmsg("--- GPT Header valid. Found ", static_cast<int>(gptHeader->numberOfPartitionEntries),
           " partition entries starting at LBA ", static_cast<unsigned long>(gptHeader->partitionEntryLba));

    bool found = false;
    uint8_t scsiId = 0;
    const uint32_t partitionsToRead = gptHeader->numberOfPartitionEntries;
    uint64_t currentLba = gptHeader->partitionEntryLba;
    char raw_img_str[64];

    // Read and process partition entries
    for (uint32_t i = 0; i < partitionsToRead && scsiId < NUM_SCSIID;) {
        if (!SD.card()->readSector(currentLba, scsiDev.data)) {
            logmsg("Error reading GPT partition entry block at LBA ", static_cast<unsigned long>(currentLba));
            break; // Stop if we can't read a sector
        }

        const uint32_t entriesPerSector = 512 / sizeof(GptPartitionEntry);
        for (uint32_t j = 0; j < entriesPerSector && i < partitionsToRead && scsiId < NUM_SCSIID; j++, i++) {
            const auto *p = reinterpret_cast<GptPartitionEntry *>(scsiDev.data) + j;

            // Check for an empty partition type GUID (all zeros)
            bool isEmpty = true;
            for (const unsigned char k : p->partitionTypeGuid) {
                if (k != 0) {
                    isEmpty = false;
                    break;
                }
            }

            if (isEmpty) {
                continue; // Skip unused entries
            }

            if (scsiId == 0 && ini_getbool("SCSI", "SkipFirstPartition", false, CONFIGFILE)) {
                logmsg("- First partition not configured as requested: SkipFirstPartition=true.");
                scsiId++;
                continue;
            }

            if (p->firstLba > 0 && p->lastLba >= p->firstLba) {
                snprintf(raw_img_str, sizeof(raw_img_str), "RAW:%llu:%llu", p->firstLba, p->lastLba);

                // Safely copy and convert partition name for logging
                char partitionNameStr[37];
                for (int k = 0; k < 36; ++k) {
                    // Basic conversion from UTF-16LE to ASCII, assuming basic characters
                    partitionNameStr[k] = (char) p->partitionName[k];
                    if (p->partitionName[k] == 0) break; // Null terminator
                }
                partitionNameStr[36] = '\0'; // Ensure null termination

                logmsg("--- SCSI ID ", static_cast<int>(scsiId), ": ", raw_img_str, " Partition Name: ",
                       partitionNameStr);
                found = true;
                g_scsi_settings.initDevice(scsiId, S2S_CFG_FIXED);
                scsiDiskOpenHDDImage(scsiId, raw_img_str, 0, RAW_FALLBACK_BLOCKSIZE);
                scsiId++;
            }
        }
        currentLba++;
    }

    return found;
}

bool checkAndConfigureMBRPartitions() {
    if (!SD.card()->readSector(MBR_HEADER_LBA, scsiDev.data)) {
        logmsg("Error reading MBR sector 0!");
        return false;
    }
    bool found = false;
    char raw_img_str[64];
    for (uint8_t i = 0; i < MBR_NUM_PARTITIONS; i++) {
        const auto *p = reinterpret_cast<MbrPartitionEntry *>(
            scsiDev.data + MBR_PARTITION_OFFSET + i * PARTITION_ENTRY_SIZE);

        if (p->type == PARTITION_TYPE_GPT) {
            return checkAndConfigureGPTPartitions();
        }

        if (i == 0 && ini_getbool("SCSI", "SkipFirstPartition", false, CONFIGFILE)) {
            logmsg("- First partition not configured as requested: SkipFirstPartition=true.");
            continue;
        }

        if (p->type != 0 && p->sectors != 0) {
            snprintf(raw_img_str, sizeof(raw_img_str), "RAW:%lu:%lu", static_cast<unsigned long>(p->lbaFirst),
                     p->lbaFirst + p->sectors - 1);
            logmsg("--- SCSI ID ", static_cast<int>(i), ":", raw_img_str);
            found = true;
            g_scsi_settings.initDevice(i, S2S_CFG_FIXED);
            scsiDiskOpenHDDImage(i, raw_img_str, 0,
                                 RAW_FALLBACK_BLOCKSIZE);
        } else {
            break;
        }
    }
    return found;
}
