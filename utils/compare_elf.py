#!/usr/bin/env python3
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

"""Compare BlueSCSI ELF files: section sizes, memory regions, and symbol placement.

Modes:
    Compare against latest GitHub release (used by build.sh):
        ./compare_elf.py --from-release <dir_or_elf>
        ./compare_elf.py --from-release <dir_or_elf> --symbols

    Memory usage summary (single build, no comparison):
        ./compare_elf.py --summary <dir_or_elf>

    Full comparison (two ELF files or directories):
        ./compare_elf.py <old_dir> <new_dir>
        ./compare_elf.py <old.elf> <new.elf> --symbols

    Brief comparison (one-line-per-target summary):
        ./compare_elf.py --brief <old_dir> <new_dir>

    Save a baseline snapshot:
        ./compare_elf.py --save-baseline <dir_or_elf> [-o baseline.json]

    Check current build against baseline:
        ./compare_elf.py --check <dir_or_elf> --baseline baseline.json

    Markdown report for GitHub PR comments:
        ./compare_elf.py --from-release <dir_or_elf> --markdown [-o report.md]

Examples:
    # Compare build against latest release (run automatically by build.sh)
    ./utils/compare_elf.py --from-release build/

    # Show memory usage of current build
    ./utils/compare_elf.py --summary build/

    # Quick size comparison between two builds
    ./utils/compare_elf.py --brief /path/to/old_build/ build/

    # Show functions that moved between FLASH and RAM
    ./utils/compare_elf.py old.elf new.elf --symbols
"""

import json
import subprocess
import sys
import os
import re
from pathlib import Path
from collections import defaultdict


# ── Color helpers (TTY-aware) ────────────────────────────────────────

_USE_COLOR = hasattr(sys.stdout, "isatty") and sys.stdout.isatty()


def _sgr(code):
    """Return an ANSI SGR escape if color is enabled, else empty string."""
    return f"\033[{code}m" if _USE_COLOR else ""


# Styles
RESET = _sgr(0)
BOLD = _sgr(1)
DIM = _sgr(2)
RED = _sgr(31)
GREEN = _sgr(32)
YELLOW = _sgr(33)
CYAN = _sgr(36)
BOLD_RED = _sgr("1;31")
BOLD_GREEN = _sgr("1;32")
BOLD_YELLOW = _sgr("1;33")
BOLD_CYAN = _sgr("1;36")
BOLD_WHITE = _sgr("1;37")

# Canonical target name mapping (PIO name → CMake name)
TARGET_ALIASES = {
    "Pico1_DaynaPORT": "Pico_DaynaPORT",
    "Pico1_Audio_SPDIF": "Pico_Audio_SPDIF",
    "Pico2_DaynaPORT": "Pico_2_DaynaPORT",
    "Pico2_Audio_SPDIF": "Pico_2_Audio_SPDIF",
    "Ultra": "Ultra",
    "Ultra_Wide": "Ultra_Wide",
    "Pico_DaynaPORT": "Pico_DaynaPORT",
    "Pico_Audio_SPDIF": "Pico_Audio_SPDIF",
    "Pico_2_DaynaPORT": "Pico_2_DaynaPORT",
    "Pico_2_Audio_SPDIF": "Pico_2_Audio_SPDIF",
}

# Size change threshold for --check mode (bytes)
REGION_CHANGE_THRESHOLD = 256


def run(cmd):
    """Run a command and return stdout."""
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Error running {' '.join(cmd)}: {result.stderr}", file=sys.stderr)
        sys.exit(1)
    return result.stdout


def check_tools():
    """Verify arm-none-eabi tools are available."""
    for tool in ["arm-none-eabi-objdump", "arm-none-eabi-nm", "arm-none-eabi-size"]:
        try:
            subprocess.run([tool, "--version"], capture_output=True, check=True)
        except FileNotFoundError:
            print(f"Error: {tool} not found. Install arm-none-eabi-binutils.", file=sys.stderr)
            sys.exit(1)


def parse_sections(elf_path):
    """Parse objdump -h output into section list."""
    output = run(["arm-none-eabi-objdump", "-h", str(elf_path)])
    sections = []
    lines = output.split("\n")
    i = 0
    while i < len(lines):
        m = re.match(
            r"\s+\d+\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+\d\*\*\d+",
            lines[i],
        )
        if m:
            name = m.group(1)
            size = int(m.group(2), 16)
            vma = int(m.group(3), 16)
            lma = int(m.group(4), 16)
            flags = ""
            if i + 1 < len(lines):
                flags = lines[i + 1].strip()
            sections.append({
                "name": name,
                "size": size,
                "vma": vma,
                "lma": lma,
                "flags": flags,
            })
        i += 1
    return sections


def classify_region(vma, is_rp2350=False):
    """Classify a VMA address into a memory region name.

    RP2040 memory map:
        RAM:       0x20000000 - 0x2003FFFF  (256 KB)
        SCRATCH_X: 0x20040000 - 0x20040FFF  (4 KB)
        SCRATCH_Y: 0x20041000 - 0x20041FFF  (4 KB)

    RP2350 memory map:
        RAM:       0x20000000 - 0x2007FFFF  (512 KB)
        SCRATCH_X: 0x20080000 - 0x20080FFF  (4 KB)
        SCRATCH_Y: 0x20081000 - 0x20081FFF  (4 KB)
    """
    if 0x10000000 <= vma < 0x10200000:
        return "FLASH"
    elif 0x20000000 <= vma < 0x20040000:
        return "RAM"
    elif 0x20040000 <= vma < 0x20080000:
        if is_rp2350:
            return "RAM"  # RP2350 extended RAM (SRAM4-7)
        if 0x20040000 <= vma < 0x20041000:
            return "SCRATCH_X"
        elif 0x20041000 <= vma < 0x20042000:
            return "SCRATCH_Y"
        return "OTHER"
    elif 0x20080000 <= vma < 0x20081000:
        return "SCRATCH_X"
    elif 0x20081000 <= vma < 0x20082000:
        return "SCRATCH_Y"
    return "OTHER"


def compute_region_usage(sections, is_rp2350=False):
    """Compute total bytes used in each memory region.

    Only counts sections with the ALLOC flag. Sections without ALLOC
    (like .heap, .stack_dummy marked COPY in linker scripts) are overlays
    and don't consume additional memory.
    """
    usage = defaultdict(int)
    for s in sections:
        if s["size"] == 0:
            continue
        if "ALLOC" not in s.get("flags", ""):
            continue
        region = classify_region(s["vma"], is_rp2350)
        usage[region] += s["size"]
        if "LOAD" in s["flags"]:
            flash_region = classify_region(s["lma"], is_rp2350)
            if flash_region == "FLASH" and region != "FLASH":
                usage["FLASH"] += s["size"]
    return dict(usage)


def parse_symbols(elf_path):
    """Parse nm output into symbol dict: name -> (address, size, type).

    Uses --demangle so names are human-readable C++ names.
    Demangled names may contain spaces, so we split only the first 3 fields
    (addr, size, type) and treat the rest as the symbol name.
    """
    output = run(["arm-none-eabi-nm", "--size-sort", "-S", "--demangle", str(elf_path)])
    symbols = {}
    for line in output.strip().split("\n"):
        parts = line.split(None, 3)
        if len(parts) >= 4:
            addr = int(parts[0], 16)
            size = int(parts[1], 16)
            stype = parts[2]
            name = parts[3]
            if name.startswith("__") or size < 4:
                continue
            symbols[name] = (addr, size, stype)
    return symbols


def get_symbol_regions(elf_path, is_rp2350=False):
    """Return dict of symbol_name -> region for all symbols in an ELF."""
    symbols = parse_symbols(elf_path)
    return {name: classify_region(addr, is_rp2350) for name, (addr, size, stype) in symbols.items()}


def format_size(n):
    """Format byte count with units."""
    if abs(n) >= 1024 * 1024:
        return f"{n / (1024 * 1024):.1f} MB"
    elif abs(n) >= 1024:
        return f"{n / 1024:.1f} KB"
    return f"{n} B"


def format_delta(old_val, new_val):
    """Format a delta with sign and percentage."""
    delta = new_val - old_val
    if old_val == 0:
        if delta == 0:
            return "  (same)"
        return f"  (+{format_size(delta)})"
    pct = (delta / old_val) * 100
    sign = "+" if delta > 0 else ""
    return f"  ({sign}{format_size(delta)}, {sign}{pct:.1f}%)"


def region_capacity(region, is_rp2350=False):
    """Return the total capacity of a memory region."""
    caps = {
        "FLASH": 2 * 1024 * 1024,
        "RAM": 512 * 1024 if is_rp2350 else 256 * 1024,
        "SCRATCH_X": 4 * 1024,
        "SCRATCH_Y": 4 * 1024,
    }
    return caps.get(region, 0)


def detect_rp2350(sections):
    """Detect RP2350 from section addresses."""
    for s in sections:
        vma = s["vma"]
        if 0x20040000 <= vma < 0x20080000 and s["name"] not in (".scratch_x", ".scratch_y", ".stack1_dummy"):
            return True
        if 0x20080000 <= vma < 0x20082000:
            return True
    return False


def extract_target_name(path):
    """Extract canonical target name from an ELF filename."""
    basename = Path(path).stem
    name = re.sub(r"^BlueSCSI_", "", basename)
    name = re.sub(r"_\d{4}-\d{2}-\d{2}_[0-9a-f]+$", "", name)
    return TARGET_ALIASES.get(name, name)


def find_elfs(path):
    """Find all firmware ELF files in a path (file or directory)."""
    p = Path(path)
    elfs = {}
    if p.is_dir():
        for f in p.rglob("BlueSCSI_*.elf"):
            if "bootloader" in f.name:
                continue
            target = extract_target_name(f)
            elfs[target] = f
    elif p.is_file():
        target = extract_target_name(p)
        elfs[target] = p
    return elfs


def find_elf_pairs(old_path, new_path):
    """Find matching ELF pairs from two paths."""
    old_elfs = find_elfs(old_path)
    new_elfs = find_elfs(new_path)

    pairs = []
    for target in sorted(set(old_elfs) | set(new_elfs)):
        if target in old_elfs and target in new_elfs:
            pairs.append((target, old_elfs[target], new_elfs[target]))
        elif target in old_elfs:
            print(f"  Warning: {target} only in old, skipping", file=sys.stderr)
        else:
            print(f"  Warning: {target} only in new, skipping", file=sys.stderr)
    return pairs


# ── Full comparison mode ──────────────────────────────────────────────


def print_section_comparison(old_sections, new_sections, is_rp2350=False):
    """Print side-by-side section comparison."""
    skip_prefixes = (".debug_", ".comment", ".ARM.attributes")

    old_by_name = {s["name"]: s for s in old_sections}
    new_by_name = {s["name"]: s for s in new_sections}
    all_names = [n for n in dict.fromkeys(
        [s["name"] for s in old_sections] + [s["name"] for s in new_sections]
    ) if not any(n.startswith(p) for p in skip_prefixes)]

    print(f"\n{'Section':<25} {'Old Size':>10} {'New Size':>10} {'Delta':>10}  {'Old VMA':>10} {'New VMA':>10} {'Old Region':>10} {'New Region':>10}")
    print("-" * 120)

    for name in all_names:
        old = old_by_name.get(name)
        new = new_by_name.get(name)

        old_size = old["size"] if old else 0
        new_size = new["size"] if new else 0
        delta = new_size - old_size

        old_vma = f"0x{old['vma']:08x}" if old else "-"
        new_vma = f"0x{new['vma']:08x}" if new else "-"
        old_region = classify_region(old["vma"], is_rp2350) if old else "-"
        new_region = classify_region(new["vma"], is_rp2350) if new else "-"

        region_marker = ""
        if old and new and old_region != new_region:
            region_marker = " *** MOVED ***"

        delta_str = f"{delta:+d}" if delta != 0 else "0"
        if old_size == 0 and new_size > 0:
            delta_str = f"+{new_size} (new)"
        elif old_size > 0 and new_size == 0:
            delta_str = f"-{old_size} (gone)"

        print(f"{name:<25} {old_size:>10,} {new_size:>10,} {delta_str:>10}  {old_vma:>10} {new_vma:>10} {old_region:>10} {new_region:>10}{region_marker}")


def print_region_summary(old_usage, new_usage, old_sections, new_sections):
    """Print memory region usage summary."""
    is_rp2350 = detect_rp2350(old_sections) or detect_rp2350(new_sections)
    all_regions = sorted(set(list(old_usage.keys()) + list(new_usage.keys())))

    print(f"\n{'Region':<12} {'Old':>12} {'New':>12} {'Delta':>20} {'Capacity':>12} {'Old %':>8} {'New %':>8}")
    print("-" * 90)

    for region in all_regions:
        if region == "OTHER":
            continue
        old_val = old_usage.get(region, 0)
        new_val = new_usage.get(region, 0)
        cap = region_capacity(region, is_rp2350)
        delta_str = format_delta(old_val, new_val)
        old_pct = f"{old_val / cap * 100:.1f}%" if cap > 0 else "-"
        new_pct = f"{new_val / cap * 100:.1f}%" if cap > 0 else "-"
        print(f"{region:<12} {format_size(old_val):>12} {format_size(new_val):>12} {delta_str:>20} {format_size(cap):>12} {old_pct:>8} {new_pct:>8}")


def print_symbol_moves(old_elf, new_elf, is_rp2350=False):
    """Show functions/data that moved between FLASH and RAM."""
    old_syms = parse_symbols(old_elf)
    new_syms = parse_symbols(new_elf)

    moves = []
    for name in sorted(set(old_syms) & set(new_syms)):
        old_addr, old_size, old_type = old_syms[name]
        new_addr, new_size, new_type = new_syms[name]
        old_region = classify_region(old_addr, is_rp2350)
        new_region = classify_region(new_addr, is_rp2350)
        if old_region != new_region:
            moves.append((name, old_region, new_region, old_size, new_size))

    if not moves:
        print("\nNo symbols moved between memory regions.")
        return

    flash_to_ram = [(n, os, ns) for n, o, new, os, ns in moves if o == "FLASH" and new == "RAM"]
    ram_to_flash = [(n, os, ns) for n, o, new, os, ns in moves if o == "RAM" and new == "FLASH"]
    other_moves = [(n, o, new, os, ns) for n, o, new, os, ns in moves
                   if not (o == "FLASH" and new == "RAM") and not (o == "RAM" and new == "FLASH")]

    if flash_to_ram:
        total = sum(ns for _, _, ns in flash_to_ram)
        print(f"\n  FLASH -> RAM ({len(flash_to_ram)} symbols, {format_size(total)} added to RAM):")
        for name, old_size, new_size in sorted(flash_to_ram, key=lambda x: -x[2])[:30]:
            print(f"    {name:<60} {format_size(new_size):>10}")
        if len(flash_to_ram) > 30:
            print(f"    ... and {len(flash_to_ram) - 30} more")

    if ram_to_flash:
        total = sum(os for _, os, _ in ram_to_flash)
        print(f"\n  RAM -> FLASH ({len(ram_to_flash)} symbols, {format_size(total)} freed from RAM):")
        for name, old_size, new_size in sorted(ram_to_flash, key=lambda x: -x[1])[:30]:
            print(f"    {name:<60} {format_size(old_size):>10}")
        if len(ram_to_flash) > 30:
            print(f"    ... and {len(ram_to_flash) - 30} more")

    if other_moves:
        print(f"\n  Other region moves ({len(other_moves)} symbols):")
        for name, old_r, new_r, old_size, new_size in other_moves[:15]:
            print(f"    {name:<50} {old_r:>10} -> {new_r:<10} {format_size(new_size):>10}")


def compare_brief(pairs):
    """One-line-per-target region summary."""
    # Find the longest target name for alignment
    max_name = max(len(t) for t, _, _ in pairs) if pairs else 0

    for target, old_elf, new_elf in pairs:
        old_sections = parse_sections(old_elf)
        new_sections = parse_sections(new_elf)
        is_rp2350 = detect_rp2350(old_sections) or detect_rp2350(new_sections)
        old_usage = compute_region_usage(old_sections, is_rp2350)
        new_usage = compute_region_usage(new_sections, is_rp2350)

        parts = []
        for region in ("FLASH", "RAM", "SCRATCH_X", "SCRATCH_Y"):
            old_val = old_usage.get(region, 0)
            new_val = new_usage.get(region, 0)
            if old_val == 0 and new_val == 0:
                continue
            delta = new_val - old_val
            cap = region_capacity(region, is_rp2350)
            pct = new_val / cap * 100 if cap > 0 else 0
            if abs(delta) < REGION_CHANGE_THRESHOLD:
                parts.append(f"{BOLD}{region}{RESET} {format_size(new_val)} [{pct:.0f}%]")
            else:
                sign = "+" if delta > 0 else ""
                delta_color = RED if delta > 0 else GREEN
                parts.append(
                    f"{BOLD}{region}{RESET} {format_size(old_val)}->{format_size(new_val)} "
                    f"{delta_color}({sign}{format_size(delta)}){RESET} [{pct:.0f}%]"
                )

        print(f"  {BOLD_WHITE}{target:<{max_name}}{RESET}  {'  |  '.join(parts)}")


def compare_full(target_name, old_elf, new_elf, show_symbols=False):
    """Full comparison of a single ELF pair."""
    print(f"\n{'=' * 120}")
    print(f"  {target_name}")
    print(f"  Old: {old_elf}")
    print(f"  New: {new_elf}")
    print(f"{'=' * 120}")

    old_sections = parse_sections(old_elf)
    new_sections = parse_sections(new_elf)

    old_output = run(["arm-none-eabi-size", str(old_elf)])
    new_output = run(["arm-none-eabi-size", str(new_elf)])
    print("\n  arm-none-eabi-size:")
    print(f"    Old: {old_output.strip().split(chr(10))[-1].strip()}")
    print(f"    New: {new_output.strip().split(chr(10))[-1].strip()}")

    is_rp2350 = detect_rp2350(old_sections) or detect_rp2350(new_sections)
    print_section_comparison(old_sections, new_sections, is_rp2350)

    old_usage = compute_region_usage(old_sections, is_rp2350)
    new_usage = compute_region_usage(new_sections, is_rp2350)
    print_region_summary(old_usage, new_usage, old_sections, new_sections)

    if show_symbols:
        print("\n--- Symbol Region Changes ---")
        print_symbol_moves(old_elf, new_elf, is_rp2350)


# ── Baseline save/check mode ─────────────────────────────────────────


def snapshot_elf(elf_path):
    """Create a serializable snapshot of an ELF's layout."""
    sections = parse_sections(elf_path)
    is_rp2350 = detect_rp2350(sections)
    usage = compute_region_usage(sections, is_rp2350)
    sym_regions = get_symbol_regions(elf_path, is_rp2350)
    return {
        "regions": usage,
        "symbol_regions": sym_regions,
        "is_rp2350": is_rp2350,
    }


def cmd_save_baseline(args):
    """Save baseline snapshots to a JSON file."""
    check_tools()

    path = args[0] if args else "."
    out_file = "baseline.json"
    if "-o" in args:
        idx = args.index("-o")
        if idx + 1 < len(args):
            out_file = args[idx + 1]

    elfs = find_elfs(path)
    if not elfs:
        print(f"No ELF files found in {path}", file=sys.stderr)
        sys.exit(1)

    baseline = {}
    for target, elf_path in sorted(elfs.items()):
        print(f"  Snapshotting {target} ({elf_path.name})")
        baseline[target] = snapshot_elf(elf_path)

    with open(out_file, "w") as f:
        json.dump(baseline, f, indent=2)
    print(f"\nBaseline saved to {out_file} ({len(baseline)} targets)")


def cmd_check(args):
    """Check current build against a saved baseline. Prints only changes."""
    check_tools()

    baseline_file = None
    path = "."

    remaining = []
    i = 0
    while i < len(args):
        if args[i] == "--baseline" and i + 1 < len(args):
            baseline_file = args[i + 1]
            i += 2
        else:
            remaining.append(args[i])
            i += 1

    if remaining:
        path = remaining[0]

    if not baseline_file:
        print("Error: --baseline <file> required", file=sys.stderr)
        sys.exit(1)

    with open(baseline_file) as f:
        baseline = json.load(f)

    elfs = find_elfs(path)
    if not elfs:
        print(f"No ELF files found in {path}", file=sys.stderr)
        sys.exit(1)

    any_changes = False

    for target in sorted(set(baseline) | set(elfs)):
        if target not in baseline:
            continue
        if target not in elfs:
            continue

        old_data = baseline[target]
        old_regions = old_data["regions"]
        old_sym_regions = old_data.get("symbol_regions", {})
        is_rp2350 = old_data.get("is_rp2350", False)

        new_sections = parse_sections(elfs[target])
        is_rp2350 = is_rp2350 or detect_rp2350(new_sections)
        new_regions = compute_region_usage(new_sections, is_rp2350)
        new_sym_regions = get_symbol_regions(elfs[target], is_rp2350)

        # Collect changes for this target
        region_changes = []
        for region in sorted(set(list(old_regions.keys()) + list(new_regions.keys()))):
            if region == "OTHER":
                continue
            old_val = old_regions.get(region, 0)
            new_val = new_regions.get(region, 0)
            delta = new_val - old_val
            if abs(delta) >= REGION_CHANGE_THRESHOLD:
                cap = region_capacity(region, is_rp2350)
                pct = new_val / cap * 100 if cap > 0 else 0
                region_changes.append((region, old_val, new_val, delta, pct))

        # Find symbols that moved regions
        flash_to_ram = []
        ram_to_flash = []
        common_syms = set(old_sym_regions) & set(new_sym_regions)
        for name in common_syms:
            old_r = old_sym_regions[name]
            new_r = new_sym_regions[name]
            if old_r != new_r:
                if old_r == "FLASH" and new_r == "RAM":
                    flash_to_ram.append(name)
                elif old_r == "RAM" and new_r == "FLASH":
                    ram_to_flash.append(name)

        if not region_changes and not flash_to_ram and not ram_to_flash:
            continue

        # Print changes
        any_changes = True
        print(f"\n  {target}:")

        if region_changes:
            for region, old_val, new_val, delta, pct in region_changes:
                sign = "+" if delta > 0 else ""
                arrow = "^" if delta > 0 else "v"
                print(f"    {arrow} {region:<10} {format_size(old_val):>10} -> {format_size(new_val):>10}  ({sign}{format_size(delta)})  [{pct:.1f}% used]")

        if flash_to_ram:
            print(f"    ! FLASH -> RAM: {len(flash_to_ram)} symbols moved INTO ram:")
            for name in sorted(flash_to_ram)[:15]:
                print(f"        {name}")
            if len(flash_to_ram) > 15:
                print(f"        ... and {len(flash_to_ram) - 15} more")

        if ram_to_flash:
            print(f"    * RAM -> FLASH: {len(ram_to_flash)} symbols moved OUT of ram:")
            for name in sorted(ram_to_flash)[:15]:
                print(f"        {name}")
            if len(ram_to_flash) > 15:
                print(f"        ... and {len(ram_to_flash) - 15} more")

    if not any_changes:
        print("  No changes from baseline.")

    return 1 if any_changes else 0


# ── Summary mode ─────────────────────────────────────────────────────


def cmd_summary(args):
    """Show memory region usage for a single build (no baseline needed)."""
    check_tools()

    path = args[0] if args else "."
    elfs = find_elfs(path)
    if not elfs:
        print(f"No ELF files found in {path}", file=sys.stderr)
        sys.exit(1)

    max_name = max(len(t) for t in elfs)

    for target in sorted(elfs):
        sections = parse_sections(elfs[target])
        is_rp2350 = detect_rp2350(sections)
        usage = compute_region_usage(sections, is_rp2350)

        parts = []
        for region in ("FLASH", "RAM", "SCRATCH_X", "SCRATCH_Y"):
            val = usage.get(region, 0)
            if val == 0:
                continue
            cap = region_capacity(region, is_rp2350)
            pct = val / cap * 100 if cap > 0 else 0
            parts.append(f"{region} {format_size(val):>10} / {format_size(cap)} [{pct:.1f}%]")

        print(f"  {target:<{max_name}}  {'  |  '.join(parts)}")


# ── Release comparison mode ──────────────────────────────────────────


def download_release_elfs(tmpdir, repo="BlueSCSI/BlueSCSI-v2"):
    """Download ELFs from the latest GitHub release's dev zip.

    Returns the directory containing the extracted ELFs, or None on failure.
    """
    import zipfile

    # Find the latest non-prerelease tag
    try:
        output = subprocess.run(
            ["gh", "release", "list", "--limit", "10", "-R", repo],
            capture_output=True, text=True, timeout=15
        )
        if output.returncode != 0:
            print(f"  Warning: gh release list failed: {output.stderr.strip()}", file=sys.stderr)
            return None
    except FileNotFoundError:
        print("  Warning: gh CLI not found, skipping release comparison", file=sys.stderr)
        return None
    except subprocess.TimeoutExpired:
        print("  Warning: gh timed out, skipping release comparison", file=sys.stderr)
        return None

    tag = None
    for line in output.stdout.strip().split("\n"):
        parts = line.split("\t")
        if len(parts) >= 4:
            release_type = parts[1].strip()
            release_tag = parts[2].strip()
            if release_type == "Latest" or (release_type == "" and release_tag.startswith("v")):
                tag = release_tag
                break

    if not tag:
        print("  Warning: no release found, skipping comparison", file=sys.stderr)
        return None

    # Find the dev zip asset
    try:
        output = subprocess.run(
            ["gh", "release", "view", tag, "-R", repo, "--json", "assets", "--jq", ".assets[].name"],
            capture_output=True, text=True, timeout=15
        )
    except subprocess.TimeoutExpired:
        print("  Warning: gh timed out, skipping release comparison", file=sys.stderr)
        return None

    dev_zip = None
    for name in output.stdout.strip().split("\n"):
        if name.startswith("dev-") and name.endswith(".zip"):
            dev_zip = name
            break

    if not dev_zip:
        print(f"  Warning: no dev zip found in release {tag}", file=sys.stderr)
        return None

    # Download and extract
    zip_path = os.path.join(tmpdir, dev_zip)
    try:
        result = subprocess.run(
            ["gh", "release", "download", tag, "-R", repo, "--pattern", dev_zip, "--dir", tmpdir],
            capture_output=True, text=True, timeout=60
        )
        if result.returncode != 0:
            print(f"  Warning: download failed: {result.stderr.strip()}", file=sys.stderr)
            return None
    except subprocess.TimeoutExpired:
        print("  Warning: download timed out, skipping release comparison", file=sys.stderr)
        return None

    elf_dir = os.path.join(tmpdir, "elfs")
    os.makedirs(elf_dir, exist_ok=True)
    with zipfile.ZipFile(zip_path) as zf:
        for name in zf.namelist():
            if name.endswith(".elf"):
                zf.extract(name, elf_dir)

    return elf_dir, tag


def find_symbol_moves(old_elf, new_elf, is_rp2350=False):
    """Return (flash_to_ram, ram_to_flash) lists of (name, size) tuples."""
    old_syms = parse_symbols(old_elf)
    new_syms = parse_symbols(new_elf)

    flash_to_ram = []
    ram_to_flash = []
    for name in set(old_syms) & set(new_syms):
        old_addr, old_size, _ = old_syms[name]
        new_addr, new_size, _ = new_syms[name]
        old_region = classify_region(old_addr, is_rp2350)
        new_region = classify_region(new_addr, is_rp2350)
        if old_region == "FLASH" and new_region == "RAM":
            flash_to_ram.append((name, new_size))
        elif old_region == "RAM" and new_region == "FLASH":
            ram_to_flash.append((name, old_size))

    flash_to_ram.sort(key=lambda x: -x[1])
    ram_to_flash.sort(key=lambda x: -x[1])
    return flash_to_ram, ram_to_flash


def print_symbol_moves_compact(pairs, show_all=False):
    """Print symbol region moves for all target pairs in a compact format.

    Returns True if any output was truncated.
    """
    limit = None if show_all else 10
    any_moves = False
    truncated = False
    for target, old_elf, new_elf in pairs:
        old_sections = parse_sections(old_elf)
        new_sections = parse_sections(new_elf)
        is_rp2350 = detect_rp2350(old_sections) or detect_rp2350(new_sections)

        flash_to_ram, ram_to_flash = find_symbol_moves(old_elf, new_elf, is_rp2350)
        if not flash_to_ram and not ram_to_flash:
            continue

        if not any_moves:
            print(f"\n  {BOLD}Symbol region changes:{RESET}")
            any_moves = True

        print(f"    {BOLD_WHITE}{target}:{RESET}")
        if flash_to_ram:
            total = sum(s for _, s in flash_to_ram)
            print(f"      {BOLD_RED}FLASH -> RAM{RESET} ({len(flash_to_ram)} symbols, {RED}+{format_size(total)} RAM{RESET}):")
            shown = flash_to_ram if show_all else flash_to_ram[:limit]
            for name, size in shown:
                print(f"        {DIM}{name:<55}{RESET} {format_size(size):>8}")
            if not show_all and len(flash_to_ram) > limit:
                print(f"        {YELLOW}... and {len(flash_to_ram) - limit} more{RESET}")
                truncated = True
        if ram_to_flash:
            total = sum(s for _, s in ram_to_flash)
            print(f"      {BOLD_GREEN}RAM -> FLASH{RESET} ({len(ram_to_flash)} symbols, {GREEN}-{format_size(total)} RAM{RESET}):")
            shown = ram_to_flash if show_all else ram_to_flash[:limit]
            for name, size in shown:
                print(f"        {DIM}{name:<55}{RESET} {format_size(size):>8}")
            if not show_all and len(ram_to_flash) > limit:
                print(f"        {YELLOW}... and {len(ram_to_flash) - limit} more{RESET}")
                truncated = True

    if not any_moves:
        print(f"\n  {GREEN}No symbols moved between FLASH and RAM.{RESET}")

    return truncated


def generate_markdown_report(pairs, tag):
    """Generate a GitHub-flavored markdown size report for PR comments."""
    lines = []
    lines.append("## BlueSCSI Memory Report")
    lines.append(f"Compared against release `{tag}`")
    lines.append("")

    # Collect all data first to determine which regions are used
    all_regions = ("FLASH", "RAM", "SCRATCH_X", "SCRATCH_Y")
    target_data = []
    used_regions = set()

    for target, old_elf, new_elf in pairs:
        old_sections = parse_sections(old_elf)
        new_sections = parse_sections(new_elf)
        is_rp2350 = detect_rp2350(old_sections) or detect_rp2350(new_sections)
        old_usage = compute_region_usage(old_sections, is_rp2350)
        new_usage = compute_region_usage(new_sections, is_rp2350)
        for r in all_regions:
            if new_usage.get(r, 0) > 0:
                used_regions.add(r)
        target_data.append((target, old_usage, new_usage, is_rp2350))

    # Build header dynamically based on which regions are used
    regions = [r for r in all_regions if r in used_regions]

    lines.append("### Memory Usage")
    lines.append("")
    header = "| Target |"
    align = "|:-------|"
    for r in regions:
        header += f" {r} |"
        align += " ------:|"
    lines.append(header)
    lines.append(align)

    for target, old_usage, new_usage, is_rp2350 in target_data:
        row = f"| {target} |"
        for region in regions:
            old_val = old_usage.get(region, 0)
            new_val = new_usage.get(region, 0)
            cap = region_capacity(region, is_rp2350)
            pct = new_val / cap * 100 if cap > 0 else 0
            delta = new_val - old_val

            if new_val == 0:
                cell = "\u2014"
            elif abs(delta) < REGION_CHANGE_THRESHOLD:
                cell = f"{format_size(new_val)} [{pct:.0f}%]"
            else:
                sign = "+" if delta > 0 else ""
                cell = f"{format_size(new_val)} ({sign}{format_size(delta)}) [{pct:.0f}%]"

            row += f" {cell} |"
        lines.append(row)

    # Symbol moves
    any_moves = False
    move_lines = []
    for target, old_elf, new_elf in pairs:
        old_sections = parse_sections(old_elf)
        new_sections = parse_sections(new_elf)
        is_rp2350 = detect_rp2350(old_sections) or detect_rp2350(new_sections)
        flash_to_ram, ram_to_flash = find_symbol_moves(old_elf, new_elf, is_rp2350)
        if not flash_to_ram and not ram_to_flash:
            continue
        any_moves = True

        if flash_to_ram:
            total = sum(s for _, s in flash_to_ram)
            move_lines.append(f"<details><summary>{target}: {len(flash_to_ram)} symbols moved FLASH \u2192 RAM (+{format_size(total)})</summary>")
            move_lines.append("")
            move_lines.append("| Symbol | Size |")
            move_lines.append("|:-------|-----:|")
            for name, size in flash_to_ram[:30]:
                move_lines.append(f"| `{name}` | {format_size(size)} |")
            if len(flash_to_ram) > 30:
                move_lines.append(f"| *... and {len(flash_to_ram) - 30} more* | |")
            move_lines.append("")
            move_lines.append("</details>")
            move_lines.append("")

        if ram_to_flash:
            total = sum(s for _, s in ram_to_flash)
            move_lines.append(f"<details><summary>{target}: {len(ram_to_flash)} symbols moved RAM \u2192 FLASH (-{format_size(total)} RAM)</summary>")
            move_lines.append("")
            move_lines.append("| Symbol | Size |")
            move_lines.append("|:-------|-----:|")
            for name, size in ram_to_flash[:30]:
                move_lines.append(f"| `{name}` | {format_size(size)} |")
            if len(ram_to_flash) > 30:
                move_lines.append(f"| *... and {len(ram_to_flash) - 30} more* | |")
            move_lines.append("")
            move_lines.append("</details>")
            move_lines.append("")

    if any_moves:
        lines.append("")
        lines.append("### Symbol Region Changes")
        lines.append("")
        lines.extend(move_lines)
    else:
        lines.append("")
        lines.append("*No symbols moved between FLASH and RAM.*")

    return "\n".join(lines) + "\n"


def cmd_from_release(args):
    """Compare current build against the latest GitHub release."""
    import tempfile

    check_tools()

    positional = [a for a in args if not a.startswith("-")]
    path = positional[0] if positional else "."
    show_all = "--symbols" in args
    markdown = "--markdown" in args
    out_file = None
    if "-o" in args:
        idx = args.index("-o")
        if idx + 1 < len(args):
            out_file = args[idx + 1]

    new_elfs = find_elfs(path)
    if not new_elfs:
        print(f"No ELF files found in {path}", file=sys.stderr)
        sys.exit(1)

    with tempfile.TemporaryDirectory(prefix="bluescsi-release-") as tmpdir:
        result = download_release_elfs(tmpdir)
        if not result:
            # Fall back to summary-only mode
            if markdown:
                report = "## BlueSCSI Memory Report\n\n*Could not download release for comparison.*\n"
                if out_file:
                    with open(out_file, "w") as f:
                        f.write(report)
                else:
                    print(report)
                return
            print("\nMemory usage:")
            cmd_summary([path])
            return

        elf_dir, tag = result

        old_elfs = find_elfs(elf_dir)
        if not old_elfs:
            print("  Warning: no ELFs found in release zip, showing summary only", file=sys.stderr)
            if not markdown:
                print("\nMemory usage:")
                cmd_summary([path])
            return

        pairs = []
        for target in sorted(set(old_elfs) | set(new_elfs)):
            if target in old_elfs and target in new_elfs:
                pairs.append((target, old_elfs[target], new_elfs[target]))

        if not pairs:
            print("  Warning: no matching targets found, showing summary only", file=sys.stderr)
            if not markdown:
                print("\nMemory usage:")
                cmd_summary([path])
            return

        if markdown:
            report = generate_markdown_report(pairs, tag)
            if out_file:
                with open(out_file, "w") as f:
                    f.write(report)
                print(f"  Markdown report written to {out_file}")
            else:
                print(report)
            return

        print(f"  {BOLD_CYAN}Memory usage vs release {tag}:{RESET}")
        compare_brief(pairs)
        truncated = print_symbol_moves_compact(pairs, show_all=show_all)

        if truncated:
            print(f"\n  {YELLOW}Full symbol analysis: utils/compare_elf.py --from-release {path} --symbols{RESET}")

    print()


# ── Main ──────────────────────────────────────────────────────────────


def main():
    args = sys.argv[1:]

    if not args:
        print(__doc__)
        sys.exit(1)

    # --summary mode
    if args[0] == "--summary":
        cmd_summary(args[1:])
        return

    # --from-release mode
    if args[0] == "--from-release":
        cmd_from_release(args[1:])
        return

    # --save-baseline mode
    if args[0] == "--save-baseline":
        cmd_save_baseline(args[1:])
        return

    # --check mode
    if args[0] == "--check":
        rc = cmd_check(args[1:])
        sys.exit(rc)

    # Full/brief comparison mode
    flags = {a for a in args if a.startswith("--")}
    positional = [a for a in args if not a.startswith("--")]

    if len(positional) < 2:
        print(__doc__)
        sys.exit(1)

    check_tools()

    old_path = positional[0]
    new_path = positional[1]

    pairs = find_elf_pairs(old_path, new_path)
    if not pairs:
        print("No matching ELF pairs found.", file=sys.stderr)
        sys.exit(1)

    if "--brief" in flags:
        compare_brief(pairs)
    else:
        for target, old_elf, new_elf in pairs:
            compare_full(target, old_elf, new_elf, "--symbols" in flags)

    print()


if __name__ == "__main__":
    main()
