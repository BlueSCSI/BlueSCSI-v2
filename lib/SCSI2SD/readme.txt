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
    Emulates up to 4 SCSI devices
    Supports sector sizes from 64 bytes to 8192 bytes
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
Micro SD Card Interface
	Standard microSDSC (1GB maximum size)
	microSDHC, microSDXC
	Communication is via the SPI protocol at 25MHz.
USB Interface (firmware updates and config)
	USB 2.0 micro-B
Power
	5V via standard molex drive connector
	USB or self-powered using the SCSI host termination power. (v5 only)
Dimensions
	10cm x 5cm x 1.5cm (v5)
	10cm x 10cm x 1.5cm (v3, v4)


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
 Computer systems

    Mac LC-III and LC-475
    Mac II running System 6.0.8
    Mac SE/30
    Apple IIgs using Apple II High Speed SCSI controller card (from v3.3)
    Symbolics Lisp Machine XL1200, using 1280 byte sectors (from v3.4)
    PDP-11/73 running RSX11M+ V4.6
    Microvax 3100 Model 80 running VMS 7.3 (needs patch against v3.5.2 firmware)
    Amiga 500+ with GVP A530
    Commodore Amiga 500 KS 1.3 with Oktagon 508 SCSI-2 controller
    Amiga 2000 (B2000 rev 6.4 ECS) with DKB RapidFire SCSI 1 card
    Amiga 4000 equipped with CyberStorm PPC using 68pin adapter.
    Atari TT030 System V
    Atari TT running TOS 3.06 with HDDRIVER software version 9.06
    Atari Mega ST 1; TOS 1.04; Protar ProFile 30 external hard drive enclosure (1GB size limitation)
    Atari MEGA STE
        needs J3 TERMPWR jumper
        1GB limit (--blocks=2048000). The OS will fail to read the boot sector if the disk is >= 1GB.
    Atari Falcon, TOS 4.02, HDDRIVER 9.06 driver
    Sharp X68000
        SASI models supported. See gamesx.com for information on building a custom cable.
        needs J3 TERMPWR jumper
        Set to SCSI ID 3. ID0 will not work. 
    Compaq XP-1000 Professional Workstation
        Alpha 21264 CPU, 667MHz, with a QLogic SCSI controller in a PCI slot 
    SCSI-based Macintosh Powerbooks (2.5" SCSI2SD)
        Also reported to work on Thinkpad 860 running Win NT 4.0 PowerPC.
    AT&T 3B2/600
    Sun 2/120 Workstation (Unit Attention disabled)
    Sun 4/50 workstation
    Data General MV/2500DC running AOS/VS
        Vendor: MICROoP
        Product: 1578-15       UP
        Revision: DG02
        Device-type modifier: 0x4c
    Applix 1616
    IMS MM/1
    NeXTcube + NeXTSTEP 3.3
    NeXTStation
    Modified geometry settings are required to avoid "cylinder group too large" errors while formatting.
    	(To simulate Quantum Fireball 1050S)
        512 bytesPerSector
        139 sectorsPerTrack
        4 tracksPerCylinder
        4135 cylinder per volume
        1 spare sector per cylinder
        2051459 usable sectors on volume
    Apollo 400/425s running DOMAIN/OS
    Motorola System V/68 R3V7 and R3V8.
        Since the installation have information about limited number of drives(most of them with custom commands) it requires a pre-installed disk image to be dd-ed on it. Works with MVME167 and MVME177
    Motorola System V/88 R40V4.0 through R40V4.4
        It requires to describe the disk into a configuration file. The process is described here - http://m88k.com/howto-001.html


Samplers

    Roland JS-30 Sampler
    Akai S1000, S3200, S3000XL, MPC 2000XL, DPS 12
        SCSI cable reversed on S3200
        There are compatibility problems with the Akai MPC3000. It works (slowly) with the alternate Vailixi OS with multi-sector transfers disabled. 
    EMU Emulator E4X with EOS 3.00b and E6400 (classic) with Eos 4.01
    EMU E6400 w/ EOS2.80f
    EMU Emax2
    Ensoniq ASR-X, ASR-10 (from v3.4, 2GB size limit)
        ASR-10 Requires TERMPWR jumper (applies to pre. 5.0 SCSI2SD boards only)
        ASR-X resets when writing to devices > 2Gb. 
    Ensoniq EPS16+
    Kurzweil K2000R
        See kurzweil.com for size limits which a dependant on the OS version. Older OS versions have a 1GB limit.
        SCSI cable reversed 
    Casio FZ-20M
        Requires TERMPWR jumper. The manual shows the pin25 of the DB25 connector is "not connected".
        May require scsi2sd-config --apple flag 
    Yamaha A5000, A3000, EX5, EX5R 
    EMU ESI4000
    Synclavier 9600.
        Disable Parity. Max size == 9GB.


Other

    HP 16601A, 16700A logic analyzers
    Fluke 9100 series 
	Reftek RT-72A Seismic datalogger.
		http://www.iris.iris.edu/passcal/Reftek/72A-R-005-00.1.pdf
		http://www.iris.iris.edu/passcal/Manual/rtfm.s3a.13.html
	Konami Simpson's Bowling arcade machine
		http://forums.arcade-museum.com/showthread.php?p=3027446
