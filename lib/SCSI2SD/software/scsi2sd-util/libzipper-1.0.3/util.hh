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

#ifndef zipper_util_hh
#define zipper_util_hh

namespace zipper
{
	template <typename T>
	struct
	dummy_delete
	{
		void operator()(T*) {}
	};

	template<typename T>
	uint32_t
	read32_le(const T& inArray, size_t pos = 0)
	{
		// Read 4 bytes in little-endian order.
		// Return results in host-endian.
		return uint32_t(
			inArray[pos] |
			(uint32_t(inArray[pos+1]) << 8) |
			(uint32_t(inArray[pos+2]) << 16) |
			(uint32_t(inArray[pos+3]) << 24)
			);
	}

	template<typename T>
	uint16_t
	read16_le(const T& inArray, size_t pos = 0)
	{
		// Read 2 bytes in little-endian order.
		// Return results in host-endian.
		return uint16_t(
			inArray[pos] |
			(uint16_t(inArray[pos+1]) << 8)
			);
	}

	template<typename T>
	void
	write32_le(uint32_t value, T& outArray, size_t pos = 0)
	{
		// Write 4 bytes in little-endian order.
		outArray[pos] = value & 0xff;
		outArray[pos + 1] = (value >> 8) & 0xff;
		outArray[pos + 2] = (value >> 16) & 0xff;
		outArray[pos + 3] = (value >> 24) & 0xff;
	}

	template<typename T>
	void
	write32_le(uint32_t value, T* outArray, size_t pos = 0)
	{
		// Write 4 bytes in little-endian order.
		outArray[pos] = value & 0xff;
		outArray[pos + 1] = (value >> 8) & 0xff;
		outArray[pos + 2] = (value >> 16) & 0xff;
		outArray[pos + 3] = (value >> 24) & 0xff;
	}

	template<typename T>
	void
	write16_le(uint16_t value, T& outArray, size_t pos = 0)
	{
		// Write 4 bytes in little-endian order.
		outArray[pos] = value & 0xff;
		outArray[pos + 1] = (value >> 8);
	}

	template<typename T>
	void
	write16_le(uint16_t value, T* outArray, size_t pos = 0)
	{
		// Write 4 bytes in little-endian order.
		outArray[pos] = value & 0xff;
		outArray[pos + 1] = (value >> 8);
	}
}

#endif

