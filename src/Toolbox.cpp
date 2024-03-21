/** 
 * Copyright (C) 2023 Eric Helgeson
 * Copyright (C) 2024 Rabbit Hole Computing
 * This file is originally part of BlueSCSI adopted for ZuluSCSI
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
#include "ZuluSCSI_disk.h"
#include "ZuluSCSI_cdrom.h"
#include "ZuluSCSI_log.h"
#include <minIni.h>
#include <SdFat.h>
extern "C" {
#include <toolbox.h>
#include <scsi2sd_time.h>
#include <sd.h>
#include <mode.h>
}


extern "C" int8_t scsiToolboxEnabled()
{
    static int8_t enabled = -1;
    if (enabled == -1)
    {
        enabled = ini_getbool("SCSI", "EnableToolbox", 0, CONFIGFILE);
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
        file.getName(name, MAX_FILE_PATH);
        file.close();
        if (isCD && isDir)
            continue;
        // only count valid files.
        if(toolboxFilenameValid(name, isCD))
        {
            file_count = file_count + 1;
            if(file_count > 100) {
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

    memset(scsiDev.data, 0, 4096);
    int ENTRY_SIZE = 40;
    char name[MAX_FILE_PATH] = {0};
    dir.open(dir_name);
    dir.rewindDirectory();
    uint8_t index = 0;
    uint8_t file_entry[40] = {0};
    while (file.openNext(&dir, O_RDONLY))
    {   
        uint8_t isDir = file.isDirectory() ? 0x00 : 0x01;
        int len = file.getName(name, MAX_FILE_PATH);
        if (len > MAX_MAC_PATH)
            name[MAX_MAC_PATH] = 0x0;
        uint64_t size = file.fileSize();
        file.close();
        if (!toolboxFilenameValid(name, isCD))
            continue;
        if (isCD && isDir == 0x00)
            continue;
        file_entry[0] = index;
        file_entry[1] = isDir;
        int c = 0; // Index of char in name[]

        for(int i = 2; i < (MAX_MAC_PATH + 1 + 2); i++) {   // bytes 2 - 34
            file_entry[i] = name[c++];
        }
        file_entry[35] = 0; //(size >> 32) & 0xff;
        file_entry[36] = (size >> 24) & 0xff;
        file_entry[37] = (size >> 16) & 0xff;
        file_entry[38] = (size >> 8) & 0xff;
        file_entry[39] = (size) & 0xff;
        memcpy(&(scsiDev.data[ENTRY_SIZE * index]), file_entry, ENTRY_SIZE);
        index = index + 1;
    }
    dir.close();

    scsiDev.dataLen = 4096;
    scsiDev.phase = DATA_IN;
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
    file_test.getName(name, MAX_FILE_PATH);
    if (isCD && file_test.isDirectory())
    {
        file_test.close();
        continue;
    }
    if(!toolboxFilenameValid(name, isCD))
    {
        file_test.close();
        continue;
    }
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
    cdromSwitchNextImage(img, full_path);
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
    memset(file_name, '\0', 32+1);
    scsiEnterPhase(DATA_OUT);
    for (int i = 0; i < 32+1; ++i)
    {
        file_name[i] = scsiReadByte();
    }
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
    uint16_t buf_size   = 512;
    uint8_t buf[512];

    // Check if last block of file, and not the only bock in file.
    if(bytes_sent < buf_size)
    {
        buf_size = bytes_sent;
    }

    scsiEnterPhase(DATA_OUT);
    scsiRead(buf, bytes_sent, NULL);
    gFile.seekCur(offset * 512);
    gFile.write(buf, buf_size);
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
        getToolBoxSharedDir(img_dir);
        doCountFiles(img_dir);
    }
    else if (unlikely(command == TOOLBOX_LIST_FILES))
    {
        char img_dir[MAX_FILE_PATH];
        getToolBoxSharedDir(img_dir);
        onListFiles(img_dir);
    }
    else if (unlikely(command == TOOLBOX_GET_FILE))
    {
        char img_dir[MAX_FILE_PATH];
        getToolBoxSharedDir(img_dir);
        onGetFile10(img_dir);
    }
    else if (unlikely(command == TOOLBOX_SEND_FILE_PREP))
    {
        char img_dir[MAX_FILE_PATH];
        getToolBoxSharedDir(img_dir);
        onSendFilePrep(img_dir);
    }
    else if (unlikely(command == TOOLBOX_SEND_FILE_10))
    {
        onSendFile10();
    }
    else if (unlikely(command == TOOLBOX_SEND_FILE_END))
    {
        onSendFileEnd();
    }
    else if(unlikely(command == TOOLBOX_TOGGLE_DEBUG))
    {
        onToggleDebug();
    }
    else if(unlikely(command == TOOLBOX_LIST_CDS))
    {
        char img_dir[4];
        snprintf(img_dir, sizeof(img_dir), CD_IMG_DIR, (int)img.scsiId & S2S_CFG_TARGET_ID_BITS);
        onListFiles(img_dir, true);
    }
    else if(unlikely(command == TOOLBOX_SET_NEXT_CD))
    {
        char img_dir[4];
        snprintf(img_dir, sizeof(img_dir), CD_IMG_DIR, (int)img.scsiId & S2S_CFG_TARGET_ID_BITS);
        onSetNextCD(img_dir);
    }
    else if(unlikely(command == TOOLBOX_LIST_DEVICES))
    {
        onListDevices();
    }
    else if (unlikely(command == TOOLBOX_COUNT_CDS))
    {
        char img_dir[4];
        snprintf(img_dir, sizeof(img_dir), CD_IMG_DIR, (int)img.scsiId & S2S_CFG_TARGET_ID_BITS);
        doCountFiles(img_dir, true);
    }
    else
    {
        commandHandled = 0;
    }

    return commandHandled;
}