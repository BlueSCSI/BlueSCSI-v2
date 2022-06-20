#!/usr/bin/env bash

set -e
set -x

DATE=$(date +%Y%m%d)
VERSION="v1.1-$DATE"
mkdir -p dist
rm -f dist/*.bin

for f in $(ls .pio/build/*/firmware.bin); do
    NAME="BlueSCSI-$VERSION-$(echo $f | cut -d\/ -f3).bin"
    cp $f dist/$NAME
done
