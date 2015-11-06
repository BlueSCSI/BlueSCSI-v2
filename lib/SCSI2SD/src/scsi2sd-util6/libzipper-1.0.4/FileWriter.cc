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

#include <algorithm>
#include <cassert>
#include <sstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>
#include <errno.h>

#include "config.h"

using namespace zipper;

class FileWriter::FileWriterImpl
{
public:
	FileWriterImpl(
		const std::string& filename,
		mode_t createPermissions,
		const timeval& modTime
		) :
		m_filename(filename),
		m_modTime(modTime),
		m_fd(-1),
		m_closeOnExit(true),
		m_setModTimeOnExit(true)
	{
		m_fd =
			::open(
				filename.c_str(),
				O_WRONLY | O_TRUNC | O_CREAT | O_BINARY,
				createPermissions);

		if (m_fd < 0)
		{
			std::string errMsg(zipper::strerror(errno));

			std::stringstream message;
			message << "Could not open file \"" << filename << "\": " <<
				errMsg;
			throw IOException(message.str());
		}
	}

	FileWriterImpl(const std::string& filename, int fd, bool closeFd) :
		m_filename(filename),
		m_fd(fd),
		m_closeOnExit(closeFd),
		m_setModTimeOnExit(false)
	{
	}

	~FileWriterImpl()
	{
		close();

		if (m_setModTimeOnExit)
		{
			struct timeval times[2];
			if (s_now.tv_sec == m_modTime.tv_sec)
			{
				gettimeofday(&times[0], NULL);
				times[1] = times[0];
			}
			else
			{
				times[0] = m_modTime;
				times[1] = m_modTime;
			}
			utimes(m_filename.c_str(), times);
		}
	}

	virtual void writeData(
		zsize_t offset, zsize_t bytes, const uint8_t* data
		) const
	{
		assert(m_fd >= 0);

		zsize_t bytesWritten(0);
		while(bytesWritten < bytes)
		{
			ssize_t currentBytes(
				pwrite(
					m_fd,
					data + bytesWritten,
					bytes - bytesWritten,
					offset + bytesWritten)
				);

			if (currentBytes >= 0)
			{
				bytesWritten += static_cast<zsize_t>(currentBytes);
			}
			else if ((currentBytes < 0) && (errno != EINTR))
			{
				std::string errMsg(zipper::strerror(errno));
				throw IOException(errMsg);
			}
		}
	}

	zsize_t getSize() const
	{
		assert(m_fd >= 0);
		zsize_t result(lseek(m_fd, 0, SEEK_END));
		return result;
	}

private:
	void close()
	{
		if ((m_fd >= 0) && m_closeOnExit)
		{
			::close(m_fd);
			m_fd = -1;
		}
	}

	std::string m_filename;
	timeval m_modTime;
	int m_fd;
	bool m_closeOnExit;
	bool m_setModTimeOnExit;
};

FileWriter::FileWriter(
	const std::string& filename,
	mode_t createPermissions,
	const timeval& modTime) :
	m_impl(new FileWriterImpl(filename, createPermissions, modTime))
{
}

FileWriter::FileWriter(const std::string& filename, int fd, bool closeFd) :
	m_impl(new FileWriterImpl(filename, fd, closeFd))
{
}

FileWriter::~FileWriter()
{
	delete m_impl;
}

void
FileWriter::writeData(zsize_t offset, zsize_t bytes, const uint8_t* data)
{
	m_impl->writeData(offset, bytes, data);
}

zsize_t
FileWriter::getSize() const
{
	return m_impl->getSize();
}

