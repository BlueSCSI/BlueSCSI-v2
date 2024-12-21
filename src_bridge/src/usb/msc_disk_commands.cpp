// Command line commands for USB bridge mode
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

#ifdef LIB_FREERTOS_KERNEL
#include "FreeRTOS.h"
#ifdef __cplusplus
extern "C"
{
#endif
#include "FreeRTOS_CLI.h"
#include "cmd_console_task.h"
#ifdef __cplusplus
}
#endif
#include <stdio.h>
#include <stdarg.h>
#include "msc_disk.h"
#include <memory>
#include <string.h>

using namespace USB;
/*
 * Implements the task-stats command.
 */
static BaseType_t prvListDevicesCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t prvAddRamDiskCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);

// static char cmd_buffer[FREERTOS_CLI_MAX_OUTPUT_LENGTH];

/* Structure that defines the command line commands. */
static const CLI_Command_Definition_t xListDevices =
    {
        "list",
        "\nlist:\n Lists all the devices connected to the system\n",
        prvListDevicesCommand, /* The function to run. */
        0                      /* There are no parameters to this function */
};
static const CLI_Command_Definition_t xAddRamDisk =
    {
        "ramdisk",
        "\nramdisk:\n Adds a ramdisk to the system\n",
        prvAddRamDiskCommand, /* The function to run. */
        0                     /* There are no parameters to this function */
};

void sprintf_disk(char *buffer, size_t buffer_len, std::shared_ptr<MscDisk> disk)
{
    if (disk == nullptr)
    {
        snprintf(buffer, buffer_len, "  No Disk\n");
        return;
    }

    if (disk->IsRamDisk())
    {
        snprintf(buffer, buffer_len, "[RAM]      ");
    }
    else
    {
        snprintf(buffer, buffer_len, "[SCSI-%d:%d]", disk->getAnsiVersion(), disk->getTargetId());
    }
    size_t cur_str_len = strlen(buffer);
    buffer += cur_str_len;
    buffer_len -= cur_str_len;

    // snprintf(buffer, buffer_len, " %d (%.2f MB) %.8s %.16s %.4s\n",
    snprintf(buffer, buffer_len, " %d (%.2f MB) %s %s %s\n",
             disk->getTotalSize(),
             (float)disk->getTotalSize() / 1024 / 1024, "x", "y", "z");
            //  disk->getVendorId().c_str(),
            //  disk->getProductId().c_str(),
            //  disk->getProductRev().c_str());
}

// One lines printed before the disk information
const int FIRST_LINE = -1;
const int LAST_LINE = INT32_MAX;
static int list_devices_line_counter = FIRST_LINE;
static BaseType_t prvListDevicesCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    (void)pcCommandString;
    configASSERT(pcWriteBuffer);
    BaseType_t moreLines = pdFALSE;
    static const char disk_separator_line[] = "----+----------+---------------------";

    int last_disk = USB::MscDisk::DiskList.size()-1;
    switch (list_devices_line_counter)
    {
    case FIRST_LINE:
        snprintf(pcWriteBuffer, xWriteBufferLen, "LUN | DISK     | SIZE\n%s\n", disk_separator_line);
        list_devices_line_counter++;
        moreLines = pdTRUE; // More lines to print
        break;
    case (LAST_LINE):
        // Last line... print total disks
        snprintf(pcWriteBuffer, xWriteBufferLen, "%s\nTotal Disks: %d\n",disk_separator_line, USB::MscDisk::DiskList.size());
        // Reset to first line
        list_devices_line_counter = FIRST_LINE;
        moreLines = pdFALSE; // No more data to print
        break;
    default:
        auto current_disk = USB::MscDisk::GetMscDiskByLun(list_devices_line_counter);
        if (current_disk == nullptr)
        {
            snprintf(pcWriteBuffer, xWriteBufferLen, "%s INTERNAL ERROR: null ptr\n", __func__);
            list_devices_line_counter = FIRST_LINE; // Reset for next iteration
            return pdFALSE;
        }

        snprintf(pcWriteBuffer, xWriteBufferLen, "%3d | %5s | %d bytes (%.2f MB)\n", \
            list_devices_line_counter, current_disk->toString(), current_disk->getTotalSize(), ((float)current_disk->getTotalSize())/1024/1024);
        if (list_devices_line_counter >= last_disk)
        {
            list_devices_line_counter = LAST_LINE;
        }
        else
        {
            list_devices_line_counter++;
        }
        moreLines = pdTRUE; // More lines to print
        break;
    }
    return moreLines;
}

static BaseType_t prvAddRamDiskCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    (void)pcCommandString;
    configASSERT(pcWriteBuffer);

    snprintf(pcWriteBuffer, xWriteBufferLen, "Not implemented yet.....\n");
    return pdFALSE;
}

void msc_disk_register_cli(void)
{
    FreeRTOS_CLIRegisterCommand(&xListDevices);
    // FreeRTOS_CLIRegisterCommand(&xAddRamDisk);
}

#endif
