# Building BlueSCSI Firmware

## Option 1: Nix Development Shell (Recommended)

The Nix flake provides the exact toolchain, Pico SDK, and dependencies used for official builds.

### Prerequisites

- [Nix](https://nixos.org/download/) with [flakes enabled](https://nixos.wiki/wiki/Flakes)

### Enter the Shell

```bash
nix develop
```

This sets `PICO_SDK_PATH`, `PICO_EXTRAS_PATH`, and provides GCC ARM 14, CMake, and picotool automatically.

If you use [direnv](https://direnv.net/), the environment loads automatically when you `cd` into the project (run `direnv allow` once).

### Build

```bash
./build.sh              # all default targets
./build.sh Ultra        # specific target
./build.sh clean        # remove all build artifacts
./build.sh clean Ultra  # clean specific target
```

## Option 2: Manual CMake Setup

### Prerequisites

- **GCC ARM Embedded 14.x** — [arm-gnu-toolchain-14.x](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) (`arm-none-eabi-gcc` must be on your PATH)
- **CMake 3.20+**
- **Python 3**
- **Git**

### Clone the Pico SDK

BlueSCSI uses a forked Pico SDK with Ultra hardware support. Clone it with submodules:

```bash
git clone --recurse-submodules https://github.com/bluescsi/pico-sdk-internal.git -b v2.2.0-UltraSupport-rel1
git clone --recurse-submodules https://github.com/raspberrypi/pico-extras.git -b sdk-2.2.0
```

Set the environment variables:

```bash
export PICO_SDK_PATH=/path/to/pico-sdk-internal
export PICO_EXTRAS_PATH=/path/to/pico-extras
```

### Build with build.sh

With the environment variables set, `build.sh` works the same as the Nix setup:

```bash
./build.sh              # all default targets
./build.sh Ultra        # specific target
```

### Build a Single Target Manually

Each target needs its own build directory. Configure once, then build:

```bash
mkdir -p build/Ultra && cd build/Ultra
cmake ../.. -DPICO_BOARD=bluescsi_ultra -DBLUESCSI_TARGET=Ultra -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

After the initial configure, subsequent builds only need:

```bash
cmake --build . --parallel
```

Reconfigure (required after changing CMakeLists.txt or linker script templates):

```bash
cmake ../..    # re-run from build dir
cmake --build . --parallel
```

## Available Targets

| BLUESCSI_TARGET | PICO_BOARD | Description |
|---|---|---|
| `Pico` | `pico` | RP2040, no WiFi |
| `Pico_DaynaPORT` | `pico_w` | RP2040 + WiFi/DaynaPORT |
| `Pico_Audio_SPDIF` | `pico_w` | RP2040 + WiFi + SPDIF audio |
| `Pico_2` | `pico2` | RP2350, no WiFi |
| `Pico_2_DaynaPORT` | `pico2_w` | RP2350 + WiFi/DaynaPORT |
| `Pico_2_Audio_SPDIF` | `pico2_w` | RP2350 + WiFi + SPDIF audio |
| `Ultra` | `bluescsi_ultra` | RP2350B + I2S audio + WiFi |
| `Ultra_Wide` | `bluescsi_ultra_wide` | RP2350B + I2S audio + Wide SCSI |

Default targets (built by `./build.sh`): all except `Pico` and `Pico_2`.

> **Note:** `build.sh` maps target names to the correct `PICO_BOARD` automatically. When building manually with cmake, use the PICO_BOARD values from the table above.

## Build Artifacts

| File | Description |
|---|---|
| `BlueSCSI_<target>.uf2` | Flashable firmware (drag to RP2040/RP2350 USB bootloader) |
| `BlueSCSI_<target>.elf` | Firmware with debug symbols |
| `BlueSCSI_bootloader.elf` | Bootloader (embedded automatically into main firmware) |
| `BlueSCSI_v<version>_<hash>.zip` | Universal firmware zip for SD card update |

## Firmware Update Methods

### Method 1: SD Card Update (Recommended)

1. Build all targets with `./build.sh` — this produces `build/output/BlueSCSI_v*.zip`
2. Copy the zip file to the root of the BlueSCSI SD card
3. Power cycle the BlueSCSI — the bootloader extracts the correct .bin for your hardware and flashes it automatically
4. The zip file is deleted from the SD card after a successful update

The zip contains firmware for all targets. The bootloader automatically selects the correct one.

### Method 2: UF2 via USB Bootloader

1. Hold the BOOTSEL button and plug in USB (or double-tap reset)
2. Drag the appropriate `BlueSCSI_<target>.uf2` file to the USB drive that appears
3. The device reboots with new firmware

Note: On Windows, the USB drive may eject early and show an error — this is a known RP2040/RP2350 USB limitation and the update still succeeds.

## Debugging

```bash
# GDB (requires CMSIS-DAP debugger)
utils/run_gdb_rp2040.sh

# Analyze crash logs
utils/analyze_crashlog.sh log.txt
```

Use `hbreak` in GDB for hardware breakpoints — RAM breakpoints can be unreliable on RP2040 since the boot routine overwrites them.
