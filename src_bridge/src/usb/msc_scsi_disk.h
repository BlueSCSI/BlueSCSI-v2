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

        // bool is_writable_;
        // uint8_t *msc_disk_;
        // static const uint32_t DISK_BLOCK_SIZE = 512;
        // static const uint32_t DISK_BLOCK_COUNT = 16; // 8KB is the smallest size that windows allow to mount
        // uint8_t *ram_disk_[DISK_BLOCK_COUNT][DISK_BLOCK_SIZE];

        // static const uint8_t readme_disk_[DISK_BLOCK_COUNT][DISK_BLOCK_SIZE];
    };
}