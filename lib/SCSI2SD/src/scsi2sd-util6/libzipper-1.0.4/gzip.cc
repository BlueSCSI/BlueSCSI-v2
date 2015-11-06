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
#include "gzip.hh"
#include "util.hh"
#include "deflate.hh"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

#include <string.h>

using namespace zipper;

namespace
{
	size_t
	findNull(const std::vector<uint8_t>& zipData, size_t start)
	{
		if (start >= zipData.size())
		{
			throw FormatException("Unexpected end-of-file");
		}

		while (zipData[start] != 0)
		{
			++start;
			if (start >= zipData.size())
			{
				throw FormatException("Unexpected end-of-file");
			}
		}
		return start;
	}

	class FileEntry : public CompressedFile
	{
	public:
		FileEntry(
			const ReaderPtr& reader,
			zsize_t dataOffset,
			const std::string& filename,
			time_t modTime
			) :
			m_reader(reader),
			m_dataOffset(dataOffset),
			m_fileName(filename)
		{
			m_modTime.tv_sec = modTime;
			m_modTime.tv_usec = 0;
		}

		virtual bool isDecompressSupported() const
		{
			return true;
		}

		virtual const std::string& getPath() const
		{
			return m_fileName;
		}

		virtual zsize_t getCompressedSize() const { return -1; }
		virtual zsize_t getUncompressedSize() const { return -1; }
		virtual const timeval& getModificationTime() const { return m_modTime; }

		virtual void decompress(Writer& writer)
		{
			zsize_t endCompressedBytes = m_reader->getSize() - 8; // CRC+ISIZE
			zsize_t inPos(m_dataOffset);
			zsize_t outPos(0);
			uint32_t crc(0);
			inflate(
				m_reader,
				writer,
				inPos,
				endCompressedBytes,
				outPos,
				crc);

			uint8_t crcBuffer[4];
			m_reader->readData(inPos, sizeof(crcBuffer), &crcBuffer[0]);
			uint32_t savedCRC = read32_le(&crcBuffer[0]);
			if (savedCRC != crc)
			{
				throw FormatException("Corrupt Data (CRC Failure)");
			}
		}

	private:
		ReaderPtr m_reader;
		zsize_t m_dataOffset;
		std::string m_fileName;
		timeval m_modTime;
	};
}

std::vector<zipper::CompressedFilePtr>
zipper::ungzip(const ReaderPtr& reader)
{
	enum
	{
		MaxHeader = 64*1024  // Artifical limit to simplify code
	};

	if (!isGzip(reader))
	{
		throw FormatException("Invalid gzip file");
	}

	std::vector<uint8_t> header(
		std::min(reader->getSize(), zsize_t(MaxHeader)));
	reader->readData(0, header.size(), &header[0]);

	if (header[2] != 8) // "deflate" method
	{
		throw UnsupportedException("Unknown gzip compression method");
	}

	bool fextra = (header[3] & 4) != 0;
	bool fname = (header[3] & 8) != 0;
	bool fcomment = (header[3] & 0x10) != 0;
	bool fhcrc = (header[3] & 2) != 0;

	time_t modTime = read32_le(&header[4]);

	size_t offset(10);

	if (fextra)
	{
		if (offset + 2 > header.size())
		{
			throw FormatException("Unexpected end-of-file");
		}
		uint16_t fextraBytes(read16_le(header, offset));
		offset += 2;

		offset += fextraBytes;
	}

	std::string embeddedName(reader->getSourceName());
	if (fname)
	{
		size_t nullOffset(findNull(header, offset));
		embeddedName =
			std::string(
				reinterpret_cast<char*>(&header[offset]), nullOffset - offset);
		offset = nullOffset + 1;
	}

	if (fcomment)
	{
		size_t nullOffset(findNull(header, offset));
		offset = nullOffset + 1;
	}

	if (fhcrc)
	{
		offset += 2;
	}

	if (offset >= header.size())
	{
		throw FormatException("Unexpected end-of-file");
	}

	std::vector<CompressedFilePtr> result;
	result.push_back(
		CompressedFilePtr(
			new FileEntry(reader, offset, embeddedName, modTime)));

	return result;
}

bool
zipper::isGzip(const ReaderPtr& reader)
{
	enum Constants
	{
		MinFileBytes = 18, // Header + CRC + size
		ID1 = 0x1f,
		ID2 = 0x8b
	};

	bool isGzip(false);
	if (reader->getSize() >= MinFileBytes)
	{
		uint8_t magic[2];
		reader->readData(0, sizeof(magic), &magic[0]);
		isGzip = (magic[0] == ID1) && (magic[1] == ID2);
	}
	return isGzip;
}

void
zipper::gzip(
	const std::string& filename,
	const Reader& reader,
	const WriterPtr& writer)
{
	enum Constants
	{
		ChunkSize = 64*1024,
		WindowBits = 15
	};

	static uint8_t Header[] =
	{
		0x1f, 0x8b, // ID
		0x08, // deflate
		0x8, // Flags (filename set)
		0x0, 0x0, 0x0, 0x0, // mtime
		0x0, // Extra flags
		0xff  // OS
	};

	zsize_t outPos(writer->getSize());

	// Write header
	{
		uint8_t buffer[ChunkSize];
		memcpy(buffer, Header, sizeof(Header));

		write32_le(reader.getModTime().tv_sec, &buffer[4]); // modtime

		zsize_t pos(sizeof(Header));

		zsize_t filenameSize(filename.size());
		if (filenameSize > (ChunkSize - pos - 1))
		{
			filenameSize = ChunkSize - pos - 1;
		}
		std::copy(&filename[0], &filename[filenameSize], &buffer[pos]);
		pos += filenameSize;
		buffer[pos++] = '\0';

		writer->writeData(outPos, pos, &buffer[0]);
		outPos += pos;
	}

	// Compress data
	zsize_t uncompressedSize(0);
	zsize_t compressedSize(0);
	uint32_t crc(0);
	deflate(reader, writer, outPos, uncompressedSize, compressedSize, crc);

	// Write trailer.
	uint8_t trailer[8];
	write32_le(crc, &trailer[0]);
	write32_le(reader.getSize(), &trailer[4]);
	writer->writeData(outPos, sizeof(trailer), &trailer[0]);
}

