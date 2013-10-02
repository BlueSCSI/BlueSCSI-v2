	                             # 78 for SOT23
	                             # 82 for SOT23
	      # 41 for SOT23
	               # 34 for SOT23, 24 for SOT25
Element(0x00 "SMT transistor, 6 pins" "" "SOT26" 138 0 3 100 0x00)
(
	ElementLine(0 0 0 139 10)
	ElementLine(0 139 118 139 10)
	ElementLine(118 139 118 0 10)
	ElementLine(118 0 0 0 10)
	# 1st side, 1st pin
	Pad(20 102
	       20 118
			   24 "1" "D" 0x100)
	# 1st side, 2nd pin
	Pad(59 102
	       59 118
			   24 "2" "D" 0x100)
	# 1st side, 3rd pin
	Pad(98 102
	    98 118
			   24 "3" "G" 0x100)
	# 2nd side, 3rd pin
	Pad(98 20
	       98 36
			   24 "4" "S" 0x100)
	# 2nd side, 2nd pin
	Pad(59 20
	       59 36
			   24 "5" "D" 0x100)
	# 2nd side, 1st pin
	Pad(20 20
	       20 36
			   24 "6" "D" 0x100)
	Mark(20 110)
)
