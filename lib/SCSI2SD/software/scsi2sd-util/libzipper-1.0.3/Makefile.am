#	Copyright (C) 2011 Michael McMaster <michael@codesrc.com>
#
#	This file is part of libzipper.
#
#	libzipper is free software: you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation, either version 3 of the License, or
#	(at your option) any later version.
#
#	libzipper is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with libzipper.  If not, see <http://www.gnu.org/licenses/>.

include doxygen.am

dist_noinst_SCRIPTS = \
	autogen.sh

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libzipper1.pc

EXTRA_DIST = \
	COPYING \
	INSTALL \
	INSTALL.Android \
	NEWS \
	README \
	VERSION \
	android/zipper/Android.mk


lib_LTLIBRARIES = libzipper.la
libzipper_la_SOURCES = \
	Compressor.cc \
	CompressedFile.cc \
	Container.cc \
	Decompressor.cc \
	deflate.cc \
	deflate.hh \
	Exception.cc \
	FileReader.cc \
	FileWriter.cc \
	gzip.cc \
	gzip.hh \
	Reader.cc \
	split.hh \
	strerror.hh \
	util.hh \
	Writer.cc \
	zip.hh \
	zip.cc

if HAVE_GNU_STRERROR
libzipper_la_SOURCES += \
	port/strerror_gnu.cc
else
libzipper_la_SOURCES += \
	port/strerror_posix.cc
endif

# Public API headers go here, for installation to /usr/include
include_HEADERS = zipper.hh

libzipper_la_LDFLAGS = ${ZLIB_LIBS} -version-info 1:0
libzipper_la_CFLAGS = ${ZLIB_CFLAGS}

bin_PROGRAMS = zipper
man_MANS = zipper.1

zipper_SOURCES = \
	zipper.cc

zipper_LDADD = libzipper.la

CXXFLAGS=-g -O2 -W -Wall -std=c++0x

MOSTLYCLEANFILES=$(DX_CLEANFILES)

install-data-local: doxygen-install
all-local: doxygen-doc

