# BlueSCSI

BlueSCSI & ArdSCSino are hardware that reproduces SCSI devices (hard disks) with an Arduino STM32F103C (aka Blue Pill.)

`BlueSCSI` created by [erichelgeson](https://github.com/erichelgeson) is a fork of `ArdSCSino-stm32` which adds:
* Mac specific functionality
* Passive SCSI termination
* An alternative power source if not able to be powered by the SCSI bus

`ArdSCSino-stm32` created by [ztto](https://github.com/ztto/ArdSCSino-stm32) is the STM32 version of `ArdSCSino`

`ArdSCSino` created by [Tambo (TNB Seisakusho)](https://twitter.com/h_koma2)

# Purchase a Kit or Fully Assembled

https://gum.co/bluescsi-1b

# Questions?

Join us in #bluescsi on [Discord](https://discord.gg/Hgz25jrA)

Or open an issue on this repo.

## Compatibility

If your computer is not listed below - it may be compatible but there is no guarantee it will work. This device is designed and tested to work with pre-PowerPC Macs.

### Compatible

#### Tested by Me

SE, SE/30, Classic, Classic II, Color Classic, LC I/II/III, LC 475, LC 575

#### Reported by others

[IIcx](https://68kmla.org/forums/topic/61045-arduino-scsi-device-work-in-progress/?do=findComment&comment=663077), [PowerBook 100 (with custom adapter)](https://68kmla.org/forums/topic/61045-arduino-scsi-device-work-in-progress/?do=findComment&comment=664446)

### Incompatible 

Plus, PowerPC (SCSI-2?)

## Performance

Tested on a Color Classic with [SCSI Director Pro 4](https://macintoshgarden.org/apps/scsi-director-pro-40)
```
1024KB/Sec Write
900KB/Sec Read
1.01ms Seek
```

## Assembly

See [Assembly.md](docs/assembly.md)
## Usage

Hard Disk image files are placed in the root directory of a `FAT32` (or `ExFat`) formatted SD card. You can create a disk image using `dd` or download one from the `blank_images.zip` file. These images are compatible with the RaSCSI device and Basilisk II emulator.

The naming convention is as follows (file name max 32 characters). Note you may mount multiple drives at a time to different SCSI ID's.

`HDxy_512.hda`
```
HD - Hard Disk
x - SCSI ID to attach to. 0-7 (though 7 on a Macintosh is the System)
y - LUN id usually 0. 
512 - Sector size. Usually 512. 256, 512, & 1024 supported.
```

### Examples:

`HD10_512.hda` - Hard Disk at SCSI ID 1, LUN 0, sector size of 512. 
`HD51_512 System 6.0.8L LC.hda` - Hard Disk at SCSI ID 5, LUN 1, sector size of 512. Text between block size and .hda is ignored.

### Bad 

`HD99_712 foo bar fizz buzz bang.hda` Over 32 chars and invalid SCSI/LUN/Block size....

If no image files are found PC13 will pulse on and off. Check the log file for any errors.

### Termination

To enable termination place the two jumpers on the TERM block. Termination should be enabled if it is the last device in the chain - which is normally how it is used.

## Troubleshooting

### Log Files

If your device is not working - check the `LOG.txt` in the root of the SD card.

### Verify you are not using a fake

How to tell if your STM32 is fake: https://github.com/keirf/Greaseweazle/wiki/STM32-Fakes

### Performance is slower than expected

Try a different SD card. Cheap/old SD cards can affect performance.

# Development

Below is for users who wish to edit or develop on the BluePill - normal users should not need to worry about this.

## PlatformIO

Open the project in [PlatformIO](https://platformio.org/) and everything is pre-configured.

## Setup Arduino IDE (Depricated, Do Not Use)

You only need to setup the Arduino IDE if you are planning on developing and contributing to the project. Otherwise flash a provided `.bin` file.

1. Install `Arduino IDE` version `1.8.x` - https://www.arduino.cc/en/software
    1. Instructions may require extra steps to allow for serial/usb access depending on your OS. Read all instructions carefully.
1. Follow these instructions _**carefully**_ as well - https://github.com/rogerclarkmelbourne/Arduino_STM32/wiki/Installation
    1. **Use the 1.0.0 core** - not the master - https://github.com/rogerclarkmelbourne/Arduino_STM32/releases/tag/v1.0.0
    1. Note I used the `Arduino SAM Boards (32-bits ARM) Cortext-M3)` version `1.6.12`
    1. Additional Help https://sites.google.com/site/ericmklaus/projects-1/stm32f103-minimum-system-development-board/arduino-ide-for-stm32103-board
    1. Some additional hints here - https://github.com/ztto/ArdSCSino-stm32/issues/5
    1. If you get a warning about an unsigned binary - open the System Preferences -> Security -> allow stm32...
1. Install SdFat - Tools -> Manage Libraries -> `SdFat by Bill Greiman` version `2.0.2`
1. Make sure the correct board is selected `Generic STM32F103C series` - Copy the latest `.ino` file into a sketch and compile.
1. Flash using your preferred method.

## Flashing

When flashing you have two options:

Flash by setting up the IDE and re-compiling and programming. If you are a developer and plan to contribute this method is what you should use.

You may also flash the `.bin` file directly from STM32CubeProgrammer. If you are not a developer and just wish to get the code to the BluePill, use this (as development environment setup can be a bit tricky!)

### Using STLink v2 (prefered)

0. Remove SD Card
0. Make sure the STLink is up to date - [Latest Firmware](https://my.st.com/content/my_st_com/en/products/development-tools/software-development-tools/stm32-software-development-tools/stm32-programmers/stsw-link007.license=1610785194992.product=STSW-LINK007.version=2.37.26.html)
1. Connect pins `SWDIO`(2), `GND`(4), `SWCLK`(6), and `3.3v`(8) from the programmer to the Blue Pill. NOTE: These are not the same as Serial ports.
1. Using Arduino Studio Select Tools -> Upload Method -> STLink.
1. You should see the LED PC_13 flashing indicating no SD Card detected.
1. Insert SD Card, boot on your favorite Mac!

#### Trouble Shooting

If the device is not detected you may need to hold reset, click program, once it is detected release reset.

If you are unable to get the device in DFU mode for programming you may need to use [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html) to erase the chip.

### Using UART Serial

I used these - but any USB->TTL device should work https://www.amazon.com/gp/product/B07D6LLX19/

0. Remove SD Card if you have one installed.
1. Set the BOOT0 jumper to 1
1. Connect to your UART flashing device via the debug pins, then connect to your computer.
1. Click the reset button on the BluePill to reset and enter programming mode.
1. In Arduino Studio click Upload
    - Note you may have to click Upload more than once, or try restarting more than once.
1. You should see the LED flashing if you have no SD Card.
1. Unplug UART device.
1. Set the BOOT0 jumper to 0
1. Insert SD Card, boot on your favorite Mac!

### Using USB HID

Note: I have not been able to get this method to work.

## Making Gerber files

https://support.jlcpcb.com/article/44-how-to-export-kicad-pcb-to-gerber-files
