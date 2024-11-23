#!/bin/bash

# Deploy compiled firmware to RP2040-based board
#
# Usage:
#   ./deploy.sh {path/to/device} {path/to/uf2}
#
# Examples:
#   macOS:       ./deploy.sh /dev/cu.usbmodem1.1 /build/App-IRQs/IRQS_DEMO.uf2
#   Linux RPiOS: ./deploy.sh /dev/ttyACMO        /build/App-IRQs/IRQS_DEMO.uf2

show_error_and_exit() {
    echo "[ERROR] $1"
    exit 1
}

if [[ -z ${1} ]]; then
    echo "Usage: deploy.sh {path/to/device} {path/to/uf2}"
    exit 0
fi

if [[ -z ${2} || ${2##*.} != "uf2" ]]; then
    echo "[ERROR] No .uf2 file specified"
    exit 1
fi

if [[ ! -f ${2} ]]; then
    echo "[ERROR] ${2} cannot be found"
    exit 1
fi

# Put the Pico onto BOOTSEL mode
platform=$(uname)
if [[ ${platform} = Darwin ]]; then
    # macOS mount path
    pico_path=/Volumes/RPI-RP2
    stty -f ${1} 1200 || show_error_and_exit "Could not connect to device ${1}"
else
    # NOTE This is for Raspberry Pi -- you may need to change it
    #      depending on how you or your OS locate USB drive mount points
    pico_path="/media/$USER/RPI-RP2"
    stty -F ${1} 1200 || show_error_and_exit "Could not connect to device ${1}"

    # Allow for command line usage -- ie. not in a GUI terminal
    # Command line is SHLVL 1, so script is SHLVL 2 (under the GUI we'd be a SHLVL 3)
    if [[ $SHLVL -eq 2 ]]; then
        # Mount the disk, but allow time for it to appear (not immediate on RPi)
        sleep 5
        rp2_disk=$(sudo fdisk -l | grep FAT16 | cut -f 1 -d ' ')
        if [[ -z ${rp2_disk} ]]; then
            show_error_and_exit "Could not see device ${1}"
        fi

        sudo mkdir ${pico_path} || show_error_and_exit "Could not make mount point ${pico_path}"
        sudo mount ${rp2_disk} ${pico_path} -o rw || show_error_and_exit "Could not mount device ${1}"
    fi
fi

echo "Waiting for Pico to mount..."
count=0
while [ ! -d ${pico_path} ]; do
    sleep 0.1
    ((count+=1))
    [[ ${count} -eq 200 ]] && show_error_and_exit "Pico mount timed out"
done
sleep 0.5

# Copy the target file
echo "Copying ${2} to ${1}..."
if [[ ${platform} = Darwin ]]; then
    cp ${2} ${pico_path}
else
    sudo cp ${2} ${pico_path}
    if [[ $SHLVL -eq 2 ]]; then
        # We're at the command line, so unmount (RPi GUI does this automatically)
        sudo umount ${rp2_disk} && echo "Pico unmounted" && sudo rm -rf ${pico_path} && echo "Mountpoint removed"
    fi
fi
echo Done
