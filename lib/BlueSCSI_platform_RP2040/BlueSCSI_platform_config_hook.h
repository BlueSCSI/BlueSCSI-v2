
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

#pragma once
#include "BlueSCSI_disk.h"

#define PLATFORM_CONFIG_HOOK(X) platformConfigHook(X);
void platformConfigHook(image_config_t *img);

// Macintosh Device image constants
#define MACINTOSH_SCSI_DRIVER_OFFSET 18
#define MACINTOSH_SCSI_DRIVER_SIZE_OFFSET MACINTOSH_SCSI_DRIVER_OFFSET + 4
#define MACINTOSH_BLOCK_SIZE 512
#define MACINTOSH_SCSI_DRIVER_MAX_SIZE 64 * MACINTOSH_BLOCK_SIZE // 32768
#define LIDO_SIG_OFFSET 24