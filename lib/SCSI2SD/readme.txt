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
    Supports sector sizes from 64 bytes to 2048 bytes
    Firmware updatable over USB
    Highly configurable over USB
        Selectable SCSI ID
        Selectable parity support
        Enable/disable Unit Attention Condition
        Artificial limits on the SCSI disk size (eg. limit size to 4G to avoid OS bugs) 
        Sector size (can also be set via the SCSI MODE SELECT command, as sent by SCSI format utilities).


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

Transfer size:    512        2048        8192        65536
-------------------------------------------------------
read:			2MB/s     2.1MB/s     2.5MB/s     2.6MB/s
write:			125kB/s   441kB/s     1.5MB/s     2.3MB/s
-------------------------------------------------------


Tested with a 16GB class 10 SD card, via the commands:

 # WRITE TEST
 sudo dd bs=${SIZE} count=100 if=/dev/zero of=/dev/sdX oflag=dsync

 # READ TEST
 sudo dd bs=${SIZE} count=100 if=/dev/sdX of=/dev/null

Compatibility

 Desktop systems

    Mac LC-III and LC-475
    Mac II running System 6.0.8
    Mac SE/30
    Apple IIgs using Apple II High Speed SCSI controller card (from v3.3)
    Symbolics Lisp Machine XL1200, using 1280 byte sectors (from v3.4)
    PDP-11/73 running RSX11M+ V4.6
    Microvax 3100 Model 80 running VMS 7.3 (needs patch against v3.5.2 firmware) 
    Amiga 500+ with GVP A530
    Atari TT030 System V 
    Atari MEGA STE
        needs J3 TERMPWR jumper
        1GB limit (--blocks=2048000)

Samplers

    Roland JS-30 Sampler
    Akai S1000, S3200, S3000XL, MPC 2000XL, DPS 12
        SCSI cable reversed on S3200
        There are compatibility problems with the Akai MPC3000. It works (slowly) with the alternate Vailixi OS with multi-sector transfers disabled. 
    EMU Emulator E4X with EOS 3.00b and E6400 (classic) with Eos 4.01
    Ensoniq ASR-X, ASR-10 (from v3.4, 2GB size limit)
        ASR-20 Requires TERMPWR jumper. 
    Kurzweil K2000R
        See kurzweil.com for size limits which a dependant on the OS version. Older OS versions have a 1GB limit.
        SCSI cable reversed 
    Casio FZ-20M
        Requires TERMPWR jumper. The manual shows the pin25 of the DB25 connector is "not connected".
        May require scsi2sd-config --apple flag 
    Yamaha EX5R 

Other

    HP 16601A, 16700A logic analyzers
    Fluke 9100 series 
