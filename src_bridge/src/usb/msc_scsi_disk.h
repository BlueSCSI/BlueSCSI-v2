// Provides a SCSI based MSC disk that can be used by the USB host to read/
// write to the SCSI device.
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

#include <stdint.h>
#include "msc_disk.h"
namespace USB
{

    class MscScsiDisk : public MscDisk
    {
    public:
        // enum peripheral_type : uint8_t
        // {
        //     eDirectAccessDevice = 0x00,
        //     eSequentialAccessDevice = 0x01,
        //     ePrinterDevice = 0x02,
        //     eProcessorDevice = 0x03,
        //     eWriteOnceDevice = 0x04,
        //     eCDRomDevice = 0x05,
        //     eScannerDevice = 0x06,
        //     OpticalMemoryDevice = 0x07,
        //     eMediumChangerDevice = 0x08,
        //     eCommunicationsDevice = 0x09,
        //     eUnknownOrNoDevicType = 0x1F,
        // };
// S2S_CFG_TYPE

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

        enum class status_byte_code :uint8_t{
            eGood = 0x00,
            eCheckCondition = 0x02,
            eConditionMet = 0x04,
            eBusy = 0x08,
            eReservationConflict = 0x0A,
            eCommandTerminated = 0x0C,
            eQueueFull = 0x0E
        };

                    static uint8_t mask_reserved(uint8_t status) { return status & 0b001111110; }


        MscScsiDisk(uint8_t id) : MscDisk(id) {}
        ~MscScsiDisk() {};
        bool Inquiry(bool refresh_required = false) override;
        bool ReadCapacity(uint32_t *sectorcount, uint32_t *sectorsize) override;
        bool IsWritable() override;
        bool TestUnitReady() override;
        int TestUnitReadyStatus(); // Same as TestUnitReady, but returns the status
        bool RequestSense(uint8_t *sense_key) override;
        bool StartStopUnit(uint8_t power_condition, bool start, bool load_eject) override;
        uint32_t Read10(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t buffersize) override;
        uint32_t Write10(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t buffersize) override;

        static void StaticInit();

        const bool removable() const
        {
            return (inquiry_data[1] & 0x80) != 0;
        }
        const uint8_t peripheralDeviceType() const
        {
            return (inquiry_data[0] & 0x1F);
        }
        const uint8_t peripheralQualifier() const
        {
            return (inquiry_data[0] & 0xE0) >> 5;
        }
        const uint8_t ansiVersionSupported() const
        {
            return (inquiry_data[2] & 0x07);
        }
        const uint8_t ecmaVersionSupported() const
        {
            return ((inquiry_data[2] & 0x70) >> 4);
        }

    protected:
        static void ReadConfiguration();
        static uint8_t initiator_id_;
        static uint8_t configured_retry_count_;
        static bool initialization_complete_;
        uint8_t inquiry_data_[36] = {0};

        int RunCommand(
            const uint8_t *command, size_t cmdLen,
            uint8_t *bufIn, size_t bufInLen,
            const uint8_t *bufOut, size_t bufOutLen,
            bool returnDataPhase = false);
    };
}