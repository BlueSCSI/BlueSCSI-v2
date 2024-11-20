/**
 * Copyright (C) 2024 Eric Helgeson
 *
 * This file is part of BlueSCSI
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

#include "BlueSCSI_platform.h"
#include "BlueSCSI_console.h"
#include "BlueSCSI_disk.h"
#include "BlueSCSI_cdrom.h"

char serial_buffer[MAX_SERIAL_INPUT_CHARS] = {0};
int cdb_len = 0;
String inputString;
image_config_t img;

void clearBuffer() {
    memset(serial_buffer, 0, sizeof(serial_buffer));
}

void printBinary(uint8_t num) {
    for (int i = sizeof(num) * 8 - 1; i >= 0; i--) {
        (num & (1 << i)) ? log_raw("1") : log_raw("0");
    }
    log("\nSCSI ID:           76543210");
}

void handleUsbInputTargetMode(int32_t data)
{
    uint8_t size = strlen(serial_buffer);
    debuglog("buffer size: ", size);
    serial_buffer[size] = tolower((char)data);
    debuglog("buffer: ", serial_buffer);
    bool valid_scsi_id = false;
    volatile uint32_t* scratch0 = (uint32_t *)(WATCHDOG_BASE + WATCHDOG_SCRATCH0_OFFSET);
    char name[MAX_FILE_PATH+1];
    char rename[MAX_FILE_PATH+1];
    // Echo except for newline.
    if(serial_buffer[size] != '\n') {
        log_f("%c", serial_buffer[size]);
    }
    // Numeric input
    if ((char)data >= '0' && (char)data <= '9')
    {
        uint8_t num_input = atoi(&serial_buffer[1]);
        debuglog("num_input: ", num_input);
        if(num_input >= 0 && num_input < 8) valid_scsi_id = true;
        else valid_scsi_id = false;

        switch(serial_buffer[0])
        {
            case 'e':
                if(!valid_scsi_id) break;
                log("Ejecting SCSI ID ", (int)num_input);
                img = scsiDiskGetImageConfig(num_input);
                // todo if valid id
                if(img.deviceType == S2S_CFG_OPTICAL)
                    cdromPerformEject(img);
                else if(img.deviceType == S2S_CFG_REMOVEABLE)
                    removableEject(img);
                else
                    log("Not an eject-able drive.");
                clearBuffer();
                break;
            case 'x':
                if(!valid_scsi_id) break;
                img = scsiDiskGetImageConfig(num_input);
                img.file.getName(name, MAX_FILE_PATH+1);
                debuglog("Found file ", name);
                if (name[0] != '\0' && name[0] != DISABLE_CHAR) {
                    log("Disabling SCSI ID ", (int)num_input);
                    if(img.image_directory) {
                        // FIXME: hard coded to CD - lookup imgdir by type
                        snprintf(name, sizeof(name), "CD%d", num_input);
                        snprintf(rename, sizeof(rename), "%cCD%d", DISABLE_CHAR, num_input);
                        debuglog("name: ", name, " rename: ", rename);
                        SD.rename(name, rename);
                    } else {
                        memmove(name + 1, name, strlen(name) + 1);
                        name[0] = DISABLE_CHAR;
                        img.file.rename(name);
                    }
                } else {
                    FsFile dir;
                    FsFile file;
                    dir.openCwd();
                    dir.rewindDirectory();
                    while (file.openNext(&dir, O_RDONLY)) {
                        file.getName(name, MAX_FILE_PATH);
                        debuglog("list files: ", name);
                        if(name[0] == DISABLE_CHAR && (name[3] - '0') == num_input) {
                            memmove(name, name + 1, strlen(name));
                            file.rename(name);
                            log("Enabling SCSI ID ", (int) num_input, " ", name);
                            break;
                        }
                    }
                }
                clearBuffer();
                break;
            case 'p':
                log("Switching to profile ID ", (int)num_input);
                log("NOTE: Placeholder command, no action taken.");
                clearBuffer();
                break;
            case 'm':
                g_scsi_log_mask = (uint8_t)num_input;
                log_raw("Set debug mask to: ");
                printBinary(g_scsi_log_mask);
                log("Hit return to complete mask entry.");
                break;
        }
    }

    switch(serial_buffer[size]) {
        case 'e':
            log_raw("Enter SCSI ID to eject: ");
            break;
        case 'x':
            log_raw("Enter SCSI ID to disable/enable: ");
            break;
        case 'p':
            log_raw("Enter profile ID to switch to: ");
            break;
        case 'd':
            g_log_debug = !g_log_debug;
            log("Debug flipped to ", g_log_debug);
            clearBuffer();
            break;
        case 'm':
            log_raw("Enter debug mask as int: ");
            break;
        case 'r':
            log("Rebooting...");
            *scratch0 = PICO_REBOOT_MAGIC;
            watchdog_reboot(0, 0, 2000);
            break;
        case 'l':
            printConfiguredDevices();
            clearBuffer();
            break;
        case 'b':
            log("Rebooting into uf2 bootloader....");
            rom_reset_usb_boot(0, 0);
            break;
        case 106:
            log("Why did BlueSCSI start a bakery? Because it loved making *byte*-sized treats!");
            break;
        case 'h':
            log("\nAvailable commands:");
            log("  e <SCSI_ID>: Eject the specified SCSI device");
            log("  x <SCSI_ID>: Disable/Enable the specified SCSI device");
            log("  p <PROFILE_ID>: Switch to the specified profile");
            log("  l: List configured SCSI Devices");
            log("  d: Toggle debug mode");
            log("  m: Debug Mask as integer");
            log("  r: Reboot the system");
            log("  b: Reboot to uf2 bootloader");
            log("  h: Display this help message\n");
            clearBuffer();
            break;
        case '\n':
            if(serial_buffer[0] == 'm')
                log("Mask set complete.");
            log_raw("Command: ");
            clearBuffer();
            break;
        default:
            // Don't clear buffer here as we may be inputting a multi digit number.
            debuglog("Unknown input, but wait for newline.");
    }
}

/**
 * Check to see if we should pause initiator and setup interactive user console.
 */
void handleUsbInputInitiatorMode()
{
    if (Serial.available()) {

        int32_t data = tolower((char) Serial.read());
        if(data == 'p')
        {
            Serial.println("Pausing initiator scan and starting initiator console...");
            initiatorConsoleLoop();
        }
    }
}

/**
 *  Hold initiator loop and handle commands
 *  Must use Serial directly here as logger isn't called frequently enough for user interaction.
 */
void initiatorConsoleLoop()
{
    int target_id = 0;
    Serial.printf("Current Target: %d\n", target_id);
    uint8_t response_buffer[RESPONSE_BUFFER_LEN] = {0};
    bool in_console = true;
    int new_id;
    size_t len;
    Serial.println("Command: ");
    Serial.flush();
    while (in_console) {
        int32_t data = tolower((char) Serial.read());
        if (!data) continue;
        switch((char)data) {
            case 'c':
                Serial.println("c\nEnter CDB (6 or 10 length) followed by newline: ");
                Serial.flush();
                clearBuffer();
                Serial.setTimeout(INT_MAX);
                len = Serial.readBytesUntil('\n', serial_buffer, MAX_SERIAL_INPUT_CHARS);
                Serial.setTimeout(1);
                serial_buffer[len-1] = '\0'; // remove new line
//                Serial.printf("User CDB input: %s\n", serial_buffer);
                uint8_t cdb[MAX_CBD_LEN];
                cdb_len = hexToBytes(serial_buffer, cdb, 10);

                if (cdb_len > 0) {
                    Serial.printf("Parsed CDB (%d bytes): ", cdb_len);
                    for (size_t i = 0; i < cdb_len; i++) {
                        Serial.printf("%02X", cdb[i]);
                    }
                    Serial.println();

                    int status = scsiInitiatorRunCommand(target_id,
                                                         cdb, cdb_len,
                                                         response_buffer, RESPONSE_BUFFER_LEN,
                                                         nullptr, 0);

                    Serial.printf("SCSI Command Status: %d\n", status);
                    if(status == 0) {
                        Serial.println("Command succeeded!");
                        for (size_t i = 0; i < RESPONSE_BUFFER_LEN; i++) {
                            Serial.printf("%02x ", response_buffer[i]);
                        }
                    } else if (status == 2) {
                        uint8_t sense_key;
                        scsiRequestSense(target_id, &sense_key);
                        Serial.printf("Command on target %d failed, sense key %d\n", target_id, sense_key);
                    } else if (status == -1) {
                        Serial.printf("Target %d did not respond.\n", target_id);
                    }
                } else {
                    Serial.println("Timed out waiting for CDB from input.");
                }
                clearBuffer();
                break;
            case 'r':
                Serial.println("Resuming Initiator main loop.");
                in_console = false;
                break;
            case 't':
                Serial.print("Enter SCSI ID for target [0-7]: ");
                Serial.flush();
                Serial.setTimeout(INT_MAX);
                Serial.readBytes(serial_buffer,1);
                Serial.setTimeout(1);
                new_id = atoi(serial_buffer);
                Serial.printf("%d\nNew Target entry: %d\n", new_id, new_id);
                target_id = new_id;
                clearBuffer();
                break;
            case 'h':
                Serial.println("\nAvailable commands:");
                Serial.println("  c <CDB>: Run a CDB against the current target.");
                Serial.println("  t <SCSI_ID>: Set the current target");
                Serial.println("  r: resume initiator scanning");
                Serial.println("  h: Display this help message\n");
                clearBuffer();
                break;
            case '\n':
                Serial.print("Command: ");
                clearBuffer();
                break;
        }
        Serial.flush();
        // We're holding the loop so just keep resetting the watchdog till we're done.
        platform_reset_watchdog();
    }
    Serial.flush();
    Serial.setTimeout(1);
}

/**
 * Given a string of hex, convert it into raw bytes that that hex would represent.
 * @return size
 */
int hexToBytes(const char *hex_string, uint8_t *bytes_array, size_t max_length) {
    size_t len = strlen(hex_string);

    if ((len != 12 && len != 20) || (len % 2 != 0)) {
        return -1; // Invalid input length
    }

    for (size_t i = 0; i < min(len / 2, max_length); i++) {
        sscanf(&hex_string[i*2], "%2hhx", &bytes_array[i]);
    }

    return min(len / 2, max_length);
}

void serialPoll()
{
    if (Serial.available() && !platform_is_initiator_mode_enabled()) {
        int32_t data = Serial.read();
        if (data) {
            handleUsbInputTargetMode(data);
        }
    }
}