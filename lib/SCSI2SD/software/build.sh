#!/bin/sh

case `uname -s` in
Linux)
	# Builds all of the utilities (not firmware) under Linux.
	# Requires mingw installed to cross-compile Windows targets.

	(cd bootloaderhost && ./build.sh) &&
	(cd scsi2sd-config && ./build.sh) &&
	(cd scsi2sd-debug && ./build.sh)

	if [ $? -eq 0 ]; then
		mkdir -p build/linux
		mkdir -p build/windows/64bit
		mkdir -p build/windows/32bit

		cp bootloaderhost/build/linux/bootloaderhost build/linux
		cp scsi2sd-config/build/linux/scsi2sd-config build/linux
		cp scsi2sd-debug/build/linux/scsi2sd-debug build/linux

		cp bootloaderhost/build/windows/32bit/bootloaderhost.exe build/windows/32bit
		cp scsi2sd-config/build/windows/32bit/scsi2sd-config.exe build/windows/32bit
		cp scsi2sd-debug/build/windows/32bit/scsi2sd-debug.exe build/windows/32bit

		cp bootloaderhost/build/windows/64bit/bootloaderhost.exe build/windows/64bit
		cp scsi2sd-config/build/windows/64bit/scsi2sd-config.exe build/windows/64bit
		cp scsi2sd-debug/build/windows/64bit/scsi2sd-debug.exe build/windows/64bit
	fi
;;

Darwin)
	make -C bootloaderhost &&
	make -C scsi2sd-config &&
	make -C scsi2sd-debug

	if [ $? -eq 0 ]; then
		mkdir -p build/mac

		cp bootloaderhost/build/mac/bootloaderhost build/mac
		cp scsi2sd-config/build/mac/scsi2sd-config build/mac
		cp scsi2sd-debug/build/mac/scsi2sd-debug build/mac
	fi

esac
