# Building BlueSCSI Firmware

## Prerequisites

Enter the Nix development shell, which provides the Pico SDK, GCC ARM toolchain, and CMake:

```bash
nix develop
```

This sets `PICO_SDK_PATH` and `PICO_EXTRAS_PATH` automatically.

If you use [direnv](https://direnv.net/), the environment loads automatically when you `cd` into the project (run `direnv allow` once).

> **Note:** Requires [Nix flakes](https://nixos.wiki/wiki/Flakes) to be enabled.

## Build All Targets

```bash
./build.sh
```

Builds all 6 default targets and copies UF2 files to `build/output/`.

Build specific targets:

```bash
./build.sh Ultra Pico_DaynaPORT
```

## Build a Single Target

Each target needs its own build directory under `build/`. Configure once, then build:

```bash
mkdir -p build/Ultra && cd build/Ultra
cmake ../.. -DPICO_BOARD=pico2_w -DBLUESCSI_TARGET=Ultra -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

The UF2 file is written to the build directory (e.g. `build/Ultra/BlueSCSI_Ultra.uf2`).

After the initial configure, subsequent builds only need:

```bash
cmake --build . --parallel
```

Reconfigure (required after changing CMakeLists.txt or linker script templates):

```bash
cmake ../..    # re-run from build dir
cmake --build . --parallel
```

## Clean

Remove all build artifacts:

```bash
./build.sh clean
```

Clean specific targets:

```bash
./build.sh clean Ultra Pico_DaynaPORT
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
| `Ultra` | `pico2_w` | RP2350B + I2S audio + WiFi |
| `Ultra_Wide` | `pico2` | RP2350B + I2S audio + Wide SCSI |

Default targets (built by `./build.sh`): all except `Pico` and `Pico_2`.

## Build Artifacts

| File | Description |
|---|---|
| `BlueSCSI_<target>.uf2` | Flashable firmware (drag to RP2040/RP2350 USB bootloader) |
| `BlueSCSI_<target>.elf` | Firmware with debug symbols |
| `BlueSCSI_bootloader.elf` | Bootloader (embedded automatically into main firmware) |

## Debugging

```bash
# GDB (requires CMSIS-DAP debugger)
utils/run_gdb_rp2040.sh

# Analyze crash logs
utils/analyze_crashlog.sh log.txt
```

Use `hbreak` in GDB for hardware breakpoints — RAM breakpoints can be unreliable on RP2040 since the boot routine overwrites them.
