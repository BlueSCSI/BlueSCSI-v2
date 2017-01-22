//	Copyright (C) 2016 Michael McMaster <michael@codesrc.com>
//
//	This file is part of SCSI2SD.
//
//	SCSI2SD is free software: you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	SCSI2SD is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with SCSI2SD.  If not, see <http://www.gnu.org/licenses/>.

#include "spinlock.h"

int s2s_spin_trylock(s2s_lock_t* lock)
{
	if (__LDREXW(lock) == 0)
	{
		// Try to set lock
		int status = __STREXW(1, lock);
		if (status == 0)
		{
			// got lock
			// Do not start any other memory access
			// until memory barrier is completed
			__DMB();
			return 1;
		}
	}

	return 0;
}

void s2s_spin_lock(s2s_lock_t* lock)
{
	int status = 0;
	do
	{
		// Wait until lock is free
		while (__LDREXW(lock) != 0);

		// Try to set lock
		status = __STREXW(1, lock);
	} while (status!=0); //retry until lock successfully

	// Do not start any other memory access
	// until memory barrier is completed
	__DMB();
}

void s2s_spin_unlock(s2s_lock_t* lock)
{
	// Ensure memory operations completed before releasing
	__DMB();
	*lock = 0;
}
