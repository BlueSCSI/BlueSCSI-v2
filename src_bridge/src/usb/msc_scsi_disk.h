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
        MscScsiDisk(uint8_t id) : MscDisk(id) {}
        ~MscScsiDisk() {};
        bool Inquiry(bool refresh_required = false) override;
        bool ReadCapacity(uint32_t *sectorcount, uint32_t *sectorsize) override;
        bool IsWritable() override;
        bool TestUnitReady() override;
        bool RequestSense(uint8_t *sense_key) override;
        bool StartStopUnit(uint8_t power_condition, bool start, bool load_eject) override;
        uint32_t Read10(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t buffersize) override;
        uint32_t Write10(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t buffersize) override;

        static void StaticInit();

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