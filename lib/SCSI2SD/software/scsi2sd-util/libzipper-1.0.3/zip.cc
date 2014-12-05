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
#include "zip.hh"
#include "util.hh"
#include "deflate.hh"


#include <algorithm>
#include <cassert>
#include <iostream>

#include <time.h>
#include <string.h>

using namespace zipper;

namespace
{
	time_t convertDosDateTime(uint16_t date, uint16_t time)
	{
		struct tm parts;
		memset(&parts, 0, sizeof(parts));
		parts.tm_sec = time & 0x1F;
		parts.tm_min = (time & 0x7E0) >> 5;
		parts.tm_hour = (time  >> 11);
		parts.tm_mday = date & 0x1F;
		parts.tm_mon = ((date & 0x1E0) >> 5) - 1;
		parts.tm_year = (date >> 9) + 80;
		return mktime(&parts);
	}

	void convertDosDateTime(time_t in, uint16_t& date, uint16_t& time)
	{
		struct tm buf;
		struct tm* parts(localtime_r(&in, &buf));

		time =
			parts->tm_sec +
			(parts->tm_min << 5) +
			(parts->tm_hour << 11);

		date =
			parts->tm_mday +
			((parts->tm_mon + 1) << 5) +
			((parts->tm_year - 80) << 9);
	}

	class FileEntry : public CompressedFile
	{
	public:
		FileEntry(
			const ReaderPtr& reader,
			uint16_t versionNeeded,
			uint16_t gpFlag,
			uint16_t compressionMethod,
			uint32_t crc,
			zsize_t compressedSize,
			zsize_t uncompressedSize,
			time_t modTime,
			zsize_t localHeaderOffset,
			std::string fileName
			) :
			m_reader(reader),
			m_versionNeeded(versionNeeded),
			m_gpFlag(gpFlag),
			m_compressionMethod(compressionMethod),
			m_crc(crc),
			m_compressedSize(compressedSize),
			m_uncompressedSize(uncompressedSize),
			m_localHeaderOffset(localHeaderOffset),
			m_fileName(fileName)
		{
			m_modTime.tv_sec = modTime;
			m_modTime.tv_usec = 0;
		}

		virtual bool isDecompressSupported() const
		{
			return ((m_versionNeeded & 0xf) <= 20) &&
				((m_gpFlag & 0x1) == 0) && // Not encrypted
				((m_compressionMethod == 0) || (m_compressionMethod == 8));
		}

		virtual const std::string& getPath() const
		{
			return m_fileName;
		}

		virtual zsize_t getCompressedSize() const { return m_compressedSize; }
		virtual zsize_t getUncompressedSize() const
		{
			return m_uncompressedSize;
		}

		virtual const timeval& getModificationTime() const { return m_modTime; }

		virtual void decompress(Writer& writer)
		{
			enum
			{
				Signature = 0x04034b50,
				MinRecordBytes = 30,
				ChunkSize = 64*1024
			};

			std::vector<uint8_t> localRecord(MinRecordBytes);
			m_reader->readData(
				m_localHeaderOffset, MinRecordBytes, &localRecord[0]
				);
			if (read32_le(localRecord, 0) != Signature)
			{
				throw FormatException("Invalid local ZIP record");
			}

			// Don't trust the lengths for filename and extra content read from
			// the central records.  At least for extra, these DO differ for
			// unknown reasons
			zsize_t filenameLength(read16_le(localRecord, 26));
			zsize_t extraLength(read16_le(localRecord, 28));

			zsize_t startCompressedBytes(
				m_localHeaderOffset +
				MinRecordBytes +
				filenameLength +
				extraLength
				);

			zsize_t endCompressedBytes(
				startCompressedBytes + m_compressedSize
				);

			if (endCompressedBytes > m_reader->getSize())
			{
				throw FormatException("Compressed file size is too long");
			}

			switch (m_compressionMethod)
			{
			case 0: // No compression
			{
				for (zsize_t pos(startCompressedBytes);
					pos < endCompressedBytes;
					pos += ChunkSize
					)
				{
					uint8_t buf[ChunkSize];
					zsize_t bytes(
						std::min(zsize_t(ChunkSize), endCompressedBytes - pos)
						);
					m_reader->readData(pos, bytes, &buf[0]);
					writer.writeData(pos, bytes, &buf[0]);
				}
			}; break;

			case 8: // Deflate
			{
				uint32_t crc(0);
				zsize_t inPos(startCompressedBytes);
				zsize_t outPos(0);
				inflate(
					m_reader,
					writer,
					inPos,
					endCompressedBytes,
					outPos,
					crc);

				if (m_gpFlag & 0x4) // CRC is after compressed data
				{
					uint8_t dataDescriptor[12];
					m_reader->readData(
						inPos, sizeof(dataDescriptor), &dataDescriptor[0]);
					m_crc = read32_le(dataDescriptor, 0);
					m_compressedSize = read32_le(dataDescriptor, 4);
					m_uncompressedSize = read32_le(dataDescriptor, 8);
				}

				if (crc != m_crc)
				{
					throw FormatException("Corrupt Data (CRC failure)");
				}

			}; break;
			default:
				throw UnsupportedException("Unsupported compression scheme");
			};
		}

	private:
		ReaderPtr m_reader;
		uint16_t m_versionNeeded;
		uint16_t m_gpFlag;
		uint16_t m_compressionMethod;
		uint32_t m_crc;
		zsize_t m_compressedSize;
		zsize_t m_uncompressedSize;
		timeval m_modTime;
		zsize_t m_localHeaderOffset;
		std::string m_fileName;
	};

	bool readEndCentralDirectory(
		const ReaderPtr& reader,
		zsize_t& centralDirectoryBytes,
		zsize_t& centralDirectoryOffset,
		zsize_t& centralDirectoryEntries
		)
	{
		// Read the end of central directory record. This
		// record enables us to find the remainding
		// records without searching for record signatures.

		// TODO does not consider the Zip64 entries.

		enum
		{
			MinRecordBytes = 22, // Minimum size with no comment
			MaxCommentBytes = 65535, // 2 bytes to store comment length
			Signature = 0x06054b50
		};

		zsize_t providerSize(reader->getSize());
		if (providerSize < MinRecordBytes)
		{
			throw FormatException("Too small");
		}

		size_t bufSize(
			std::min(zsize_t(MinRecordBytes + MaxCommentBytes), providerSize)
			);
		std::vector<uint8_t> buffer(bufSize);
		reader->readData(providerSize - bufSize, bufSize, &buffer[0]);

		// Need to search for this record, as it ends in a variable-length
		// comment field. Search backwards, with the assumption that the
		// comment doesn't exist, or is much smaller than the maximum
		// length

		bool recordFound(false);
		ssize_t pos(bufSize - MinRecordBytes);
		for (; pos >= 0; --pos)
		{
			recordFound = (read32_le(buffer, pos) == Signature);
			break;
		}

		if (recordFound)
		{
			if (read16_le(buffer, pos + 4) != 0)
			{
				throw UnsupportedException("Spanned disks not supported");
			}

			centralDirectoryBytes = read32_le(buffer, pos + 12);
			centralDirectoryOffset = read32_le(buffer, pos + 16);
			centralDirectoryEntries = read16_le(buffer, pos + 10);
		}
		return recordFound;
	}

	std::vector<CompressedFilePtr>
	readCentralDirectory(const ReaderPtr& reader)
	{
		enum Constants
		{
			MinRecordBytes = 46,
			Signature = 0x02014b50
		};

		zsize_t centralDirectoryBytes(0);
		zsize_t centralDirectoryOffset(0);
		zsize_t centralDirectoryEntries(0);
		bool isZip(
			readEndCentralDirectory(
				reader,
				centralDirectoryBytes,
				centralDirectoryOffset,
				centralDirectoryEntries
				)
			);
		(void) isZip; // Avoid unused warning.
		assert(isZip);

		std::vector<uint8_t> buffer(centralDirectoryBytes);
		reader->readData(
			centralDirectoryOffset,
			centralDirectoryBytes,
			&buffer[0]
			);

		zsize_t pos(0);
		std::vector<CompressedFilePtr> entries;
		while ((pos + MinRecordBytes) < buffer.size())
		{
			if (read32_le(buffer, pos) != Signature)
			{
				// Unknown record type.
				pos += 1;
				continue;
			}

			uint16_t versionNeeded(read16_le(buffer, pos + 6));
			uint16_t gpFlag(read16_le(buffer, pos + 8));
			uint16_t compressionMethod(read16_le(buffer, pos + 10));
			uint16_t modTime(read16_le(buffer, pos + 12));
			uint16_t modDate(read16_le(buffer, pos + 14));
			uint32_t crc(read32_le(buffer, pos + 16));
			uint32_t compressedSize(read32_le(buffer, pos + 20));
			uint32_t uncompressedSize(read32_le(buffer, pos + 24));
			size_t fileNameLen(read16_le(buffer, pos + 28));
			size_t extraLen(read16_le(buffer, pos + 30));
			size_t commentLen(read16_le(buffer, pos + 32));
			uint32_t localHeaderOffset(read32_le(buffer, pos + 42));

			if ((fileNameLen + extraLen + commentLen + MinRecordBytes + pos) >
				buffer.size()
				)
			{
				throw FormatException("File comments are too long");
			}

			std::string fileName(
				&buffer[pos + MinRecordBytes],
				&buffer[pos + MinRecordBytes + fileNameLen]
				);

			entries.push_back(
				CompressedFilePtr(
					new FileEntry(
						reader,
						versionNeeded,
						gpFlag,
						compressionMethod,
						crc,
						compressedSize,
						uncompressedSize,
						convertDosDateTime(modDate, modTime),
						localHeaderOffset,
						fileName
						)
					)
				);

			pos += MinRecordBytes + fileNameLen + extraLen + commentLen;
		}
		return entries;
	}

}


void
zipper::zip(
	const std::string& filename,
	const Reader& reader,
	const WriterPtr& writer,
	ZipFileRecord& outRecord)
{
	enum Constants
	{
		ChunkSize = 64*1024,
		WindowBits = 15,
		TimePos = 10
	};

	static uint8_t Header[] =
	{
		0x50, 0x4b, 0x03, 0x04,  // Header
		20, // Version (2.0)
		0, // File attributes
		0,0, // gp flag.
		8,0, // deflate method
		0,0, // file time
		0,0, // file date
		0,0,0,0, // CRC32
		0,0,0,0, // Compressed size
		0,0,0,0 // Uncompressed size
	};

	zsize_t outPos(writer->getSize());
	outRecord.localHeaderOffset = outPos;
	outRecord.filename = filename;

	// Write header
	{
		uint8_t buffer[ChunkSize];
		memcpy(buffer, Header, sizeof(Header));
		zsize_t pos(sizeof(Header));

		std::string::size_type filenameSize(filename.size());
		if (filenameSize > (ChunkSize - pos))
		{
			filenameSize = ChunkSize - pos;
		}
		buffer[pos++] = filenameSize & 0xff;
		buffer[pos++] = (filenameSize >> 8);
		buffer[pos++] = 0; // extra field len
		buffer[pos++] = 0; // extra field len
		memcpy(buffer + pos, filename.data(), filenameSize);
		pos += filenameSize;
		writer->writeData(outPos, pos, &buffer[0]);
		outPos += pos;
	}

	// Write compressed data
	deflate(
		reader,
		writer,
		outPos,
		outRecord.uncompressedSize,
		outRecord.compressedSize,
		outRecord.crc32);

	// Go back and complete the header.
	convertDosDateTime(
		reader.getModTime().tv_sec, outRecord.dosDate, outRecord.dosTime);
	uint8_t trailer[16];
	write16_le(outRecord.dosTime, &trailer[0]);
	write16_le(outRecord.dosDate, &trailer[2]);
	write32_le(outRecord.crc32, &trailer[4]);
	write32_le(outRecord.compressedSize, &trailer[8]);
	write32_le(outRecord.uncompressedSize, &trailer[12]);
	writer->writeData(
		outRecord.localHeaderOffset + TimePos, sizeof(trailer), &trailer[0]);
}

void
zipper::zipFinalise(
	const std::vector<ZipFileRecord>& records,
	const WriterPtr& writer)
{
	enum Constants
	{
		ChunkSize = 64*1024
	};

	static uint8_t FileHeader[] =
	{
		0x50, 0x4b, 0x01, 0x02,  // Header
		20, 0x00, // Version (2.0)
		20, 0x00, // Version Needed to extract (2.0)
		0,0, // gp flag.
		8,0 // deflate method
	};

	zsize_t outPos(writer->getSize());
	uint32_t centralDirOffset(outPos);

	for (size_t i = 0; i < records.size(); ++i)
	{
		uint8_t buffer[ChunkSize];
		memcpy(buffer, FileHeader, sizeof(FileHeader));
		zsize_t pos(sizeof(FileHeader));

		write16_le(records[i].dosTime, &buffer[pos]);
		pos += 2;
		write16_le(records[i].dosDate, &buffer[pos]);
		pos += 2;

		write32_le(records[i].crc32, &buffer[pos]);
		pos += 4;

		write32_le(records[i].compressedSize, &buffer[pos]);
		pos += 4;

		write32_le(records[i].uncompressedSize, &buffer[pos]);
		pos += 4;

		std::string::size_type filenameSize(records[i].filename.size());
		if (filenameSize > (ChunkSize - pos))
		{
			filenameSize = ChunkSize - pos;
		}
		write16_le(filenameSize, &buffer[pos]);
		pos += 2;

		write16_le(0, &buffer[pos]); // extra field len
		pos += 2;

		write16_le(0, &buffer[pos]); // file comment len
		pos += 2;

		write16_le(0, &buffer[pos]); // disk number
		pos += 2;

		write16_le(0, &buffer[pos]); // internal file attributes
		pos += 2;

		write32_le(0, &buffer[pos]); // external file attributes
		pos += 4;

		write32_le(records[i].localHeaderOffset, &buffer[pos]);
		pos += 4;

		memcpy(buffer + pos, records[i].filename.data(), filenameSize);
		pos += filenameSize;

		writer->writeData(outPos, pos, &buffer[0]);
		outPos += pos;
	}

	uint32_t centralDirSize(writer->getSize() - centralDirOffset);

	{
		// End-of-directory record.
		static uint8_t EndDirectory[] =
		{
			0x50, 0x4b, 0x05, 0x06,  // Header
			0x00, 0x00, // Disk num
			0x00, 0x00 // Disk with central dir
		};
		uint8_t buffer[ChunkSize];
		memcpy(buffer, EndDirectory, sizeof(EndDirectory));
		zsize_t pos(sizeof(EndDirectory));

		write16_le(records.size(), &buffer[pos]); // Entries on this disk
		pos += 2;
		write16_le(records.size(), &buffer[pos]); // Total entries
		pos += 2;

		write32_le(centralDirSize, &buffer[pos]);
		pos += 4;
		write32_le(centralDirOffset, &buffer[pos]);
		pos += 4;

		write16_le(0, &buffer[pos]); // Zip comment length
		pos += 2;

		writer->writeData(outPos, pos, &buffer[0]);
		outPos += pos;
	}
}

std::vector<CompressedFilePtr>
zipper::unzip(const ReaderPtr& reader)
{
	return readCentralDirectory(reader);
}

bool
zipper::isZip(const ReaderPtr& reader)
{
	zsize_t centralDirectoryBytes(0);
	zsize_t centralDirectoryOffset(0);
	zsize_t centralDirectoryEntries(0);
	bool result(
		readEndCentralDirectory(
			reader,
			centralDirectoryBytes,
			centralDirectoryOffset,
			centralDirectoryEntries
			)
		);
	return result;
}

