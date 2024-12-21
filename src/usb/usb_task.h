// FreeRTOS task that will periodically call the TinyUSB stack
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
//
#pragma once

// The following flag will change the startup behavior of the USB task.
// By setting this flag the rp2040 will immediately release the USB task
// but will hard-code the maximum number of USB LUNs to 16. This will
// cause performance issues, since the host will continuously run TEST 
// UNIT READY approximately every second to wait for these other LUNs to
// come online. Typically, this is only used for debugging purposes.
extern bool g_early_usb_initialization;

// The following flag is used to delay the usb task while we're configuring
// the rest of the system. In most cases, we don't want to start the USB 
// task to start until we know how many drives/devices are attached.
extern bool g_scsi_setup_complete;

// This is the main TinyUSB task that handles USB events
void usb_device_task(void *param);