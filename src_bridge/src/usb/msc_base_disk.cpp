// Defines the base characteristics of a disk that is attached to TinyUSB
// which uses the MSC (Mass Storage Class) USB interface to communicate
// with the host.
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

  std::shared_ptr<MscDisk> MscDisk::GetMscDiskByLun(uint8_t lun)
  {
    if (DiskList.size() <= lun)
    {
      printf("WARNING: GetMscDiskByLun lun %d not found", (int)lun);
      return nullptr;
    }
    return DiskList[lun];
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