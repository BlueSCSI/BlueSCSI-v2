// Call-backs for TinyUSB to interface with the SCSI disks
//
// Copyright (C) 2024 akuker
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along
// with this program. If not, see <https://www.gnu.org/licenses/>.

#include <algorithm>
#include "bsp/board_api.h"
#include "tusb.h"

#include "msc_disk.h"
#include "msc_scsi_disk.h"
#include "msc_ram_disk.h"

bool g_scsi_msc_mode;
bool g_disable_usb_cdc;

#define VERBOSE 0

namespace USB
{
  // Note: USB supports a maximum of 16 LUNs
  std::vector<std::shared_ptr<USB::MscDisk>> DiskList();
}

void msc_disk_init(void)
{
  USB::MscScsiDisk::StaticInit();
  USB::MscRamDisk::StaticInit();

  // If there was no SCSI device detected, create a RAM disk
  // with a README.txt with further details.
  if (USB::MscDisk::DiskList.size() < 1)
  {
    printf("SCSI bus scan failed. Adding RAM Disk");
    auto ram_disk = std::make_shared<USB::MscRamDisk>();
    USB::MscDisk::DiskList.push_back(ram_disk);
  }

  // TEMPORARY
  auto ram_disk2 = std::make_shared<USB::MscRamDisk>();
  USB::MscDisk::DiskList.push_back(ram_disk2);
}

// Invoked to determine max LUN
uint8_t tud_msc_get_maxlun_cb(void)
{
  printf("%s size: %d\n", __func__, USB::MscDisk::DiskList.size());
  return USB::MscDisk::DiskList.size();
}

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
#if VERBOSE
  printf("%s lun %d\n", __func__, lun);
#endif
  std::shared_ptr<USB::MscDisk> disk = USB::MscDisk::GetMscDiskByLun(lun);
  if (disk == nullptr)
  {
    return;
  }

  disk->Inquiry();

  // We need to limit the size of the inquiry strings
  size_t len = std::min((size_t)disk->getVendorId().size(), (size_t)8);
  memcpy(vendor_id, disk->getVendorId().c_str(), len);
  len = std::min((size_t)disk->getProductId().size(), (size_t)16);
  memcpy(product_id, disk->getProductId().c_str(), len);
  len = std::min((size_t)disk->getProductRev().size(), (size_t)4);
  memcpy(product_rev, disk->getProductRev().c_str(), len);
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
#if VERBOSE
  printf("%s lun %d\n", __func__, lun);
#endif
  auto disk = USB::MscDisk::GetMscDiskByLun(lun);
  if (disk == nullptr)
  {
    return false;
  }
  return disk->TestUnitReady();
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size)
{

  std::shared_ptr<USB::MscDisk> disk = USB::MscDisk::GetMscDiskByLun(lun);
  if (disk == nullptr)
  {
    return;
  }

#if VERBOSE
  printf("%s sectorcount %d sectorsize %d\n", __func__, (int)diskInfo->sectorcount, (int)diskInfo->sectorsize);
#endif

  *block_count = disk->sectorcount;
  *block_size = disk->sectorsize;
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
#if VERBOSE
  printf("%s lun %d power_condition %d start %d load_eject %d\n", __func__, lun, power_condition, start, load_eject);
#endif

  std::shared_ptr<USB::MscDisk> disk = USB::MscDisk::GetMscDiskByLun(lun);
  if (disk == nullptr)
  {
    return false;
  }

  bool startstopok = disk->StartStopUnit(power_condition, start, load_eject);
  return startstopok;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
#if VERBOSE
  printf("%s lun %d lba %d offset %d bufsize %d\n", __func__, lun, (int)lba, (int)offset, (int)bufsize);
#endif

  std::shared_ptr<USB::MscDisk> disk = USB::MscDisk::GetMscDiskByLun(lun);
  if (disk == nullptr)
  {
    return 0;
  }
  uint32_t read_bytes = disk->Read10(lba, offset, (uint8_t *)buffer, bufsize);
  return (int32_t)read_bytes;
}

bool tud_msc_is_writable_cb(uint8_t lun)
{
#if VERBOSE
  printf("%s lun %d \n", __func__, lun);
#endif

  std::shared_ptr<USB::MscDisk> disk = USB::MscDisk::GetMscDiskByLun(lun);
  if (disk == nullptr)
  {
    return false;
  }

  return disk->IsWritable();
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
#if VERBOSE
  printf("%s lun %d lba %d offset %d bufsize %d\n", __func__, lun, (int)lba, (int)offset, (int)bufsize);
#endif

  std::shared_ptr<USB::MscDisk> disk = USB::MscDisk::GetMscDiskByLun(lun);
  if (disk == nullptr)
  {
    return false;
  }

  static int counter = 0;
  if (++counter > 100)
  {
    printf("%s lba %d offset %d bufsize %d\n", __func__, (int)lba, (int)offset, (int)bufsize);
    counter = 0;
  }
  uint32_t written_bytes = disk->Write10(lba, offset, buffer, bufsize);
  return (int32_t)written_bytes;
}

// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks (MUST not be handled here)
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize)
{
  printf("%s lun %d scsi_cmd %02X %02X %02X %02X %02X %02X %02X\n", __func__, lun, scsi_cmd[0],
         scsi_cmd[1], scsi_cmd[2], scsi_cmd[3], scsi_cmd[4], scsi_cmd[5], scsi_cmd[6]);

  std::shared_ptr<USB::MscDisk> disk = USB::MscDisk::GetMscDiskByLun(lun);
  if (disk == nullptr)
  {
    return 0;
  }
  void const *response = NULL;
  int32_t resplen = 0;

  // most scsi handled is input
  bool in_xfer = true;

  switch (scsi_cmd[0])
  {
  default:
    printf("%s Invalid SCSI Command %02X\n", __func__, scsi_cmd[0]);
    // Set Sense = Invalid Command Operation
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

    // negative means error -> tinyusb could stall and/or response with failed status
    return -1;
  }

  // return resplen must not larger than bufsize
  if (resplen > bufsize)
    resplen = bufsize;

  if (response && (resplen > 0))
  {
    if (in_xfer)
    {
      memcpy(buffer, response, (size_t)resplen);
    }
    else
    {
      // SCSI output
    }
  }

  return resplen;
}
