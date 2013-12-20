SCSI2SD, The SCSI Hard Drive Emulator for retro computing.

Traditional hard drives last 5 years*. Maybe, if you're luckly, you'll get 10
years of service from a particular drive. The lubricants wear out, the spindles
rust. SCSI2SD is a modern replacement for failed drives. It allows the use of
vintage computer hardware long after their mechanical drives fail. The use of
SD memory cards solves the problem of transferring data between the vintage
computer and a modern PC (who still has access to a working floppy drive ?)

*All statistics are made up.


Features

    In-built active terminator.
    Can optional supply terminator power back to the SCSI bus
    Emulates a non-removable hard drive for maximum compatibility.
    Firmware updatable over USB
    Highly configurable over USB
        Selectable SCSI ID
        Selectable parity support
        Enable/disable Unit Attention Condition
        Artificial limits on the SCSI disk size (eg. limit size to 4G to avoid OS bugs) 


Technical Specifications

SCSI Interface
	SCSI-2 Narrow 8-bit 50-pin connector. Supports asynchronous transfers only.
SD Card Interface
	Standard SDSC (1GB maximum size)
	SDHC (32GB maximum size)
	SDXC cards are untested. Donations welcome.
	Communication is via the SPI protocol at 25MHz.
Power
	5V via standard molex drive connector.
Dimensions
	10cm x 10cm x 1.5cm
	A 3D-printable bracket is in testing to suit a standard 3.5" hard disk bay.


Performance

As currently implemented:

Sequential read: 930kb/sec Sequential write: 414kb/sec

Tested with a 16GB class 10 SD card, via the commands:

 # WRITE TEST
 sudo dd bs=8192 count=100 if=/dev/zero of=/dev/sdX oflag=dsync

 # READ TEST
 sudo dd bs=8192 count=100 if=/dev/sdX of=/dev/null

Compatibility

Tested with Linux (current), Apple Macintosh System 7.5.3 on LC-III, and LC-475
hardware. 

