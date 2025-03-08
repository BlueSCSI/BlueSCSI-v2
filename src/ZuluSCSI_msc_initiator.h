/* Initiator mode USB Mass Storage Class connection.
 * This file binds platform-specific MSC routines to the initiator mode
 * SCSI bus interface. The call structure is modeled after TinyUSB, but
 * should be usable with other USB libraries.
 *
 * ZuluSCSI™ - Copyright (c) 2023-2025 Rabbit Hole Computing™
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

#pragma once
#include <stdint.h>

// When true, initiator MSC mode is enabled.
extern bool g_msc_initiator;
bool setup_msc_initiator();
void poll_msc_initiator();

void init_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]);
uint8_t init_msc_get_maxlun_cb(void);
bool init_msc_is_writable_cb (uint8_t lun);
bool init_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject);
bool init_msc_test_unit_ready_cb(uint8_t lun);
void init_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size);
int32_t init_msc_scsi_cb(uint8_t lun, const uint8_t scsi_cmd[16], void *buffer, uint16_t bufsize);
int32_t init_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize);
int32_t init_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize);
void init_msc_write10_complete_cb(uint8_t lun);


