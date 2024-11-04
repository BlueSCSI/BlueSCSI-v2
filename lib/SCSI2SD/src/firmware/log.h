//	Copyright (C) 2023 joshua stein <jcs@jcs.org>
//
//	This file is part of SCSI2SD.
//
//	SCSI2SD is free software: you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	SCSI2SD is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with SCSI2SD.  If not, see <http://www.gnu.org/licenses/>.
#ifndef SCSI2SD_LOG_H
#define SCSI2SD_LOG_H
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif

#ifdef NETWORK_DEBUG_LOGGING
extern bool g_log_debug;

extern void logmsg_buf(const unsigned char *buf, unsigned long size);
extern void logmsg_f(const char *format, ...);

extern void dbgmsg_buf(const unsigned char *buf, unsigned long size);
extern void dbgmsg_f(const char *format, ...);

#define DBGMSG_BUF(buf, size) dbgmsg_buf(buf, size)
#define DBGMSG_F(format, ...) dbgmsg_f(format, __VA_ARGS__)
#define LOGMSG_BUF(buf, size) logmsg_buf(buf, size)
#define LOGMSG_F(format, ...) logmsg_f(format, __VA_ARGS__)

#else

#define DBGMSG_BUF(buf, size) // Empty
#define DBGMSG_F(format, ...) // Empty
#define LOGMSG_BUF(buf, size) // Empty
#define LOGMSG_F(format, ...) // Empty

#endif // NETWORK_DEBUG_LOGGING

#ifdef __cplusplus
}
#endif

#endif // SCSI2SD_LOG_H