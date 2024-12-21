// Provides a SCSI based MSC disk that can be used by the USB host to read/
// write to the SCSI device.
//
// Copyright (C) 2024 akuker
// Copyright (c) 2022 Rabbit Hole Computing
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
#include <stdint.h>
#include "usb_task.h"
#include "msc_scsi_disk.h"
#include <minIni.h>
#include "SdFat.h"
#include "BlueSCSI_config.h"
#include "BlueSCSI_log.h"
#include <string>
#include <memory>

using namespace std;

// TODO: Do something better with these....
#define DEVICE_TYPE_CD 5
#define DEVICE_TYPE_DIRECT_ACCESS 0

extern "C"
{
#include <scsi.h>
}

namespace USB
{

  uint8_t MscScsiDisk::initiator_id_ = 7;
  int MscScsiDisk::configured_retry_count_ = 5;
  bool MscScsiDisk::initialization_complete_ = false;

  /*************************************
   * High level initiator mode logic   *
   *************************************/
  void MscScsiDisk::ReadConfiguration()
  {
    initiator_id_ = ini_getl("SCSI", "InitiatorID", 7, CONFIGFILE);
    if (initiator_id_ > 7)
    {
      log("InitiatorID set to illegal value in, ", CONFIGFILE, ", defaulting to 7");
      initiator_id_ = 7;
    }
    else
    {
      log_f("InitiatorID set to ID %d", initiator_id_);
    }

    configured_retry_count_ = ini_getl("SCSI", "InitiatorMaxRetry", 1, CONFIGFILE);
  }

  void MscScsiDisk::StaticInit()
  {

    ReadConfiguration();
    // // The USB MSC spec supports up to 16 LUNs
    // USB::DiskInfoList.reserve(16);

    int retry_count = 0;
    // We'll keep scanning the SCSI bus until we find at least one device
    while (!initialization_complete_)
    {
      retry_count++;
      scsiHostPhyReset();

      // Scan the SCSI bus to see which devices exist
      for (int target_id = 0; target_id < 8; target_id++)
      {

        // Skip ourselves
        if (target_id == initiator_id_)
        {
          continue;
        }
        printf("%s ** Looking for SCSI ID: %d\n", __func__, target_id);

        SCSI_RELEASE_OUTPUTS();
        SCSI_ENABLE_INITIATOR();
        if (g_scsiHostPhyReset)
        {
          log("Executing BUS RESET after aborted command");
          scsiHostPhyReset();
        }

        auto cur_target = std::make_shared<USB::MscScsiDisk>(target_id);
        cur_target->target_id = target_id;

        LED_ON();
        sense_key_type test_unit_read_sense;
        status_byte_t test_unit_ready_status = cur_target->TestUnitReady(&test_unit_read_sense);

        sense_key_type start_stop_sense;
        status_byte_t start_stop_status = cur_target->StartStopUnit(0, true, false, &start_stop_sense);

        sense_key_type read_capacity_sense;
        status_byte_t read_capacity_status = cur_target->ReadCapacity(&cur_target->sectorcount,
                                                                      &cur_target->sectorsize, &read_capacity_sense);
        sense_key_type inquiry_sense;
        status_byte_t inquiry_status = cur_target->Inquiry(true, &inquiry_sense);
        LED_OFF();

        if (!read_capacity_status.isGood())
        {
          printf("%s Read capacity failed. Skipping SCSI ID %d\n", __func__, target_id);
          continue;
        }
        else if (!start_stop_status.isGood())
        {

          printf("%s SCSI ID %d responds but ReadCapacity command failed\n", __func__, target_id);
          printf("Possibly SCSI-1 drive or removable media? NOT HANDLED YET!!!\n");
          continue;
        }
        else if (!test_unit_ready_status.isGood())
        {
          printf("%s SCSI ID %d responds but TestUnitReady command failed with sense key %d\n", __func__, target_id, test_unit_read_sense);
          continue;
        }

        if (!inquiry_status.isGood())
        {
          printf("Inquiry failed for ID %d\n", target_id);
          continue;
        }

        MscDisk::AddMscDisk(cur_target);
        cur_target->ansiVersion = cur_target->inquiry_data[2] & 0x7;

        log("SCSI ID ", cur_target->target_id,
            " capacity ", (int)cur_target->sectorcount,
            " sectors x ", (int)cur_target->sectorsize, " bytes");
        log_f("SCSI-%d: Vendor: %.8s, Product: %.16s, Version: %.4s",
              cur_target->ansiVersion,
              &cur_target->inquiry_data[8],
              &cur_target->inquiry_data[16],
              &cur_target->inquiry_data[32]);
        cur_target->vendor_id = string((char *)(&cur_target->inquiry_data[8]));
        cur_target->product_id = string((char *)(&cur_target->inquiry_data[16]));
        cur_target->product_rev = string((char *)(&cur_target->inquiry_data[32]));

        // Check for well known ejectable media.
        if (strncmp((char *)(&cur_target->inquiry_data[8]), "IOMEGA", 6) == 0 &&
            strncmp((char *)(&cur_target->inquiry_data[16]), "ZIP", 3) == 0)
        {
          // g_initiator_state.ejectWhenDone = true;
          log("Ejectable media detected!!!!!");
        }

        // ..... I don't think this check is needed. Maybe there will be special handling
        // ..... for very large drives??
        // if (total_bytes >= 0xFFFFFFFF && SD.fatType() != FAT_TYPE_EXFAT)
        //     {
        //         // Note: the FAT32 limit is 4 GiB - 1 byte
        //         log("Image files equal or larger than 4 GiB are only possible on exFAT filesystem");
        //         log("Please reformat the SD card with exFAT format to image this drive.");
        //         g_initiator_state.sectorsize = 0;
        //         g_initiator_state.sectorcount = g_initiator_state.sectorcount_all = 0;
        //     }
        if (cur_target->ansiVersion < 0x02)
        {
          log("SCSI-1 Device Detected at ID %d", cur_target->target_id);
          // this is a SCSI-1 drive, use READ6 and 256 bytes to be safe.
          cur_target->max_sector_per_transfer = 256;
        }

        cur_target->deviceType = cur_target->inquiry_data[0] & 0x1F;
        if ((cur_target->deviceType != DEVICE_TYPE_CD) && (cur_target->deviceType != DEVICE_TYPE_DIRECT_ACCESS))
        {
          log("Unhandled device type: ", cur_target->deviceType, ". Unexpected behavior is likely!!!!.");
        }

        uint64_t total_bytes = (uint64_t)cur_target->sectorcount * cur_target->sectorsize;
        log("Drive total size is ", (int)(total_bytes / (1024 * 1024)), " MiB");

      } // end for each target ID

      if ((MscDisk::DiskList.size() > 0) || (retry_count > configured_retry_count_))
      {
        initialization_complete_ = true;
        g_scsi_setup_complete = true;
      }
      else
      {
        log("No SCSI devices found, retrying...");
        delay(250);
      } // end if(diskInfoList.size() > 0)
    }
  }

  status_byte_t MscScsiDisk::ReadCapacity(uint32_t *sectorcount, uint32_t *sectorsize, sense_key_type *sense_key)
  {
    uint8_t command[10] = {0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t response[8] = {0};
    int status = RunCommand(
        command, sizeof(command),
        response, sizeof(response),
        NULL, 0);

    if (status == 0)
    {
      *sectorcount = ((uint32_t)response[0] << 24) | ((uint32_t)response[1] << 16) | ((uint32_t)response[2] << 8) | ((uint32_t)response[3] << 0);

      *sectorcount += 1; // SCSI reports last sector address

      *sectorsize = ((uint32_t)response[4] << 24) | ((uint32_t)response[5] << 16) | ((uint32_t)response[6] << 8) | ((uint32_t)response[7] << 0);

      return status_byte_t::eGood;
    }
    else if (status == 2)
    {
      sense_key_type sense_key;
      RequestSense(&sense_key);
      log("READ CAPACITY on target ", target_id, " failed, sense key ", sense_key);
      return status_byte_t::eCheckCondition;
    }
    else
    {
      *sectorcount = *sectorsize = 0;
      return status_byte_t::eCommandTerminated;
    }
  }

  bool MscScsiDisk::IsWritable()
  {

    uint8_t command[6] = {0x1A, 0x08, 0, 0, 4, 0}; // MODE SENSE(6)
    uint8_t response[4] = {0};
    RunCommand(command, sizeof(command), response, sizeof(response), NULL, 0);
    return (response[2] & 0x80) == 0; // Check write protected bit
  }

  // Execute INQUIRY command
  status_byte_t MscScsiDisk::Inquiry(bool refresh_required, sense_key_type *sense_key)
  {
    if (!refresh_required && inquiry_data[0] != 0)
    {
      // We already have inquiry data. Don't need to re-poll the drive
      return true;
    }
    uint8_t command[6] = {0x12, 0, 0, 0, 36, 0};
    int status = RunCommand(
        command, sizeof(command),
        inquiry_data, 36,
        NULL, 0);
    // TODO: should set sense_key to something meaningful
    if (sense_key != nullptr)
    {
      *sense_key = eNoError;
    }
    return (status == 0) ? status_byte_t::eGood : status_byte_t::eCheckCondition;
  }

  // // Execute TEST UNIT READY command and handle unit attention state
  // status_byte_t MscScsiDisk::TestUnitReadyStatus(sense_key_type *sense_key)
  // {
  //   uint8_t command[6] = {0x00, 0, 0, 0, 0, 0};
  //   int status = RunCommand(
  //       command, sizeof(command),
  //       NULL, 0,
  //       NULL, 0);
  //   return status;
  // }

  status_byte_t MscScsiDisk::TestUnitReady(sense_key_type *sense_key)
  {
    for (int retries = 0; retries < 2; retries++)
    {
      uint8_t command[6] = {0x00, 0, 0, 0, 0, 0};
      int status = RunCommand(
          command, sizeof(command),
          NULL, 0,
          NULL, 0);

      // If the caller didn't pass in a pointer for sense_key, we
      // should use a local stack variable
      sense_key_type local_sense_key;
      if (sense_key == nullptr)
      {
        sense_key = &local_sense_key;
      }

      if (status == 0)
      {
        return true;
      }
      else if (status == -1)
      {
        // No response to select
        return false;
      }
      else if (status == 2)
      {
        RequestSense(sense_key);

        if (*sense_key == eUnitAttention)
        {
          log("Target ", target_id, " reports UNIT_ATTENTION, running INQUIRY");
          Inquiry();
        }
        else if (*sense_key == eNotReady)
        {
          log("Target ", target_id, " reports NOT_READY, running STARTSTOPUNIT");
          StartStopUnit(0, true, false);
        }
      }
      else
      {
        log("Target ", target_id, " TEST UNIT READY response: ", status);
      }
    }

    return false;
  }

  int MscScsiDisk::RunCommand(
      const uint8_t *command, size_t cmdLen,
      uint8_t *bufIn, size_t bufInLen,
      const uint8_t *bufOut, size_t bufOutLen,
      bool returnDataPhase)
  {
    if (!scsiHostPhySelect(target_id, initiator_id_))
    {
      debuglog("------ Target ", target_id, " did not respond");
      scsiHostPhyRelease();
      return -1;
    }

    SCSI_PHASE phase;
    int status = -1;
    while ((phase = (SCSI_PHASE)scsiHostPhyGetPhase()) != BUS_FREE)
    {
      platform_poll();
      // vTaskDelay(1);

      if (phase == MESSAGE_IN)
      {
        uint8_t dummy = 0;
        scsiHostRead(&dummy, 1);
      }
      else if (phase == MESSAGE_OUT)
      {
        uint8_t identify_msg = 0x80;
        scsiHostWrite(&identify_msg, 1);
      }
      else if (phase == COMMAND)
      {
        scsiHostWrite(command, cmdLen);
      }
      else if (phase == DATA_IN)
      {
        if (returnDataPhase)
          return 0;
        if (bufInLen == 0)
        {
          log("DATA_IN phase but no data to receive!");
          status = -3;
          break;
        }

        if (scsiHostRead(bufIn, bufInLen) == 0)
        {
          log("scsiHostRead failed, tried to read ", (int)bufInLen, " bytes");
          status = -2;
          break;
        }
      }
      else if (phase == DATA_OUT)
      {
        if (returnDataPhase)
          return 0;
        if (bufOutLen == 0)
        {
          log("DATA_OUT phase but no data to send!");
          status = -3;
          break;
        }

        if (scsiHostWrite(bufOut, bufOutLen) < bufOutLen)
        {
          log("scsiHostWrite failed, was writing ", bytearray(bufOut, bufOutLen));
          status = -2;
          break;
        }
      }
      else if (phase == STATUS)
      {
        uint8_t tmp = -1;
        scsiHostRead(&tmp, 1);
        status = tmp;
        debuglog("------ STATUS: ", tmp);
      }
    }

    scsiHostPhyRelease();

    return status;
  }

  // Execute REQUEST SENSE command to get more information about error status
  status_byte_t MscScsiDisk::RequestSense(sense_key_type *sense_key)
  {
    uint8_t command[6] = {0x03, 0, 0, 0, 18, 0};
    uint8_t response[18] = {0};

    int status = RunCommand(
        command, sizeof(command),
        response, sizeof(response),
        NULL, 0);

    if (sense_key != nullptr)
    {
      *sense_key = sense_key_type(response[2] & 0x0F);
    }
    // TODO: eCheckCondition isn't necessarily the right error status. Need to
    // decode this from RunCommand
    return (status == 0) ? status_byte_t::eGood : status_byte_t::eCheckCondition;
  }

  // Execute UNIT START STOP command to load/unload media
  status_byte_t MscScsiDisk::StartStopUnit(uint8_t power_condition, bool start, bool load_eject, sense_key_type *sense_key)
  {
    // uint8_t target_id = disk->target_id;
    uint8_t command[6] = {0x1B, 0x1, 0, 0, 0, 0};
    uint8_t response[4] = {0};

    if (start)
    {
      command[4] |= 1; // Start
      command[1] = 0;  // Immediate
    }
    else // stop
    {
      if (deviceType == DEVICE_TYPE_CD)
      {
        command[4] = 0b00000010; // eject(6), stop(7).
      }
    }

    // TODO: Need to validate this. Didn't see it in the scsi spec
    command[4] |= power_condition << 4;

    int status = RunCommand(
        command, sizeof(command),
        response, sizeof(response),
        NULL, 0);
    // printf("%s status: %d\n", __func__, (int)status);

    if (status == status_byte_t::eCheckCondition)
    {
      // Even if the caller doesn't want the sense key status, we'll pull it ourselves
      sense_key_type local_sense_key;
      if (sense_key == nullptr)
      {
        sense_key = &local_sense_key;
      }
      RequestSense(sense_key);
      log("START STOP UNIT on target ", target_id, " failed, sense key ", sense_key);
      return status_byte_t::eCheckCondition;
    }

    // TODO: eCheckCondition isn't REALLY the correct thing to return here. We need to
    // decypher the real error from RunCommand()
    return (status == 0) ? status_byte_t::eGood : status_byte_t::eCheckCondition;
  }

  uint32_t MscScsiDisk::Read10(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t buffersize)
  {
    int status = -1;
    uint32_t sectorcount = buffersize / sectorsize;
    uint32_t start_sector = lba;
    uint32_t sector_size = sectorsize;

    if (sectorcount == 0)
    {
      log("Read10: invalid sector count: ", sectorcount);
      return 0;
    }
    // // Read6 command supports 21 bit LBA - max of 0x1FFFFF
    // // ref: https://www.seagate.com/files/staticfiles/support/docs/manual/Interface%20manuals/100293068j.pdf pg 134
    // if (g_initiator_state.ansiVersion < 0x02 || (start_sector < 0x1FFFFF && sectorcount <= 256))
    // {
    //     // Use READ6 command for compatibility with old SCSI1 drives
    //     uint8_t command[6] = {0x08,
    //         (uint8_t)(start_sector >> 16),
    //         (uint8_t)(start_sector >> 8),
    //         (uint8_t)start_sector,
    //         (uint8_t)sectorcount,
    //         0x00
    //     };

    //     // Start executing command, return in data phase
    //     status = scsiInitiatorRunCommand(target_id, command, sizeof(command), NULL, 0, NULL, 0, true);
    // }
    // else
    // {
    // Use READ10 command for larger number of blocks
    uint8_t command[10] = {0x28, 0x00,
                           (uint8_t)(start_sector >> 24), (uint8_t)(start_sector >> 16),
                           (uint8_t)(start_sector >> 8), (uint8_t)start_sector,
                           0x00,
                           (uint8_t)(sectorcount >> 8), (uint8_t)(sectorcount),
                           0x00};

    status = RunCommand(command, sizeof(command), (uint8_t *)buffer, buffersize, NULL, 0);
    // }

    if (status != 0)
    {
      sense_key_type sense_key;
      RequestSense(&sense_key);

      log("scsiInitiatorReadDataToFile: READ failed: ", status, " sense key ", (int)sense_key);
      scsiHostPhyRelease();
      return 0;
    }
    return sectorcount * sector_size;
  }

  uint32_t MscScsiDisk::Write10(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t buffersize)
  {
    int status = -1;
    uint32_t sectorcount = buffersize / sectorsize;
    uint32_t start_sector = lba;
    uint32_t sector_size = sectorsize;

    if (sectorcount == 0)
    {
      log("Write10: invalid sector count: ", sectorcount);
      return 0;
    }

    // Use WRITE10 command for larger number of blocks
    uint8_t command[10] = {scsi_command::eCmdWrite10, 0x00,
                           (uint8_t)(start_sector >> 24), (uint8_t)(start_sector >> 16),
                           (uint8_t)(start_sector >> 8), (uint8_t)start_sector,
                           0x00,
                           (uint8_t)(sectorcount >> 8), (uint8_t)(sectorcount),
                           0x00};

    status = RunCommand(command, sizeof(command), NULL, 0, buffer, buffersize);

    if (status != 0)
    {
      sense_key_type sense_key;
      RequestSense(&sense_key);

      log("scsiInitiatorReadDataToFile: WRITE failed: ", status, " sense key ", (int)sense_key);
      scsiHostPhyRelease();
      return 0;
    }
    return sectorcount * sector_size;
  }

  char *MscScsiDisk::toString()
  {
    snprintf(disk_string_, sizeof(disk_string_), "SCSI%d-%d", getAnsiVersion(), getTargetId());
    return disk_string_;
  }

} // end USB namespace