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

// TODO: This should be moved into the core BlueSCSI functionality. Its not
// MSC specific
typedef enum {
    eCmdTestUnitReady  = 0x00,
    eCmdRezero         = 0x01,
    eCmdRequestSense   = 0x03,
    eCmdFormatUnit     = 0x04,
    eCmdReassignBlocks = 0x07,
    eCmdRead6          = 0x08,
    // Bridge specific command
    eCmdGetMessage10 = 0x08,
    // DaynaPort specific command
    eCmdRetrieveStats = 0x09,
    eCmdWrite6        = 0x0A,
    // Bridge specific ommand
    eCmdSendMessage10 = 0x0A,
    eCmdPrint         = 0x0A,
    eCmdSeek6         = 0x0B,
    // DaynaPort specific command
    eCmdSetIfaceMode = 0x0C,
    // DaynaPort specific command
    eCmdSetMcastAddr = 0x0D,
    // DaynaPort specific command
    eCmdEnableInterface            = 0x0E,
    eCmdSynchronizeBuffer          = 0x10,
    eCmdInquiry                    = 0x12,
    eCmdModeSelect6                = 0x15,
    eCmdReserve6                   = 0x16,
    eCmdRelease6                   = 0x17,
    eCmdModeSense6                 = 0x1A,
    eCmdStartStop                  = 0x1B,
    eCmdStopPrint                  = 0x1B,
    eCmdSendDiagnostic             = 0x1D,
    eCmdPreventAllowMediumRemoval  = 0x1E,
    eCmdReadCapacity10             = 0x25,
    eCmdRead10                     = 0x28,
    eCmdWrite10                    = 0x2A,
    eCmdSeek10                     = 0x2B,
    eCmdVerify10                   = 0x2F,
    eCmdSynchronizeCache10         = 0x35,
    eCmdReadDefectData10           = 0x37,
    eCmdReadLong10                 = 0x3E,
    eCmdWriteLong10                = 0x3F,
    eCmdReadToc                    = 0x43,
    eCmdGetEventStatusNotification = 0x4A,
    eCmdModeSelect10               = 0x55,
    eCmdModeSense10                = 0x5A,
    eCmdRead16                     = 0x88,
    eCmdWrite16                    = 0x8A,
    eCmdVerify16                   = 0x8F,
    eCmdSynchronizeCache16         = 0x91,
    eCmdReadCapacity16_ReadLong16  = 0x9E,
    eCmdWriteLong16                = 0x9F,
    eCmdReportLuns                 = 0xA0
} scsi_command;


namespace USB
{

    class MscScsiDisk : public MscDisk
    {
    public:


        MscScsiDisk(uint8_t id) : MscDisk(id) {}
        ~MscScsiDisk() {};
        status_byte_t Inquiry(bool refresh_required = false, sense_key_type *sense_key=nullptr) override;
        status_byte_t ReadCapacity(uint32_t *sectorcount, uint32_t *sectorsize, sense_key_type *sense_key = nullptr) override;
        bool IsWritable() override;
        status_byte_t TestUnitReady(sense_key_type *sense_key=nullptr) override;
        status_byte_t RequestSense(sense_key_type *sense_key=nullptr) override;
        status_byte_t StartStopUnit(uint8_t power_condition, bool start, bool load_eject, sense_key_type *sense_key=nullptr) override;
        uint32_t Read10(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t buffersize) override;
        uint32_t Write10(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t buffersize) override;
        char* toString() override;

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
        static int configured_retry_count_;
        static bool initialization_complete_;
        uint8_t inquiry_data_[36] = {0};
        char disk_string_[16] = {0};

        int RunCommand(
            const uint8_t *command, size_t cmdLen,
            uint8_t *bufIn, size_t bufInLen,
            const uint8_t *bufOut, size_t bufOutLen,
            bool returnDataPhase = false);
    };
}