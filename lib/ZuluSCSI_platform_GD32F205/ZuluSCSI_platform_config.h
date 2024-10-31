/** 
 * ZuluSCSI™ - Copyright (c) 2024 Rabbit Hole Computing™
 * 
 * ZuluSCSI™ firmware is licensed under the GPL version 3 or any later version. 
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
**/
#pragma once
#if defined(ZULUSCSI_V1_0)
#   if defined(ZULUSCSI_V1_0_mini)
#       define PLATFORM_NAME "ZuluSCSI mini v1.0"
#   else
#       define PLATFORM_NAME "ZuluSCSI v1.0"
#   endif
#   define PLATFORM_REVISION "1.0"
#   define PLATFORM_MAX_SCSI_SPEED S2S_CFG_SPEED_ASYNC_50
#   include "ZuluSCSI_v1_0_gpio.h"
#else
#   define PLATFORM_NAME "ZuluSCSI v1.1+"
#   define PLATFORM_REVISION "1.1+"
#   define PLATFORM_MAX_SCSI_SPEED S2S_CFG_SPEED_SYNC_10
#   define PLATFORM_OPTIMAL_MIN_SD_WRITE_SIZE 4096
#   define PLATFORM_OPTIMAL_MAX_SD_WRITE_SIZE 65536
#   define PLATFORM_OPTIMAL_LAST_SD_WRITE_SIZE 8192
#   define PLATFORM_VERSION_1_1_PLUS
#   define ZULUSCSI_HARDWARE_CONFIG
#   include "ZuluSCSI_v1_1_gpio.h"
#endif

#ifndef PLATFORM_VDD_WARNING_LIMIT_mV
#define PLATFORM_VDD_WARNING_LIMIT_mV 2800
#endif

#define PLATFORM_DEFAULT_SCSI_SPEED_SETTING 10