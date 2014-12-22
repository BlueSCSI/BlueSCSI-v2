//	Copyright (C) 2011 Michael McMaster <michael@codesrc.com>
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

#include "../strerror.hh"

#include <string.h>

using namespace zipper;

std::string
zipper::strerror(int errnum)
{
	char buf[1024];
	const char* message(NULL);

#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600)
	int error(strerror_r(errnum, buf, sizeof(buf)));
	if (error != 0)
	{
		message = "Unknown error";
	}
#else
	// Thread-unsafe fallback
	message = ::strerror(errnum);
#endif

	return std::string(message);
}

