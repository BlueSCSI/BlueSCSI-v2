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

#include <sys/types.h>
#include <sys/time.h>
#include <utime.h>

// Replacement function using the older low-resolution utime function
int utimes(const char *filename, const struct timeval times[2])
{
	struct utimbuf buf;
	buf.actime = times[0].tv_sec;
	buf.modtime = times[1].tv_sec;
	return utime(filename, &buf);
}

