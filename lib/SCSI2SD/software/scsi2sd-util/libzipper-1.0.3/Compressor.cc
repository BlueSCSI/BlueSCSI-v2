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
#include "zip.hh"
#include "util.hh"

#include <algorithm>

using namespace zipper;

class Compressor::CompressorImpl
{
public:
	virtual ~CompressorImpl() {}

	virtual void
	addFile(const std::string& filename, const Reader& reader) = 0;
};

namespace
{
	class PlainCompressor : public Compressor::CompressorImpl
	{
	public:
		PlainCompressor(const WriterPtr& writer) : m_writer(writer) {}

		virtual void
		addFile(const std::string&, const Reader& reader)
		{
			enum Constants
			{
				ChunkSize = 64*1024
			};

			uint8_t buffer[ChunkSize];
			zsize_t offset(0);
			while (offset < reader.getSize())
			{
				zsize_t bytes(
					std::min(zsize_t(ChunkSize), reader.getSize() - offset));
				reader.readData(offset, bytes, &buffer[0]);
				m_writer->writeData(offset, bytes, &buffer[0]);
				offset += bytes;
			}
		}
	private:
		WriterPtr m_writer;
	};

	class ZipCompressor : public Compressor::CompressorImpl
	{
	public:
		ZipCompressor(const WriterPtr& writer) : m_writer(writer) {}

		virtual ~ZipCompressor()
		{
			zipFinalise(m_records, m_writer);
		}

		virtual void
		addFile(const std::string& filename, const Reader& reader)
		{
			ZipFileRecord record;
			zip(filename, reader, m_writer, record);
			m_records.push_back(record);
		}
	private:
		WriterPtr m_writer;
		std::vector<ZipFileRecord> m_records;
	};

	class GzipCompressor : public Compressor::CompressorImpl
	{
	public:
		GzipCompressor(const WriterPtr& writer) : m_writer(writer) {}

		virtual void
		addFile(const std::string& filename, const Reader& reader)
		{
			gzip(filename, reader, m_writer);
		}
	private:
		WriterPtr m_writer;
	};
}

Compressor::Compressor(ContainerFormat format, const WriterPtr& writer)
{
	switch (format)
	{
	case Container_none:
		m_compressor = new PlainCompressor(writer); break;

	case Container_zip:
		m_compressor = new ZipCompressor(writer); break;

	case Container_gzip:
		m_compressor = new GzipCompressor(writer); break;

	default:
		throw UnsupportedException("Unknown format");
	}
}

Compressor::Compressor(ContainerFormat format, Writer& writer) :
	m_compressor(NULL)
{
	WriterPtr ptr(&writer, dummy_delete<Writer>());
	switch (format)
	{
	case Container_none:
		m_compressor = new PlainCompressor(ptr); break;

	case Container_zip:
		m_compressor = new ZipCompressor(ptr); break;

	case Container_gzip:
		m_compressor = new GzipCompressor(ptr); break;

	default:
		throw UnsupportedException("Unknown format");
	}
}

Compressor::~Compressor()
{
	delete m_compressor;
}

void
Compressor::addFile(const Reader& reader)
{
	m_compressor->addFile(reader.getSourceName(), reader);
}


