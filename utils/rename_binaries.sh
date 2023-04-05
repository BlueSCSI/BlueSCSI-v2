#!/bin/bash

# This script renames the built binaries according to version
# number and platform.

mkdir -p distrib

DATE=$(date +%Y-%m-%d)
VERSION=$(git describe --always)

for file in $(ls .pio/build/*/*.bin .pio/build/*/*.elf .pio/build/*/*.uf2)
do
    NEWNAME=$(echo $file | sed 's|.pio/build/\([^/]*\)/\(.*\)\.\(.*\)|\1_'$DATE'_'$VERSION'.\3|')
    echo $file to distrib/$NEWNAME
    cp $file distrib/$NEWNAME
done
