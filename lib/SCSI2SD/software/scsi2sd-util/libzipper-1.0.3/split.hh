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

#ifndef zipper_split_hh
#define zipper_split_hh

#include <string>

namespace zipper
{

	template<typename OutputIterator>
	void
	split(const std::string& in, char delim, OutputIterator out)
	{
		std::string::size_type start(0);
		std::string::size_type pos(0);
		while (pos < in.size())
		{
			if (in[pos] == delim)
			{
				if (pos != start)
				{
					*out = in.substr(start, pos - start);
					++out;
				}
				++pos;
				start = pos;
			}
			else
			{
				++pos;
			}
		}

		if (pos != start)
		{
			*out = in.substr(start, pos - start);
			++out;
		}
	}
}

#endif
