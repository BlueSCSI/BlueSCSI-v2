#!/usr/bin/env bash

# BlueSCSI - Copyright (c) 2025 Eric Helgeson
#
# BlueSCSI file is licensed under the GPL version 3 or any later version.
#
# https://www.gnu.org/licenses/gpl-3.0.html
# ----
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
cd "$SCRIPT_DIR/.." || exit 1
OUT_DIR=./dist
mkdir -p $OUT_DIR || exit 1

DATE=$(date +%Y-%m-%d)
VERSION=$(git rev-parse --short HEAD)

for file in .pio/build/*/firmware.bin .pio/build/*/firmware.elf .pio/build/*/firmware.uf2
do
    BUILD_ENV=$(echo "$file" | cut -d'/' -f3)

    if [[ "$BUILD_ENV" == *"Pico_2"* ]]; then
        BOARD="Pico2"
    elif [[ "$BUILD_ENV" == *"Pico"* ]]; then
        BOARD="Pico1"
    else
        echo "Warning: Could not determine board for $file"
        continue
    fi

    VARIANT=""
    if [[ "$BUILD_ENV" == *"DaynaPORT"* ]]; then
        VARIANT="_DaynaPORT"
    fi

    EXT="${file##*.}"

    RENAME="BlueSCSI_${BOARD}${VARIANT}_${DATE}_${VERSION}.${EXT}"

    echo "$file to $OUT_DIR/$RENAME"
    cp "$file" "$OUT_DIR/$RENAME"
done
set -e
set -x
ls -1 "$OUT_DIR"

# Zip up elf/uf2 files for each board variant
zip -j "$OUT_DIR/dev-BlueSCSI_${DATE}_${VERSION}.zip" "$OUT_DIR"/*.elf "$OUT_DIR"/*.uf2
rm "$OUT_DIR/"*.elf
# Create universal UF2 by combining the Pico1 and Pico2 UF2 files;
cat "$OUT_DIR/BlueSCSI_Pico1_DaynaPORT_${DATE}_${VERSION}.uf2" \
    "$OUT_DIR/BlueSCSI_Pico2_DaynaPORT_${DATE}_${VERSION}.uf2" > "$OUT_DIR/BlueSCSI_Universal_${DATE}_${VERSION}.uf2"
# Remove unused UF2 files
#rm "$OUT_DIR/BlueSCSI_Pico1_${DATE}_${VERSION}.uf2"
#rm "$OUT_DIR/BlueSCSI_Pico2_${DATE}_${VERSION}.uf2"
rm "$OUT_DIR/BlueSCSI_Pico1_DaynaPORT_${DATE}_${VERSION}.uf2"
rm "$OUT_DIR/BlueSCSI_Pico2_DaynaPORT_${DATE}_${VERSION}.uf2"

# Rename bins for SD Card update.
mv "$OUT_DIR/BlueSCSI_Pico1_DaynaPORT_${DATE}_${VERSION}.bin" "$OUT_DIR/BlueSCSI_Pico1_${DATE}_${VERSION}.bin"
mv "$OUT_DIR/BlueSCSI_Pico2_DaynaPORT_${DATE}_${VERSION}.bin" "$OUT_DIR/BlueSCSI_Pico2_${DATE}_${VERSION}.bin"

# Make a zip file with all the pico1 and pico2 files together by variant such as DaynaPORT
#zip -j "dist/BlueSCSI-Universal-DaynaPORT-${DATE}-${VERSION}.zip" dist/BlueSCSI-Pico*-DaynaPORT-${DATE}-${VERSION}.*
#find dist/ -maxdepth 1 -type f -name "BlueSCSI-Pico*-${DATE}-${VERSION}.*" ! -name "*-DaynaPORT-*" -print0 | xargs -0 zip -j "dist/BlueSCSI-Universal-${DATE}-${VERSION}.zip"
