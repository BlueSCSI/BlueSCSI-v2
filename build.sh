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

set -euo pipefail

# Require Bash 4+ for associative arrays
if [ "${BASH_VERSINFO[0]}" -lt 4 ]; then
    echo "Error: build.sh requires Bash 4.0 or later (you have ${BASH_VERSION})."
    echo ""
    echo "macOS ships Bash 3.2 due to GPL v3 licensing. To fix:"
    echo "  brew install bash"
    echo "Then re-run with:  /opt/homebrew/bin/bash build.sh"
    echo "Or add /opt/homebrew/bin/bash to /etc/shells and update your PATH."
    exit 1
fi

# Build BlueSCSI firmware targets
# Usage: ./build.sh [target...]       Build specified (or all default) targets
#        ./build.sh clean [target...]  Remove build artifacts

# Default targets
declare -A TARGETS=(
    [Pico_DaynaPORT]=pico_w
    [Pico_Audio_SPDIF]=pico_w
    [Pico_2_DaynaPORT]=pico2_w
    [Pico_2_Audio_SPDIF]=pico2_w
    [Ultra]=bluescsi_ultra
    [Ultra_Wide]=bluescsi_ultra_wide
)

# Non-default targets (build only when explicitly requested)
declare -A EXTRA_TARGETS=(
    [Pico]=pico
    [Pico_2]=pico2
)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_ROOT="${SCRIPT_DIR}/build"
OUTPUT_DIR="${BUILD_ROOT}/output"

# --- colors (auto-disable when not a terminal) ---
if [ -t 1 ]; then
    BOLD='\033[1m'
    CYAN='\033[36m'
    GREEN='\033[32m'
    RED='\033[31m'
    YELLOW='\033[33m'
    BOLD_CYAN='\033[1;36m'
    BOLD_GREEN='\033[1;32m'
    BOLD_RED='\033[1;31m'
    RESET='\033[0m'
else
    BOLD='' CYAN='' GREEN='' RED='' YELLOW=''
    BOLD_CYAN='' BOLD_GREEN='' BOLD_RED='' RESET=''
fi

# --- clean subcommand ---
if [ "${1:-}" = "clean" ]; then
    shift
    if [ $# -gt 0 ]; then
        # Clean specific targets
        for target in "$@"; do
            dir="${BUILD_ROOT}/${target}"
            if [ -d "${dir}" ]; then
                echo "Removing ${dir}"
                rm -rf "${dir}"
            else
                echo "Nothing to clean for ${target}"
            fi
        done
    else
        # Clean everything
        if [ -d "${BUILD_ROOT}" ]; then
            echo "Removing ${BUILD_ROOT}"
            rm -rf "${BUILD_ROOT}"
        else
            echo "Nothing to clean"
        fi
    fi
    exit 0
fi

# --- resolve targets ---
if [ $# -gt 0 ]; then
    declare -A BUILD_TARGETS
    for target in "$@"; do
        if [[ -v "TARGETS[$target]" ]]; then
            BUILD_TARGETS[$target]=${TARGETS[$target]}
        elif [[ -v "EXTRA_TARGETS[$target]" ]]; then
            BUILD_TARGETS[$target]=${EXTRA_TARGETS[$target]}
        else
            echo "Unknown target: $target"
            echo "Available targets: ${!TARGETS[*]} ${!EXTRA_TARGETS[*]}"
            exit 1
        fi
    done
else
    declare -A BUILD_TARGETS
    for target in "${!TARGETS[@]}"; do
        BUILD_TARGETS[$target]=${TARGETS[$target]}
    done
fi

mkdir -p "${OUTPUT_DIR}"

FAILED=()

for target in "${!BUILD_TARGETS[@]}"; do
    board=${BUILD_TARGETS[$target]}
    build_dir="${BUILD_ROOT}/${target}"
    echo -e "${BOLD_CYAN}========================================${RESET}"
    echo -e "${BOLD_CYAN}Building${RESET} ${BOLD}BlueSCSI_${target}${RESET} ${CYAN}(board=${board})${RESET}"
    echo -e "${BOLD_CYAN}========================================${RESET}"

    mkdir -p "${build_dir}"
    if cmake -S "${SCRIPT_DIR}" -B "${build_dir}" \
        -DPICO_BOARD="${board}" \
        -DBLUESCSI_TARGET="${target}" \
        -DCMAKE_BUILD_TYPE=Release 2>&1 && \
       cmake --build "${build_dir}" --parallel 2>&1; then
        # Copy UF2 to output directory
        uf2_file=$(find "${build_dir}" -maxdepth 1 -name "BlueSCSI_*.uf2" | head -1)
        if [ -n "${uf2_file}" ]; then
            cp "${uf2_file}" "${OUTPUT_DIR}/"
            echo -e "${GREEN}-> Copied $(basename "${uf2_file}") to build/output/${RESET}"
        fi
    else
        echo -e "${BOLD_RED}FAILED: BlueSCSI_${target}${RESET}"
        FAILED+=("${target}")
    fi
    echo ""
done

echo -e "${BOLD_CYAN}========================================${RESET}"
echo -e "${BOLD_GREEN}Build complete.${RESET} Firmware files in: ${OUTPUT_DIR}/"
ls -la "${OUTPUT_DIR}/"
echo ""

if [ ${#FAILED[@]} -gt 0 ]; then
    echo -e "${BOLD_RED}FAILED targets: ${FAILED[*]}${RESET}"
    exit 1
fi

# --- create universal firmware zip package ---
echo -e "${BOLD_CYAN}========================================${RESET}"
echo -e "${BOLD_CYAN}Creating firmware zip package${RESET}"
"${SCRIPT_DIR}/utils/create_firmware_zip.sh" "${BUILD_ROOT}" "${OUTPUT_DIR}"
echo ""

# --- memory usage comparison against latest release ---
echo -e "${BOLD_CYAN}========================================${RESET}"
python3 "${SCRIPT_DIR}/utils/compare_elf.py" --from-release "${BUILD_ROOT}"

# --- stack usage analysis ---
echo ""
echo -e "${BOLD_CYAN}========================================${RESET}"
echo -e "${BOLD_CYAN}Stack usage analysis${RESET}"
python3 "${SCRIPT_DIR}/utils/check_stack_usage.py" "${BUILD_ROOT}"
