/* replacement pread function
   Copyright (C) 2009-2014 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>

/* Specification.  */
#include <unistd.h>

#include <errno.h>

#define __libc_lseek(f,o,w) lseek (f, o, w)
#define __set_errno(Val) errno = (Val)
#define __libc_read(f,b,n) read (f, b, n)

/* pread substitute for systems that the function, such as mingw32 and BeOS.  */
/* The following is identical to the function from glibc's
   sysdeps/posix/pread.c */

/* Note: This implementation of pread is not multithread-safe.  */

ssize_t
pread (int fd, void *buf, size_t nbyte, off_t offset)
{
  /* Since we must not change the file pointer preserve the value so that
     we can restore it later.  */
  int save_errno;
  ssize_t result;
  off_t old_offset = __libc_lseek (fd, 0, SEEK_CUR);
  if (old_offset == (off_t) -1)
    return -1;

  /* Set to wanted position.  */
  if (__libc_lseek (fd, offset, SEEK_SET) == (off_t) -1)
    return -1;

  /* Write out the data.  */
  result = __libc_read (fd, buf, nbyte);

  /* Now we have to restore the position.  If this fails we have to
     return this as an error.  But if the writing also failed we
     return this error.  */
  save_errno = errno;
  if (__libc_lseek (fd, old_offset, SEEK_SET) == (off_t) -1)
    {
      if (result == -1)
        __set_errno (save_errno);
      return -1;
    }
  __set_errno (save_errno);

  return result;
}
