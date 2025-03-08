/** 
 * ZuluSCSI™ - Copyright (c) 2024-2025 Rabbit Hole Computing™
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

#ifdef ZULUSCSI_PICO
# ifdef ZULUSCSI_DAYNAPORT
#   define PLATFORM_NAME "ZuluSCSI Pico DaynaPORT"
# else
#   define PLATFORM_NAME "ZuluSCSI Pico"
# endif
# define PLATFORM_PID "Pico"
# define PLATFORM_REVISION "2.0"
# define PLATFORM_HAS_INITIATOR_MODE 1
# define DISABLE_SWO
# define PLATFORM_MAX_SCSI_SPEED S2S_CFG_SPEED_SYNC_20
# define PLATFORM_DEFAULT_SCSI_SPEED_SETTING 20
#elif defined(ZULUSCSI_PICO_2)
# ifdef ZULUSCSI_PICO_2_DAYNAPORT
#   define PLATFORM_NAME "ZuluSCSI Pico 2 DaynaPORT"
# else
#   define PLATFORM_NAME "ZuluSCSI Pico 2"
# endif
# define PLATFORM_PID "Pico 2"
# define PLATFORM_REVISION "2.3A"
# define PLATFORM_HAS_INITIATOR_MODE 1
# define DISABLE_SWO
# define PLATFORM_MAX_SCSI_SPEED S2S_CFG_SPEED_SYNC_20
# define PLATFORM_DEFAULT_SCSI_SPEED_SETTING 20

#elif defined(ZULUSCSI_BLASTER)
# define PLATFORM_NAME "ZuluSCSI Blaster"
# define PLATFORM_PID "Blaster"
# define PLATFORM_REVISION "2.3B"
# define PLATFORM_HAS_INITIATOR_MODE 1
# define PLATFORM_MAX_SCSI_SPEED S2S_CFG_SPEED_SYNC_20
# define PLATFORM_DEFAULT_SCSI_SPEED_SETTING 20
#elif defined(ZULUSCSI_BS2)
# define PLATFORM_NAME "ZuluSCSI BS2"
# define PLATFORM_PID "BS2"
# define PLATFORM_REVISION "1.0"
# define PLATFORM_MAX_SCSI_SPEED S2S_CFG_SPEED_SYNC_20
# define PLATFORM_DEFAULT_SCSI_SPEED_SETTING 20
#else
# define PLATFORM_NAME "ZuluSCSI RP2040"
# define PLATFORM_PID "RP2040"
# define PLATFORM_REVISION "2.0"
# define PLATFORM_HAS_INITIATOR_MODE 1
# define PLATFORM_MAX_SCSI_SPEED S2S_CFG_SPEED_SYNC_20
# define PLATFORM_DEFAULT_SCSI_SPEED_SETTING 20
#endif

#define PLATFORM_OPTIMAL_MIN_SD_WRITE_SIZE 32768
#define PLATFORM_OPTIMAL_MAX_SD_WRITE_SIZE 65536
#define PLATFORM_OPTIMAL_LAST_SD_WRITE_SIZE 8192
#define SD_USE_SDIO 1
#define PLATFORM_HAS_PARITY_CHECK 1

#ifndef PLATFORM_VDD_WARNING_LIMIT_mV
#define PLATFORM_VDD_WARNING_LIMIT_mV 2800
#endif
