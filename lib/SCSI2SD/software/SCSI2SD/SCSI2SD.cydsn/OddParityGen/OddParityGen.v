
//`#start header` -- edit after this line, do not edit this line
//	Copyright (C) 2013 Michael McMaster <michael@codesrc.com>
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
`include "cypress.v"
//`#end` -- edit above this line, do not edit this line
// Generated on 10/15/2013 at 22:01
// Component: OddParityGen
module OddParityGen (
	output  DBP,
	input  [7:0] DBx,
	input   EN
);

//`#start body` -- edit after this line, do not edit this line

	// For some reason the "simple" implementation uses up about 34% of all
	// PLD resources on a PSoC 5LP
	// 1 ^ DBx[0] ^ DBx[1] ^ DBx[2] ^ DBx[3] ^ DBx[4] ^ DBx[5] ^ DBx[6] ^ DBx[7]

	// Breaking the expression up into parts seems to use much less resources.
	wire tmp = 1 ^ DBx[0];
	wire tmpa = DBx[1] ^ DBx[2];
	wire tmpb = DBx[3] ^ DBx[4];
	wire tmpc = DBx[5] ^ DBx[6] ^ DBx[7];
	assign DBP = EN ? tmp ^ tmpa ^ tmpb ^ tmpc : 0;
//`#end` -- edit above this line, do not edit this line
endmodule
//`#start footer` -- edit after this line, do not edit this line
//`#end` -- edit above this line, do not edit this line
