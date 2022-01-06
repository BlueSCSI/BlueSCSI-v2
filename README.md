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
It also reports following error conditions:

- 3 fast blinks: No images found on SD card
- 5 fast blinks: SD card not detected
- Continuous morse pattern: firmware crashed, morse code indicates crash location

In crashes the firmware will also attempt to save information into `azulerr.txt`.

Configuration file
------------------
Optional configuration can be stored in `azulscsi.ini`.
Example format for config file:

    [SCSI]
    Vendor = "QUANTUM "
    Product = "FIREBALL1       "
    Version = "1.0 "
    Quirks = 0   # 0: Standard, 1: Sharp, 2: NEC PC98

Performance
-----------
With verbose log messages disabled, expected SCSI performance is 1.7 MB/s read and 1.5 MB/s write.
Slow SD card or fragmented filesystem can slow down access.

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

This firmware is derived from [BlueSCSI](https://github.com/erichelgeson/BlueSCSI), which in turn is derived from [ArdSCSIno-stm32](https://github.com/ztto/ArdSCSino-stm32). The firmware is available under GPL 3 license.

Major changes from BlueSCSI include:

- Separation of platform-specific functionality to separate file to ease porting.
- Ported to GD32F205.
- Removal of Arduino core dependency, as it was not currently available for GD32F205.
- Buffered log functions.
- Direct streaming between SD card and SCSI for slightly improved performance.
