#include <stdint.h>


class MscExampleDisk
{
public:
    static const uint32_t DISK_BLOCK_NUM = 16; // 8KB is the smallest size that windows allow to mount
    static const uint32_t DISK_BLOCK_SIZE = 512;
    static uint8_t msc_disk0[DISK_BLOCK_NUM][DISK_BLOCK_SIZE];

    static uint8_t msc_disk1[DISK_BLOCK_NUM][DISK_BLOCK_SIZE];
};
