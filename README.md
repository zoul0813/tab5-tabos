# TabOS

TabOS is a Zeal OS-inspired, keyboard-first operating environment for the M5Stack Tab5 and Tab5 Keyboard. It runs on ESP-IDF and FreeRTOS while presenting applications with a small TabOS-owned API.

This repository currently contains a buildable scaffold with terminal, foreground console, keyboard input, application lifecycle, experimental ELF loading, and an initial POSIX-style filesystem API. Most hardware services and the final executable ABI are not yet implemented or frozen.

Project identity, version, window title, preference names, target display names, and log tags are centralized in `config/Identity.cmake`. CMake generates a private C header from these values for every platform build.

Project documentation starts in [`docs/`](docs/README.md). See the [directory structure guide](docs/directory-structure.md) for repository layout and ownership boundaries.

## First Run on Tab5 Hardware

Default configuration starts `T:/bin/shell`; running the interactive configurator is
not required. For a new Tab5 checkout:

```sh
./tools/tabos setup
./tools/tabos tab5 build
./tools/tabos tab5 flash
```

After flashing, enter MSC mode: hold the Tab5 Keyboard `Del` key while resetting
the Tab5. Keep `Del` held until the `TAB5` volume mounts, then run:

```sh
./apps/build.sh --msc
```

After the MSC copy finishes and the device restarts, TabOS launches the shell from
`T:/bin/shell`.

## First Run with the macOS Host Simulator

For the macOS host simulator, no flash or MSC reset is needed:

```sh
./tools/tabos setup
./apps/build.sh
./tools/tabos macos debug run
```

The application build installs the shell and other default applications under the
host root filesystem before the simulator starts.

## Targets

- `tab5`: ESP32-P4 firmware built with ESP-IDF v5.4.4.
- `macos`: Native macOS development target using SDL3.
- `linux`: Native Linux development target using SDL3.

macOS and Linux build the same portable core and shared host backend. Native host builds are development tools, not complete hardware emulators.

## Prerequisites

Install host build dependencies and a project-local ESP-IDF toolchain with:

```sh
./tools/tabos setup
```

On macOS, setup uses Homebrew. On Debian and Ubuntu Linux, setup uses `apt-get`
and `sudo` when needed. It checks each prerequisite first, installs only missing
tools, and skips the package manager entirely when host tools are ready. Before
installing packages or downloading ESP-IDF, setup shows the commands and requires
explicit confirmation; Enter alone aborts. All host builds require:

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

Tab5 builds require ESP-IDF v5.4.4. Setup installs a project-local copy and its
tools under `.local/`:

```sh
./tools/tabos setup
```

Setup uses a depth-one clone of the v5.4.4 tag and shallow submodule clones to
avoid downloading ESP-IDF history.

Tab5 build and flash commands automatically activate this copy when a supported
ESP-IDF environment is not already active. To activate it in the current interactive
shell for direct use of ESP-IDF commands:

```sh
eval "$(./tools/tabos activate-idf)"
```

The build rejects other ESP-IDF versions until they have been validated deliberately.

## Optional Configuration

Defaults provide a working shell-first host and Tab5 system. To override TabOS-owned
host or device behavior, run:

```sh
./tools/tabos config
```

The interactive command stores project-local settings in ignored
`.local/tabos.config`. Settings include separate host and Tab5 startup applications,
bitmap font file and dimensions, terminal scale, scrollback capacity, and cursor
blink interval. Every build and flash passes these values explicitly, so they survive
`fullclean` and never depend on stale CMake cache state.

ESP-IDF's own `menuconfig` remains separate for advanced hardware configuration.
Activate the local ESP-IDF environment with `eval "$(./tools/tabos activate-idf)"`
before invoking it directly.

## Build

Use the project wrapper from the repository root:

Target commands use `./tools/tabos <target> [debug|release] <action>`. Configuration
defaults to `debug` when omitted.

```sh
./tools/tabos macos debug build
./tools/tabos macos release build
./tools/tabos linux debug build
./tools/tabos linux release build
./tools/tabos tab5 debug build
./tools/tabos tab5 release build
```

A host target must be built on its matching operating system. Build output goes under `build/<target>-<configuration>/`.

## Test

Run host tests with:

```sh
./tools/tabos macos debug test
./tools/tabos linux debug test
```

Tests currently cover portable runtime bootstrap, bitmap font and terminal rendering, colored cell history, scrollback navigation, cursor and scale reflow, console ownership and controls, keyboard translation and queue behavior, Tab5 display rotation, headless SDL host integration, target-selection rejection, and platform-header boundaries.

## Run and Flash

Run host display:

```sh
./tools/tabos macos debug run
./tools/tabos linux debug run
```

Host executable opens a resizable window containing the shared 1280x720 RGB565 boot console.
It remembers its last window position in the SDL per-user preferences directory. If that position is no longer visible on a connected display, normal system placement is used instead.
Cmd+Shift+F12 captures the logical framebuffer to a timestamped PNG under
`screenshots/`; plain F12 remains available to TabOS applications.

Tab5 firmware initializes the built-in display and presents the same structured boot report shown over serial. See the [display system guide](docs/display.md) for the current contract and hardware-validation procedure.

Flash Tab5 only through explicit command:

```sh
./tools/tabos tab5 debug flash
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

- Portable TabOS code lives in subsystem directories such as `kernel/`, `fs/`, and `graphics/`; bundled applications, including the future shell, live under `apps/`.
- SDL3 stays inside `platform/host/`.
- ESP-IDF and FreeRTOS stay inside `platform/esp32p4/` and `targets/tab5/`.
- Application-facing headers belong in `sdk/include/tabos/`.
- Current bootstrap interfaces are internal and unstable.
- Empty subsystem directories are placeholders for future work, not settled APIs.

Keep user and contributor documentation in `docs/` current as the project evolves. The `agents/` directory contains coding-agent context and is not user documentation.
