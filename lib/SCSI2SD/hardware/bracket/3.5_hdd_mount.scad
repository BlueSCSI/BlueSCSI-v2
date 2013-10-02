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


$fa = 3; // 4-times as many angles per circle.
$fs = 0.1; // 0.1mm accuracy

	// A* taken from SFF-8301 Specification for Form Factor of 3.5" Disk Drives

	A3 = 101.6; // width
	A5 = 3.19; // Distance from side to bottom hole.
	A6 = 44.45; // Distance from A7 to second bottom hole.
	A7 = 41.28; // Distance from front to bottom holes.
	A8 = 28.5; // Distance from front to side holes
	A9 = 101.60; // Distance between side holes.
	A10 = 6.35; // Height from base to site holes.
	A13 = 76.2; // Distance from A7 to third bottom hole.
	m3HoleRadius=2.667/2; // M3x0.50 minimum hole size (aluminium or softer material)
	holeBulk=4; // Extra around holes
	tmp = 10;
	wallWidth = 1.3;
	screwWidth = 3;
	foo = 6;
	bar = 4;  // PSOC MOUNT

PCB_DIFF=90.42; // Clearance line of "fat" via is 10mil from edge.
PCB_off = (A3 - PCB_DIFF) / 2;
// from front = A7 + foo = 47.28
// second = 47.28 + A6 = 91.73. Perfect!
// Height between board and screw:
// screwWidth + 1.6mm pcb only = 3 + 1.6 =4.6. Not a problem!
// Width of PCB vs side hole bulk: 101.6 - 97.5360  = 4.064
// only 2mm to spare on either side.
// TODO Made a NOTCH in the PCB to handle this!
// notch: A8 +- holeBulk = 28.5 - 4, 28.5 + 4 = 24 -> 33mm. 3mm in.



module hdd_side()
{
	difference()
	{
		union()
		{
			cube([A8 + A9 + tmp, wallWidth, A10 + holeBulk]);

			// Bottom mount 1
			translate([A7 - (foo / 2), 0, 0])
			{
				cube([foo, foo, screwWidth]); 
			}

			// Bottom mount 2
			translate([A6 + A7 - (foo / 2), 0, 0])
			{
				cube([foo, foo, screwWidth]); 
			}

			// Bottom mount 3
			translate([A13 + A7 - (foo / 2), 0, 0])
			{
				cube([foo, foo, screwWidth]); 
			}

			// psoc mount 1
			translate([A7 - (foo / 2) + foo, 0, 0])
			{
				cube([foo, foo + bar, screwWidth]); 
			}

			// psoc mount 2
			translate([A6 + A7 - (foo / 2) + foo, 0, 0])
			{
				cube([foo, foo + bar, screwWidth]); 
			}

			// Extra bulk behind side holes
			translate([A8, 0, A10])
			{
				rotate([270, 0, 0])
				{
					cylinder(h=screwWidth, r=holeBulk);
				}
			}

			translate([A8 + A9, 0, A10])
			{
				rotate([270, 0, 0])
				{
					cylinder(h=screwWidth, r=holeBulk);
				}
			}
		}
	
		// Remove excess material from the side
		translate([-0.5, -0.5,screwWidth + wallWidth])
		{
			cube([A8 - tmp + 0.5, wallWidth + 1, A10 + holeBulk]);
		}
		translate([A8 + tmp, -0.5, screwWidth + wallWidth])
		{
			cube([A9 - (tmp * 2), wallWidth + 1, A10 + holeBulk]);
		}


		// SIDE HOLES
	
		translate([A8, -0.5, A10])
		{
			rotate([270, 0, 0])
			{
				cylinder(h=screwWidth + 1, r=m3HoleRadius);
			}
		}

		translate([A8 + A9, -0.5, A10])
		{
			rotate([270, 0, 0])
			{
				cylinder(h=screwWidth + 1, r=m3HoleRadius);
			}
		}

		// BOTTOM HOLES
		// Bottom hole 1
		translate([A7, A5, -0.5])
		{
			cylinder(h=screwWidth + 1, r = m3HoleRadius); 
		}

		// Bottom hole 2
		translate([A6 + A7, A5, -0.5])
		{
			cylinder(h=screwWidth + 1, r = m3HoleRadius); 
		}

		// Bottom hole 3
		translate([A13 + A7, A5, -0.5])
		{
			cylinder(h=screwWidth + 1, r = m3HoleRadius); 
		}

		// PSOC hole1
		translate([A7 + foo, PCB_off, -0.5])
		{
			cylinder(h=screwWidth + 1, r = m3HoleRadius); 
		}
		// PSOC hole2
		translate([A6 + A7 + foo, PCB_off, -0.5])
		{
			cylinder(h=screwWidth + 1, r = m3HoleRadius); 
		}
	}
}

union()
{
	hdd_side();
	translate([0, A3, 0])
	{
		mirror([0, 1, 0])
		{
			hdd_side();
		}
	}

	cube([wallWidth * 2, A3, wallWidth]);

	translate([A8 + A9 + tmp - wallWidth * 2, 0, 0])
	{
		cube([wallWidth * 2, A3, wallWidth]);
	}

		// Bottom hole 1
		translate([A7 + foo, foo + bar, 0])
		{
			cube([wallWidth * 2, A3 - ((foo + bar) * 2), wallWidth]); 
		}

		// Bottom hole 2
		translate([A6 + A7 + foo, foo + bar, 0])
		{
			cube([wallWidth * 2, A3 - ((foo + bar) * 2), wallWidth]); 
		}


	for (i = [0:3])
	{
		translate([0, (i * (A3 - wallWidth) / 3), 0])
		{
			cube([A8 + A9 + tmp, wallWidth, wallWidth]);
		}
	}
}
