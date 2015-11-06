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
#include "deflate.hh"
#include "util.hh"

#include <zlib.h>

#include <cassert>
#include <iostream>

using namespace zipper;

namespace
{
	struct DeflateDeleter
	{
	public:
		DeflateDeleter(z_stream* stream) : m_stream(stream) {}
		~DeflateDeleter()
		{
			deflateEnd(m_stream);

		}
	private:
		z_stream* m_stream;
	};

	struct InflateDeleter
	{
	public:
		InflateDeleter(z_stream* stream) : m_stream(stream) {}
		~InflateDeleter()
		{
			inflateEnd(m_stream);

		}
	private:
		z_stream* m_stream;
	};

	enum Constants
	{
		ChunkSize = 64*1024,
		WindowBits = 15
	};
}

void
zipper::deflate(
	const Reader& reader,
	const WriterPtr& writer,
	zsize_t& writeOffset,
	zsize_t& uncompressedSize,
	zsize_t& compressedSize,
	uint32_t& crc)
{
	uint8_t inChunk[ChunkSize];
	uint8_t outChunk[ChunkSize];

	uncompressedSize = 0;
	compressedSize = 0;

	z_stream stream;
	stream.zalloc = NULL;
	stream.zfree = NULL;
	stream.opaque = NULL;
	int zlibErr(
		deflateInit2(
			&stream,
			Z_DEFAULT_COMPRESSION,
			Z_DEFLATED,
			-WindowBits,
			MAX_MEM_LEVEL,
			Z_DEFAULT_STRATEGY)
			);

	assert(zlibErr == Z_OK);
	DeflateDeleter deleter(&stream);
	stream.next_in = NULL;
	stream.avail_in = 0;
	bool finished(false);

	zsize_t pos(0);
	zsize_t end(reader.getSize());
	crc = crc32(0, NULL, 0);

	while (!finished)
	{
		if ((stream.avail_in == 0) && (pos < end))
		{
			stream.avail_in =
				std::min(zsize_t(ChunkSize), end - pos);
			reader.readData(
				pos, stream.avail_in, &inChunk[0]);
			stream.next_in = reinterpret_cast<Bytef*>(&inChunk);
			pos += stream.avail_in;
			uncompressedSize += stream.avail_in;
			crc = crc32(crc, stream.next_in, stream.avail_in);
		}

		stream.next_out = reinterpret_cast<Bytef*>(&outChunk);
		stream.avail_out = sizeof(outChunk);

		finished = false;
		zlibErr = deflate(&stream, (pos < end) ? Z_NO_FLUSH : Z_FINISH);

		if (zlibErr == Z_STREAM_END)
		{
			if (pos < end)
			{
				assert(!"zlib buffer unexpectedly empty");
				std::terminate();
			}
			finished = true;
		}
		else if (zlibErr != Z_OK)
		{
			throw FormatException("Corrupt Data");
		}

		zsize_t bytesToWrite(sizeof(outChunk) - stream.avail_out);
		writer->writeData(writeOffset, bytesToWrite, &outChunk[0]);
		writeOffset += bytesToWrite;
		compressedSize += bytesToWrite;
	}
}


void
zipper::inflate(
	const ReaderPtr& reader,
	Writer& writer,
	zsize_t& readOffset,
	zsize_t readEnd,
	zsize_t& writeOffset,
	uint32_t& crc)
{
	uint8_t inChunk[ChunkSize];
	uint8_t outChunk[ChunkSize];

	z_stream stream;
	stream.zalloc = NULL;
	stream.zfree = NULL;
	stream.opaque = NULL;
	int zlibErr(inflateInit2(&stream, -WindowBits));
	assert(zlibErr == Z_OK);
	InflateDeleter deleter(&stream);
	stream.next_in = NULL;
	stream.avail_in = 0;
	bool finished(false);

	zsize_t pos(readOffset);
	crc = crc32(0, NULL, 0);
	while (!finished)
	{
		if (stream.avail_in == 0)
		{
			stream.avail_in = std::min(zsize_t(ChunkSize), readEnd - pos);
			if (stream.avail_in == 0)
			{
				break;
			}
			reader->readData(pos, stream.avail_in, &inChunk[0]);
			stream.next_in = reinterpret_cast<Bytef*>(&inChunk);
			pos += stream.avail_in;
		}

		stream.next_out = reinterpret_cast<Bytef*>(&outChunk);
		stream.avail_out = sizeof(outChunk);

		zlibErr = inflate(&stream, Z_SYNC_FLUSH);

		finished = false;
		if (zlibErr == Z_STREAM_END)
		{
			finished = true;
		}
		else if (zlibErr != Z_OK)
		{
			throw FormatException("Corrupt Data");
		}

		zsize_t bytesToWrite(sizeof(outChunk) - stream.avail_out);
		writer.writeData(writeOffset, bytesToWrite, &outChunk[0]);
		writeOffset += bytesToWrite;
		crc = crc32(crc, &outChunk[0], bytesToWrite);
	}
	if (!finished)
	{
		// Ran out of data to process
		throw FormatException("Corrupt Data");
	}

	// We've read data that wasn't consumed!
	readOffset = pos - stream.avail_in;
}

