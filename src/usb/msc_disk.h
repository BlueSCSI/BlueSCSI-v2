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

#include <vector>
#include <stdint.h>
#include <memory>
#include <string>
#include "scsi2sd.h"

#pragma once

extern "C"
{
    extern bool g_scsi_msc_mode;
    extern bool g_disable_usb_cdc;
    void msc_disk_init(void);
    void msc_disk_register_cli(void);
}

namespace USB
{

    enum sense_key_type : uint8_t
    {
        eNoSense = 0x00,
        eRecoveredError = 0x01,
        eNotReady = 0x02,
        eMediumError = 0x03,
        eHardwareError = 0x04,
        eIllegalRequest = 0x05,
        eUnitAttention = 0x06,
        eDataProtect = 0x07,
        eBlankCheck = 0x08,
        eVendorSpecific = 0x09,
        eCopyAborted = 0x0A,
        eAbortedCommand = 0x0B,
        eVolumeOverflow = 0x0D,
        eMiscompare = 0x0E,
        eReserved = 0x0F
    };
    // When the operation succeeds, there won't be a sense key
    static const sense_key_type eNoError = eNoSense;

    class status_byte_t
    {
    public:
        enum status_byte_type : uint8_t
        {
            eGood = 0x00,
            eCheckCondition = 0x02,
            eConditionMet = 0x04,
            eBusy = 0x08,
            eIntermediate = 0x10,
            eIntermediateConditionMet = 0x14,
            eReservationConflict = 0x0A,
            eCommandTerminated = 0x0C,
            eQueueFull = 0x0E
        };

        status_byte_t(uint8_t status)
        {
            this->status = mask_reserved(status);
        }
        status_byte_t(status_byte_type status)
        {
            this->status = status;
        }

        inline bool isGood() const { return status == eGood; }
        inline bool isCheckCondition() const { return status == eCheckCondition; }
        inline bool isConditionMet() const { return status == eConditionMet; }
        inline bool isBusy() const { return status == eBusy; }
        inline bool isReservationConflict() const { return status == eReservationConflict; }
        inline bool isCommandTerminated() const { return status == eCommandTerminated; }
        inline bool isQueueFull() const { return status == eQueueFull; }

    private:
        uint8_t status;

    protected:
        static status_byte_type mask_reserved(uint8_t in_status)
        {
            // Bits 0, 6 and 7 are "reserved" (not used). This
            // will mask them out.
            return (status_byte_type)(in_status & 0b001111110);
        }
    };

    class MscDisk
    {
    public:
        // Information about drive
        int target_id = -1;
        uint32_t sectorsize = 0;
        uint32_t sectorcount = 0;
        uint32_t sectorcount_all = 0;
        uint32_t sectors_done = 0;
        uint32_t max_sector_per_transfer = 512;
        uint32_t badSectorCount = 0;
        uint8_t ansiVersion = 0;
        uint8_t maxRetryCount = 0;
        int deviceType = (int)S2S_CFG_FIXED;
        uint8_t inquiry_data[36] = {0};

        std::string vendor_id = "TinyUSB";
        std::string product_id = "Mass Storage";
        std::string product_rev = "1.0";

        const std::string getVendorId() const
        {
            return vendor_id;
        }
        const std::string getProductId() const
        {
            return product_id;
        }
        const std::string getProductRev() const
        {
            return product_rev;
        }
        const int getSectorSize() const {
            return sectorsize;
        }
        const int getSectorCount() const {
            return sectorcount;
        }
        const int getAnsiVersion() const {
            return ansiVersion;
        }
        const int getTargetId() const {
            return target_id;
        }
        const int getTotalSize() const {
            return sectorcount * sectorsize;
        }

        static const int RAM_DISK = -2;
        virtual ~MscDisk() = default;

        virtual status_byte_t Inquiry(bool refresh_required = false, sense_key_type *sense_key=nullptr) = 0;
        virtual status_byte_t ReadCapacity(uint32_t *sectorcount, uint32_t *sectorsize, sense_key_type *sense_key=nullptr) = 0;
        virtual bool IsWritable() = 0;
        virtual status_byte_t TestUnitReady(sense_key_type *sense_key = nullptr) = 0;
        virtual status_byte_t RequestSense(sense_key_type *sense_key = nullptr) = 0;
        virtual status_byte_t StartStopUnit(uint8_t power_condition, bool start, bool load_eject, sense_key_type *sense_key = nullptr) = 0;
        virtual uint32_t Read10(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t buffersize) = 0;
        virtual uint32_t Write10(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t buffersize) = 0;

        int32_t UnhandledScsiCommand(uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize);

         bool IsRamDisk() const {return (target_id == RAM_DISK);}
        virtual  char* toString() = 0;

        static std::shared_ptr<USB::MscDisk> GetMscDiskByScsiId(uint8_t target_id);
        static std::shared_ptr<USB::MscDisk> GetMscDiskByLun(uint8_t lun);
        static void AddMscDisk(std::shared_ptr<USB::MscDisk> disk)
        {
            DiskList.push_back(disk);
        }
        static std::vector<std::shared_ptr<MscDisk>> DiskList;

    protected:
        // Constructor can't be called directly, since this is an abstract class
        explicit MscDisk(uint8_t id) : target_id(id)
        {
        }
    };
}
