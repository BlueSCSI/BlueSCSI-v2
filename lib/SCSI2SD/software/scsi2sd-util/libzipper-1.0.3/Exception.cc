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

Exception::Exception(const std::string& what) :
	std::runtime_error(what)
{
}

FormatException::FormatException(const std::string& what) :
	Exception(what)
{
}

UnsupportedException::UnsupportedException(const std::string& what) :
	Exception(what)
{
}

IOException::IOException(const std::string& what) :
	Exception(what)
{
}

