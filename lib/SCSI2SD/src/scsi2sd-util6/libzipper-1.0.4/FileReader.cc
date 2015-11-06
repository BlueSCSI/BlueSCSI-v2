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
#include "strerror.hh"

#include <cassert>
#include <sstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "config.h"

using namespace zipper;

const timeval zipper::s_now = {0,0};


class FileReader::FileReaderImpl
{
public:
	FileReaderImpl(const std::string& filename) :
		m_filename(filename),
		m_fd(-1),
		m_closeOnExit(true)
	{
		m_fd = ::open(filename.c_str(), O_RDONLY | O_BINARY);

		if (m_fd < 0)
		{
			std::string errMsg(zipper::strerror(errno));

			std::stringstream message;
			message << "Could not open file \"" << filename << "\": " <<
				errMsg;
			throw IOException(message.str());
		}
		initStats();
	}

	FileReaderImpl(const std::string& filename, int fd, bool closeFd) :
		m_filename(filename),
		m_fd(fd),
		m_closeOnExit(closeFd)
	{
		initStats();
	}

	~FileReaderImpl()
	{
		close();
	}

	const std::string& getSourceName() const { return m_filename; }
	const timeval& getModTime() const { return m_modTime; }

	zsize_t getSize() const { return m_size; }

	virtual void readData(
		zsize_t offset, zsize_t bytes, uint8_t* dest
		) const
	{
		assert(m_fd >= 0);

		zsize_t bytesRead(0);
		while(bytesRead < bytes)
		{
			ssize_t currentBytes(
				pread(
					m_fd,
					dest + bytesRead,
					bytes - bytesRead,
					offset + bytesRead)
				);

			if (currentBytes > 0)
			{
				bytesRead += static_cast<zsize_t>(currentBytes);
			}
			else if (currentBytes == 0)
			{
				throw FormatException("Unexpected end-of-file");
			}
			else if ((currentBytes < 0) && (errno != EINTR))
			{
				std::string errMsg(zipper::strerror(errno));
				throw IOException(errMsg);
			}
		}
	}

private:
	void initStats()
	{
		// If we fail here, we need to essentially run the dtor manually.
		// initStats is called from the constructors, and so the dtor will
		// NOT run if an exception is thrown.

		struct stat buf;
		int result(fstat(m_fd, &buf));
		if (result != 0)
		{
			int errnoLocal = errno;
			close();

			std::string errMsg(zipper::strerror(errnoLocal));

			std::stringstream message;
			message << "Could not get filesize for file " <<
				"\"" << m_filename << "\": " << errMsg;
			throw IOException(message.str());
		}
		else
		{
			m_size = buf.st_size;
			m_modTime.tv_sec = buf.st_mtime;
			m_modTime.tv_usec = 0;
		}
	}

	void close()
	{
		if ((m_fd >= 0) && m_closeOnExit)
		{
			::close(m_fd);
			m_fd = -1;
		}
	}
	std::string m_filename;
	int m_fd;
	bool m_closeOnExit;
	zsize_t m_size;
	timeval m_modTime;
};

FileReader::FileReader(const std::string& filename) :
	m_impl(new FileReaderImpl(filename))
{
}

FileReader::FileReader(const std::string& filename, int fd, bool closeFd) :
	m_impl(new FileReaderImpl(filename, fd, closeFd))
{
}

FileReader::~FileReader()
{
	delete m_impl;
}

const std::string&
FileReader::getSourceName() const
{
	return m_impl->getSourceName();
}

const timeval&
FileReader::getModTime() const
{
	return m_impl->getModTime();
}

zsize_t
FileReader::getSize() const
{
	return m_impl->getSize();
}

void
FileReader::readData(
	zsize_t offset, zsize_t bytes, uint8_t* dest
	) const
{
	return m_impl->readData(offset, bytes, dest);
}

