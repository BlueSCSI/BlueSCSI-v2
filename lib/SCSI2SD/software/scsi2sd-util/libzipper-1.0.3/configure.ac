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

AC_INIT([libzipper], m4_esyscmd_s([cat VERSION]), [michael@codesrc.com])
AC_CANONICAL_HOST
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADERS([autoconfig.h])
AC_CONFIG_FILES([Makefile Doxyfile libzipper1.pc zipper.1])

AM_MAINTAINER_MODE

AC_SUBST([libzipper_version], m4_esyscmd_s([cat VERSION]))

AC_PROG_CXX
AC_LANG([C++])
AM_PROG_LIBTOOL

AC_FUNC_STRERROR_R
AM_CONDITIONAL(HAVE_GNU_STRERROR,[test x$ac_cv_func_strerror_r_char_p = xyes])

DX_DOXYGEN_FEATURE([ON])
DX_HTML_FEATURE([ON])
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(OFF)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(OFF)
DX_PDF_FEATURE(OFF)
DX_PS_FEATURE(OFF)

DX_INIT_DOXYGEN([libzipper], [Doxyfile], [doc])

PKG_CHECK_MODULES([ZLIB], [zlib >= 1.2.3],,
	AC_MSG_ERROR([zlib 1.2.3 or newer not found.])
)

AC_OUTPUT

