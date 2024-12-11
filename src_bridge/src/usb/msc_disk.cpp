/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "bsp/board_api.h"
#include "msc_example_disk.h"
#include "tusb.h"
#include <algorithm>
#include "BlueSCSI_usbbridge.h"

#include "msc_disk.h"

#define VERBOSE 0

namespace USB
{
  std::vector<std::shared_ptr<USB::DiskInfo>> DiskInfoList;
}

#if CFG_TUD_MSC

// When button is pressed, LUN1 will be set to not ready to simulate
// medium not present (e.g SD card removed)

// Some MCU doesn't have enough 8KB SRAM to store the whole disk
// We will use Flash as read-only disk with board that has
// CFG_EXAMPLE_MSC_READONLY defined
#if defined(CFG_EXAMPLE_MSC_READONLY) || defined(CFG_EXAMPLE_MSC_DUAL_READONLY)
#define MSC_CONST const
#else
#define MSC_CONST
#endif

enum
{
  DISK_BLOCK_NUM = 16, // 8KB is the smallest size that windows allow to mount
  DISK_BLOCK_SIZE = 512
};

// Invoked to determine max LUN
uint8_t tud_msc_get_maxlun_cb(void)
{
  printf("%s size: %d\n", __func__, USB::DiskInfoList.size());
  return USB::DiskInfoList.size();
  // return 2; // dual LUN
}

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
  #if VERBOSE
  printf("%s lun %d\n", __func__, lun);
  #endif
  if (lun >= USB::DiskInfoList.size())
  {
    printf("%s invalid lun requested %d\n", __func__, lun);
    return;
  }
  std::shared_ptr<USB::DiskInfo> diskInfo = USB::DiskInfoList[lun];

  // We need to limit the size of the inquiry strings
  size_t len = std::min((size_t)diskInfo->vendor_id.size(), (size_t)8);
  memcpy(vendor_id, diskInfo->vendor_id.c_str(), len);
  len = std::min((size_t)diskInfo->product_id.size(), (size_t)16);
  memcpy(product_id, diskInfo->product_id.c_str(), len);
  len = std::min((size_t)diskInfo->product_rev.size(), (size_t)4);
  memcpy(product_rev, diskInfo->product_rev.c_str(), len);

  // (void) lun; // use same ID for both LUNs

  // const char vid[] = "TinyUSB";
  // const char pid[] = "Mass Storage";
  // const char rev[] = "1.0";

  // memcpy(vendor_id  , vid, strlen(vid));
  // memcpy(product_id , pid, strlen(pid));
  // memcpy(product_rev, rev, strlen(rev));
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
  #if VERBOSE
  printf("%s lun %d\n", __func__, lun);
  #endif
  if (lun >= USB::DiskInfoList.size())
  {
    printf("%s invalid lun requested %d\n", __func__, lun);
    return false;
  }
  std::shared_ptr<USB::DiskInfo> diskInfo = USB::DiskInfoList[lun];

  // If this is a ram disk, its always read
  if (diskInfo->target_id == USB::DiskInfo::RAM_DISK)
  {
    return true;
  }
  else
  {
    bool test_result = BlueScsiBridge::TestUnitReady(diskInfo->target_id);
    printf("\t%s test_result %d\n", __func__, test_result);
    return test_result;
  }
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size)
{

#if VERBOSE
  printf("%s lun %d\n", __func__, lun);
  #endif
  if (lun >= USB::DiskInfoList.size())
  {
    printf("\t%s invalid lun requested %d\n", __func__, lun);
    return;
  }
  std::shared_ptr<USB::DiskInfo> diskInfo = USB::DiskInfoList[lun];
  printf("\t%s sectorcount %d sectorsize %d\n", __func__, (int)diskInfo->sectorcount, (int)diskInfo->sectorsize);
  *block_count = diskInfo->sectorcount;
  *block_size = diskInfo->sectorsize;
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
  #if VERBOSE
  printf("%s lun %d power_condition %d start %d load_eject %d\n", __func__, lun, power_condition, start, load_eject);
  #endif
  if (lun >= USB::DiskInfoList.size())
  {
    printf("\t%s invalid lun requested %d\n", __func__, lun);
    return false;
  }
  std::shared_ptr<USB::DiskInfo> diskInfo = USB::DiskInfoList[lun];

  // If this is a ram disk, just return true
  if (diskInfo->target_id == USB::DiskInfo::RAM_DISK)
  {
    return true;
  }
  else
  {
    bool startstopok = BlueScsiBridge::StartStopUnit(lun, power_condition, start, load_eject);
    printf("\t%s startstopok %d\n", __func__, startstopok);
    return startstopok;
  }
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
  #if VERBOSE
  printf("%s lun %d lba %d offset %d bufsize %d\n", __func__, lun, (int)lba, (int)offset, (int)bufsize);
  #endif
  if (lun >= USB::DiskInfoList.size())
  {
    printf("\t%s invalid lun requested %d\n", __func__, lun);
    return false;
  }
  std::shared_ptr<USB::DiskInfo> diskInfo = USB::DiskInfoList[lun];

  if (diskInfo->target_id == USB::DiskInfo::RAM_DISK)
  {
    // out of ramdisk
    if (lba >= diskInfo->sectorcount)
      return -1;

    uint8_t const *addr = &diskInfo->ram_disk_buffer[lba] + offset;
    memcpy(buffer, addr, bufsize);

    return (int32_t)bufsize;
  }
  else
  {
    // printf("\t%s lba %d offset %d bufsize %d\n", __func__, (int)lba, (int)offset, (int)bufsize);
    uint32_t read_bytes = BlueScsiBridge::Read10(lun, lba, offset, (uint8_t *)buffer, bufsize);
    // for(int i=0; i<bufsize; i++){
    //   printf("%02x ", ((uint8_t *)buffer)[i]);
    //   if(i%16==15) printf("\n");
    //   if(i%10)vTaskDelay(1);
    // }
    printf("\t%s read_bytes %d\n", __func__, (int)read_bytes);
    return (int32_t)read_bytes;
  }
}

bool tud_msc_is_writable_cb(uint8_t lun)
{
  #if VERBOSE
  printf("%s lun %d \n", __func__, lun);
  #endif

 if (lun >= USB::DiskInfoList.size())
  {
    printf("\t%s invalid lun requested %d\n", __func__, lun);
    return false;
  }
  std::shared_ptr<USB::DiskInfo> diskInfo = USB::DiskInfoList[lun];

  if (diskInfo->target_id == USB::DiskInfo::RAM_DISK)
  {
    // The RAM disk can always be writable
    return true;
  }
  else{
    return BlueScsiBridge::IsWritable(lun);
  }
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
  #if VERBOSE
  printf("%s lun %d lba %d offset %d bufsize %d\n", __func__, lun, (int)lba, (int)offset, (int)bufsize);
  #endif

 if (lun >= USB::DiskInfoList.size())
  {
    printf("\t%s invalid lun requested %d\n", __func__, lun);
    return false;
  }
  std::shared_ptr<USB::DiskInfo> diskInfo = USB::DiskInfoList[lun];

  if(diskInfo->target_id == USB::DiskInfo::RAM_DISK){
    // out of ramdisk
    if (lba >= diskInfo->sectorcount){
        printf("\t%s invalid lba requested %d\n", __func__, (int)lba);
      return -1;
    }

    uint8_t *addr = &diskInfo->ram_disk_buffer[lba] + offset;
    memcpy(addr, buffer, bufsize);

    return (int32_t)bufsize;
  }
  else{
    static int counter = 0;
    if(++counter > 100){
      printf("%s lba %d offset %d bufsize %d\n", __func__, (int)lba, (int)offset, (int)bufsize);
      counter = 0;
    }
    //SCSI Disk
    uint32_t written_bytes = BlueScsiBridge::Write10(lun, lba, offset, buffer, bufsize);
    // printf("\t%s written_bytes %d\n", __func__, (int)written_bytes);
    return (int32_t)written_bytes;
  }
}

// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks (MUST not be handled here)
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize)
{
  printf("%s lun %d scsi_cmd %02X %02X %02X %02X %02X %02X %02X\n", __func__, lun, scsi_cmd[0], 
    scsi_cmd[1], scsi_cmd[2], scsi_cmd[3], scsi_cmd[4], scsi_cmd[5], scsi_cmd[6]);
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

#endif