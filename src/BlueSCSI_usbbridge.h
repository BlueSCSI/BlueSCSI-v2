// Main state machine for SCSI initiator mode

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <list>
#include <semphr.h>
#include "scsi2sd.h"
// #ifdef __cplusplus
// extern "C" {
// #endif
// void scsiUsbBridgeInit();
// void scsiUsbBridgeMainLoop(void *param);

using namespace std;

class BlueScsiBridge{





public:
    // BlueScsiBridge() {}
    // ~BlueScsiBridge() {};
    void init();
    // void mainLoop(void *param);
    void mainLoop(void);



private:
    uint8_t initiator_id;
    bool initialization_complete = false;
    uint8_t configured_retry_count;
    

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
    };

    list<DiskInfo*> diskInfoList;

    SemaphoreHandle_t mutex;
    StaticSemaphore_t mutexBuffer;

void UpdateLed();
void DebugPrint();
void ReadConfiguration();

DiskInfo* GetDiskInfo(int target_id);

// Select target and execute SCSI command
int RunCommand(int target_id,
                            const uint8_t *command, size_t cmdLen,
                            uint8_t *bufIn, size_t bufInLen,
                            const uint8_t *bufOut, size_t bufOutLen,
                            bool returnDataPhase = false);

// Execute READ CAPACITY command
bool ReadCapacity(int target_id, uint32_t *sectorcount, uint32_t *sectorsize);

// Execute REQUEST SENSE command to get more information about error status
bool RequestSense(int target_id, uint8_t *sense_key);

// Execute UNIT START STOP command to load/unload media
bool StartStopUnit(int target_id, bool start);

// Execute INQUIRY command
bool Inquiry(int target_id, uint8_t inquiry_data[36]);

// Execute TEST UNIT READY command and handle unit attention state
bool TestUnitReady(int target_id);

//     // Retry information for sector reads.
//     // If a large read fails, retry is done sector-by-sector.
//     int retrycount;
//     uint32_t failposition;
//     bool ejectWhenDone;

//     FsFile target_file;
// } g_initiator_state;
};

// // Select target and execute SCSI command
// int scsiUsbBridgeRunCommand(int target_id,
//                             const uint8_t *command, size_t cmdLen,
//                             uint8_t *bufIn, size_t bufInLen,
//                             const uint8_t *bufOut, size_t bufOutLen,
//                             bool returnDataPhase = false);

// // Execute READ CAPACITY command
// bool scsiUsbBridgeReadCapacity(int target_id, uint32_t *sectorcount, uint32_t *sectorsize);

// // Execute REQUEST SENSE command to get more information about error status
// bool scsiRequestSense(int target_id, uint8_t *sense_key);

// // Execute UNIT START STOP command to load/unload media
// bool scsiStartStopUnit(int target_id, bool start);

// // Execute INQUIRY command
// bool scsiInquiry(int target_id, uint8_t inquiry_data[36]);

// // Execute TEST UNIT READY command and handle unit attention state
// bool scsiTestUnitReady(int target_id);

// Read a block of data from SCSI device and write to file on SD card
// class FsFile;

// bool scsiUsbBridgeReadDataToFile(int target_id, uint32_t start_sector, uint32_t sectorcount, uint32_t sectorsize,
// //                                  FsFile &file);
// #ifdef __cplusplus
// }
// #endif