AzulSCSI Firmware
=================

Harddrive image files
---------------------
AzulSCSI uses the same harddrive image file format as in [BlueSCSI instructions](https://github.com/erichelgeson/BlueSCSI/wiki/Usage).

The image files should be named e.g. `HD00_512.hda` for ID 0, LUN 0, 512 byte block.

Log files and error indications
-------------------------------
Log messages are stored in `azullog.txt`, which is cleared on every boot.
Normally only basic initialization information is stored, but turning `DIPSW2` on will cause every SCSI command to be logged.

The indicator LED will normally report disk access.
It also reports following status conditions:

- 1 fast blink on boot: Image file loaded successfully
- 3 fast blinks: No images found on SD card
- 5 fast blinks: SD card not detected
- Continuous morse pattern: firmware crashed, morse code indicates crash location

In crashes the firmware will also attempt to save information into `azulerr.txt`.

Configuration file
------------------
Optional configuration can be stored in `azulscsi.ini`.
If image file is found but configuration is missing, a default configuration is used.

A single AzulSCSI device can represent multiple devices on the SCSI bus.
The configuration sections are numbered `SCSI0` to `SCSI7` and correspond to images `HD00.hda` to `HD70.hda`.

Example format for config file:

    [SCSI]
    # Settings that apply to all devices
    Debug = 0   # Same effect as DIPSW2

    [SCSI0]
    Vendor = "QUANTUM "
    Product = "FIREBALL1       "
    Version = "1.0 "
    Serial = "0123456789ABCDEF"
    Quirks = 0   # 0: Standard, 1: Apple, 2: OMTI, 4: Xebec, 8: VMS

Performance
-----------
With verbose log messages disabled, expected SCSI performance is 2.4 MB/s read and 1.5 MB/s write.
Slow SD card or fragmented filesystem can slow down access.

Seek performance is best if image files are contiguous.
For ExFAT filesystem this relies on a file flag set by PC.
Current versions of exfat-fuse on Linux have an [issue](https://github.com/relan/exfat/pull/101) that causes the files not to be marked contiguous even when they are.
This is indicated by message `WARNING: file HD00_512.hda is not contiguous. This will increase read latency.` in the log.

Hotplugging
-----------
The firmware supports hot-plug removal and reinsertion of SD card.
The status led will blink continuously when card is not present, then blink once when card is reinserted successfully.

It will depend on the host system whether it gets confused by hotplugging.
Any IO requests issued when card is removed will be timeouted.

Programming
-----------
The AzulSCSI v1 board can be programmed using USB connection in DFU mode.
The necessary programmer utility for Windows can be downloaded from [GD32 website](http://www.gd32mcu.com/en/download?kw=dfu&lan=en). On Linux and Mac [gd32-dfu-utils](https://github.com/riscv-mcu/gd32-dfu-utils) can be used.

DIP switches
------------
The DIP switch settings are as follows:

- DIPSW1: Not used
- DIPSW2: Enable verbose debug log (saved to `azullog.txt`)
- DIPSW3: Enable SCSI termination
- DIPSW4: Enable built-in USB bootloader

Project structure
-----------------
- **src/AzulSCSI.cpp**: Main portable SCSI implementation.
- **src/AzulSCSI_log.cpp**: Simple logging functionality, uses memory buffering.
- **src/AzulSCSI_config.h**: Some compile-time options, usually no need to change.
- **lib/AzulSCSI_platform_GD32F205**: Platform-specific code for GD32F205.
- **lib/minIni**: Ini config file access library
- **lib/SdFat_NoArduino**: Modified version of [SdFat](https://github.com/greiman/SdFat) library for use without Arduino core.
- **utils/run_gdb.sh**: Helper script for debugging with st-link adapter. Displays SWO log directly in console.

Building
--------
This codebase uses [PlatformIO](https://platformio.org/).
To build run the command:

    pio run


Origins and License
-------------------

This firmware is derived from two sources, both under GPL 3 license:

* [SCSI2SD V6](http://www.codesrc.com/mediawiki/index.php/SCSI2SD)
* [BlueSCSI](https://github.com/erichelgeson/BlueSCSI), which in turn is derived from [ArdSCSIno-stm32](https://github.com/ztto/ArdSCSino-stm32).

Main program structure:

* SCSI command implementations are from SCSI2SD.
* SCSI physical layer code is mostly custom, with some inspiration from BlueSCSI.
* Image file access is derived from BlueSCSI.

Major changes from BlueSCSI and SCSI2SD include:

* Separation of platform-specific functionality to separate file to ease porting.
* Ported to GD32F205.
* Removal of Arduino core dependency, as it was not currently available for GD32F205.
* Buffered log functions.
* Direct streaming between SD card and SCSI for slightly improved performance.
