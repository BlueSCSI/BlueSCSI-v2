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

#ifndef zipper_hh
#define zipper_hh

#include <stdexcept>
#include <memory>
#include <string>
#include <vector>

#include <cstdint>

#include <sys/stat.h> // For mode_t
#include <sys/time.h> // For timeval

/**
\mainpage libzipper C++ (de)compression library

\section intro Introduction
libzipper offers a flexible C++ interface for reading compressed files
in multiple formats.

<a href="http://www.codesrc.com/src/libzipper">Homepage</a>

libzipper aims to provide applications a transparent method of accessing
compressed data. eg. libzipper is suited to reading XML config files that
are compressed to save space.

libzipper is not a general-purpose archive management library, as it
does not provide access to the filesystem attributes of each file.
(ie. libzipper does not support the concepts of file owner, group or
permissions.

\section formats Supported Formats
<ul>
	<li>gzip</li>
	<li>zip</li>
</ul>

\section example_read Reading a compressed file into memory
\code
#include <zipper.hh>
#include <algorithm>
#include <vector>

class MemWriter : public zipper::Writer
{
public:
	std::vector<uint8_t> data;

	virtual void writeData(
		zsize_t offset, zsize_t bytes, const uint8_t* inData)
	{
		data.resize(std::max(offset + bytes, data.size()));
		std::copy(inData, inData + bytes, &data[offset]);
	}
	virtual zsize_t getSize() const { return data.size(); }
};

std::vector<uint8_t> readSavedGame(const std::string& filename)
{
	// open the compressed input file. FileReader will throw an
	// exception if an IO error occurs.
	zipper::FileReader reader(filename);

	MemWriter writer;

	zipper::Decompressor decomp(reader);

	std::vector<zipper::CompressedFilePtr> entries(decomp.getEntries());

	if (!entries.empty())
	{
		// Uncompress the first file. Will pass-though data as-is if the
		// file is not compressed.
		entries.front()->decompress(writer);
	}
	return writer.data;
}

\endcode

\section example_write Writing compressed files.
\code
#include <zipper.hh>
#include <algorithm>
#include <vector>

class MemReader : public zipper::Reader
{
public:
	MemReader(const vector<uint8_t>& data) : m_data(data) {}

	virtual const std::string& getSourceName() const
	{
		static std::string Name("savedGame.dat");
		return Name;
	}

	virtual const timeval& getModTime() const
	{
		return zipper::s_now;
	}

	virtual zsize_t getSize() const { return m_data.size(); }

	virtual void readData(zsize_t offset, zsize_t bytes, uint8_t* dest) const
	{
		std::copy(&m_data[offset], &m_data[offset + bytes], dest);
	}

private:
	std::vector<uint8_t> m_data;
};

void writeSavedGame(
	const std::string& filename, const std::vector<uint8_t>& gameData)
{
	zipper::FileWriter writer(filename);
	zipper::Compressor comp(zipper::Container_zip, writer);
	comp.addFile(MemReader(gameData));
}

\endcode
*/

/// \namespace zipper
/// \brief The zipper namespace contains the libzipper public API.
namespace zipper
{
	/// \typedef zsize_t
	/// zsize_t should be used exclusively when dealing with file offsets
	/// and sizes to support large files (>4Gb).
	///
	/// Unlike size_t on some systems, zsize_t will be 64bit when compiling for
	/// a 32bit target.
	typedef uint64_t zsize_t;

	/// \enum ContainerFormat
	/// ContainerFormat enumerates the compressed archive formats supported
	/// by libzipper.
	///
	/// An application can determine the supported formats by iterating
	/// over the Container_begin to Container_end range. eg.
	/// \code
	/// for (int i = Container_begin; i < Container_end; ++i)
	/// {
	///     const Container& container(getContainer(ContainerFormat(i)));
	/// }
	/// \endcode
	enum ContainerFormat
	{
		/// Iteration marker
		Container_begin = 0,

		/// No container (eg. plain text)
		Container_none = 0,

		/// ZIP
		Container_zip,

		/// gzip.
		Container_gzip,

		/// Iteration marker
		Container_end
	};

	/// \struct Container
	/// Provides libzipper capability details for a compressed archive
	/// format.
	/// \see getContainer
	struct Container
	{
		/// \enum CapabilityBits allows a bitmask to be specified with a
		/// combination of boolean flags.
		enum CapabilityBits
		{
			/// Compression bit is set if the format is usable with Compressor
			Compression = 1,

			/// Decompression bit is set if the format is usable with
			/// Decompressor
			Decompression = 2,

			/// EmbeddedFilenames bit is set if CompressedFile::getPath() is
			/// supported
			EmbeddedFilenames = 4,

			/// Archive bit is set if multiple compressed files may exist in
			/// a single container.
			Archive = 8,

			/// FileSize bit is set if the uncompressed size for each
			/// compressed file is recorded in the container.
			FileSize = 16
		};

		/// %Container Type
		ContainerFormat format;

		/// %Container Internet Media Type (aka MIME type).
		/// eg. "application/zip"
		std::string mediaType;

		/// Bitmask comprised of CapabilityBits enum values.
		uint32_t capabilities;
	};

	/// \brief When passed as a method parameter, it requests that the
	/// current time be used instead.
	extern const timeval s_now;

	/// \brief Returns the capability details of the given format.
	const Container& getContainer(ContainerFormat format);

	/// \brief Base class for all exceptions thrown by libzipper
	class Exception : public std::runtime_error
	{
	public:
		/// Exception ctor
		/// \param what A description of the error encountered.
		Exception(const std::string& what);
	};

	/// \brief Exception thrown when the input data does not match
	/// the expected Container format.
	class FormatException : public Exception
	{
	public:
		/// FormatException ctor
		/// \param what A description of the error encountered.
		FormatException(const std::string& what);
	};

	/// \brief Exception thrown when a Reader or Writer instance is unable
	/// to satisfy an IO request due to an external error.
	class IOException : public Exception
	{
	public:
		/// IOException ctor
		/// \param what A description of the error encountered.
		IOException(const std::string& what);
	};

	/// \brief Exception thrown when an operation is requested on a compressed
	/// archive that libzipper does not implement.
	///
	/// This exception may be thrown even if libzipper advertises general
	/// support for the Container format.  eg. libzipper supports most
	/// ZIP files, but an UnsupportedException will be thrown if given an
	/// encrypted ZIP file.
	class UnsupportedException : public Exception
	{
	public:
		/// UnsupportedException ctor
		/// \param what A description of the error encountered.
		UnsupportedException(const std::string& what);
	};

	/// \brief Reader supplies input data to the compression/decompression
	/// functions.
	///
	/// Normally, an application using libzipper provides the Reader
	/// implementation. The implementation could supply data from files,
	/// in-memory buffers, or it could be generated on-the-fly.
	///
	/// The Reader implementation must support random access, and must
	/// determine at creation time the number of bytes available. The
	/// Reader interface is not suitable for use with streaming data.
	class Reader
	{
	public:
		/// Reader dtor
		virtual ~Reader();

		/// Returns a name for this source of the data.
		///
		/// For file-based Reader implementations, this would normally be
		/// the input filename.
		virtual const std::string& getSourceName() const = 0;

		/// Return the last-modified timestamp of the data.
		/// If the special s_now value is returned, the current time should be
		/// used instead.
		virtual const timeval& getModTime() const = 0;

		/// Returns the number of bytes available via readData()
		///
		/// \invariant getSize() is stable throughout the lifetime
		/// of the Reader instance.
		virtual zsize_t getSize() const = 0;

		/// Copies data into the dest buffer
		///
		/// An exception must be thrown if it is not possible to copy the
		/// requested data into the supplied buffer (eg. file IO error).
		///
		/// \pre offset + bytes <= getSize()
		///
		/// \param offset Number of bytes to skip at the front of the data
		/// source.
		/// \param bytes Number of bytes to copy
		/// \param dest Destination buffer.
		///
		virtual void readData(
			zsize_t offset, zsize_t bytes, uint8_t* dest
			) const = 0;

	};

	/// \brief FileReader is a file-based implementation of the Reader
	/// interface.
	class FileReader : public Reader
	{
	public:
		/// Read data from the supplied file.
		FileReader(const std::string& filename);

		/// Read data from the supplied file.
		///
		/// \param filename The value used by getSourceName(). This name
		/// is arbitary, and does not need to be related to fd.
		///
		/// \param fd The descriptor to source data from.  The descriptor
		/// must be open for reading, blocking, and seekable (ie. lseek(2)).
		///
		/// \param closeFd If true, fd will be closed by this object
		/// when it is no longer needed.
		FileReader(const std::string& filename, int fd, bool closeFd);

		/// FileReader dtor
		virtual ~FileReader();

		/// Inherited from Reader
		virtual const std::string& getSourceName() const;

		/// Inherited from Reader
		virtual const timeval& getModTime() const;

		/// Inherited from Reader
		virtual zsize_t getSize() const;

		/// Inherited from Reader
		virtual void readData(
			zsize_t offset, zsize_t bytes, uint8_t* dest
			) const;
	private:
		FileReader(const FileReader&);
		FileReader& operator=(const FileReader&);

		class FileReaderImpl;
		FileReaderImpl* m_impl;
	};

	/// \typedef ReaderPtr
	/// A shared pointer to a Reader
	typedef std::shared_ptr<Reader> ReaderPtr;

	/// \brief Writer accepts output data from the compression/decompression
	/// functions.
	///
	/// Normally, an application using libzipper provides the Writer
	/// implementation. The implementation could write data to files,
	/// in-memory buffers, or it could be simply discarded.
	///
	/// The Writer implementation needs only to support sequential access.
	class Writer
	{
	public:
		/// Writer dtor
		virtual ~Writer();

		/// Returns the size of the written data.
		virtual zsize_t getSize() const = 0;

		/// Accepts output from libzipper
		///
		/// An exception must be thrown if it is not possible to accept
		/// given data. (eg. file IO error).
		///
		/// \param offset Number of bytes to skip at the front of the data
		/// source.  Skipped bytes will contain null characters if not already
		/// assigned a value.
		/// \param bytes Number of bytes in data
		/// \param data Output from libzipper.
		///
		virtual void writeData(
			zsize_t offset, zsize_t bytes, const uint8_t* data
			) = 0;
	};

	/// \typedef WriterPtr
	/// A shared pointer to a Writer
	typedef std::shared_ptr<Writer> WriterPtr;

	/// \brief FileWrter is a file-based implementation of the Writer
	/// interface.
	class FileWriter : public Writer
	{
	public:
		/// Write data to the supplied file.
		/// If the file already exists, it will be truncated.
		/// If the file does not exist, it will be created with the
		/// given permissions.
		///
		/// \param filename The file to open for writing.
		///
		/// \param createPermissions The permissions set on the file if it is to
		/// be created.
		///
		/// \param modTime Set a specific modification time on the created file.
		/// If the special s_now value is provided, the current time will be
		/// used.
		///
		FileWriter(
			const std::string& filename,
			mode_t createPermissions = 0664,
			const timeval& modTime = s_now);

		/// Write data to the supplied file.
		///
		/// \param filename The filename reported in any exception error
		/// messages. This name  is arbitary, and does not need to be
		/// related to fd.
		///
		/// \param fd The descriptor to write data to.  The descriptor
		/// must be open for writing in blocking mode.
		///
		/// \param closeFd If true, fd will be closed by this object
		/// when it is no longer needed.
		FileWriter(const std::string& filename, int fd, bool closeFd);

		/// FileWriter dtor
		virtual ~FileWriter();

		/// Inherited from Writer
		virtual zsize_t getSize() const;

		/// Inherited from Writer
		virtual void writeData(
			zsize_t offset, zsize_t bytes, const uint8_t* data
			);
	private:
		FileWriter(const FileWriter&);
		FileWriter& operator=(const FileWriter&);

		class FileWriterImpl;
		FileWriterImpl* m_impl;
	};

	/// \brief CompressedFile represents an entry within a compressed archive.
	///
	/// CompressedFile instances are created by Decompressor, and allow
	/// selectively extracting the contents of an archive.
	class CompressedFile
	{
	public:
		/// CompressedFile dtor
		virtual ~CompressedFile();

		/// Return true if decompress is likely to succeed.
		///
		/// isDecompressSupported may return false if libzipper doesn't know
		/// how to deal with the compressed data. eg. encrypted files,
		/// or ZIP files compressed with non-standard schemes.
		virtual bool isDecompressSupported() const = 0;

		/// Decompress the file, and store the results via the given
		/// writer object.
		virtual void decompress(Writer& writer) = 0;

		/// Return the file path of the compressed file.
		///
		/// Unix-style path separaters ('/') are returned, even if the
		/// archive was created under an alternative OS.
		virtual const std::string& getPath() const = 0;

		/// Return the compressed size of the file
		///
		/// getCompressedSize() will return -1 of the FileSize capability
		/// bit of the container is false.
		virtual zsize_t getCompressedSize() const = 0;

		/// Return the uncompressed size of the file
		///
		/// The decompress method will pass exactly this number of bytes
		/// to the Writer.
		///
		/// getUncompressedSize() will return -1 of the FileSize capability
		/// bit of the container is false.
		virtual zsize_t getUncompressedSize() const = 0;

		/// Return the modification time of the original file
		virtual const timeval& getModificationTime() const = 0;
	};
	/// \typedef CompressedFilePtr
	/// A shared pointer to a CompressedFile
	typedef std::shared_ptr<CompressedFile> CompressedFilePtr;

	/// \brief Decompressor detects the compressed archive type of the data,
	/// and creates suitable CompressedFile instances to access the compressed
	/// data.
	class Decompressor
	{
	public:
		/// Create a decompressor from the data made available by reader.
		Decompressor(const ReaderPtr& reader);

		/// Create a decompressor from the data made available by reader.
		///
		/// \param reader must remain in scope for the lifetime of the
		/// Decompressor, and lifetime of any CompressedFile objects returned
		/// from getEntries()
		Decompressor(Reader& reader);

		/// Decompressor dtor
		~Decompressor();

		/// Return the detected Container type of the compressed archive.
		ContainerFormat getContainerFormat() const;

		/// Return CompressedFile entries to represent the file entries within
		/// a compressed archive.
		std::vector<CompressedFilePtr> getEntries() const;
	private:
		Decompressor(const Decompressor&);
		Decompressor& operator=(const Decompressor&);

		class DecompressorImpl;
		DecompressorImpl* m_decompressor;
	};

	/// \brief Compressor creates a compressed archive from the supplied
	/// Reader objects.
	/// data.
	class Compressor
	{
	public:
		/// Create a Compressor to output the given compressed archived format
		/// to writer.
		/// \param writer destination of the compressed data
		/// \param format determines the output archive file type to
		/// create.
		Compressor(ContainerFormat format, const WriterPtr& writer);

		/// Create a Compressor to output the given compressed archived format
		/// to writer.
		///
		/// \param writer is the destination of the compressed data.  writer
		/// must remain in scope for the lifetime of the Compressor.
		/// \param format determines the output archive file type to
		/// create.
		Compressor(ContainerFormat format, Writer& writer);

		/// \brief Compressor dtor
		///
		/// Additional data may be passed to writer (given in ctor) to close
		/// the compressed archive.
		~Compressor();

		/// Compress the data given by reader, and add it to the compressed
		/// archive.
		void addFile(const Reader& reader);

		class CompressorImpl;
	private:
		Compressor(const Compressor&);
		Compressor& operator=(const Compressor&);

		CompressorImpl* m_compressor;
	};
}

#endif

