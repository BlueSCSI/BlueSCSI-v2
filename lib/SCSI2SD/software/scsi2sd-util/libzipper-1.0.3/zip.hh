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

#include <string>
#include <vector>

namespace zipper
{
	struct ZipFileRecord
	{
		zsize_t localHeaderOffset;
		uint32_t crc32;
		zsize_t compressedSize;
		zsize_t uncompressedSize;
		uint16_t dosDate;
		uint16_t dosTime;
		std::string filename;
	};

	void zip(
		const std::string& filename,
		const Reader& reader,
		const WriterPtr& writer,
		ZipFileRecord& outRecord);

	void zipFinalise(
		const std::vector<ZipFileRecord>& records,
		const WriterPtr& writer);

	bool isZip(const ReaderPtr& reader);

	std::vector<CompressedFilePtr> unzip(const ReaderPtr& reader);
}

