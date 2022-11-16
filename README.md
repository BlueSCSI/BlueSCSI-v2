ZuluSCSI™ Firmware
=================

Hard Drive & ISO image files
---------------------
ZuluSCSI uses raw hard drive image files, which are stored on a FAT32 or exFAT-formatted SD card. These are often referred to as "hda" files.

Examples of valid filenames:
* `HD5.hda` or `HD5.img`: hard drive with SCSI ID 5
* `HD20_512.hda`: hard drive with SCSI ID 2, LUN 0, block size 512
* `CD3.iso`: CD drive with SCSI ID 3

In addition to the simplified filenames style above, the ZuluSCSI firmware also looks for images using the BlueSCSI-style "HDxy_512.hda" filename formatting.

The media type can be set in `zuluscsi.ini`, or directly by the file name prefix.
Supported prefixes are `HD` (hard drive), `CD` (cd-rom), `FD` (floppy), `MO` (magneto-optical), `RE` (generic removeable media), `TP` (sequential tape drive).

Log files and error indications
-------------------------------
Log messages are stored in `zululog.txt`, which is cleared on every boot.
Normally only basic initialization information is stored, but switching the `DBG` DIP switch on will cause every SCSI command to be logged, once the board is power cycled.

The indicator LED will normally report disk access.
It also reports following status conditions:

- 1 fast blink on boot: Image file loaded successfully
- 3 fast blinks: No images found on SD card
- 5 fast blinks: SD card not detected
- Continuous morse pattern: firmware crashed, morse code indicates crash location

In crashes the firmware will also attempt to save information into `zuluerr.txt`.

Configuration file
------------------
Optional configuration can be stored in `zuluscsi.ini`.
If image file is found but configuration is missing, a default configuration is used.

Example config file is available here: [zuluscsi.ini](zuluscsi.ini).

Performance
-----------
Performance information for the various ZuluSCSI hardware models is [documented separately, here](Performance.md)

Hotplugging
-----------
The firmware supports hot-plug removal and reinsertion of SD card.
The status led will blink continuously when card is not present, then blink once when card is reinserted successfully.

It will depend on the host system whether it gets confused by hotplugging.
Any IO requests issued when card is removed will be timeouted.

Programming & bootloader
------------------------
There is a bootloader that loads new firmware from SD card on boot.
The firmware file must be e.g. `ZuluSCSI.bin` or `ZuluSCSIv1_0_2022-xxxxx.bin`.
Firmware update takes about 1 second, during which the LED will flash rapidly.

When successful, the bootloader removes the update file and continues to main firmware.
On failure, `Zuluerr.txt` is written on the SD card.

Alternatively, the board can be programmed using USB connection in DFU mode by setting DIP switch 4.
The necessary programmer utility for Windows can be downloaded from [GD32 website](http://www.gd32mcu.com/en/download?kw=dfu&lan=en). On Linux and MacOS, the standard 'dfu-util' can be used. It can be installed via your package manager under Linux. On MacOS, it is available through MacPorts and Brew as a package.

`dfu-util --alt 0 --dfuse-address 0x08000000 --download ZuluSCSIv1_1_XXXXXX.bin`

For RP2040-based boards, the USB programming uses `.uf2` format file that can be copied to the USB drive that shows up in bootloader mode.

DIP switches
------------
For ZuluSCSI V1.1, the DIP switch settings are as follows:

- DEBUG: Enable verbose debug log (saved to `zululog.txt`)
- TERM: Enable SCSI termination
- BOOT: Enable built-in USB bootloader, this DIP switch MUST remain off during normal operation.
- SW1: Enables/disables Macintosh/Apple specific mode-pages and device strings, which eases disk initialization when performing fresh installs on legacy Macintosh computers.

ZuluSCSI Mini has no DIP switches, so all optional configuration parameters must be defined in zuluscsi.ini

ZuluSCSI RP2040 DIP switch settings are:
- INITIATOR: Enable SCSI initiator mode for imaging SCSI drives
- DEBUG LOG: Enable verbose debug log (saved to `zululog.txt`)
- TERMINATION: Enable SCSI termination
- BOOTLOADER: Enable built-in USB bootloader, this DIP switch MUST remain off during normal operation.

SCSI initiator mode
-------------------
The RP2040 model supports SCSI initiator mode for reading SCSI drives.
When enabled by the DIP switch, ZuluSCSI RP2040 will scan for SCSI drives on the bus and copy the data as `HDxx_imaged.hda` to the SD card.

LED indications in initiator mode:

- Short blink once a second: idle, searching for SCSI drives
- Fast blink 4 times per second: copying data. The blink acts as a progress bar: first it is short and becomes longer when data copying progresses.

The firmware retries reads up to 5 times and attempts to skip any sectors that have problems.
Any read errors are logged into `zululog.txt`.

Depending on hardware setup, you may need to mount diode `D205` and jumper `JP201` to supply `TERMPWR` to the SCSI bus.
This is necessary if the drives do not supply their own SCSI terminator power.

ROM drive in microcontroller flash
----------------------------------
The RP2040 model supports storing up to 1660kB image as a read-only drive in the
flash chip on the PCB itself. This can be used as e.g. a boot floppy that is available
even without SD card.

To initialize a ROM drive, name your image file as e.g. `HD0.rom`.
The drive type, SCSI ID and blocksize can be set in the filename the same way as for normal images.
On first boot, the LED will blink rapidly while the image is being loaded into flash memory.
Once loading is complete, the file is renamed to `HD0.rom_loaded` and the data is accessed from flash instead.

The status and maximum size of ROM drive are reported in `zululog.txt`.
To disable a previously programmed ROM drive, create empty file called `HD0.rom`.
If there is a `.bin` file with the same ID as the programmed ROM drive, it overrides the ROM drive.
There can be at most one ROM drive enabled at a time.

Project structure
-----------------
- **src/ZuluSCSI.cpp**: Main portable SCSI implementation.
- **src/ZuluSCSI_disk.cpp**: Interface between SCSI2SD code and SD card reading.
- **src/ZuluSCSI_log.cpp**: Simple logging functionality, uses memory buffering.
- **src/ZuluSCSI_config.h**: Some compile-time options, usually no need to change.
- **lib/ZuluSCSI_platform_GD32F205**: Platform-specific code for GD32F205.
- **lib/SCSI2SD**: SCSI2SD V6 code, used for SCSI command implementations.
- **lib/minIni**: Ini config file access library
- **lib/SdFat_NoArduino**: Modified version of [SdFat](https://github.com/greiman/SdFat) library for use without Arduino core.
- **utils/run_gdb.sh**: Helper script for debugging with st-link adapter. Displays SWO log directly in console.

To port the code to a new platform, see README in [lib/ZuluSCSI_platform_template](lib/ZuluSCSI_platform_template) folder.

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

* Separation of platform-specific functionality to separate directory to ease porting.
* Ported to GD32F205.
* Removal of Arduino core dependency, as it was not currently available for GD32F205.
* Buffered log functions.
* Simultaneous transfer between SD card and SCSI for improved performance.
