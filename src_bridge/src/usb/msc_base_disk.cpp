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
#include <stdio.h>
#include <cstring>
#include "msc_disk.h"
#include "tusb.h"

namespace USB
{
  std::vector<std::shared_ptr<MscDisk>> MscDisk::DiskList;

  std::shared_ptr<MscDisk> MscDisk::GetMscDiskByScsiId(uint8_t target_id)
  {
    for (auto cur_disk : DiskList)
    {
      if (cur_disk->target_id == target_id)
      {
        return cur_disk;
      }
    }
    printf("WARNING: GetMscDiskByScsiId target_id %d not found", (int)target_id);
    return nullptr;
  }

  std::shared_ptr<MscDisk> MscDisk::GetMscDiskByLun(uint8_t lun){
    if(DiskList.at(lun)){
      return DiskList.at(lun);
    }
    return nullptr;
  }

  // void MscDisk::Inquiry(uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]);
  // {
  //   // We need to limit the size of the inquiry strings
  //   size_t len = std::min((size_t)vendor_id.size(), (size_t)8);
  //   memcpy(vendor_id, vendor_id.c_str(), len);
  //   len = std::min((size_t)product_id.size(), (size_t)16);
  //   memcpy(product_id, product_id.c_str(), len);
  //   len = std::min((size_t)product_rev.size(), (size_t)4);
  //   memcpy(product_rev, product_rev.c_str(), len);
  // }

  int32_t MscDisk::UnhandledScsiCommand(uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize)
  {
    printf("%s lun %d scsi_cmd %02X %02X %02X %02X %02X %02X %02X\n", __func__, target_id, scsi_cmd[0],
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
      tud_msc_set_sense(target_id, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

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

}