/* Initiator mode USB Mass Storage Class connection.
 * This file binds platform-specific MSC routines to the initiator mode
 * SCSI bus interface. The call structure is modeled after TinyUSB, but
 * should be usable with other USB libraries.
 *
 * ZuluSCSI™ - Copyright (c) 2023 Rabbit Hole Computing™
 *
 * This file is licensed under the GPL version 3 or any later version. 
 * It is derived from cdrom.c in SCSI2SD V6
 *
 * https://www.gnu.org/licenses/gpl-3.0.html
 * ----
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "ZuluSCSI_config.h"
#include "ZuluSCSI_log.h"
#include "ZuluSCSI_log_trace.h"
#include "ZuluSCSI_initiator.h"
#include "ZuluSCSI_platform_msc.h"
#include <scsi.h>
#include <ZuluSCSI_platform.h>
#include <minIni.h>
#include "SdFat.h"

bool g_msc_initiator;

#ifndef PLATFORM_HAS_INITIATOR_MODE

bool setup_msc_initiator() { return false; }
void poll_msc_initiator() {}

void init_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {}
uint8_t init_msc_get_maxlun_cb(void) { return 0; }
bool init_msc_is_writable_cb (uint8_t lun) { return false; }
bool init_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) { return false; }
bool init_msc_test_unit_ready_cb(uint8_t lun) { return false; }
void init_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size) {}
int32_t init_msc_scsi_cb(uint8_t lun, const uint8_t scsi_cmd[16], void *buffer, uint16_t bufsize) {return -1;}
int32_t init_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {return -1;}
int32_t init_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) { return -1;}
void init_msc_write10_complete_cb(uint8_t lun) {}

#else

// If there are multiple SCSI devices connected, they are mapped into LUNs for host.
static struct {
    int target_id;
    uint32_t sectorsize;
    uint32_t sectorcount;
} g_msc_initiator_targets[NUM_SCSIID];
static int g_msc_initiator_target_count;

// Prefetch next sector in main loop while USB is transferring previous one.
static struct {
    uint8_t *prefetch_buffer; // Buffer to use for storing the data
    uint32_t prefetch_bufsize;
    uint32_t prefetch_lba; // First sector to fetch
    int prefetch_target_id; // Target to read from
    size_t prefetch_sectorcount; // Number of sectors to fetch
    size_t prefetch_sectorsize;
    bool prefetch_done; // True after prefetch is complete

    // Periodic status reporting to log output
    uint32_t status_prev_time;
    uint32_t status_interval;
    uint32_t status_reqcount;
    uint32_t status_bytecount;

    // Scan new targets if none found
    uint32_t last_scan_time;
} g_msc_initiator_state;

static int do_read6_or_10(int target_id, uint32_t start_sector, uint32_t sectorcount, uint32_t sectorsize, void *buffer);

static void scan_targets()
{
    int found_count = 0;
    int initiator_id = scsiInitiatorGetOwnID();
    uint8_t inquiry_data[36] = {0};
    g_msc_initiator_target_count = 0;
    for (int target_id = 0; target_id < NUM_SCSIID; target_id++)
    {
        if (target_id == initiator_id) continue;

        if (scsiTestUnitReady(target_id))
        {
            uint32_t sectorcount, sectorsize;

            bool inquiryok =
                scsiStartStopUnit(target_id, true) &&
                scsiInquiry(target_id, inquiry_data);
            bool readcapok =
                scsiInitiatorReadCapacity(target_id, &sectorcount, &sectorsize);

            char vendor_id[9] = {0};
            char product_id[17] = {0};
            memcpy(vendor_id, &inquiry_data[8], 8);
            memcpy(product_id, &inquiry_data[16], 16);

            if (inquiryok)
            {
                if (readcapok)
                {
                    logmsg("Found SCSI drive with ID ", target_id, ": ", vendor_id, " ", product_id,
                        " capacity ", (int)(((uint64_t)sectorcount * sectorsize) / 1024 / 1024), " MB");
                    g_msc_initiator_targets[found_count].target_id = target_id;
                    g_msc_initiator_targets[found_count].sectorcount = sectorcount;
                    g_msc_initiator_targets[found_count].sectorsize = sectorsize;
                    found_count++;
                }
                else
                {
                    logmsg("Found SCSI drive with ID ", target_id, ": ", vendor_id, " ", product_id,
                           " but failed to read capacity. Assuming SCSI-1 drive up to 1 GB.");
                    g_msc_initiator_targets[found_count].target_id = target_id;
                    g_msc_initiator_targets[found_count].sectorcount = 2097152;
                    g_msc_initiator_targets[found_count].sectorsize = 512;
                    found_count++;
                }
            }
            else
            {
                logmsg("Detected SCSI device with ID ", target_id, ", but failed to get inquiry response, skipping");
            }
        }
    }

    // USB MSC requests can start processing after we set this
    g_msc_initiator_target_count = found_count;
}

bool setup_msc_initiator()
{
    logmsg("SCSI Initiator: activating USB MSC mode");
    g_msc_initiator = true;

    if (!ini_getbool("SCSI", "InitiatorMSCDisablePrefetch", false, CONFIGFILE))
    {
        // We can use the device mode buffer for prefetching data in initiator mode
        g_msc_initiator_state.prefetch_buffer = scsiDev.data;
        g_msc_initiator_state.prefetch_bufsize = sizeof(scsiDev.data);
    }

    g_msc_initiator_state.status_interval = ini_getl("SCSI", "InitiatorMSCStatusInterval", 5000, CONFIGFILE);

    scsiInitiatorInit();

    // Scan for targets
    scan_targets();

    logmsg("SCSI Initiator: found " , g_msc_initiator_target_count, " SCSI drives");
    return g_msc_initiator_target_count > 0;
}

void poll_msc_initiator()
{
    uint32_t time_now = millis();
    uint32_t time_since_scan = time_now - g_msc_initiator_state.last_scan_time;
    if (g_msc_initiator_target_count == 0 && time_since_scan > 5000)
    {
        // Scan for targets until we find one - drive might be slow to start up.
        // MSC lock is not required here because commands will early exit when target_count is 0.
        platform_reset_watchdog();
        scan_targets();
        g_msc_initiator_state.last_scan_time = time_now;
    }

    uint32_t delta = time_now - g_msc_initiator_state.status_prev_time;
    if (g_msc_initiator_state.status_interval > 0 &&
        delta > g_msc_initiator_state.status_interval)
    {
        if (g_msc_initiator_state.status_reqcount > 0)
        {
            logmsg("USB MSC: ", (int)g_msc_initiator_state.status_reqcount, " commands, ",
                   (int)(g_msc_initiator_state.status_bytecount / delta), " kB/s");
        }

        g_msc_initiator_state.status_reqcount = 0;
        g_msc_initiator_state.status_bytecount = 0;
        g_msc_initiator_state.status_prev_time = time_now;
    }


    platform_poll();
    platform_msc_lock_set(true); // Cannot handle new MSC commands while running prefetch
    if (g_msc_initiator_state.prefetch_sectorcount > 0
        && !g_msc_initiator_state.prefetch_done)
    {
        LED_ON();

        dbgmsg("Prefetch ", (int)g_msc_initiator_state.prefetch_lba, " + ",
                (int)g_msc_initiator_state.prefetch_sectorcount, "x",
                (int)g_msc_initiator_state.prefetch_sectorsize);
        // Read next block while USB is transferring
        int status = do_read6_or_10(g_msc_initiator_state.prefetch_target_id,
                                    g_msc_initiator_state.prefetch_lba,
                                    g_msc_initiator_state.prefetch_sectorcount,
                                    g_msc_initiator_state.prefetch_sectorsize,
                                    g_msc_initiator_state.prefetch_buffer);
        if (status == 0)
        {
            g_msc_initiator_state.prefetch_done = true;
        }
        else
        {
            logmsg("Prefetch of sector ", g_msc_initiator_state.prefetch_lba, " failed: status ", status);
            g_msc_initiator_state.prefetch_sectorcount = 0;
        }

        LED_OFF();
    }
    platform_msc_lock_set(false);
}

static int get_target(uint8_t lun)
{
    if (lun >= g_msc_initiator_target_count)
    {
        logmsg("Host requested access to non-existing lun ", (int)lun);
        return 0;
    }
    else
    {
        return g_msc_initiator_targets[lun].target_id;
    }
}

void init_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
    dbgmsg("-- MSC Inquiry");

    if (g_msc_initiator_target_count == 0)
    {
        memset(vendor_id, 0, 8);
        memset(product_id, 0, 8);
        memset(product_rev, 0, 8);
        return;
    }

    LED_ON();
    g_msc_initiator_state.status_reqcount++;

    int target = get_target(lun);
    uint8_t response[36] = {0};
    bool status = scsiInquiry(target, response);
    if (!status)
    {
        logmsg("SCSI Inquiry to target ", target, " failed");
    }

    memcpy(vendor_id, &response[8], 8);
    memcpy(product_id, &response[16], 16);
    memcpy(product_rev, &response[32], 4);

    LED_OFF();
}

uint8_t init_msc_get_maxlun_cb(void)
{
    return g_msc_initiator_target_count;
}

bool init_msc_is_writable_cb (uint8_t lun)
{
    if (g_msc_initiator_target_count == 0)
    {
        return false;
    }

    LED_ON();
    g_msc_initiator_state.status_reqcount++;

    int target = get_target(lun);
    uint8_t command[6] = {0x1A, 0x08, 0, 0, 4, 0}; // MODE SENSE(6)
    uint8_t response[4] = {0};
    scsiInitiatorRunCommand(target, command, 6, response, 4, NULL, 0);

    LED_OFF();
    return (response[2] & 0x80) == 0; // Check write protected bit
}

bool init_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
    dbgmsg("-- MSC Start Stop, start: ", (int)start, ", load_eject: ", (int)load_eject);

    if (g_msc_initiator_target_count == 0)
    {
        return false;
    }

    LED_ON();
    g_msc_initiator_state.status_reqcount++;

    int target = get_target(lun);
    uint8_t command[6] = {0x1B, 0x1, 0, 0, 0, 0};
    uint8_t response[4] = {0};
    
    if (start)
    {
        command[4] |= 1; // Start
        command[1] = 0;  // Immediate
    }

    if (load_eject)
    {
        command[4] |= 2;
    }

    command[4] |= power_condition << 4;

    int status = scsiInitiatorRunCommand(target,
                                         command, sizeof(command),
                                         response, sizeof(response),
                                         NULL, 0);

    if (status == 2)
    {
        uint8_t sense_key;
        scsiRequestSense(target, &sense_key);
        scsiLogInitiatorCommandFailure("START STOP UNIT", target, status, sense_key);
    }

    LED_OFF();

    return status == 0;
}

bool init_msc_test_unit_ready_cb(uint8_t lun)
{
    dbgmsg("-- MSC Test Unit Ready");

    if (g_msc_initiator_target_count == 0)
    {
        return false;
    }

    g_msc_initiator_state.status_reqcount++;
    return scsiTestUnitReady(get_target(lun));
}

void init_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size)
{
    dbgmsg("-- MSC Get Capacity");
    g_msc_initiator_state.status_reqcount++;

    if (g_msc_initiator_target_count == 0)
    {
        *block_count = 0;
        *block_size = 0;
        return;
    }

    uint32_t sectorcount = 0;
    uint32_t sectorsize = 0;
    scsiInitiatorReadCapacity(get_target(lun), &sectorcount, &sectorsize);
    *block_count = sectorcount;
    *block_size = sectorsize;
}

int32_t init_msc_scsi_cb(uint8_t lun, const uint8_t scsi_cmd[16], void *buffer, uint16_t bufsize)
{
    if (g_msc_initiator_target_count == 0)
    {
        return -1;
    }

    dbgmsg("-- MSC Raw SCSI command ", bytearray(scsi_cmd, 16));
    LED_ON();
    g_msc_initiator_state.status_reqcount++;

    // NOTE: the TinyUSB API around free-form commands is not very good,
    // this function could need improvement.
    
    // Figure out command length
    static const uint8_t CmdGroupBytes[8] = {6, 10, 10, 6, 16, 12, 6, 6}; // From SCSI2SD
    int cmdlen = CmdGroupBytes[scsi_cmd[0] >> 5];

    int target = get_target(lun);
    int status = scsiInitiatorRunCommand(target,
                                         scsi_cmd, cmdlen,
                                         NULL, 0,
                                         (const uint8_t*)buffer, bufsize);

    LED_OFF();

    return status;
}

static int do_read6_or_10(int target_id, uint32_t start_sector, uint32_t sectorcount, uint32_t sectorsize, void *buffer)
{
    int status;

    // Read6 command supports 21 bit LBA - max of 0x1FFFFF
    // ref: https://www.seagate.com/files/staticfiles/support/docs/manual/Interface%20manuals/100293068j.pdf pg 134
    if (start_sector < 0x1FFFFF && sectorcount <= 256)
    {
        // Use READ6 command for compatibility with old SCSI1 drives
        uint8_t command[6] = {0x08,
            (uint8_t)(start_sector >> 16),
            (uint8_t)(start_sector >> 8),
            (uint8_t)start_sector,
            (uint8_t)sectorcount,
            0x00
        };

        // Note: we must not call platform poll in the commands,
        status = scsiInitiatorRunCommand(target_id, command, sizeof(command), (uint8_t*)buffer, sectorcount * sectorsize, NULL, 0);
    }
    else
    {
        // Use READ10 command for larger number of blocks
        uint8_t command[10] = {0x28, 0x00,
            (uint8_t)(start_sector >> 24), (uint8_t)(start_sector >> 16),
            (uint8_t)(start_sector >> 8), (uint8_t)start_sector,
            0x00,
            (uint8_t)(sectorcount >> 8), (uint8_t)(sectorcount),
            0x00
        };

        status = scsiInitiatorRunCommand(target_id, command, sizeof(command), (uint8_t*)buffer, sectorcount * sectorsize, NULL, 0);
    }

    return status;
}

int32_t init_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
    if (g_msc_initiator_target_count == 0)
    {
        return -1;
    }

    LED_ON();
    int status = 0;

    int target_id = get_target(lun);
    int sectorsize = g_msc_initiator_targets[lun].sectorsize;
    uint32_t sectorcount = bufsize / sectorsize;
    uint32_t total_sectorcount = sectorcount;
    uint32_t orig_lba = lba;

    if (sectorcount == 0)
    {
        // Not enough buffer left for a full sector
        return 0;
    }

    if (g_msc_initiator_state.prefetch_done)
    {
        int32_t offset = (int32_t)lba - (int32_t)g_msc_initiator_state.prefetch_lba;
        uint8_t *dest = (uint8_t*)buffer;
        while (offset >= 0 && offset < g_msc_initiator_state.prefetch_sectorcount && sectorcount > 0)
        {
            // Copy sectors from prefetch
            memcpy(dest, g_msc_initiator_state.prefetch_buffer + sectorsize * offset, sectorsize);
            dest += sectorsize;
            offset += 1;
            lba += 1;
            sectorcount -= 1;
        }
    }

    if (sectorcount > 0)
    {
        dbgmsg("USB Read command ", (int)orig_lba, " + ", (int)total_sectorcount, "x", (int)sectorsize,
               " got ", (int)(total_sectorcount - sectorcount), " sectors from prefetch");
        status = do_read6_or_10(target_id, lba, sectorcount, sectorsize, buffer);
        lba += sectorcount;
    }
    else
    {
        dbgmsg("USB Read command ", (int)orig_lba, " + ", (int)total_sectorcount, "x", (int)sectorsize, " fully satisfied from prefetch");
    }

    g_msc_initiator_state.status_reqcount++;
    g_msc_initiator_state.status_bytecount += total_sectorcount * sectorsize;
    LED_OFF();

    if (status != 0)
    {
        uint8_t sense_key;
        scsiRequestSense(target_id, &sense_key);

        if (sense_key == RECOVERED_ERROR)
        {
            dbgmsg("SCSI Initiator read: RECOVERED_ERROR at ", (int)orig_lba);
        }
        else if (sense_key == UNIT_ATTENTION)
        {
            dbgmsg("SCSI Initiator read: UNIT_ATTENTION");
        }
        else
        {
            scsiLogInitiatorCommandFailure("SCSI Initiator read", target_id, status, sense_key);
            return -1;
        }
    }

    if (lba + total_sectorcount <= g_msc_initiator_targets[lun].sectorcount)
    {
        int prefetch_sectorcount = total_sectorcount;
        if (prefetch_sectorcount * sectorsize > g_msc_initiator_state.prefetch_bufsize)
        {
            prefetch_sectorcount = g_msc_initiator_state.prefetch_bufsize / sectorsize;
        }

        // Request prefetch of the next block while USB transfers the previous one
        g_msc_initiator_state.prefetch_lba = lba;
        g_msc_initiator_state.prefetch_target_id = target_id;
        g_msc_initiator_state.prefetch_sectorcount = total_sectorcount;
        g_msc_initiator_state.prefetch_sectorsize = sectorsize;
        g_msc_initiator_state.prefetch_done = false;
    }

    return total_sectorcount * sectorsize;
}

int32_t init_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
    if (g_msc_initiator_target_count == 0)
    {
        return -1;
    }

    int status = -1;

    int target_id = get_target(lun);
    int sectorsize = g_msc_initiator_targets[lun].sectorsize;
    uint32_t start_sector = lba;
    uint32_t sectorcount = bufsize / sectorsize;

    if (sectorcount == 0)
    {
        // Not a complete sector
        return 0;
    }

    LED_ON();

    // Write6 command supports 21 bit LBA - max of 0x1FFFFF
    if (start_sector < 0x1FFFFF && sectorcount <= 256)
    {
        // Use WRITE6 command for compatibility with old SCSI1 drives
        uint8_t command[6] = {0x0A,
            (uint8_t)(start_sector >> 16),
            (uint8_t)(start_sector >> 8),
            (uint8_t)start_sector,
            (uint8_t)sectorcount,
            0x00
        };

        status = scsiInitiatorRunCommand(target_id, command, sizeof(command), NULL, 0, buffer, bufsize);
    }
    else
    {
        // Use WRITE10 command for larger number of blocks
        uint8_t command[10] = {0x2A, 0x00,
            (uint8_t)(start_sector >> 24), (uint8_t)(start_sector >> 16),
            (uint8_t)(start_sector >> 8), (uint8_t)start_sector,
            0x00,
            (uint8_t)(sectorcount >> 8), (uint8_t)(sectorcount),
            0x00
        };

        status = scsiInitiatorRunCommand(target_id, command, sizeof(command), NULL, 0, buffer, bufsize);
    }

    g_msc_initiator_state.status_reqcount++;
    g_msc_initiator_state.status_bytecount += sectorcount * sectorsize;
    LED_OFF();

    if (status != 0)
    {
        uint8_t sense_key;
        scsiRequestSense(target_id, &sense_key);

        if (sense_key == RECOVERED_ERROR)
        {
            dbgmsg("SCSI Initiator write: RECOVERED_ERROR at ", (int)start_sector);
        }
        else if (sense_key == UNIT_ATTENTION)
        {
            dbgmsg("SCSI Initiator write: UNIT_ATTENTION");
        }
        else
        {
            scsiLogInitiatorCommandFailure("SCSI Initiator write", target_id, status, sense_key);
            return -1;
        }
    }

    return sectorcount * sectorsize;
}

void init_msc_write10_complete_cb(uint8_t lun)
{
    (void)lun;
}


#endif
