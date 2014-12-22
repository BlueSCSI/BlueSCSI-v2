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
#include "util.hh"

#include "gzip.hh"
#include "zip.hh"

using namespace zipper;

namespace
{
	class PlainFile : public CompressedFile
	{
	public:
		PlainFile(const ReaderPtr& reader) :
			m_reader(reader)
		{}

		virtual bool isDecompressSupported() const { return true; }
		virtual const std::string& getPath() const
		{
			return m_reader->getSourceName();
		}
		virtual zsize_t getCompressedSize() const
		{
			return m_reader->getSize();
		}
		virtual zsize_t getUncompressedSize() const
		{
			return m_reader->getSize();
		}

		virtual void decompress(Writer& writer)
		{
			enum Constants
			{
				ChunkSize = 64*1024
			};
			zsize_t end(m_reader->getSize());

			for (zsize_t pos(0); pos < end; pos += ChunkSize)
			{
				uint8_t buf[ChunkSize];
				size_t bytes(
					std::min(zsize_t(ChunkSize), end - pos)
					);
				m_reader->readData(pos, bytes, &buf[0]);
				writer.writeData(pos, bytes, &buf[0]);
			}
		}

		virtual const timeval& getModificationTime() const
		{
			return m_reader->getModTime();
		}

	private:
		ReaderPtr m_reader;
	};
}

class Decompressor::DecompressorImpl
{
public:
	DecompressorImpl(const ReaderPtr& reader) :
		m_reader(reader),
		m_format(Container_none)
	{
		if (isZip(reader))
		{
			m_format = Container_zip;
			m_entries = unzip(reader);
		}
		else if (isGzip(reader))
		{
			m_format = Container_gzip;
			m_entries = ungzip(reader);
		}
		else
		{
			m_format = Container_none;
			m_entries.push_back(
				CompressedFilePtr(new PlainFile(reader))
				);
		}
	}

	ContainerFormat getContainerFormat() const { return m_format; }

	std::vector<CompressedFilePtr> getEntries() const { return m_entries; }

private:
	ReaderPtr m_reader;
	ContainerFormat m_format;
	std::vector<CompressedFilePtr> m_entries;
};

Decompressor::Decompressor(const ReaderPtr& reader) :
	m_decompressor(new DecompressorImpl(reader))
{
}

Decompressor::Decompressor(Reader& reader) :
	m_decompressor(
		new DecompressorImpl(ReaderPtr(&reader, dummy_delete<Reader>()))
		)
{
}

Decompressor::~Decompressor()
{
	delete m_decompressor;
}

ContainerFormat
Decompressor::getContainerFormat() const
{
	return m_decompressor->getContainerFormat();
}

std::vector<CompressedFilePtr>
Decompressor::getEntries() const
{
	return m_decompressor->getEntries();
}

