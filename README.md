# TabOS

TabOS is a Zeal OS-inspired, keyboard-first operating environment for the M5Stack Tab5 and Tab5 Keyboard. It runs on ESP-IDF and FreeRTOS while presenting applications with a small TabOS-owned API.

This repository currently contains a buildable scaffold. Public application APIs, executable format, filesystem policy, and hardware services are not implemented or frozen.

Project identity, version, window title, preference names, target display names, and log tags are centralized in `config/Identity.cmake`. CMake generates a private C header from these values for every platform build.

Project documentation starts in [`docs/`](docs/README.md). See the [directory structure guide](docs/directory-structure.md) for repository layout and ownership boundaries.

## Targets

- `tab5`: ESP32-P4 firmware built with ESP-IDF v5.4.4.
- `macos`: Native macOS development target using SDL3.
- `linux`: Native Linux development target using SDL3.

macOS and Linux build the same portable core and shared host backend. Native host builds are development tools, not complete hardware emulators.

## Prerequisites

All host builds require:

- Python 3
- CMake 3.22 or newer
- Ninja
- C17 compiler
- SDL3 installed as a system package

Install SDL3 on macOS:

```sh
brew install sdl3
```

On Ubuntu 26.04 or newer:

```sh
sudo apt-get install build-essential cmake ninja-build libsdl3-dev python3
```

Tab5 builds require ESP-IDF v5.4.4 with its environment activated. The build rejects other ESP-IDF versions until they have been validated deliberately.

## Build

Use the project wrapper from the repository root:

```sh
./tools/tabos build macos debug
./tools/tabos build macos release
./tools/tabos build linux debug
./tools/tabos build linux release
./tools/tabos build tab5 debug
./tools/tabos build tab5 release
```

A host target must be built on its matching operating system. Build output goes under `build/<target>-<configuration>/`.

## Test

Run host tests with:

```sh
./tools/tabos test macos debug
./tools/tabos test linux debug
```

Tests currently cover portable runtime bootstrap, diagnostic framebuffer rendering, Tab5 display rotation, headless SDL host integration, target-selection rejection, and platform-header boundaries.

## Run and Flash

Run host display:

```sh
./tools/tabos run macos debug
./tools/tabos run linux debug
```

Host executable opens a resizable window containing the shared 1280x720 RGB565 diagnostic frame.
It remembers its last window position in the SDL per-user preferences directory. If that position is no longer visible on a connected display, normal system placement is used instead.

Tab5 firmware initializes the built-in display through the official M5Stack BSP and presents the same diagnostic frame. See the [display system guide](docs/display.md) for the current contract and hardware-validation procedure.

Flash Tab5 only through explicit command:

```sh
./tools/tabos flash tab5 debug
```

When firmware was built using the ESP-IDF container, flash the generated image using a local `esptool` installation:

```sh
./tools/flash.sh
./tools/flash.sh release
```

The script defaults to `debug` and automatically selects the serial port when exactly one supported device is connected. Set `ESPPORT=/dev/cu.usbmodem...` when automatic selection is ambiguous.

Ordinary Tab5 builds never flash hardware.

## Download CI Artifacts

Each successful GitHub Actions run publishes release packages from its release jobs:

- `tabos-macos-arm64.tar.gz`: macOS Apple Silicon host and bundled SDL3 library. Run `./run.sh` after extracting it. The launcher removes quarantine attributes from the extracted package before starting TabOS.
- `tabos-linux-x64.tar.gz`: Linux x86-64 host and bundled SDL3 library. Run `./run.sh` after extracting it. The launcher configures the bundled SDL3 library path before starting TabOS.
- `tabos-tab5-release.tar.gz`: Tab5 application, bootloader, partition table, and ESP-IDF flash metadata.

Open the latest successful `build` workflow run on GitHub to download its artifacts. These are development snapshots, not signed GitHub Releases. macOS may require explicit approval before opening the ad-hoc-signed host binary downloaded from the internet.

## Architecture Boundaries

- Portable TabOS code lives in subsystem directories such as `kernel/`, `fs/`, `shell/`, and `graphics/`.
- SDL3 stays inside `platform/host/`.
- ESP-IDF and FreeRTOS stay inside `platform/esp32p4/` and `targets/tab5/`.
- Application-facing headers belong in `sdk/include/tabos/`.
- Current bootstrap interfaces are internal and unstable.
- Empty subsystem directories are placeholders for future work, not settled APIs.

Keep user and contributor documentation in `docs/` current as the project evolves. The `agents/` directory contains coding-agent context and is not user documentation.
