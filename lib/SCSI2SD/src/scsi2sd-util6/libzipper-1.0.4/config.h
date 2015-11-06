//	Copyright (C) 2014 Michael McMaster <michael@codesrc.com>
//
//	This file is part of libzipper.
//
//	libzipper is free software: you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	libzipper is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with libzipper.  If not, see <http://www.gnu.org/licenses/>.

#ifndef zipper_config_h
#define zipper_config_h

#include "autoconfig.h"
#include <sys/types.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HAVE_PREAD
// Use port/pread.c
extern ssize_t pread(int fd, void *buf, size_t nbyte, off_t offset);
#endif
#ifndef HAVE_PWRITE
// Use port/pwrite.c
extern ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
#endif

#ifndef HAVE_UTIMES
extern int utimes(const char *filename, const struct timeval times[2]);
#endif

// Thread un-safe alternative
#ifndef HAVE_LOCALTIME_R
#define localtime_r(timep, result) localtime( (timep) )
#endif

#ifdef _WIN32
	#define MKDIR(file,mode) mkdir(file)
#else
	#define MKDIR(file,mode) mkdir(file,mode)

	// no automagic CR/LF conversion undr mingw
	#define O_BINARY 0
#endif

#ifdef __cplusplus
// extern "C"
}
#endif

#endif
