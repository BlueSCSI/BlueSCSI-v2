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

#ifdef LIB_FREERTOS_KERNEL
#include <cstring>
#endif
#include "minIni.h"
#include "BlueSCSI_config.h"

int getBlockSize(char *filename, int scsiId, int default_size)
{
  char section[6] = "SCSI0";
  section[4] = '0' + scsiId;

  default_size = ini_getl(section, "BlockSize", default_size, CONFIGFILE);
  // Parse block size (HD00_NNNN)
  const char *blksize = strchr(filename, '_');
  if (blksize)
  {
    int blktmp = strtoul(blksize + 1, NULL, 10);
    if (blktmp == 256 || blktmp == 512 || blktmp == 1024 ||
        blktmp == 2048 || blktmp == 4096 || blktmp == 8192)
    {
      return blktmp;
    }
  }
  return default_size;
}

int getImgDir(int scsiId, char* dirname)
{
  char section[6] = "SCSI0";
  section[4] = '0' + scsiId;

  char key[] = "ImgDir";
  int dirlen = ini_gets(section, key, "", dirname, sizeof(dirname), CONFIGFILE);
  return dirlen;
}


int getImg(int scsiId, int img_index, char* filename)
{
  char section[6] = "SCSI0";
  section[4] = '0' + scsiId;

  char key[] = "IMG0";
  key[3] = '0' + img_index;

  int dirlen = ini_gets(section, key, "", filename, sizeof(filename), CONFIGFILE);
  return dirlen;
}

int getToolBoxSharedDir(char * dir_name)
{
  return ini_gets("SCSI", "ToolBoxSharedDir", "/shared", dir_name, MAX_FILE_PATH, CONFIGFILE);
}