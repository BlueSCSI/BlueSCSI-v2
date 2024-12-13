// Provides a RAM-backed MSC disk that can be connected to the host. This is
// intended to be very small, just big enough for a README.txt or to do
// basic testing.
//
// Copyright (C) 2024 akuker
// Copyright (c) 2019 Ha Thach (tinyusb.org)
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
#include <memory.h>
#include "msc_disk.h"
namespace USB
{

    class MscRamDisk : public MscDisk
    {
    public:
        MscRamDisk(bool is_writable = true);
        ~MscRamDisk() {};
        bool Inquiry(bool refresh_required = false) override
        {
            (void)refresh_required;
            return true;
        }
        bool ReadCapacity(uint32_t *sectorcount, uint32_t *sectorsize) override;
        bool IsWritable() override;
        bool TestUnitReady() override;
        bool RequestSense(uint8_t *sense_key) override;
        bool StartStopUnit(uint8_t power_condition, bool start, bool load_eject) override;
        uint32_t Read10(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t buffersize) override;
        uint32_t Write10(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t buffersize) override;

        static void StaticInit() {}

    protected:
        bool is_writable_;
        static const uint32_t DISK_BLOCK_SIZE = 512;
        static const uint32_t DISK_BLOCK_COUNT = 16; // 8KB is the smallest size that windows allow to mount
        uint8_t ram_disk_[DISK_BLOCK_COUNT][DISK_BLOCK_SIZE];

        static const uint8_t readme_disk_[DISK_BLOCK_COUNT][DISK_BLOCK_SIZE];
    };
}