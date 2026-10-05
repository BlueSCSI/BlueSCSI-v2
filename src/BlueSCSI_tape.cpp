/* Tape device emulation
 * Will be called by scsi.c from SCSI2SD.
 *
 * ZuluSCSI™ - Copyright (c) 2023-2025 Rabbit Hole Computing™
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

/* Tape image layout
 *
 * A tape holds a sequence of tape files, each terminated by a filemark, and
 * ends with end-of-data after the last filemark. Two image layouts exist:
 *
 * - A single image file holds one tape file. The end of the image file is
 *   its filemark.
 * - A folder holds one image file per tape file, in directory name order.
 *
 * A tape whose only image file is empty is blank: it has no filemark, and a
 * read at beginning of tape reports end of data, as on an erased tape. A
 * lone filemark written on a blank tape is therefore not stored; the first
 * data block or a second filemark makes the tape non-blank.
 *
 * Image files grow as the host writes them. A write at a position inside a
 * file truncates the file there and, for a folder tape, removes the files
 * that follow, since a tape write destroys everything behind the head.
 *
 * Position state: tape_pos is the absolute head position in blocks,
 * tape_mark_block_offset is the absolute position of the current file's
 * first block, and tape_mark_index numbers the current file. An index equal
 * to tape_mark_count means the head is behind the last filemark.
 */

#include "BlueSCSI_disk.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_config.h"
#include <BlueSCSI_platform.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

extern "C" {
#include <scsi.h>
}

static void tapeCheckCondition(uint8_t key, uint16_t asc)
{
    scsiDev.status = CHECK_CONDITION;
    scsiDev.target->sense.code = key;
    scsiDev.target->sense.asc = asc;
    scsiDev.phase = STATUS;
}

static void tapeGood()
{
    scsiDev.status = GOOD;
    scsiDev.phase = STATUS;
}

// Head position inside the currently loaded image file, in blocks
static uint32_t tapeFilePos(image_config_t &img)
{
    return img.tape_pos - img.tape_mark_block_offset;
}

// True when the tape is a folder with one image file per tape file
static bool tapeIsMultiFile(image_config_t &img)
{
    return img.bin_container.isOpen() && img.bin_container.isDir();
}

// Absolute position of the end of the currently loaded image file, in blocks
static uint32_t tapeFileEnd(image_config_t &img)
{
    return img.tape_mark_block_offset + img.get_capacity_lba();
}

// True when the tape holds a single, empty tape file
static bool tapeIsBlank(image_config_t &img)
{
    return img.tape_mark_count == 1 && img.get_capacity_lba() == 0;
}

// Number of filemarks on the tape; the head is behind the last one when
// tape_mark_index reaches this count.
static uint32_t tapeFilemarkCount(image_config_t &img)
{
    return tapeIsBlank(img) ? 0 : img.tape_mark_count;
}

// Find the tape file following `current` in directory name order on a
// folder tape, or the first one when `current` is empty. Returns the length
// of the name, 0 when there is none.
static size_t tapeFindFileAfter(image_config_t &img, const char* current, char* next, size_t next_len)
{
    char name[MAX_FILE_PATH + 1];
    next[0] = '\0';

    FsFile file;
    img.bin_container.rewind();
    while (file.openNext(&img.bin_container, O_RDONLY))
    {
        bool is_tape_file = file.getName(name, sizeof(name)) > 0
            && !file.isDir() && !file.isHidden() && scsiDiskFilenameValid(name);
        file.close();
        if (!is_tape_file) continue;
        if (current[0] != '\0' && strcasecmp(name, current) <= 0) continue;
        if (next[0] == '\0' || strcasecmp(name, next) < 0)
        {
            strncpy(next, name, next_len);
            next[next_len - 1] = '\0';
        }
    }
    return strlen(next);
}

// Load the image file for tape_mark_index on a folder tape when a previous
// operation requested it. Sets sense and returns false when no file opens.
static bool tapeLoadNextFile(image_config_t &img)
{
    if (!img.tape_load_next_file || !tapeIsMultiFile(img))
    {
        return true;
    }
    img.tape_load_next_file = false;

    char current_filename[MAX_FILE_PATH + 1] = {0};
    char next_filename[MAX_FILE_PATH + 1] = {0};

    // The first tape file is the first name in directory order; every other
    // one is the name following the file loaded before it.
    uint32_t previous_capacity = 0;
    if (img.tape_mark_index > 0)
    {
        img.file.getFilename(current_filename, sizeof(current_filename));
        previous_capacity = img.get_capacity_lba();
    }

    if (tapeFindFileAfter(img, current_filename, next_filename, sizeof(next_filename)) > 0
        && img.file.selectImageFile(next_filename))
    {
        img.tape_mark_block_offset = (img.tape_mark_index > 0) ? img.tape_mark_block_offset + previous_capacity : 0;
        dbgmsg("------ Tape file ", (int)img.tape_mark_index, " is ", next_filename, " with ",
               (int)img.get_capacity_lba(), " blocks, starting at tape position ", (int)img.tape_mark_block_offset);
        return true;
    }

    char dir_name[MAX_FILE_PATH + 1] = {0};
    img.file.getFoldername(dir_name, sizeof(dir_name));
    logmsg("No tape element images found or openable in tape directory ", dir_name);
    tapeCheckCondition(MEDIUM_ERROR, MEDIUM_NOT_PRESENT);
    return false;
}

static void doRewind(image_config_t &img)
{
    img.tape_mark_block_offset = 0;
    img.tape_mark_index = 0;
    img.tape_pos = 0;
    img.tape_load_next_file = true;
}

// Position the head at the start of tape file k, or at its end when at_end
// is set. A k of tape_mark_count or more leaves the head behind the last
// filemark. Sets sense and returns false when a file fails to open.
static bool tapeSeekToFile(image_config_t &img, uint32_t k, bool at_end)
{
    doRewind(img);
    if (!tapeLoadNextFile(img))
    {
        return false;
    }
    for (uint32_t i = 0; i < k; i++)
    {
        img.tape_pos = tapeFileEnd(img);
        img.tape_mark_index++;
        if (img.tape_mark_index >= img.tape_mark_count)
        {
            return true;
        }
        img.tape_load_next_file = true;
        if (!tapeLoadNextFile(img))
        {
            return false;
        }
    }
    if (at_end)
    {
        img.tape_pos = tapeFileEnd(img);
    }
    return true;
}

// Derive the name of the tape file that follows `current` in directory
// order: the trailing number in the name is incremented within its width. A
// name without a number, or whose number has no room left in its width, gets
// "_001" inserted before the extension.
static bool tapeNextFileName(const char* current, char* next, size_t next_len)
{
    size_t len = strlen(current);
    const char* dot = strrchr(current, '.');
    const char* ext = dot ? dot : current + len;

    bool ext_numeric = dot && dot[1] != '\0';
    for (const char* p = dot ? dot + 1 : current + len; ext_numeric && *p; p++)
    {
        if (!isdigit((unsigned char)*p)) ext_numeric = false;
    }

    const char* digits_begin;
    const char* digits_end;
    if (ext_numeric)
    {
        digits_begin = dot + 1;
        digits_end = current + len;
    }
    else
    {
        digits_end = ext;
        digits_begin = ext;
        while (digits_begin > current && isdigit((unsigned char)digits_begin[-1]))
        {
            digits_begin--;
        }
    }

    char buf[MAX_FILE_PATH + 1];
    int written = -1;
    char num[16];
    size_t width = digits_end - digits_begin;
    if (width > 0 && width < sizeof(num))
    {
        memcpy(num, digits_begin, width);
        num[width] = '\0';
        int i = (int)width - 1;
        while (i >= 0 && num[i] == '9')
        {
            num[i] = '0';
            i--;
        }
        if (i >= 0)
        {
            num[i]++;
            written = snprintf(buf, sizeof(buf), "%.*s%s%s", (int)(digits_begin - current), current, num, digits_end);
        }
    }
    if (written < 0)
    {
        written = snprintf(buf, sizeof(buf), "%.*s_001%s", (int)(ext - current), current, ext);
    }

    if (written < 0 || (size_t)written >= sizeof(buf) || (size_t)written >= next_len)
    {
        return false;
    }
    strcpy(next, buf);
    return true;
}

// Remove the image files behind the current one on a folder tape, so that
// the current file is the last tape file.
static void tapeRemoveFollowingFiles(image_config_t &img)
{
    char current[MAX_FILE_PATH + 1] = {0};
    char dir_name[MAX_FILE_PATH + 1] = {0};
    char name[MAX_FILE_PATH + 1];
    char path[MAX_FILE_PATH * 2 + 2];
    img.file.getFilename(current, sizeof(current));
    img.file.getFoldername(dir_name, sizeof(dir_name));

    while (tapeFindFileAfter(img, current, name, sizeof(name)) > 0)
    {
        dbgmsg("------ Removing tape file ", name, " behind the write position");
        snprintf(path, sizeof(path), "%s/%s", dir_name, name);
        if (!SD.remove(path))
        {
            logmsg("Failed to remove tape file ", path);
            break;
        }
    }
    img.tape_mark_count = img.tape_mark_index + 1;
}

// Start a new, empty tape file behind the current one on a folder tape and
// make it the current file. Sets sense and returns false on failure.
static bool tapeStartNewFile(image_config_t &img)
{
    char current[MAX_FILE_PATH + 1] = {0};
    char next[MAX_FILE_PATH + 1];
    img.file.getFilename(current, sizeof(current));

    if (!tapeNextFileName(current, next, sizeof(next)))
    {
        logmsg("Cannot derive a tape file name following ", current);
        scsiDev.target->sense.eom = true;
        tapeCheckCondition(VOLUME_OVERFLOW, NO_ADDITIONAL_SENSE_INFORMATION);
        return false;
    }

    FsFile file;
    if (!file.open(&img.bin_container, next, O_RDWR | O_CREAT | O_TRUNC))
    {
        logmsg("Failed to create tape file ", next);
        tapeCheckCondition(MEDIUM_ERROR, WRITE_ERROR_AUTO_REALLOCATION_FAILED);
        return false;
    }
    file.close();

    uint32_t previous_capacity = img.get_capacity_lba();
    if (!img.file.selectImageFile(next))
    {
        logmsg("Failed to open new tape file ", next);
        tapeCheckCondition(MEDIUM_ERROR, MEDIUM_NOT_PRESENT);
        return false;
    }

    img.tape_mark_block_offset += previous_capacity;
    img.tape_mark_index = img.tape_mark_count;
    img.tape_mark_count++;
    img.tape_load_next_file = false;
    dbgmsg("------ Tape file ", (int)img.tape_mark_index, " is ", next, ", starting at tape position ", (int)img.tape_mark_block_offset);
    return true;
}

// Cut the tape at the head position: load the image file, set its length to
// the head position and drop the files behind it. A head behind the last
// filemark sits at the end of the last file, so nothing is cut there. Sets
// sense and returns false when the tape cannot be written here.
static bool tapeCutHere(image_config_t &img)
{
    if (!tapeLoadNextFile(img))
    {
        return false;
    }

    if (!img.file.isOpen() || !img.file.isWritable())
    {
        logmsg("WARNING: Host attempted write to read-only tape ID ", (int)img.getTargetId());
        tapeCheckCondition(DATA_PROTECT, WRITE_PROTECTED);
        return false;
    }

    uint64_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;
    if (!img.file.truncate(tapeFilePos(img) * bytesPerSector))
    {
        logmsg("Failed to set tape image length at block ", (int)tapeFilePos(img));
        tapeCheckCondition(MEDIUM_ERROR, WRITE_ERROR_AUTO_REALLOCATION_FAILED);
        return false;
    }

    if (tapeIsMultiFile(img) && img.tape_mark_index < img.tape_mark_count)
    {
        tapeRemoveFollowingFiles(img);
    }
    return true;
}

// Prepare the head position for writing data. Behind the last filemark a
// new tape file starts, which only a folder tape can hold. Sets sense and
// returns false when the tape cannot be written here.
static bool tapePrepareDataWrite(image_config_t &img)
{
    if (!tapeCutHere(img))
    {
        return false;
    }

    if (img.tape_mark_index >= img.tape_mark_count)
    {
        if (tapeIsMultiFile(img))
        {
            return tapeStartNewFile(img);
        }
        logmsg("A single-file tape image holds one tape file; data written behind its filemark extends that file");
        img.tape_mark_index = img.tape_mark_count - 1;
    }
    return true;
}

// True when writing `blocks` more blocks stays within the configured tape length
static bool tapeWriteFits(image_config_t &img, uint32_t blocks)
{
    if (img.tape_capacity_mb == 0)
    {
        return true;
    }
    uint64_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;
    return ((uint64_t)img.tape_pos + blocks) * bytesPerSector <= ((uint64_t)img.tape_capacity_mb << 20);
}

// True when the head is at or behind the configured tape length
static bool tapeAtCapacity(image_config_t &img)
{
    return img.tape_capacity_mb != 0 && !tapeWriteFits(img, 0);
}

static void doSeek(uint32_t lba)
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;
    uint32_t capacity = img.file.size() / bytesPerSector;

    dbgmsg("------ Locate tape to LBA ", (int)lba);

    if (lba >= capacity)
    {
        tapeCheckCondition(ILLEGAL_REQUEST, LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE);
    }
    else
    {
        platform_delay_ms(10);
        img.tape_pos = lba;
        tapeGood();
    }
}

// Read `blocks` blocks at the head. The sense information field holds the
// untransferred part of the request in units of `residual_unit`: one per
// block for fixed-block reads, the requested byte count for a variable one.
static void doTapeRead(uint32_t blocks, uint32_t residual_unit)
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;

    if (!tapeLoadNextFile(img))
    {
        return;
    }

    if (!img.file.isOpen())
    {
        dbgmsg("------ No image open");
        scsiDev.target->sense.filemark = true;
        tapeCheckCondition(MEDIUM_ERROR, MEDIUM_NOT_PRESENT);
        return;
    }

    uint32_t capacity_lba = img.get_capacity_lba();
    uint32_t file_pos = tapeFilePos(img);
    bool passed_filemarker = false;
    uint32_t blocks_till_eof = 0;
    if (unlikely(((uint64_t) file_pos) + blocks > capacity_lba))
    {
        // The read runs into the filemark at the end of the file: transfer
        // the blocks before it and report the filemark.
        blocks_till_eof = (file_pos < capacity_lba) ? capacity_lba - file_pos : 0;
        passed_filemarker = true;
        // SCSI-2 Spec: "If the fixed bit is one, the information field shall be set to the requested transfer length minus the
        //               actual number of blocks read (not including the filemark)"
        scsiDev.target->sense.info = (blocks - blocks_till_eof) * residual_unit;
        dbgmsg("------ Read tape reached end of file, blocks left to be read ", (int) blocks_till_eof, " out of ", (int) blocks, " sense info set to ", (int) scsiDev.target->sense.info);

        blocks = blocks_till_eof;
    }

    if (blocks > 0)
    {
        dbgmsg("------ Read tape ", (int)blocks, "x", (int)scsiDev.target->liveCfg.bytesPerSector, " tape position ",(int)img.tape_pos,
                        " file position ", (int)file_pos, " in file ",
                        (int)(img.tape_mark_index + 1), "/", (int) img.tape_mark_count, passed_filemarker ? ", file mark reached" : "");
        scsiDiskStartRead(file_pos, blocks);
        scsiFinishWrite();
        img.tape_pos += blocks;
    }

    if (passed_filemarker)
    {
        if (img.tape_mark_index < tapeFilemarkCount(img))
        {
            img.tape_mark_index++;
            if (img.tape_mark_index < img.tape_mark_count)
                img.tape_load_next_file = true;

            scsiDev.target->sense.filemark = true;
            tapeCheckCondition(NO_SENSE, NO_ADDITIONAL_SENSE_INFORMATION);
        }
        else
        {
            // End of data is reported with BLANK CHECK alone; the EOM bit
            // means the physical end of the medium.
            dbgmsg("------ Reached end of data");
            tapeCheckCondition(BLANK_CHECK, NO_ADDITIONAL_SENSE_INFORMATION);
        }
    }
}

// SPACE over filemarks; a negative count moves towards the beginning of tape
static void doSpaceFilemarks(image_config_t &img, int32_t count)
{
    if (!tapeLoadNextFile(img))
    {
        return;
    }

    if (count > 0)
    {
        // Behind the last filemark only end-of-data remains.
        uint32_t marks = tapeFilemarkCount(img);
        uint32_t marks_ahead = (img.tape_mark_index < marks) ? marks - img.tape_mark_index : 0;
        if ((uint32_t)count > marks_ahead)
        {
            if (!tapeSeekToFile(img, marks, false)) return;
            scsiDev.target->sense.info = count - marks_ahead;
            tapeCheckCondition(BLANK_CHECK, NO_ADDITIONAL_SENSE_INFORMATION);
            return;
        }
        if (!tapeSeekToFile(img, img.tape_mark_index + count, false)) return;
        tapeGood();
    }
    else if (count < 0)
    {
        // Spacing back over a filemark lands on its beginning-of-tape side,
        // at the end of the preceding file.
        int64_t target = (int64_t)img.tape_mark_index + count;
        if (target < 0)
        {
            if (!tapeSeekToFile(img, 0, false)) return;
            scsiDev.target->sense.eom = true;
            scsiDev.target->sense.info = (uint32_t)(-target);
            tapeCheckCondition(NO_SENSE, NO_ADDITIONAL_SENSE_INFORMATION);
            return;
        }
        if (!tapeSeekToFile(img, (uint32_t)target, true)) return;
        tapeGood();
    }
    else
    {
        tapeGood();
    }
}

// SPACE over blocks within the current file; a filemark or beginning of
// tape stops the motion.
static void doSpaceBlocks(image_config_t &img, int32_t count)
{
    if (!tapeLoadNextFile(img))
    {
        return;
    }

    uint32_t capacity = img.get_capacity_lba();
    int64_t new_pos = (int64_t)tapeFilePos(img) + count;

    if (new_pos > (int64_t)capacity)
    {
        // Ran into the filemark at the end of the file and stopped behind it.
        uint32_t remaining = (uint32_t)(new_pos - capacity);
        img.tape_pos = tapeFileEnd(img);
        if (img.tape_mark_index < tapeFilemarkCount(img))
        {
            img.tape_mark_index++;
            if (img.tape_mark_index < img.tape_mark_count)
                img.tape_load_next_file = true;
            scsiDev.target->sense.filemark = true;
            scsiDev.target->sense.info = remaining;
            tapeCheckCondition(NO_SENSE, NO_ADDITIONAL_SENSE_INFORMATION);
        }
        else
        {
            scsiDev.target->sense.info = remaining;
            tapeCheckCondition(BLANK_CHECK, NO_ADDITIONAL_SENSE_INFORMATION);
        }
    }
    else if (new_pos < 0)
    {
        uint32_t remaining = (uint32_t)(-new_pos);
        if (img.tape_mark_index > 0)
        {
            // Ran into the filemark in front of the file and stopped in
            // front of it.
            if (!tapeSeekToFile(img, img.tape_mark_index - 1, true)) return;
            scsiDev.target->sense.filemark = true;
            scsiDev.target->sense.info = remaining;
            tapeCheckCondition(NO_SENSE, NO_ADDITIONAL_SENSE_INFORMATION);
        }
        else
        {
            img.tape_pos = 0;
            scsiDev.target->sense.eom = true;
            scsiDev.target->sense.info = remaining;
            tapeCheckCondition(NO_SENSE, NO_ADDITIONAL_SENSE_INFORMATION);
        }
    }
    else
    {
        img.tape_pos = img.tape_mark_block_offset + (uint32_t)new_pos;
        tapeGood();
    }
}

static void doWriteFilemarks(image_config_t &img, uint32_t count)
{
    if (count == 0)
    {
        // Only the buffered data has to reach the medium.
        img.file.flush();
        tapeGood();
        return;
    }

    if (!tapeCutHere(img))
    {
        return;
    }

    // The end of the current file is the first filemark when the head is
    // inside that file. Every further filemark is the end of an empty file.
    uint32_t files_to_create = count - ((img.tape_mark_index < img.tape_mark_count) ? 1 : 0);
    if (tapeIsMultiFile(img))
    {
        for (uint32_t i = 0; i < files_to_create; i++)
        {
            if (!tapeStartNewFile(img))
            {
                return;
            }
        }
    }
    else if (files_to_create > 0)
    {
        logmsg("A single-file tape image holds one filemark; ", (int)files_to_create, " additional filemark(s) not stored");
    }

    // The head is behind the last filemark written.
    img.tape_mark_index = img.tape_mark_count;

    dbgmsg("------ Wrote ", (int)count, " filemark(s) at tape position ", (int)img.tape_pos);
    tapeGood();
}

// ---- COPY (SCSI-2 §8.2.3) ----
// The tape acts as copy manager for segments between itself and a disk
// emulated by this BlueSCSI. Function code 00h copies disk blocks onto the
// tape at the head position, function code 01h copies tape blocks at the
// head position onto the disk. The tape blocks are written and read the way
// WRITE and READ do it, so a filemark ends a segment read from the tape.

// Longest COPY parameter list accepted: the header and 21 segment descriptors
#define TAPE_COPY_LIST_MAX 256

// The disk with the given SCSI address when this BlueSCSI emulates it
static image_config_t *tapeCopyFindDisk(uint8_t scsi_id, uint8_t lun)
{
    const S2S_TargetCfg *cfg = s2s_getConfigById(scsi_id);
    if (lun != 0 || cfg == NULL)
    {
        return NULL;
    }
    switch (cfg->deviceType)
    {
        case S2S_CFG_FIXED:
        case S2S_CFG_REMOVABLE:
        case S2S_CFG_OPTICAL:
        case S2S_CFG_FLOPPY_14MB:
        case S2S_CFG_MO:
        case S2S_CFG_ZIP100:
            break;
        default:
            return NULL;
    }
    image_config_t *disk = (image_config_t*)cfg;
    return disk->file.isOpen() ? disk : NULL;
}

// Copy `blocks` disk blocks starting at `lba` onto the tape at the head
// position. A partial tape block at the end is padded with zeros when `pad`
// is set. Sets sense and returns false on failure, with the sense
// information field holding the number of disk blocks not copied.
static bool tapeCopyFromDisk(image_config_t &img, image_config_t &disk, uint32_t lba, uint32_t blocks, bool pad)
{
    uint32_t disk_bps = disk.bytesPerSector;
    uint32_t tape_bps = scsiDev.target->liveCfg.bytesPerSector;
    uint64_t total_bytes = (uint64_t)blocks * disk_bps;
    uint32_t tape_blocks = total_bytes / tape_bps;
    uint32_t pad_bytes = (total_bytes % tape_bps) ? tape_bps - total_bytes % tape_bps : 0;

    scsiDev.target->sense.info = blocks;
    if (pad_bytes && !pad)
    {
        dbgmsg("------ Copy of ", (int)blocks, "x", (int)disk_bps, " bytes does not fill whole tape blocks");
        tapeCheckCondition(ILLEGAL_REQUEST, INVALID_FIELD_IN_PARAMETER_LIST);
        return false;
    }
    if (pad_bytes)
    {
        tape_blocks++;
    }
    if ((uint64_t)lba + blocks > disk.file.size() / disk_bps)
    {
        dbgmsg("------ Copy source blocks ", (int)lba, "+", (int)blocks, " exceed the disk size");
        tapeCheckCondition(COPY_ABORTED, LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE);
        return false;
    }
    if (!tapeWriteFits(img, tape_blocks))
    {
        dbgmsg("------ Copy of ", (int)tape_blocks, " blocks at tape position ", (int)img.tape_pos,
               " exceeds the configured tape length");
        scsiDev.target->sense.eom = true;
        tapeCheckCondition(VOLUME_OVERFLOW, NO_ADDITIONAL_SENSE_INFORMATION);
        return false;
    }
    if (!tapePrepareDataWrite(img))
    {
        return false;
    }
    if (!disk.file.seek((uint64_t)lba * disk_bps) ||
        !img.file.seek((uint64_t)tapeFilePos(img) * tape_bps))
    {
        tapeCheckCondition(COPY_ABORTED, NO_SEEK_COMPLETE);
        return false;
    }

    dbgmsg("------ Copy ", (int)blocks, "x", (int)disk_bps, " from disk ID ", (int)disk.getTargetId(),
           " LBA ", (int)lba, " to ", (int)tape_blocks, "x", (int)tape_bps, " at tape position ", (int)img.tape_pos);

    uint32_t chunk_blocks_max = sizeof(scsiDev.data) / disk_bps;
    uint32_t done = 0;
    while (done < blocks)
    {
        uint32_t chunk_blocks = blocks - done;
        if (chunk_blocks > chunk_blocks_max)
        {
            chunk_blocks = chunk_blocks_max;
        }
        size_t chunk_bytes = chunk_blocks * disk_bps;
        platform_reset_watchdog();
        if (disk.file.read(scsiDev.data, chunk_bytes) != (ssize_t)chunk_bytes)
        {
            logmsg("Copy read from disk ID ", (int)disk.getTargetId(), " failed at LBA ", (int)(lba + done));
            tapeCheckCondition(COPY_ABORTED, UNRECOVERED_READ_ERROR);
            return false;
        }
        if (img.file.write(scsiDev.data, chunk_bytes) != (ssize_t)chunk_bytes)
        {
            logmsg("Copy write to tape failed at tape position ", (int)img.tape_pos);
            tapeCheckCondition(COPY_ABORTED, WRITE_ERROR_AUTO_REALLOCATION_FAILED);
            return false;
        }
        done += chunk_blocks;
        scsiDev.target->sense.info = blocks - done;
    }
    if (pad_bytes)
    {
        memset(scsiDev.data, 0, pad_bytes);
        if (img.file.write(scsiDev.data, pad_bytes) != (ssize_t)pad_bytes)
        {
            logmsg("Copy write to tape failed at tape position ", (int)img.tape_pos);
            tapeCheckCondition(COPY_ABORTED, WRITE_ERROR_AUTO_REALLOCATION_FAILED);
            return false;
        }
    }
    img.file.flush();
    img.tape_pos += tape_blocks;
    scsiDiskInvalidatePrefetch();
    return true;
}

// Copy tape blocks at the head position onto `blocks` disk blocks starting
// at `lba`. Sets sense and returns false on failure, with the sense
// information field holding the number of disk blocks not copied.
static bool tapeCopyToDisk(image_config_t &img, image_config_t &disk, uint32_t lba, uint32_t blocks, bool pad)
{
    uint32_t disk_bps = disk.bytesPerSector;
    uint32_t tape_bps = scsiDev.target->liveCfg.bytesPerSector;
    uint64_t total_bytes = (uint64_t)blocks * disk_bps;
    uint32_t tape_blocks = (total_bytes + tape_bps - 1) / tape_bps;

    scsiDev.target->sense.info = blocks;
    if (total_bytes % tape_bps && !pad)
    {
        dbgmsg("------ Copy of ", (int)blocks, "x", (int)disk_bps, " bytes does not cover whole tape blocks");
        tapeCheckCondition(ILLEGAL_REQUEST, INVALID_FIELD_IN_PARAMETER_LIST);
        return false;
    }
    if ((uint64_t)lba + blocks > disk.file.size() / disk_bps)
    {
        dbgmsg("------ Copy destination blocks ", (int)lba, "+", (int)blocks, " exceed the disk size");
        tapeCheckCondition(COPY_ABORTED, LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE);
        return false;
    }
    if (!disk.file.isWritable())
    {
        logmsg("WARNING: Copy to read-only drive ID ", (int)disk.getTargetId());
        tapeCheckCondition(COPY_ABORTED, WRITE_PROTECTED);
        return false;
    }
    if (!tapeLoadNextFile(img))
    {
        return false;
    }
    uint32_t blocks_till_eof = img.get_capacity_lba() - tapeFilePos(img);
    if (tape_blocks > blocks_till_eof)
    {
        dbgmsg("------ Copy of ", (int)tape_blocks, " tape blocks at tape position ", (int)img.tape_pos,
               " reaches the filemark after ", (int)blocks_till_eof, " blocks");
        scsiDev.target->sense.filemark = true;
        tapeCheckCondition(COPY_ABORTED, NO_ADDITIONAL_SENSE_INFORMATION);
        return false;
    }
    if (!disk.file.seek((uint64_t)lba * disk_bps) ||
        !img.file.seek((uint64_t)tapeFilePos(img) * tape_bps))
    {
        tapeCheckCondition(COPY_ABORTED, NO_SEEK_COMPLETE);
        return false;
    }

    dbgmsg("------ Copy ", (int)tape_blocks, "x", (int)tape_bps, " at tape position ", (int)img.tape_pos,
           " to ", (int)blocks, "x", (int)disk_bps, " on disk ID ", (int)disk.getTargetId(), " LBA ", (int)lba);

    uint32_t chunk_blocks_max = sizeof(scsiDev.data) / disk_bps;
    uint32_t done = 0;
    while (done < blocks)
    {
        uint32_t chunk_blocks = blocks - done;
        if (chunk_blocks > chunk_blocks_max)
        {
            chunk_blocks = chunk_blocks_max;
        }
        size_t chunk_bytes = chunk_blocks * disk_bps;
        platform_reset_watchdog();
        if (img.file.read(scsiDev.data, chunk_bytes) != (ssize_t)chunk_bytes)
        {
            logmsg("Copy read from tape failed at tape position ", (int)img.tape_pos);
            tapeCheckCondition(COPY_ABORTED, UNRECOVERED_READ_ERROR);
            return false;
        }
        if (disk.file.write(scsiDev.data, chunk_bytes) != (ssize_t)chunk_bytes)
        {
            logmsg("Copy write to disk ID ", (int)disk.getTargetId(), " failed at LBA ", (int)(lba + done));
            tapeCheckCondition(COPY_ABORTED, WRITE_ERROR_AUTO_REALLOCATION_FAILED);
            return false;
        }
        done += chunk_blocks;
        scsiDev.target->sense.info = blocks - done;
    }
    disk.file.flush();
    img.tape_pos += tape_blocks;
    scsiDiskInvalidatePrefetch();
    return true;
}

// Run the COPY parameter list received in the DATA OUT phase
static void doCopy(void)
{
    if (scsiDev.status == GOOD)
    {
        image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
        uint8_t list[TAPE_COPY_LIST_MAX];
        uint32_t len = scsiDev.dataLen;
        bool pad = scsiDev.cdb[1] & 1;
        memcpy(list, scsiDev.data, len);

        uint8_t function = list[0] >> 3;
        if (len < 4 || (len - 4) % 12 != 0)
        {
            dbgmsg("------ Copy parameter list of ", (int)len, " bytes has no whole segment descriptors");
            tapeCheckCondition(ILLEGAL_REQUEST, PARAMETER_LIST_LENGTH_ERROR);
            return;
        }
        if (function != 0 && function != 1)
        {
            dbgmsg("------ Copy function code ", (int)function, " is not supported");
            tapeCheckCondition(ILLEGAL_REQUEST, INVALID_FIELD_IN_PARAMETER_LIST);
            return;
        }

        for (uint32_t offset = 4; offset < len; offset += 12)
        {
            const uint8_t *d = list + offset;
            uint8_t src_id = d[0] >> 5, src_lun = d[0] & 7;
            uint8_t dst_id = d[1] >> 5, dst_lun = d[1] & 7;
            uint32_t stream_bps = ((uint32_t)d[2] << 8) | d[3];
            uint32_t blocks = ((uint32_t)d[4] << 24) | ((uint32_t)d[5] << 16) | ((uint32_t)d[6] << 8) | d[7];
            uint32_t lba = ((uint32_t)d[8] << 24) | ((uint32_t)d[9] << 16) | ((uint32_t)d[10] << 8) | d[11];
            bool to_tape = function == 0;
            uint8_t tape_id = to_tape ? dst_id : src_id;
            uint8_t tape_lun = to_tape ? dst_lun : src_lun;
            uint8_t disk_id = to_tape ? src_id : dst_id;
            uint8_t disk_lun = to_tape ? src_lun : dst_lun;

            dbgmsg("------ Copy segment ", (int)((offset - 4) / 12), ": function ", (int)function,
                   " source ID ", (int)src_id, " LUN ", (int)src_lun,
                   " destination ID ", (int)dst_id, " LUN ", (int)dst_lun,
                   " stream block length ", (int)stream_bps, " blocks ", (int)blocks, " LBA ", (int)lba);

            image_config_t *disk = tapeCopyFindDisk(disk_id, disk_lun);
            if (tape_id != scsiDev.target->targetId || tape_lun != 0 || disk == NULL)
            {
                dbgmsg("------ Copy is only supported between this tape and a disk emulated by this device");
                tapeCheckCondition(ILLEGAL_REQUEST, INVALID_FIELD_IN_PARAMETER_LIST);
                return;
            }
            if (stream_bps != 0 && stream_bps != scsiDev.target->liveCfg.bytesPerSector)
            {
                dbgmsg("------ Copy stream block length ", (int)stream_bps, " differs from the tape block length");
                tapeCheckCondition(ILLEGAL_REQUEST, INVALID_FIELD_IN_PARAMETER_LIST);
                return;
            }
            if (blocks == 0)
            {
                continue;
            }
            bool ok = to_tape ? tapeCopyFromDisk(img, *disk, lba, blocks, pad)
                              : tapeCopyToDisk(img, *disk, lba, blocks, pad);
            if (!ok)
            {
                return;
            }
        }
        tapeGood();
    }
    scsiDev.phase = STATUS;
}

extern "C" uint8_t scsiTapeDensityCode()
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    uint8_t selected = scsiDev.target->liveCfg.tapeDensity;
    return selected ? selected : img.tape_density;
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

            // SCSI-2 §10.2.4: a transfer length of zero is not an error —
            // no data is transferred and position is unchanged.
            if (length == 0)
            {
                blocks_to_read = 0;
            }
            else
            {
                // SCSI-2 Section 10.2.4: variable-block length checking
                // Underlength: block is larger than host's requested length.
                // Error unless SILI (Suppress Incorrect Length Indicator) is set.
                // Overlength (host wants more than block has) is not an error.
                bool underlength = (length < blocklen);
                if (underlength && !supress_invalid_length)
                {
                    dbgmsg("------ Host requested variable block max ", (int)length, " bytes, blocksize is ", (int)blocklen);
                    tapeCheckCondition(ILLEGAL_REQUEST, INVALID_FIELD_IN_CDB);
                    return 1;
                }
            }
        }


        if (blocks_to_read > 0)
        {
            doTapeRead(blocks_to_read, fixed ? 1 : length);
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

            // SCSI-2 §10.2.14: transfer length of zero is not an error.
            if (length == 0)
            {
                blocks_to_write = 0;
            }
            else if (length != blocklen)
            {
                dbgmsg("------ Host requested variable block ", (int)length, " bytes, blocksize is ", (int)blocklen);
                tapeCheckCondition(ILLEGAL_REQUEST, INVALID_FIELD_IN_CDB);
                return 1;
            }
        }

        if (blocks_to_write > 0)
        {
            if (!tapeWriteFits(img, blocks_to_write))
            {
                // SCSI-2 §10.2.14: nothing is written past the end of the
                // medium; the information field holds the untransferred amount.
                dbgmsg("------ Write of ", (int)blocks_to_write, " blocks at tape position ", (int)img.tape_pos,
                       " exceeds the configured tape length");
                scsiDev.target->sense.eom = true;
                scsiDev.target->sense.info = fixed ? blocks_to_write : length;
                tapeCheckCondition(VOLUME_OVERFLOW, NO_ADDITIONAL_SENSE_INFORMATION);
                return 1;
            }

            if (!tapePrepareDataWrite(img))
            {
                return 1;
            }

            scsiDiskStartWrite(tapeFilePos(img), blocks_to_write);
            if (scsiDev.phase == DATA_OUT)
            {
                img.tape_pos += blocks_to_write;
            }
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
            tapeCheckCondition(ILLEGAL_REQUEST, INVALID_FIELD_IN_CDB);
        }
        else
        {
            // Host requests ECC check, report that it passed.
            tapeGood();
            img.tape_pos += length;
        }
    }
    else if (command == 0x18 || command == 0x3A)
    {
        // COPY and COPY AND VERIFY; the copy lands in an image file, so the
        // verification pass of COPY AND VERIFY has nothing left to check.
        uint32_t offset = (command == 0x18) ? 2 : 3;
        uint32_t length =
            (((uint32_t) scsiDev.cdb[offset]) << 16) +
            (((uint32_t) scsiDev.cdb[offset + 1]) << 8) +
            scsiDev.cdb[offset + 2];

        if (length == 0)
        {
            tapeGood();
        }
        else if (length > TAPE_COPY_LIST_MAX)
        {
            dbgmsg("------ Copy parameter list of ", (int)length, " bytes is longer than supported");
            tapeCheckCondition(ILLEGAL_REQUEST, PARAMETER_LIST_LENGTH_ERROR);
        }
        else
        {
            scsiDev.dataLen = length;
            scsiDev.phase = DATA_OUT;
            scsiDev.postDataOutHook = doCopy;
        }
    }
    else if (command == 0x19)
    {
        // ERASE: everything from the head position to the end of tape is gone.
        if (tapeCutHere(img))
        {
            dbgmsg("------ Erased tape from position ", (int)img.tape_pos);
            tapeGood();
        }
    }
    else if (command == 0x01)
    {
        // REWIND
        doRewind(img);
    }
    else if (command == 0x05)
    {
        // READ BLOCK LIMITS
        uint32_t blocklen = scsiDev.target->liveCfg.bytesPerSector;
        scsiDev.data[0] = 0; // Reserved
        scsiDev.data[1] = (blocklen >> 16) & 0xFF; // Maximum block length (MSB)
        scsiDev.data[2] = (blocklen >>  8) & 0xFF;
        scsiDev.data[3] = (blocklen >>  0) & 0xFF; // Maximum block length (LSB)
        // SCSI-2 Section 10.2.4: Bytes 4-5 = minimum block length (16-bit big-endian)
        scsiDev.data[4] = (blocklen >>  8) & 0xFF; // Minimum block length (MSB)
        scsiDev.data[5] = (blocklen >>  0) & 0xFF; // Minimum block length (LSB)
        scsiDev.dataLen = 6;
        scsiDev.phase = DATA_IN;
    }
    else if (command == 0x10)
    {
        // WRITE FILEMARKS
        uint32_t count =
            (((uint32_t) scsiDev.cdb[2]) << 16) |
            (((uint32_t) scsiDev.cdb[3]) << 8) |
            scsiDev.cdb[4];
        doWriteFilemarks(img, count);
    }
    else if (command == 0x11)
    {
        // SPACE
        // SCSI-2 Section 10.2.11: Count is a 24-bit two's complement value
        // in CDB bytes 2-4. Byte 5 is the control byte. Negative values
        // mean space in the reverse direction. The count is relative to
        // the current position.
        uint8_t code = scsiDev.cdb[1] & 7;
        uint32_t raw_count =
            (((uint32_t) scsiDev.cdb[2]) << 16) |
            (((uint32_t) scsiDev.cdb[3]) << 8) |
            scsiDev.cdb[4];
        // Sign-extend from 24-bit two's complement
        int32_t count = (raw_count & 0x800000) ? (int32_t)(raw_count | 0xFF000000) : (int32_t)raw_count;
        if (code == 0)
        {
            doSpaceBlocks(img, count);
        }
        else if (code == 1)
        {
            doSpaceFilemarks(img, count);
        }
        else if (code == 3)
        {
            // End-of-data: behind the last filemark.
            if (tapeSeekToFile(img, tapeFilemarkCount(img), false))
            {
                tapeGood();
            }
        }
        else
        {
            tapeCheckCondition(ILLEGAL_REQUEST, INVALID_FIELD_IN_CDB);
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
        if (lba == 0) scsiDev.data[0] |= 0x80; // Beginning of partition
        if (tapeAtCapacity(img)) scsiDev.data[0] |= 0x40; // End of partition
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
