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

#include "zipper.hh"

using namespace zipper;

namespace
{
	struct Container info_none =
		{ Container_none, "application/octet-stream", 13 };
	struct Container info_zip =
		{ Container_zip, "application/zip", 0x1f };
	struct Container info_gzip =
		{ Container_gzip, "zpplication/x-gzip", 7 };
}

namespace zipper
{
	const Container&
	getContainer(ContainerFormat format)
	{
		switch (format)
		{
		case Container_none: return info_none;
		case Container_zip: return info_zip;
		case Container_gzip: return info_gzip;
		default: throw Exception("Unknown format type requested");
		}

	}
}

