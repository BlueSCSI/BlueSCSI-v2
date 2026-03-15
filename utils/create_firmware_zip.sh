#!/usr/bin/env bash
# Copyright (C) 2026 Eric Helgeson
#
# This file is part of BlueSCSI
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

# Creates a universal firmware zip package from CMake build output.
# The zip contains one .bin per target, named:
#   BlueSCSI_<target>_<date>_<hash>.bin
#
# The zip itself is named:
#   BlueSCSI_v<version>_<hash>.zip
#
# Usage: utils/create_firmware_zip.sh <build_root> <output_dir>

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_ROOT="${1:?Usage: $0 <build_root> <output_dir>}"
OUTPUT_DIR="${2:?Usage: $0 <build_root> <output_dir>}"

# Extract version from BlueSCSI_config.h
FW_VER=$(grep 'FW_VER_NUM' "${PROJECT_DIR}/src/BlueSCSI_config.h" | head -1 | sed 's/.*"\(.*\)".*/\1/')
SHORT_HASH=$(git -C "${PROJECT_DIR}" rev-parse --short=7 HEAD 2>/dev/null || echo "unknown")
DATE=$(date +%Y-%m-%d)

ZIP_NAME="BlueSCSI_v${FW_VER}_${SHORT_HASH}.zip"
TMPDIR=$(mktemp -d)
trap 'rm -rf "${TMPDIR}"' EXIT

BIN_COUNT=0

# Find all built .bin files (produced by objcopy from .elf)
for target_dir in "${BUILD_ROOT}"/*/; do
    [ -d "${target_dir}" ] || continue
    target=$(basename "${target_dir}")

    # Skip the output directory itself
    [ "${target}" = "output" ] && continue

    # Look for the main firmware .elf (not bootloader)
    elf_file=$(find "${target_dir}" -maxdepth 1 -name "BlueSCSI_${target}.elf" 2>/dev/null | head -1)
    [ -n "${elf_file}" ] || continue

    bin_name="BlueSCSI_${target}_${DATE}_${SHORT_HASH}.bin"
    arm-none-eabi-objcopy -O binary "${elf_file}" "${TMPDIR}/${bin_name}"
    echo "  Added: ${bin_name}"
    BIN_COUNT=$((BIN_COUNT + 1))
done

if [ "${BIN_COUNT}" -eq 0 ]; then
    echo "No firmware binaries found in ${BUILD_ROOT}" >&2
    exit 1
fi

# Create zip with store-only compression (firmware can't decompress deflate)
mkdir -p "${OUTPUT_DIR}"
(cd "${TMPDIR}" && zip -0 "${OUTPUT_DIR}/${ZIP_NAME}" *.bin)

echo "Created ${OUTPUT_DIR}/${ZIP_NAME} with ${BIN_COUNT} firmware images"
