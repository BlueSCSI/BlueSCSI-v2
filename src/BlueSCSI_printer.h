/*
 * Copyright (c) 2026 Eric Helgeson
 *
 * This file is part of BlueSCSI.
 *
 * BlueSCSI is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BlueSCSI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// Apple LaserWriter IISC SCSI printer emulation.

#ifndef BLUESCSI_PRINTER_H
#define BLUESCSI_PRINTER_H

#ifdef __cplusplus
extern "C" {
#endif

// Returns 1 if the command was handled by the printer layer, 0 to fall
// through to the generic disk handler.
int scsiPrinterCommand(void);

// Format of the spool file (per-print-job, /PR<id>/print_NNNN.bin):
//
//   repeat:
//     uint8_t  cdb[6];           // raw 6-byte CDB
//     uint32_t payload_len;      // big-endian byte count
//     uint8_t  payload[payload_len];
//
// The PRINT (0x0A) command with bit 7 of byte 5 set marks end-of-job and
// closes the file. SETUP (0x06) at the start of a new job truncates the
// previous file (PRINT may not have been seen between jobs on cancel).

#ifdef __cplusplus
}
#endif

#endif // BLUESCSI_PRINTER_H
