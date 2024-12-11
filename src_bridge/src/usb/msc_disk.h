#include <vector>
#include <stdint.h>
#include <memory>
#include <string>

#pragma once

namespace USB{



    class DiskInfo{
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
            uint8_t deviceType = 0;
            uint8_t inquiry_data[36] = {0};
            uint8_t *ram_disk_buffer = nullptr;

            std::string vendor_id = "TinyUSB";
            std::string product_id = "Mass Storage";
            std::string product_rev = "1.0";

        static std::shared_ptr<DiskInfo> buildFromScsiInquiry(std::vector<uint8_t> inq_data);
        static const int RAM_DISK = -2;
    };

    extern std::vector<std::shared_ptr<DiskInfo>> DiskInfoList;
}