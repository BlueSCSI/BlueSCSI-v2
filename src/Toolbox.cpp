/** 
 * Copyright (C) 2023 Eric Helgeson
 * Copyright (c) 2024-2025 Rabbit Hole Computing
 * Copyright (C) 2025 Niels Martin Hansen
 * 
 * This file is originally part of BlueSCSI adopted for BlueSCSI
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
#include "Toolbox.h"
#include "BlueSCSI_disk.h"
#include "BlueSCSI_cdrom.h"
#include "BlueSCSI_log.h"
#include <minIni.h>
#include <SdFat.h>
extern "C" {
#include <toolbox.h>
#include <scsi2sd_time.h>
#include <sd.h>
#include <mode.h>
}


const uint8_t MAX_FILE_LISTING_FILES = 100;


extern "C" int8_t scsiToolboxEnabled()
{
    static int8_t enabled = -1;
    if (enabled == -1)
    {
        enabled = ini_getbool("SCSI", "EnableToolbox", 0, CONFIGFILE);
        logmsg("Toolbox enabled = ", enabled);
    }
    return enabled == 1;
}


static bool toolboxFilenameValid(const char* name, bool isCD = false)
{
    if(strlen(name) == 0)
    {
        dbgmsg("toolbox: Ignoring filename empty file name");
        return false;
    }
    if (isCD)
    {
      return scsiDiskFilenameValid(name);
    }
    return true;
}

static void doCountFiles(const char * dir_name, bool isCD = false)
{
    FsFile dir;
    FsFile file;
    char name[MAX_FILE_PATH] = {0};
    dir.open(dir_name);
    dir.rewindDirectory();
    uint8_t file_count = 0;
    while (file.openNext(&dir, O_RDONLY))
    {
        if(file.getError() > 0)
        {
            file.close();
            break;
        }
        bool isDir = file.isDirectory();
        size_t len = file.getName(name, MAX_FILE_PATH);
        file.close();
        if (isCD && isDir)
            continue;
        // truncate filename the same way listing does, before validating name
        if (len > MAX_MAC_PATH)
            name[MAX_MAC_PATH] = 0x0;
        dbgmsg("TOOLBOX COUNT FILES: truncated filename is '", name, "'");
        // only count valid files.
        if(toolboxFilenameValid(name, isCD))
        {
            file_count = file_count + 1;
            if(file_count > MAX_FILE_LISTING_FILES) {
                scsiDev.status = CHECK_CONDITION;
                scsiDev.target->sense.code = ILLEGAL_REQUEST;
                scsiDev.target->sense.asc = OPEN_RETRO_SCSI_TOO_MANY_FILES;
                scsiDev.phase = STATUS;
                dir.close();
                return;
            }
        }
  }
  scsiDev.data[0] = file_count;
  scsiDev.dataLen = sizeof(file_count);
  scsiDev.phase = DATA_IN;
}

static void onListFiles(const char * dir_name, bool isCD = false) {
    FsFile dir;
    FsFile file;
    const size_t ENTRY_SIZE = 40;

    memset(scsiDev.data, 0, ENTRY_SIZE * (MAX_FILE_LISTING_FILES + 1));
    char name[MAX_FILE_PATH] = {0};
    uint8_t index = 0;
    uint8_t file_entry[ENTRY_SIZE] = {0};

    dir.open(dir_name);
    dir.rewindDirectory();
    while (file.openNext(&dir, O_RDONLY))
    {   
        memset(name, 0, sizeof(name));
        // get base information
        uint8_t isDir = file.isDirectory() ? 0x00 : 0x01;
        size_t len = file.getName(name, MAX_FILE_PATH);
        uint64_t size = file.fileSize();
        file.close();
        // validate file is allowed for this listing
        if (!toolboxFilenameValid(name, isCD))
            continue;
        if (isCD && isDir == 0x00)
            continue;
        // truncate filename to fit in destination buffer
        if (len > MAX_MAC_PATH)
            name[MAX_MAC_PATH] = 0x0;
        dbgmsg("TOOLBOX LIST FILES: truncated filename is '", name, "'");
        // fill output buffer
        file_entry[0] = index;
        file_entry[1] = isDir;
        for(int i = 0; i < MAX_MAC_PATH + 1 ; i++) {
            file_entry[i + 2] = name[i];   // bytes 2 - 34
        }
        file_entry[35] = 0; //(size >> 32) & 0xff;
        file_entry[36] = (size >> 24) & 0xff;
        file_entry[37] = (size >> 16) & 0xff;
        file_entry[38] = (size >> 8) & 0xff;
        file_entry[39] = (size) & 0xff;
        // send to SCSI output buffer
        memcpy(&(scsiDev.data[ENTRY_SIZE * index]), file_entry, ENTRY_SIZE);
        // increment index
        index = index + 1;
        if (index >= MAX_FILE_LISTING_FILES) break;
    }
    dir.close();

    scsiDev.dataLen = ENTRY_SIZE * index;
    scsiDev.phase = DATA_IN;
    dbgmsg("TOOLBOX LIST FILES: returning ", index, " files for size ", scsiDev.dataLen);
}

static FsFile get_file_from_index(uint8_t index, const char * dir_name, bool isCD = false)
{
    FsFile dir;
    FsFile file_test;
    char name[MAX_FILE_PATH] = {0};

    dir.open(dir_name);
    dir.rewindDirectory(); // Back to the top
    int count = 0;
    while (file_test.openNext(&dir, O_RDONLY))
    {
        // If error there is no next file to open.
        if(file_test.getError() > 0) {
            file_test.close();
            break;
        }
        // no directories in CD image listing
        if (isCD && file_test.isDirectory())
        {
            file_test.close();
            continue;
        }
        // truncate filename the same way listing does, before validating name
        size_t len = file_test.getName(name, MAX_FILE_PATH);
        if (len > MAX_MAC_PATH)
            name[MAX_MAC_PATH] = 0x0;
        // validate filename
        if(!toolboxFilenameValid(name, isCD))
        {
            file_test.close();
            continue;
        }
        // found file?
        if (count == index)
        {
            dir.close();
            return file_test;
        }
        else
        {
            file_test.close();
        }
        count++;
    }
    file_test.close();
    dir.close();
    return file_test;
}

// Devices that are active on this SCSI device.
static void onListDevices()
{
    for (int i = 0; i < NUM_SCSIID; i++)
    {
        const S2S_TargetCfg* cfg = s2s_getConfigById(i);
        if (cfg && (cfg->scsiId & S2S_CFG_TARGET_ENABLED))
        {
            scsiDev.data[i] = (int)cfg->deviceType; // 2 == cd
        }
        else
        {
            scsiDev.data[i] = 0xFF; // not enabled target.
        }
    }
    scsiDev.dataLen = NUM_SCSIID;
}

static void onSetNextCD(const char * img_dir)
{
    char name[MAX_FILE_PATH] = {0};
    char full_path[MAX_FILE_PATH * 2] = {0};
    uint8_t file_index = scsiDev.cdb[1];
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    FsFile next_cd = get_file_from_index(file_index, img_dir, true);
    next_cd.getName(name, sizeof(name));
    next_cd.close();
    snprintf(full_path, (MAX_FILE_PATH * 2), "%s/%s", img_dir, name);
    switchNextImage(img, full_path);
}

FsFile gFile; // global so we can keep it open while transfering.
void onGetFile10(char * dir_name) {
    uint8_t index = scsiDev.cdb[1];

    uint32_t offset = ((uint32_t)scsiDev.cdb[2] << 24) | ((uint32_t)scsiDev.cdb[3] << 16) | ((uint32_t)scsiDev.cdb[4] << 8) | scsiDev.cdb[5];

    if (offset == 0) // first time, open the file.
    {
        gFile = get_file_from_index(index, dir_name);
        if(!gFile.isDirectory() && !gFile.isReadable())
        {
            scsiDev.status = CHECK_CONDITION;
            scsiDev.target->sense.code = ILLEGAL_REQUEST;
            //SCSI_ASC_INVALID_FIELD_IN_CDB
            scsiDev.phase = STATUS;
            return;
        }
    }

    uint32_t file_total = gFile.size();
    memset(scsiDev.data, 0, 4096);
    gFile.seekSet(offset * 4096);
    int bytes_read = gFile.read(scsiDev.data, 4096);
    if(offset * 4096 >= file_total) // transfer done, close.
    {
        gFile.close();
    }
    scsiDev.dataLen = bytes_read;
    scsiDev.phase = DATA_IN;
}

/*
  Prepares a file for receving. The file name is null terminated in the scsi data.
*/
static void onSendFilePrep(char * dir_name)
{
    char file_name[32+1];

    scsiEnterPhase(DATA_OUT);
    scsiRead(static_cast<uint8_t *>(static_cast<void *>(file_name)), 32+1, NULL);
    file_name[32] = '\0';

    dbgmsg("TOOLBOX OPEN FILE FOR WRITE: '", file_name, "'");
    SD.chdir(dir_name);
    gFile.open(file_name, FILE_WRITE);
    SD.chdir("/");
    if(gFile.isOpen() && gFile.isWritable())
    {
        gFile.rewind();
        gFile.sync();
        // do i need to manually set phase to status here?
        return;
    } else {
        gFile.close();
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        //SCSI_ASC_INVALID_FIELD_IN_CDB
        scsiDev.phase = STATUS;
    }
}

static void onSendFileEnd(void)
{
    gFile.sync();
    gFile.close();
    scsiDev.phase = STATUS;
}

static void onSendFile10(void)
{
    if(!gFile.isOpen() || !gFile.isWritable())
    {
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        //SCSI_ASC_INVALID_FIELD_IN_CDB
        scsiDev.phase = STATUS;
    }

    // Number of bytes sent this request, 1..512.
    uint16_t bytes_sent = ((uint16_t)scsiDev.cdb[1] << 8)  | scsiDev.cdb[2];
    // 512 byte offset of where to put these bytes.
    uint32_t offset     = ((uint32_t)scsiDev.cdb[3] << 16) | ((uint32_t)scsiDev.cdb[4] << 8) | scsiDev.cdb[5];
    const uint16_t BUFSIZE   = 512;
    uint8_t buf[BUFSIZE];

    // Do not allow buffer overrun
    if (bytes_sent > BUFSIZE)
    {
        dbgmsg("TOOLBOX SEND FILE 10 ILLEGAL DATA SIZE");
        gFile.close();
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
    }

    scsiEnterPhase(DATA_OUT);
    scsiRead(buf, bytes_sent, NULL);
    gFile.seekCur(offset * 512);
    gFile.write(buf, bytes_sent);
    if(gFile.getWriteError())
    {
        gFile.clearWriteError();
        gFile.close();
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
    }
    //scsiDev.phase = STATUS;
}
static void onToggleDebug()
{
    if(scsiDev.cdb[1] == 0) // 0 == Set Debug, 1 == Get Debug State
    {
        g_log_debug = scsiDev.cdb[2];
        logmsg("Set debug logs to: ", g_log_debug);
        scsiDev.phase = STATUS;
    }
    else
    {
        logmsg("Debug currently set to: ", g_log_debug);
        scsiDev.data[0] = g_log_debug ? 0x1 : 0x0;
        scsiDev.dataLen = 1;
        scsiDev.phase = DATA_IN;
    }
}

static int getToolBoxSharedDir(char * dir_name)
{
  return ini_gets("SCSI", "ToolBoxSharedDir", "/shared", dir_name, MAX_FILE_PATH, CONFIGFILE);
}

extern "C" int scsiToolboxCommand()
{
    int commandHandled = 1;
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    uint8_t command = scsiDev.cdb[0];

    if (unlikely(command == TOOLBOX_COUNT_FILES))
    {
        char img_dir[MAX_FILE_PATH];
        dbgmsg("TOOLBOX_COUNT_FILES");
        getToolBoxSharedDir(img_dir);
        doCountFiles(img_dir);
    }
    else if (unlikely(command == TOOLBOX_LIST_FILES))
    {
        char img_dir[MAX_FILE_PATH];
        dbgmsg("TOOLBOX_LIST_FILES");
        getToolBoxSharedDir(img_dir);
        onListFiles(img_dir);
    }
    else if (unlikely(command == TOOLBOX_GET_FILE))
    {
        char img_dir[MAX_FILE_PATH];
        dbgmsg("TOOLBOX_GET_FILE");
        getToolBoxSharedDir(img_dir);
        onGetFile10(img_dir);
    }
    else if (unlikely(command == TOOLBOX_SEND_FILE_PREP))
    {
        char img_dir[MAX_FILE_PATH];
        dbgmsg("TOOLBOX_SEND_FILE_PREP");
        getToolBoxSharedDir(img_dir);
        onSendFilePrep(img_dir);
    }
    else if (unlikely(command == TOOLBOX_SEND_FILE_10))
    {
        dbgmsg("TOOLBOX_SEND_FILE_10");
        onSendFile10();
    }
    else if (unlikely(command == TOOLBOX_SEND_FILE_END))
    {
        dbgmsg("TOOLBOX_SEND_FILE_END");
        onSendFileEnd();
    }
    else if(unlikely(command == TOOLBOX_TOGGLE_DEBUG))
    {
        dbgmsg("TOOLBOX_TOGGLE_DEBUG");
        onToggleDebug();
    }
    else if(unlikely(command == TOOLBOX_LIST_CDS))
    {
        char img_dir[4];
        dbgmsg("TOOLBOX_LIST_CDS");
        snprintf(img_dir, sizeof(img_dir), CD_IMG_DIR, (int)img.scsiId & S2S_CFG_TARGET_ID_BITS);
        onListFiles(img_dir, true);
    }
    else if(unlikely(command == TOOLBOX_SET_NEXT_CD))
    {
        char img_dir[4];
        dbgmsg("TOOLBOX_SET_NEXT_CD");
        snprintf(img_dir, sizeof(img_dir), CD_IMG_DIR, (int)img.scsiId & S2S_CFG_TARGET_ID_BITS);
        onSetNextCD(img_dir);
    }
    else if(unlikely(command == TOOLBOX_LIST_DEVICES))
    {
        dbgmsg("TOOLBOX_LIST_DEVICES");
        onListDevices();
    }
    else if (unlikely(command == TOOLBOX_COUNT_CDS))
    {
        char img_dir[4];
        dbgmsg("TOOLBOX_COUNT_CDS");
        snprintf(img_dir, sizeof(img_dir), CD_IMG_DIR, (int)img.scsiId & S2S_CFG_TARGET_ID_BITS);
        doCountFiles(img_dir, true);
    }
    else
    {
        commandHandled = 0;
    }

    return commandHandled;
}