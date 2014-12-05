#!/bin/sh

case `uname -s` in
Linux)
	# Builds all of the utilities (not firmware) under Linux.
	# Requires mingw installed to cross-compile Windows targets.

	(cd scsi2sd-util && ./build.sh) &&
	(cd scsi2sd-debug && ./build.sh)

	if [ $? -eq 0 ]; then
		mkdir -p build/linux
		mkdir -p build/windows/64bit
		mkdir -p build/windows/32bit

		cp scsi2sd-util/build/linux/scsi2sd-util build/linux
		cp scsi2sd-debug/build/linux/scsi2sd-debug build/linux

		cp scsi2sd-util/build/windows/32bit/scsi2sd-util.exe build/windows/32bit
		cp scsi2sd-debug/build/windows/32bit/scsi2sd-debug.exe build/windows/32bit

		cp scsi2sd-util/build/windows/64bit/scsi2sd-util.exe build/windows/64bit
		cp scsi2sd-debug/build/windows/64bit/scsi2sd-debug.exe build/windows/64bit
	fi
;;

Darwin)
	make -C scsi2sd-util &&
	make -C scsi2sd-debug

	if [ $? -eq 0 ]; then
		mkdir -p build/mac

		cp scsi2sd-util/build/mac/scsi2sd-util build/mac
		cp scsi2sd-debug/build/mac/scsi2sd-debug build/mac
	fi

esac
