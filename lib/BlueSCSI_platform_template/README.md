Porting BlueSCSI firmware to new platforms
==========================================

The BlueSCSI firmware is designed to be portable to a wide variety of platforms.
This directory contains an example platform definition that can serve as a base for
porting efforts.

Creating a new platform definition
----------------------------------

The bare minimum to support a new platform is to:

1. Make a copy of the `BlueSCSI_platform_template` folder to a new name, e.g. `BlueSCSI_platform_MyCustomHardware`
2. Make a copy of the `[env:template]` section to a new name, e.g. `[env:MyCustomHardware]`
3. Edit `BlueSCSI_platform_gpio.h` to match the pinout of your platform.
4. Edit `BlueSCSI_platform.h` for the hardware access functions implemented in your platform.
5. Edit `scsiPhy.cpp` to enable the `RST` and `BSY` interrupts.

Required IO capabilities
------------------------

The minimum IO capabilities for BlueSCSI firmware are:

* Bidirectional access to SCSI data bus: `DB0`-`DB7`, `DBP`
* Bidirectional access to SCSI signal `BSY`, with rising edge interrupt.
* Bidirectional access to SCSI signal `RST`, with falling edge interrupt.
* Output access to SCSI signals `REQ`, `IO`, `CD`, `MSG`
* Input access to SCSI signals `SEL`, `ACK`, `ATN`
* Access to SD card, using either SDIO or SPI bus.

RAM usage
---------

By default the BlueSCSI firmware uses large buffers for best performance.
The total RAM usage in default configuration is about 100 kB.
Minimum possible RAM usage is about 10 kB.

To reduce the RAM usage, following settings can be given in `platformio.ini` for the platform:

* `LOGBUFSIZE`: Default 16384, minimum 512 bytes
* `PREFETCH_BUFFER_SIZE`: Default 8192, minimum 0 bytes
* `MAX_SECTOR_SIZE`: Default 8192, minimum 512 bytes
* `SCSI2SD_BUFFER_SIZE`: Default `MAX_SECTOR_SIZE * 8`, minimum `MAX_SECTOR_SIZE * 2`

Enabling parallel transfers
---------------------------

Access performance is improved a lot if SCSI access can occur in parallel with SD card access.
To implement this, one or both of them must be able to execute transfers in background using hardware DMA.
On most platforms this is possible for SD card access.
The SCSI handshake mechanism is harder to implement using DMA.

To implement parallelism with SD card DMA, implement `platform_set_sd_callback(func, buffer)`.
It sets a callback function which should be called by the SD card driver to report how many bytes have
been transferred to/from `buffer` so far. The SD card driver should call this function in a loop while
it is waiting for SD card transfer to finish. The code in `BlueSCSI_disk.cpp` will implement the callback
that will transfer the data to SCSI bus during the wait.
