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

#pragma once

#define MAX_MAC_PATH 32
#define ENTRY_SIZE 40

#define CD_IMG_DIR "CD%d"

#define BLUESCSI_TOOLBOX_COUNT_FILES    0xD2
#define BLUESCSI_TOOLBOX_LIST_FILES     0xD0
#define BLUESCSI_TOOLBOX_GET_FILE       0xD1
#define BLUESCSI_TOOLBOX_SEND_FILE_PREP 0xD3
#define BLUESCSI_TOOLBOX_SEND_FILE_10   0xD4
#define BLUESCSI_TOOLBOX_SEND_FILE_END  0xD5
#define BLUESCSI_TOOLBOX_TOGGLE_DEBUG   0xD6
#define BLUESCSI_TOOLBOX_LIST_CDS       0xD7
#define BLUESCSI_TOOLBOX_SET_NEXT_CD    0xD8
#define BLUESCSI_TOOLBOX_LIST_DEVICES   0xD9
#define BLUESCSI_TOOLBOX_COUNT_CDS      0xDA
#define OPEN_RETRO_SCSI_TOO_MANY_FILES 0x0001