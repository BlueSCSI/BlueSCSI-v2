// /*
//  *  ZuluSCSI
//  *  Copyright (c) 2022 Rabbit Hole Computing
//  *
//  * Main program for initiator mode.
//  */

// #include "BlueSCSI_config.h"
// #include "BlueSCSI_log.h"
// #include "BlueSCSI_log_trace.h"
// // #include "BlueSCSI_initiator.h"
// #include "usb/msc_disk.h"
// #include "BlueSCSI_usbbridge.h"
// #include <BlueSCSI_platform.h>
// #include <minIni.h>
// #include "SdFat.h"
// #include <string.h>
// #include <memory>
// #include "usb/msc_example_disk.h"
// #include "usb/msc_disk.h"
// #include <scsi2sd.h>
// extern "C"
// {
// #include <scsi.h>
// }

// extern bool delay_usb_task;
// // TODO: Do something better with these....
// #define DEVICE_TYPE_CD 5
// #define DEVICE_TYPE_DIRECT_ACCESS 0

// #define DEBUG_PRINTF(...) log(__VA_ARGS__)


// // Update progress bar LED during transfers
// void BlueScsiBridge::UpdateLed()
// {
//     // Update status indicator, the led blinks every 5 seconds and is on the longer the more data has been transferred
//     const int period = 256;
//     int phase = (millis() % period);
//     int duty = 256; //////// (int64_t)g_initiator_state.sectors_done * period / g_initiator_state.sectorcount;

//     // Minimum and maximum time to verify that the blink is visible
//     if (duty < 50)
//         duty = 50;
//     if (duty > period - 50)
//         duty = period - 50;

//     if (phase <= duty)
//     {
//         LED_ON();
//     }
//     else
//     {
//         LED_OFF();
//     }
// }

// void BlueScsiBridge::DebugPrint()
// {
//     static uint32_t scsiUsbBridgeMainLoop_counter = 0;
//     static uint32_t scsiUsbBridgeMainLoop_counter2 = 0;
//     if (scsiUsbBridgeMainLoop_counter > 1000)
//     {
//         printf("BlueScsiBridge::mainLoop() running %ld\n", scsiUsbBridgeMainLoop_counter2++);
//         scsiUsbBridgeMainLoop_counter = 0;
//     }
//     scsiUsbBridgeMainLoop_counter++;
// }

// void BlueScsiBridge::mainLoop(void)
// {

//     while (1)
//     {
//         DebugPrint();

//         SCSI_RELEASE_OUTPUTS();
//         SCSI_ENABLE_INITIATOR();
//         if (g_scsiHostPhyReset)
//         {
//             log("Executing BUS RESET after aborted command");
//             scsiHostPhyReset();
//         }
//         vTaskDelay(100);
//     }
// }

// bool BlueScsiBridge::IsWritable(int lun){
//     shared_ptr<USB::DiskInfo> mydisk = USB::DiskInfoList[lun];

//     uint8_t command[6] = {0x1A, 0x08, 0, 0, 4, 0}; // MODE SENSE(6)
//     uint8_t response[4] = {0};
//     RunCommand(mydisk->target_id, command, sizeof(command), response, sizeof(response), NULL, 0);
//     return (response[2] & 0x80) == 0; // Check write protected bit
// }

// //     if (!g_initiator_state.imaging)
// //     {
// //         // Scan for SCSI drives one at a time
// //         g_initiator_state.target_id = (g_initiator_state.target_id + 1) % 8;
// //         g_initiator_state.sectors_done = 0;
// //         g_initiator_state.retrycount = 0;
// //         g_initiator_state.max_sector_per_transfer = 512;
// //         g_initiator_state.badSectorCount = 0;
// //         g_initiator_state.ejectWhenDone = false;

// //         if (!(g_initiator_state.drives_imaged & (1 << g_initiator_state.target_id)))
// //         {
// //             // #ifndef LIB_FREERTOS_KERNEL
// //             delay_with_poll(1000);
// //             // #else
// //             //             vTaskDelay(1000 / portTICK_PERIOD_MS);
// //             // #endif

// //             uint8_t inquiry_data[36] = {0};

// //             LED_ON();
// //             bool startstopok =
// //                 scsiTestUnitReady(g_initiator_state.target_id) &&
// //                 scsiStartStopUnit(g_initiator_state.target_id, true);

// //             bool readcapok = startstopok &&
// //                              scsiInitiatorReadCapacity(g_initiator_state.target_id,
// //                                                        &g_initiator_state.sectorcount,
// //                                                        &g_initiator_state.sectorsize);

// //             bool inquiryok = startstopok &&
// //                              scsiInquiry(g_initiator_state.target_id, inquiry_data);
// //             g_initiator_state.ansiVersion = inquiry_data[2] & 0x7;
// //             LED_OFF();

// //             uint64_t total_bytes = 0;
// //             if (readcapok)
// //             {
// //                 log("SCSI ID ", g_initiator_state.target_id,
// //                     " capacity ", (int)g_initiator_state.sectorcount,
// //                     " sectors x ", (int)g_initiator_state.sectorsize, " bytes");
// //                 log_f("SCSI-%d: Vendor: %.8s, Product: %.16s, Version: %.4s",
// //                       g_initiator_state.ansiVersion,
// //                       &inquiry_data[8],
// //                       &inquiry_data[16],
// //                       &inquiry_data[32]);

// //                 // Check for well known ejectable media.
// //                 if (strncmp((char *)(&inquiry_data[8]), "IOMEGA", 6) == 0 &&
// //                     strncmp((char *)(&inquiry_data[16]), "ZIP", 3) == 0)
// //                 {
// //                     g_initiator_state.ejectWhenDone = true;
// //                 }
// //                 g_initiator_state.sectorcount_all = g_initiator_state.sectorcount;

// //                 total_bytes = (uint64_t)g_initiator_state.sectorcount * g_initiator_state.sectorsize;
// //                 log("Drive total size is ", (int)(total_bytes / (1024 * 1024)), " MiB");
// // #ifndef LIB_FREERTOS_KERNEL
// //                 // TODO: sdcard support is not working in FreeRTOS
// //                 if (total_bytes >= 0xFFFFFFFF && SD.fatType() != FAT_TYPE_EXFAT)
// //                 {
// //                     // Note: the FAT32 limit is 4 GiB - 1 byte
// //                     log("Image files equal or larger than 4 GiB are only possible on exFAT filesystem");
// //                     log("Please reformat the SD card with exFAT format to image this drive.");
// //                     g_initiator_state.sectorsize = 0;
// //                     g_initiator_state.sectorcount = g_initiator_state.sectorcount_all = 0;
// //                 }
// // #endif
// //                 if (g_initiator_state.ansiVersion < 0x02)
// //                 {
// //                     // this is a SCSI-1 drive, use READ6 and 256 bytes to be safe.
// //                     g_initiator_state.max_sector_per_transfer = 256;
// //                 }
// //             }
// //             else if (startstopok)
// //             {
// //                 log("SCSI ID ", g_initiator_state.target_id, " responds but ReadCapacity command failed");
// //                 log("Possibly SCSI-1 drive? Attempting to read up to 1 GB.");
// //                 g_initiator_state.sectorsize = 512;
// //                 g_initiator_state.sectorcount = g_initiator_state.sectorcount_all = 2097152;
// //                 g_initiator_state.max_sector_per_transfer = 128;
// //             }
// //             else
// //             {
// //                 log("* No response from SCSI ID ", g_initiator_state.target_id);
// //                 g_initiator_state.sectorsize = 0;
// //                 g_initiator_state.sectorcount = g_initiator_state.sectorcount_all = 0;
// //             }

// //             if (inquiryok)
// //             {
// //                 g_initiator_state.deviceType = inquiry_data[0] & 0x1F;
// //                 if (g_initiator_state.deviceType == DEVICE_TYPE_CD)
// //                 {
// //                     g_initiator_state.ejectWhenDone = true;
// //                 }
// //                 else if (g_initiator_state.deviceType != DEVICE_TYPE_DIRECT_ACCESS)
// //                 {
// //                     log("Unhandled device type: ", g_initiator_state.deviceType, ". Handling it as Direct Access Device.");
// //                 }
// //             }

// //             if (g_initiator_state.sectorcount > 0)
// //             {
// // #ifndef LIB_FREERTOS_KERNEL
// //                 char filename[18] = "";
// //                 int image_num = 0;
// //                 uint64_t sd_card_free_bytes = (uint64_t)SD.vol()->freeClusterCount() * SD.vol()->bytesPerCluster();
// //                 if (sd_card_free_bytes < total_bytes)
// //                 {
// //                     log("SD Card only has ", (int)(sd_card_free_bytes / (1024 * 1024)), " MiB - not enough free space to image this drive!");
// //                     g_initiator_state.imaging = false;
// //                     return;
// //                 }

// //                 do
// //                 {
// //                     sprintf(filename, "%s%d_imaged-%03d.%s",
// //                             (g_initiator_state.deviceType == DEVICE_TYPE_CD) ? "CD" : "HD",
// //                             g_initiator_state.target_id,
// //                             ++image_num,
// //                             (g_initiator_state.deviceType == DEVICE_TYPE_CD) ? "iso" : "hda");
// //                 } while (SD.exists(filename));
// //                 log("Imaging filename: ", filename, ".");
// //                 g_initiator_state.target_file = SD.open(filename, O_WRONLY | O_CREAT | O_TRUNC);
// //                 if (!g_initiator_state.target_file.isOpen())
// //                 {
// //                     log("Failed to open file for writing: ", filename);
// //                     return;
// //                 }

// //                 if (SD.fatType() == FAT_TYPE_EXFAT)
// //                 {
// //                     // Only preallocate on exFAT, on FAT32 preallocating can result in false garbage data in the
// //                     // file if write is interrupted.
// //                     log("Preallocating image file");
// //                     g_initiator_state.target_file.preAllocate((uint64_t)g_initiator_state.sectorcount * g_initiator_state.sectorsize);
// //                 }

// //                 log("Starting to copy drive data to ", filename);
// // #endif
// //                 g_initiator_state.imaging = true;
// //             }
// //         }
// //     }
// //     else
// //     {
// //         // Copy sectors from SCSI drive to file
// //         if (g_initiator_state.sectors_done >= g_initiator_state.sectorcount)
// //         {
// //             scsiStartStopUnit(g_initiator_state.target_id, false);
// //             log("Finished imaging drive with id ", g_initiator_state.target_id);
// //             LED_OFF();

// //             if (g_initiator_state.sectorcount != g_initiator_state.sectorcount_all)
// //             {
// //                 log("NOTE: Image size was limited to first 4 GiB due to SD card filesystem limit");
// //                 log("Please reformat the SD card with exFAT format to image this drive fully");
// //             }

// //             if (g_initiator_state.badSectorCount != 0)
// //             {
// //                 log_f("NOTE: There were %d bad sectors that could not be read off this drive.", g_initiator_state.badSectorCount);
// //             }

// //             if (!g_initiator_state.ejectWhenDone)
// //             {
// //                 log("Marking this ID as imaged, wont ask it again.");
// //                 g_initiator_state.drives_imaged |= (1 << g_initiator_state.target_id);
// //             }
// //             g_initiator_state.imaging = false;
// // #ifndef LIB_FREERTOS_KERNEL
// //             g_initiator_state.target_file.close();
// // #endif
// //             return;
// //         }

// //         scsiInitiatorUpdateLed();

// //         // How many sectors to read in one batch?
// //         uint32_t numtoread = g_initiator_state.sectorcount - g_initiator_state.sectors_done;
// //         if (numtoread > g_initiator_state.max_sector_per_transfer)
// //             numtoread = g_initiator_state.max_sector_per_transfer;

// //         // Retry sector-by-sector after failure
// //         if (g_initiator_state.sectors_done < g_initiator_state.failposition)
// //             numtoread = 1;

// //         uint32_t time_start = millis();
// //         bool status = scsiInitiatorReadDataToFile(g_initiator_state.target_id,
// //                                                   g_initiator_state.sectors_done, numtoread, g_initiator_state.sectorsize,
// //                                                   g_initiator_state.target_file);

// //         if (!status)
// //         {
// //             log("Failed to transfer ", numtoread, " sectors starting at ", (int)g_initiator_state.sectors_done);

// //             if (g_initiator_state.retrycount < g_initiator_state.maxRetryCount)
// //             {
// //                 log("Retrying.. ", g_initiator_state.retrycount + 1, "/", (int)g_initiator_state.maxRetryCount);
// //                 // #ifndef LIB_FREERTOS_KERNEL
// //                 delay_with_poll(200);
// //                 // This reset causes some drives to hang and seems to have no effect if left off.
// //                 // scsiHostPhyReset();
// //                 delay_with_poll(200);
// //                 // #else
// //                 //                 vTaskDelay(200 / portTICK_PERIOD_MS);
// //                 // #endif

// //                 g_initiator_state.retrycount++;
// // #ifndef LIB_FREERTOS_KERNEL
// //                 g_initiator_state.target_file.seek((uint64_t)g_initiator_state.sectors_done * g_initiator_state.sectorsize);
// // #endif

// //                 if (g_initiator_state.retrycount > 1 && numtoread > 1)
// //                 {
// //                     log("Multiple failures, retrying sector-by-sector");
// //                     g_initiator_state.failposition = g_initiator_state.sectors_done + numtoread;
// //                 }
// //             }
// //             else
// //             {
// //                 log("Retry limit exceeded, skipping one sector");
// //                 g_initiator_state.retrycount = 0;
// //                 g_initiator_state.sectors_done++;
// //                 g_initiator_state.badSectorCount++;
// // #ifndef LIB_FREERTOS_KERNEL
// //                 g_initiator_state.target_file.seek((uint64_t)g_initiator_state.sectors_done * g_initiator_state.sectorsize);
// // #endif
// //             }
// //         }
// //         else
// //         {
// //             g_initiator_state.retrycount = 0;
// //             g_initiator_state.sectors_done += numtoread;
// // #ifndef LIB_FREERTOS_KERNEL
// //             g_initiator_state.target_file.flush();
// // #endif

// //             int speed_kbps = numtoread * g_initiator_state.sectorsize / (millis() - time_start);
// //             log("SCSI read succeeded, sectors done: ",
// //                 (int)g_initiator_state.sectors_done, " / ", (int)g_initiator_state.sectorcount,
// //                 " speed ", speed_kbps, " kB/s - ",
// //                 (int)(100 * (int64_t)g_initiator_state.sectors_done / g_initiator_state.sectorcount), "%");
// //         }
// //     }
// // #ifdef LIB_FREERTOS_KERNEL
// //     vTaskDelay(1);
// // }
// // #endif
// // }

// // #ifdef LIB_FREERTOS_KERNEL
// // bool scsiUsbBridgeReadDataToFile(int target_id, uint32_t start_sector, uint32_t sectorcount, uint32_t sectorsize,
// //                                  int file)
// // #else
// //     bool scsiUsbBridgeReadDataToFile(int target_id, uint32_t start_sector, uint32_t sectorcount, uint32_t sectorsize,
// //                                      FsFile &file)
// // #endif
// // {
// //     // int status = -1;

// //     // // Read6 command supports 21 bit LBA - max of 0x1FFFFF
// //     // // ref: https://www.seagate.com/files/staticfiles/support/docs/manual/Interface%20manuals/100293068j.pdf pg 134
// //     // if (g_initiator_state.ansiVersion < 0x02 || (start_sector < 0x1FFFFF && sectorcount <= 256))
// //     // {
// //     //     // Use READ6 command for compatibility with old SCSI1 drives
// //     //     uint8_t command[6] = {0x08,
// //     //         (uint8_t)(start_sector >> 16),
// //     //         (uint8_t)(start_sector >> 8),
// //     //         (uint8_t)start_sector,
// //     //         (uint8_t)sectorcount,
// //     //         0x00
// //     //     };

// //     //     // Start executing command, return in data phase
// //     //     status = scsiInitiatorRunCommand(target_id, command, sizeof(command), NULL, 0, NULL, 0, true);
// //     // }
// //     // else
// //     // {
// //     //     // Use READ10 command for larger number of blocks
// //     //     uint8_t command[10] = {0x28, 0x00,
// //     //         (uint8_t)(start_sector >> 24), (uint8_t)(start_sector >> 16),
// //     //         (uint8_t)(start_sector >> 8), (uint8_t)start_sector,
// //     //         0x00,
// //     //         (uint8_t)(sectorcount >> 8), (uint8_t)(sectorcount),
// //     //         0x00
// //     //     };

// //     //     // Start executing command, return in data phase
// //     //     status = scsiInitiatorRunCommand(target_id, command, sizeof(command), NULL, 0, NULL, 0, true);
// //     // }

// //     // if (status != 0)
// //     // {
// //     //     uint8_t sense_key;
// //     //     scsiRequestSense(target_id, &sense_key);

// //     //     log("scsiInitiatorReadDataToFile: READ failed: ", status, " sense key ", sense_key);
// //     //     scsiHostPhyRelease();
// //     //     return false;
// //     // }

// //     // SCSI_PHASE phase;

// //     // g_initiator_transfer.bytes_scsi = sectorcount * sectorsize;
// //     // g_initiator_transfer.bytes_per_sector = sectorsize;
// //     // g_initiator_transfer.bytes_sd = 0;
// //     // g_initiator_transfer.bytes_sd_scheduled = 0;
// //     // g_initiator_transfer.bytes_scsi_done = 0;
// //     // g_initiator_transfer.all_ok = true;

// //     // while (true)
// //     // {
// //     //     platform_poll();

// //     //     phase = (SCSI_PHASE)scsiHostPhyGetPhase();
// //     //     if (phase != DATA_IN && phase != BUS_BUSY)
// //     //     {
// //     //         break;
// //     //     }

// //     //     // Read next block from SCSI bus if buffer empty
// //     //     if (g_initiator_transfer.bytes_sd == g_initiator_transfer.bytes_scsi_done)
// //     //     {
// //     //         initiatorReadSDCallback(0);
// //     //     }
// //     //     else
// //     //     {
// //     //         // Write data to SD card and simultaneously read more from SCSI
// //     //         scsiInitiatorUpdateLed();
// //     //         scsiInitiatorWriteDataToSd(file, true);
// //     //     }
// //     // }

// //     // // Write any remaining buffered data
// //     // while (g_initiator_transfer.bytes_sd < g_initiator_transfer.bytes_scsi_done)
// //     // {
// //     //     platform_poll();
// //     //     scsiInitiatorWriteDataToSd(file, false);
// //     // }

// //     // if (g_initiator_transfer.bytes_sd != g_initiator_transfer.bytes_scsi)
// //     // {
// //     //     log("SCSI read from sector ", (int)start_sector, " was incomplete: expected ",
// //     //          (int)g_initiator_transfer.bytes_scsi, " got ", (int)g_initiator_transfer.bytes_sd, " bytes");
// //     //     g_initiator_transfer.all_ok = false;
// //     // }

// //     // while ((phase = (SCSI_PHASE)scsiHostPhyGetPhase()) != BUS_FREE)
// //     // {
// //     //     platform_poll();

// //     //     if (phase == MESSAGE_IN)
// //     //     {
// //     //         uint8_t dummy = 0;
// //     //         scsiHostRead(&dummy, 1);
// //     //     }
// //     //     else if (phase == MESSAGE_OUT)
// //     //     {
// //     //         uint8_t identify_msg = 0x80;
// //     //         scsiHostWrite(&identify_msg, 1);
// //     //     }
// //     //     else if (phase == STATUS)
// //     //     {
// //     //         uint8_t tmp = 0;
// //     //         scsiHostRead(&tmp, 1);
// //     //         status = tmp;
// //     //         debuglog("------ STATUS: ", tmp);
// //     //     }
// //     // }

// //     // scsiHostPhyRelease();

// //     // return status == 0 && g_initiator_transfer.all_ok;
// //     return true;
// // }

// // Execute INQUIRY command
// bool BlueScsiBridge::Inquiry(int target_id, uint8_t inquiry_data[36])
// {
//     uint8_t command[6] = {0x12, 0, 0, 0, 36, 0};
//     int status = RunCommand(target_id,
//                             command, sizeof(command),
//                             inquiry_data, 36,
//                             NULL, 0);
//     return status == 0;
// }

// // Execute TEST UNIT READY command and handle unit attention state
// bool BlueScsiBridge::TestUnitReady(int target_id)
// {
//     for (int retries = 0; retries < 2; retries++)
//     {
//         uint8_t command[6] = {0x00, 0, 0, 0, 0, 0};
//         int status = RunCommand(target_id,
//                                 command, sizeof(command),
//                                 NULL, 0,
//                                 NULL, 0);

//         if (status == 0)
//         {
//             return true;
//         }
//         else if (status == -1)
//         {
//             // No response to select
//             return false;
//         }
//         else if (status == 2)
//         {
//             uint8_t sense_key;
//             RequestSense(target_id, &sense_key);

//             if (sense_key == 6)
//             {
//                 uint8_t inquiry[36];
//                 log("Target ", target_id, " reports UNIT_ATTENTION, running INQUIRY");
//                 Inquiry(target_id, inquiry);
//             }
//             else if (sense_key == 2)
//             {
//                 log("Target ", target_id, " reports NOT_READY, running STARTSTOPUNIT");
//                 StartStopUnit(target_id, 0, true, false);
//             }
//         }
//         else
//         {
//             log("Target ", target_id, " TEST UNIT READY response: ", status);
//         }
//     }

//     return false;
// }

// int BlueScsiBridge::RunCommand(int target_id,
//                                const uint8_t *command, size_t cmdLen,
//                                uint8_t *bufIn, size_t bufInLen,
//                                const uint8_t *bufOut, size_t bufOutLen,
//                                bool returnDataPhase)
// {
//     if (!scsiHostPhySelect(target_id, initiator_id))
//     {
//         debuglog("------ Target ", target_id, " did not respond");
//         scsiHostPhyRelease();
//         return -1;
//     }

//     SCSI_PHASE phase;
//     int status = -1;
//     while ((phase = (SCSI_PHASE)scsiHostPhyGetPhase()) != BUS_FREE)
//     {
//         platform_poll();
//         // vTaskDelay(1);

//         if (phase == MESSAGE_IN)
//         {
//             uint8_t dummy = 0;
//             scsiHostRead(&dummy, 1);
//         }
//         else if (phase == MESSAGE_OUT)
//         {
//             uint8_t identify_msg = 0x80;
//             scsiHostWrite(&identify_msg, 1);
//         }
//         else if (phase == COMMAND)
//         {
//             scsiHostWrite(command, cmdLen);
//         }
//         else if (phase == DATA_IN)
//         {
//             if (returnDataPhase)
//                 return 0;
//             if (bufInLen == 0)
//             {
//                 log("DATA_IN phase but no data to receive!");
//                 status = -3;
//                 break;
//             }

//             if (scsiHostRead(bufIn, bufInLen) == 0)
//             {
//                 log("scsiHostRead failed, tried to read ", (int)bufInLen, " bytes");
//                 status = -2;
//                 break;
//             }
//         }
//         else if (phase == DATA_OUT)
//         {
//             if (returnDataPhase)
//                 return 0;
//             if (bufOutLen == 0)
//             {
//                 log("DATA_OUT phase but no data to send!");
//                 status = -3;
//                 break;
//             }

//             if (scsiHostWrite(bufOut, bufOutLen) < bufOutLen)
//             {
//                 log("scsiHostWrite failed, was writing ", bytearray(bufOut, bufOutLen));
//                 status = -2;
//                 break;
//             }
//         }
//         else if (phase == STATUS)
//         {
//             uint8_t tmp = -1;
//             scsiHostRead(&tmp, 1);
//             status = tmp;
//             debuglog("------ STATUS: ", tmp);
//         }
//     }

//     scsiHostPhyRelease();

//     return status;
// }

// // bool BlueScsiBridge::ReadCapacity(int target_id, uint32_t *sectorcount, uint32_t *sectorsize)
// // {
// //     uint8_t command[10] = {0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0};
// //     uint8_t response[8] = {0};
// //     int status = RunCommand(target_id,
// //                                          command, sizeof(command),
// //                                          response, sizeof(response),
// //                                          NULL, 0);

// //     if (status == 0)
// //     {
// //         *sectorcount = ((uint32_t)response[0] << 24)
// //                     | ((uint32_t)response[1] << 16)
// //                     | ((uint32_t)response[2] <<  8)
// //                     | ((uint32_t)response[3] <<  0);

// //         *sectorcount += 1; // SCSI reports last sector address

// //         *sectorsize = ((uint32_t)response[4] << 24)
// //                     | ((uint32_t)response[5] << 16)
// //                     | ((uint32_t)response[6] <<  8)
// //                     | ((uint32_t)response[7] <<  0);

// //         return true;
// //     }
// //     else if (status == 2)
// //     {
// //         uint8_t sense_key;
// //         RequestSense(target_id, &sense_key);
// //         log("READ CAPACITY on target ", target_id, " failed, sense key ", sense_key);
// //         return false;
// //     }
// //     else
// //     {
// //         *sectorcount = *sectorsize = 0;
// //         return false;
// //     }
// // }

// // Execute REQUEST SENSE command to get more information about error status
// bool BlueScsiBridge::RequestSense(int target_id, uint8_t *sense_key)
// {
//     uint8_t command[6] = {0x03, 0, 0, 0, 18, 0};
//     uint8_t response[18] = {0};

//     int status = RunCommand(target_id,
//                             command, sizeof(command),
//                             response, sizeof(response),
//                             NULL, 0);

//     // log("RequestSense response: ", bytearray(response, 18));

//     *sense_key = response[2] & 0x0F;
//     return status == 0;
// }

// // Execute UNIT START STOP command to load/unload media
// bool BlueScsiBridge::StartStopUnit(uint8_t target_id, uint8_t power_condition, bool start, bool load_eject)
// {
//     shared_ptr<USB::DiskInfo> disk = GetDiskInfo(target_id);
//     // uint8_t target_id = disk->target_id;
//     uint8_t command[6] = {0x1B, 0x1, 0, 0, 0, 0};
//     uint8_t response[4] = {0};

//     if (start)
//     {
//         command[4] |= 1; // Start
//         command[1] = 0;  // Immediate
//     }
//     else // stop
//     {
//         if (disk->deviceType == DEVICE_TYPE_CD)
//         {
//             command[4] = 0b00000010; // eject(6), stop(7).
//         }
//     }
    
//     // TODO: Need to validate this. Didn't see it in the scsi spec
//     command[4] |= power_condition << 4;

//     int status = RunCommand(target_id,
//                             command, sizeof(command),
//                             response, sizeof(response),
//                             NULL, 0);

//     if (status == 2)
//     {
//         uint8_t sense_key;
//         RequestSense(target_id, &sense_key);
//         log("START STOP UNIT on target ", target_id, " failed, sense key ", sense_key);
//     }

//     return status == 0;
// }



// uint32_t BlueScsiBridge::Read10(int lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t buffersize)
// {
//     int status = -1;
//     USB::DiskInfo *disk = GetDiskInfo(lun).get();
//     uint32_t sectorcount = buffersize / disk->sectorsize;
//     uint32_t start_sector = lba;
//     uint32_t sector_size = disk->sectorsize;

//     if (sectorcount == 0){
//         log("Read10: invalid sector count: ", sectorcount);
//         return 0;
//     }
//     // // Read6 command supports 21 bit LBA - max of 0x1FFFFF
//     // // ref: https://www.seagate.com/files/staticfiles/support/docs/manual/Interface%20manuals/100293068j.pdf pg 134
//     // if (g_initiator_state.ansiVersion < 0x02 || (start_sector < 0x1FFFFF && sectorcount <= 256))
//     // {
//     //     // Use READ6 command for compatibility with old SCSI1 drives
//     //     uint8_t command[6] = {0x08,
//     //         (uint8_t)(start_sector >> 16),
//     //         (uint8_t)(start_sector >> 8),
//     //         (uint8_t)start_sector,
//     //         (uint8_t)sectorcount,
//     //         0x00
//     //     };

//     //     // Start executing command, return in data phase
//     //     status = scsiInitiatorRunCommand(target_id, command, sizeof(command), NULL, 0, NULL, 0, true);
//     // }
//     // else
//     // {
//     // Use READ10 command for larger number of blocks
//     uint8_t command[10] = {0x28, 0x00,
//                            (uint8_t)(start_sector >> 24), (uint8_t)(start_sector >> 16),
//                            (uint8_t)(start_sector >> 8), (uint8_t)start_sector,
//                            0x00,
//                            (uint8_t)(sectorcount >> 8), (uint8_t)(sectorcount),
//                            0x00};


//         status = RunCommand(disk->target_id, command, sizeof(command), (uint8_t*)buffer, buffersize, NULL, 0);
//     // }


//     if (status != 0)
//     {
//         uint8_t sense_key;
//         RequestSense(disk->target_id, &sense_key);

//         log("scsiInitiatorReadDataToFile: READ failed: ", status, " sense key ", sense_key);
//         scsiHostPhyRelease();
//         return 0;
//     }
//     return sectorcount * sector_size;
// }

//     // SCSI_PHASE phase;

//     // volatile int bytes_scsi = sectorcount * sectorsize;
//     // volatile int bytes_per_sector = sectorsize;
//     // volatile int bytes_sd = 0;
//     // volatile int bytes_sd_scheduled = 0;
//     // volatile int bytes_scsi_done = 0;
//     // volatile bool all_ok = true;



//     // (void)bytes_scsi;
//     // (void)bytes_per_sector;
//     // (void)bytes_sd;
//     // (void)bytes_sd_scheduled;
//     // (void)bytes_scsi_done;
//     // (void) all_ok;

//     // while (true)
//     // {
//     //     platform_poll();

//     //     phase = (SCSI_PHASE)scsiHostPhyGetPhase();
//     //     if (phase != DATA_IN && phase != BUS_BUSY)
//     //     {
//     //         break;
//     //     }

//     //     // Read next block from SCSI bus if buffer empty
//     //     // if (g_initiator_transfer.bytes_sd == g_initiator_transfer.bytes_scsi_done)
//     //     // {
//     //         // initiatorReadSDCallback(0);
//     //         if (scsiHostRead(buffer, buffersize) != buffersize)
//     //         {
//     //          log("Read failed!!");
//     //         //   ", (int)g_initiator_transfer.bytes_scsi_done);
//     //         //  g_initiator_transfer.all_ok = false;
//     //         break;
//     //         }
//     //     // }
//     //     // else
//     //     // {
//     //     //     // Write data to SD card and simultaneously read more from SCSI
//     //     //     scsiInitiatorUpdateLed();
//     //     //     scsiInitiatorWriteDataToSd(file, true);
//     //     // }
//     // }

//     // // // Write any remaining buffered data
//     // // while (g_initiator_transfer.bytes_sd < g_initiator_transfer.bytes_scsi_done)
//     // // {
//     // //     platform_poll();
//     // //     scsiInitiatorWriteDataToSd(file, false);
//     // // }

//     // // if (g_initiator_transfer.bytes_sd != g_initiator_transfer.bytes_scsi)
//     // // {
//     // //     log("SCSI read from sector ", (int)start_sector, " was incomplete: expected ",
//     // //         (int)g_initiator_transfer.bytes_scsi, " got ", (int)g_initiator_transfer.bytes_sd, " bytes");
//     // //     g_initiator_transfer.all_ok = false;
//     // // }

//     // while ((phase = (SCSI_PHASE)scsiHostPhyGetPhase()) != BUS_FREE)
//     // {
//     //     platform_poll();

//     //     if (phase == MESSAGE_IN)
//     //     {
//     //         uint8_t dummy = 0;
//     //         scsiHostRead(&dummy, 1);
//     //     }
//     //     else if (phase == MESSAGE_OUT)
//     //     {
//     //         uint8_t identify_msg = 0x80;
//     //         scsiHostWrite(&identify_msg, 1);
//     //     }
//     //     else if (phase == STATUS)
//     //     {
//     //         uint8_t tmp = 0;
//     //         scsiHostRead(&tmp, 1);
//     //         status = tmp;
//     //         debuglog("------ STATUS: ", tmp);
//     //     }
//     // }

//     // scsiHostPhyRelease();

//     // return status == 0; //&& g_initiator_transfer.all_ok;
// // }

//                     // bool BlueScsiBridge::Read10(int target_id, uint32_t start_sector, uint32_t sectorcount, uint32_t sectorsize,
//                     //                             uint8_t *buffer, uint8_t buffersize)
//                     // {
//                     //     int status = -1;

//                     //     // Use READ10 command for larger number of blocks
//                     //     uint8_t command[10] = {SCSI_COMMAND_READ10, 0x00,
//                     //                         (uint8_t)(start_sector >> 24), (uint8_t)(start_sector >> 16),
//                     //                         (uint8_t)(start_sector >> 8), (uint8_t)start_sector,
//                     //                         0x00,
//                     //                         (uint8_t)(sectorcount >> 8), (uint8_t)(sectorcount),
//                     //                         0x00};

//                     //     // Start executing command, return in data phase
//                     //     status = RunCommand(target_id, command, sizeof(command), buffer, buffersize, NULL, 0, true);

//                     //     // static int RunCommand(int target_id,
//                     //     //                const uint8_t *command, size_t cmdLen,
//                     //     //                uint8_t *bufIn, size_t bufInLen,
//                     //     //                const uint8_t *bufOut, size_t bufOutLen,
//                     //     //                bool returnDataPhase = false);

//                     //     if (status != 0)
//                     //     {
//                     //         uint8_t sense_key;
//                     //         RequestSense(target_id, &sense_key);

//                     //         log("scsiInitiatorReadDataToFile: READ failed: ", status, " sense key ", sense_key);
//                     //         scsiHostPhyRelease();
//                     //         return false;
//                     //     }
//                     //     return true;
//                     // }

// // // scsiHostPhyRelease();

// // SCSI_PHASE phase;

// // while (true)
// // {
// //     platform_poll();

// //     phase = (SCSI_PHASE)scsiHostPhyGetPhase();
// //     if (phase != DATA_IN && phase != BUS_BUSY)
// //     {
// //         break;
// //     }

// // while ((phase = (SCSI_PHASE)scsiHostPhyGetPhase()) != BUS_FREE)
// // {
// //     platform_poll();

// //     if (phase == MESSAGE_IN)
// //     {
// //         uint8_t dummy = 0;
// //         scsiHostRead(&dummy, 1);
// //     }
// //     else if (phase == MESSAGE_OUT)
// //     {
// //         uint8_t identify_msg = 0x80;
// //         scsiHostWrite(&identify_msg, 1);
// //     }
// //     else if (phase == STATUS)
// //     {
// //         uint8_t tmp = 0;
// //         scsiHostRead(&tmp, 1);
// //         status = tmp;
// //         debuglog("------ STATUS: ", tmp);
// //     }
// // }

// // scsiHostPhyRelease();


// uint32_t BlueScsiBridge::Write10(int lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t buffersize)
// {
//     int status = -1;
//     USB::DiskInfo *disk = GetDiskInfo(lun).get();
//     uint32_t sectorcount = buffersize / disk->sectorsize;
//     uint32_t start_sector = lba;
//     uint32_t sector_size = disk->sectorsize;

//     if (sectorcount == 0){
//         log("Write10: invalid sector count: ", sectorcount);
//         return 0;
//     }

//     // Use WRITE10 command for larger number of blocks
//     uint8_t command[10] = {scsi_command::eCmdWrite10, 0x00,
//                            (uint8_t)(start_sector >> 24), (uint8_t)(start_sector >> 16),
//                            (uint8_t)(start_sector >> 8), (uint8_t)start_sector,
//                            0x00,
//                            (uint8_t)(sectorcount >> 8), (uint8_t)(sectorcount),
//                            0x00};

//     status = RunCommand(disk->target_id, command, sizeof(command), NULL, 0, buffer, buffersize );

//     if (status != 0)
//     {
//         uint8_t sense_key;
//         RequestSense(disk->target_id, &sense_key);

//         log("scsiInitiatorReadDataToFile: WRITE failed: ", status, " sense key ", sense_key);
//         scsiHostPhyRelease();
//         return 0;
//     }
//     return sectorcount * sector_size;
// }