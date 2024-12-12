// // Main state machine for SCSI initiator mode

// #pragma once

// #include <stdint.h>
// #include <stdlib.h>
// #include <list>
// #include "FreeRTOS.h"
// #include <semphr.h>
// #include "scsi2sd.h"
// #include <memory>
// // #include "usb/msc_disk.h"
// // #ifdef __cplusplus
// // extern "C" {
// // #endif
// // void scsiUsbBridgeInit();
// // void scsiUsbBridgeMainLoop(void *param);

// using namespace std;

// class BlueScsiBridge
// {

// public:
//     // BlueScsiBridge() {}
//     // ~BlueScsiBridge() {};
//     void init();
//     // void mainLoop(void *param);
//     void mainLoop(void);

//     // // Should be moved to msc_disk.h ??
//     // static std::shared_ptr<USB::DiskInfo> GetDiskInfo(int target_id);

//     // Select target and execute SCSI command
//     static int RunCommand(int target_id,
//                           const uint8_t *command, size_t cmdLen,
//                           uint8_t *bufIn, size_t bufInLen,
//                           const uint8_t *bufOut, size_t bufOutLen,
//                           bool returnDataPhase = false);

//     // Execute READ CAPACITY command
//     static bool ReadCapacity(int target_id, uint32_t *sectorcount, uint32_t *sectorsize);

//     // Execute REQUEST SENSE command to get more information about error status
//     static bool RequestSense(int target_id, uint8_t *sense_key);

//     // Execute UNIT START STOP command to load/unload media
//     static bool StartStopUnit(uint8_t lun, uint8_t power_condition, bool start, bool load_eject);

//     // Execute INQUIRY command
//     static bool Inquiry(int target_id, uint8_t inquiry_data[36]);

//     // Execute TEST UNIT READY command and handle unit attention state
//     static bool TestUnitReady(int target_id);

//     static uint32_t Read10(int lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t buffersize);

//     static uint32_t Write10(int lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t buffersize);

//     static bool IsWritable(int lun);

// private:
//     static uint8_t initiator_id;
//     bool initialization_complete = false;
//     static uint8_t configured_retry_count;

//     SemaphoreHandle_t mutex;
//     StaticSemaphore_t mutexBuffer;

//     void UpdateLed();
//     void DebugPrint();
//     void ReadConfiguration();

//     // shared_ptr<USB::DiskInfo> BlueScsiBridge::GetDiskInfo(int target_id){

//     //     // Retry information for sector reads.
//     //     // If a large read fails, retry is done sector-by-sector.
//     //     int retrycount;
//     //     uint32_t failposition;
//     //     bool ejectWhenDone;

//     //     FsFile target_file;
//     // } g_initiator_state;

// protected:
//     static const uint8_t SCSI_COMMAND_READ10 = 0x28;
//     static const uint8_t SCSI_COMMAND_WRITE10 = 0x2A;
// };

// // // Select target and execute SCSI command
// // int scsiUsbBridgeRunCommand(int target_id,
// //                             const uint8_t *command, size_t cmdLen,
// //                             uint8_t *bufIn, size_t bufInLen,
// //                             const uint8_t *bufOut, size_t bufOutLen,
// //                             bool returnDataPhase = false);

// // // Execute READ CAPACITY command
// // bool scsiUsbBridgeReadCapacity(int target_id, uint32_t *sectorcount, uint32_t *sectorsize);

// // // Execute REQUEST SENSE command to get more information about error status
// // bool scsiRequestSense(int target_id, uint8_t *sense_key);

// // // Execute UNIT START STOP command to load/unload media
// // bool scsiStartStopUnit(int target_id, bool start);

// // // Execute INQUIRY command
// // bool scsiInquiry(int target_id, uint8_t inquiry_data[36]);

// // // Execute TEST UNIT READY command and handle unit attention state
// // bool scsiTestUnitReady(int target_id);

// // Read a block of data from SCSI device and write to file on SD card
// // class FsFile;

// // bool scsiUsbBridgeReadDataToFile(int target_id, uint32_t start_sector, uint32_t sectorcount, uint32_t sectorsize,
// // //                                  FsFile &file);
// // #ifdef __cplusplus
// // }
// // #endif