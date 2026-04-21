#!/usr/bin/env bash

# BlueSCSI - Copyright (c) 2025-2026 Eric Helgeson
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

# Prepares dist/ directory for GitHub release from CMake build output.
#
# Produces:
#   BlueSCSI_v<ver>_<hash>.zip              - Universal firmware zip for SD card update
#   BlueSCSI_V2_DaynaPORT_<date>_<hash>.uf2 - Combined Pico+Pico2 DaynaPORT UF2
#   BlueSCSI_V2_Audio_SPDIF_<date>_<hash>.uf2 - Combined Pico+Pico2 Audio SPDIF UF2
#   BlueSCSI_Ultra_<date>_<hash>.uf2        - Ultra UF2
#   BlueSCSI_Ultra_Wide_<date>_<hash>.uf2   - Ultra Wide UF2
#   dev-BlueSCSI_<date>_<hash>.zip          - All ELF+UF2 files for developers

set -euo pipefail

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
cd "$SCRIPT_DIR/.." || exit 1
OUT_DIR=./dist
mkdir -p "$OUT_DIR"

DATE=$(date +%Y-%m-%d)
VERSION=$(git rev-parse --short=7 HEAD)

# --- Copy firmware zip (for SD card update) ---
# build.sh already creates this via utils/create_firmware_zip.sh
for zip in build/output/BlueSCSI_v*.zip; do
    [ -e "$zip" ] || continue
    cp "$zip" "$OUT_DIR/"
    echo "Firmware zip: $(basename "$zip")"
done

# --- Copy and rename UF2 and ELF files per target ---
# Only look in build/<target>/ dirs, skip build/output/
for dir in build/*/; do
    [ -d "$dir" ] || continue
    TARGET=$(basename "$dir")
    [ "$TARGET" = "output" ] && continue

    for file in "${dir}"BlueSCSI_"${TARGET}".uf2 "${dir}"BlueSCSI_"${TARGET}".elf; do
        [ -e "$file" ] || continue
        EXT="${file##*.}"
        RENAME="BlueSCSI_${TARGET}_${DATE}_${VERSION}.${EXT}"
        echo "$file -> $OUT_DIR/$RENAME"
        cp "$file" "$OUT_DIR/$RENAME"
    done
done

ls -1 "$OUT_DIR"

# --- Dev archive: ELF + UF2 for debugging ---
# README explains what this archive is for so end users who grabbed it by
# mistake know to fetch the SD-card .zip or a UF2 instead.
DEV_README="$OUT_DIR/DEVELOPER BUILD ARTIFACTS.txt"
cat > "$DEV_README" <<'EOF'
BlueSCSI Developer Build Artifacts
==================================

This ZIP contains developer and debugging artifacts for a BlueSCSI
release:

  - *.elf   Firmware with debug symbols (GDB, addr2line, crash analysis)
  - *.uf2   Flashable firmware images (one per build target)

If you're a regular BlueSCSI user, you do NOT need this file. Use one
of these from the release page instead:

  BlueSCSI_v<version>_<hash>.zip  SD card updater (recommended)
  BlueSCSI_V2_DaynaPORT_*.uf2     USB/BOOTSEL flashing, V2 boards
  BlueSCSI_V2_Audio_SPDIF_*.uf2   USB/BOOTSEL flashing, V2 SPDIF
  BlueSCSI_Ultra_*.uf2            USB/BOOTSEL flashing, Ultra
  BlueSCSI_Ultra_Wide_*.uf2       USB/BOOTSEL flashing, Ultra Wide

End-user update guide:

  https://github.com/BlueSCSI/BlueSCSI-v2/wiki/Updating-Firmware

Typical uses for this developer archive:

  - Post-mortem crash analysis (addr2line / utils/analyze_crashlog.sh
    against the exact release)
  - Binary comparisons between releases
  - GDB debugging with a CMSIS-DAP probe
EOF

zip -j "$OUT_DIR/dev-BlueSCSI_${DATE}_${VERSION}.zip" "$OUT_DIR"/*.elf "$OUT_DIR"/*.uf2 "$DEV_README"
rm "$OUT_DIR/"*.elf
rm "$DEV_README"

# --- Combined V2 UF2s: merge Pico + Pico 2 variants for the BlueSCSI V2 board ---
# UF2 format allows combining RP2040 and RP2350 images — the bootrom ignores
# blocks for the wrong chip, so one file works on either.
combine_v2_uf2() {
    local variant="$1"
    local pico1="$OUT_DIR/BlueSCSI_Pico_${variant}_${DATE}_${VERSION}.uf2"
    local pico2="$OUT_DIR/BlueSCSI_Pico_2_${variant}_${DATE}_${VERSION}.uf2"
    local combined="$OUT_DIR/BlueSCSI_V2_${variant}_${DATE}_${VERSION}.uf2"

    if [ -f "$pico1" ] && [ -f "$pico2" ]; then
        cat "$pico1" "$pico2" > "$combined"
        rm "$pico1" "$pico2"
        echo "Created $combined"
    fi
}

combine_v2_uf2 "DaynaPORT"
combine_v2_uf2 "Audio_SPDIF"

echo ""
echo "Distribution files:"
ls -1 "$OUT_DIR"
