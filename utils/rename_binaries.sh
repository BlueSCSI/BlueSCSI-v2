#!/usr/bin/env bash

# This script renames the built binaries according to version
# number and platform.
set -e
set -x
mkdir -p distrib

DATE=$(date +%Y-%m-%d)
VERSION=$(git describe --always)

for file in .pio/build/*/*.bin .pio/build/*/*.elf .pio/build/*/*.uf2
do
    NEWNAME=$(echo "$file" | sed 's|.pio/build/\([^/]*\)/\(.*\)\.\(.*\)|\1_'$DATE'_'$VERSION'.\3|')
    echo "$file" to distrib/"$NEWNAME"
    cp "$file" distrib/"$NEWNAME"
done

cat distrib/*.uf2 > distrib/BlueSCSI_Universal_"$DATE"_"$VERSION".uf2