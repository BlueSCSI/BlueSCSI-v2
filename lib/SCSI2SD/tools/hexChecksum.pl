#!/usr/bin/perl -w
#      Copyright (C) 2014 Michael McMaster <michael@codesrc.com>
#
#      This file is part of SCSI2SD.
#
#      SCSI2SD is free software: you can redistribute it and/or modify
#      it under the terms of the GNU General Public License as published by
#      the Free Software Foundation, either version 3 of the License, or
#      (at your option) any later version.
#
#      SCSI2SD is distributed in the hope that it will be useful,
#      but WITHOUT ANY WARRANTY; without even the implied warranty of
#      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#      GNU General Public License for more details.
#
#      You should have received a copy of the GNU General Public License
#      along with SCSI2SD.  If not, see <http://www.gnu.org/licenses/>.

# Calculates the checksum of all flash data within Cypress PSoC 5lp .hex files
# Allows fixing the checksum after manual hacking of the hex files

use strict;
use warnings;

my $sum = 0;
while (my $line = <>)
{

	$line =~ s/[\n\r]//g;

	if ($line =~ /^:40[0-9A-F]{4}00(.+)[0-9A-F]{2}$/)
	{
		my $binrec = pack('H*', $1);
		$sum += unpack('%16C*', $binrec);
	}
	elsif ($line eq ":0200000490303A")
	{
		my $checksumRec = sprintf(":02000000%04X",  ($sum & 0xffff));

		# create checksum of checksum record.
		my $sum2 = unpack('%8C*',  pack('H*', substr($checksumRec, 1)));
		$checksumRec .= sprintf('%2X', (~$sum2 + 1) & 0xFF);
		print("Flash data checksum record = $checksumRec\n");
		print("(Replace line below ':0200000490303A'\n");
		exit;
	}

}
