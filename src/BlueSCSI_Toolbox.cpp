/** 
 * Copyright (C) 2023 Eric Helgeson
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
#include "BlueSCSI_Toolbox.h"
#include "BlueSCSI_disk.h"
#include "BlueSCSI_log.h"
#include <minIni.h>
#include <SdFat.h>
extern "C" {
#include <scsi2sd_time.h>
#include <sd.h>
#include <mode.h>
}


static void doCountFiles(const char * dir_name)
{
    File dir;
    File file;
    char name[ MAX_MAC_PATH + 1];
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
        file.getName(name, 32 + 1);
        file.close();
        // only count valid files.
        if(name[0] != '.')
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

void onListFiles(const char * dir_name) {
    File dir;
    File file;

    memset(scsiDev.data, 0, 4096);
    int ENTRY_SIZE = 40;
    char name[MAX_MAC_PATH + 1];
    dir.open(dir_name);
    dir.rewindDirectory();
    uint8_t index = 0;
    byte file_entry[40] = {0};
    while (file.openNext(&dir, O_RDONLY))
    {
        uint8_t isDir = file.isDirectory() ? 0x00 : 0x01;
        file.getName(name, MAX_MAC_PATH + 1);
        uint64_t size = file.fileSize();
        file.close();
        if(name[0] == '.')
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

File get_file_from_index(uint8_t index, const char * dir_name)
{
  File dir;
  FsFile file_test;
  char name[MAX_MAC_PATH + 1];

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
    file_test.getName(name, 32 + 1);

    if(name[0] == '.')
      continue;
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
void onListDevices()
{
  for (int i = 0; i < NUM_SCSIID; i++)
  {
    const S2S_TargetCfg* cfg = s2s_getConfigById(i);
    // id, type
    if (cfg && (cfg->scsiId & S2S_CFG_TARGET_ENABLED))
    {
        scsiDev.data[i] = (int)cfg->deviceType; // 2 == cd
    }
    else
    {
        scsiDev.data[i] = 255; // not enabled target.
    }
  }
  scsiDev.dataLen = 16;
}

void onSetNextCD()
{
    char name[MAX_FILE_PATH];
    char full_path[MAX_FILE_PATH * 2];

    uint8_t file_index = scsiDev.cdb[1];
    uint8_t cd_scsi_id = scsiDev.cdb[2];
    char section[6] = "SCSI0";
    char key[5] = "IMG0";
    section[4] = '0' + cd_scsi_id;

    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    debuglog("img.image_index: ", img.image_index);
    int next_image_id = img.image_index + 1;

    if(img.image_index == 0)
    { // have to check if we have a 0, if not set the current image to 0 so we can cycle back.
        ini_gets(section, key, "", name, sizeof(name), CONFIGFILE);
        if(strlen(name) == 0)
        {
            img.file.getName(name, MAX_FILE_PATH);
            log("Nothing in IMG0, so put the current image in it: ", name);
            ini_puts(section, key, name, CONFIGFILE);
        }
        else
        {
            log("Something  in IMG0: ", name);
        }
    }

    debuglog("next_image_id: ", next_image_id);
    if(next_image_id > 9)
        next_image_id = 0;

    key[3] = '0' + next_image_id;
    File next_cd = get_file_from_index(file_index, CD_IMG_DIR);
    if(!next_cd.isFile())
    {
        log("not a file?");
        next_cd.close();
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        //SCSI_ASC_INVALID_FIELD_IN_CDB
        scsiDev.phase = STATUS;
        return;
    }
    next_cd.getName(name, sizeof(name));
    next_cd.close();
    snprintf(full_path, (MAX_FILE_PATH * 2), "%s/%s", CD_IMG_DIR, name);
    debuglog("full_path: ", full_path);
    debuglog("key: ", key, " section: ", section);
    // If file doesnt exist, ini_puts() will create it.
    if(!ini_puts(section, key, full_path, CONFIGFILE))
    {
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        //SCSI_ASC_INVALID_FIELD_IN_CDB
        scsiDev.phase = STATUS;
        return;
    }
}

File gFile; // global so we can keep it open while transfering.
void onGetFile10(void) {
    uint8_t index = scsiDev.cdb[1];

    uint32_t offset = ((uint32_t)scsiDev.cdb[2] << 24) | ((uint32_t)scsiDev.cdb[3] << 16) | ((uint32_t)scsiDev.cdb[4] << 8) | scsiDev.cdb[5];

    if (offset == 0) // first time, open the file.
    {
        gFile = get_file_from_index(index, "/shared");
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
File receveFile;
void onSendFilePrep(void)
{
    char file_name[32+1];
    memset(file_name, '\0', 32+1);
    scsiEnterPhase(DATA_OUT);
    for (int i = 0; i < 32+1; ++i)
    {
        file_name[i] = scsiReadByte();
    }
    SD.chdir("/shared");
    receveFile.open(file_name, FILE_WRITE);
    SD.chdir("/");
    if(receveFile.isOpen() && receveFile.isWritable())
    {
        receveFile.rewind();
        receveFile.sync();
        // do i need to manually set phase to status here?
        return;
    } else {
        receveFile.close();
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        //SCSI_ASC_INVALID_FIELD_IN_CDB
        scsiDev.phase = STATUS;
    }
}

void onSendFileEnd(void)
{
    receveFile.sync();
    receveFile.close();
    scsiDev.phase = STATUS;
}

void onSendFile10(void)
{
    if(!receveFile.isOpen() || !receveFile.isWritable())
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
    receveFile.seekCur(offset * 512);
    receveFile.write(buf, buf_size);
    if(receveFile.getWriteError())
    {
        receveFile.clearWriteError();
        receveFile.close();
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
    }
    //scsiDev.phase = STATUS;
}
void onToggleDebug()
{
    if(g_log_debug)
    {
        debuglog("Turning Debug logs off.");
    }
    else
    {
        log("Turning Debug logs on.");
    }
    g_log_debug = !g_log_debug;
    scsiDev.phase = STATUS;
}

extern "C" int scsiBlueSCSIToolboxCommand()
{
    int commandHandled = 1;
    uint8_t command = scsiDev.cdb[0];

     if (unlikely(command == BLUESCSI_TOOLBOX_COUNT_FILES))
    {
        doCountFiles("/shared");
    }
    else if (unlikely(command == BLUESCSI_TOOLBOX_LIST_FILES))
    {
        // TODO: Allow user to set dir name via ini
        onListFiles("/shared");
    }
    else if (unlikely(command == BLUESCSI_TOOLBOX_GET_FILE))
    {
        onGetFile10();
    }
    else if (unlikely(command == BLUESCSI_TOOLBOX_SEND_FILE_PREP))
    {
        onSendFilePrep();
    }
    else if (unlikely(command == BLUESCSI_TOOLBOX_SEND_FILE_10))
    {
        onSendFile10();
    }
    else if (unlikely(command == BLUESCSI_TOOLBOX_SEND_FILE_END))
    {
        onSendFileEnd();
    }
    else if(unlikely(command == BLUESCSI_TOOLBOX_TOGGLE_DEBUG))
    {
        onToggleDebug();
    }
    else if(unlikely(command == BLUESCSI_TOOLBOX_LIST_CDS))
    {
       onListFiles("/CDImages");
    }
    else if(unlikely(command == BLUESCSI_TOOLBOX_SET_NEXT_CD))
    {
       onSetNextCD();
    }
    else if(unlikely(command == BLUESCSI_TOOLBOX_LIST_DEVICES))
    {
        onListDevices();
    }
    else if (unlikely(command == BLUESCSI_TOOLBOX_COUNT_CDS))
    {
        // TODO: Allow user to set dir name via ini
        doCountFiles("/CDImages");
    }
    else
    {
        commandHandled = 0;
    }

    return commandHandled;
}