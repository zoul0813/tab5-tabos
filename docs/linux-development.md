# Linux development

Ubuntu 24.04 LTS and Ubuntu 26.10 are supported development environments. Other
Linux distributions need equivalent packages and UDisks2/Polkit services; their
package installation is manual. Hardware acceptance below was performed on Ubuntu
26.10; Ubuntu 24.04 compatibility is covered by portable tooling and regression
checks, not a physical run on that distribution.

## Prerequisites

Install build, ESP-IDF bootstrap, serial ownership, and removable-storage tools:

```sh
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build git pkg-config libssl-dev \
    python3 python3-venv python3-pip libffi-dev libusb-1.0-0 \
    lsof util-linux udisks2 polkitd
```

SDL3 development headers and its CMake package are required for the host simulator.
SDL2 does not substitute for SDL3. On Ubuntu 26.10:

```sh
sudo apt-get install libsdl3-dev
```

Ubuntu 24.04's standard repositories do not provide `libsdl3-dev`. Install SDL3
from source before running `./tools/tabos setup`. Follow the upstream
[Linux dependency instructions](https://wiki.libsdl.org/SDL3/README-linux) to enable
X11/Wayland and audio, then build a stable SDL3 release, for example:

```sh
git clone --branch release-3.2.10 --depth 1 https://github.com/libsdl-org/SDL.git /tmp/tabos-sdl3
cmake -S /tmp/tabos-sdl3 -B /tmp/tabos-sdl3/build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build /tmp/tabos-sdl3/build
sudo cmake --install /tmp/tabos-sdl3/build
sudo ldconfig
pkg-config --modversion sdl3
```

See upstream [CMake build instructions](https://wiki.libsdl.org/SDL3/README-cmake).
For a custom prefix, export `CMAKE_PREFIX_PATH`, `PKG_CONFIG_PATH`, and the runtime
library search path as appropriate. Setup detects an existing SDL3 installation;
if no apt candidate exists, it gives source-install guidance instead of requesting
an unavailable package. Refresh apt indexes if a normally available candidate is missing.

Run `./tools/tabos setup` to install pinned ESP-IDF v5.4.4 locally. Application
builds use Bash explicitly, including when launched from a fresh dash shell. Manual
`eval "$(./tools/tabos activate-idf)"` activation also requires Bash.

```sh
./apps/build.sh
./tools/tabos linux debug test
./tools/tabos linux release test
```

## Serial access

USB-C supplies the programming/control serial device, normally `/dev/ttyACM0`.
Give your login serial access, then log out and back in:

```sh
sudo usermod -aG dialout "$USER"
```

Check `id`, `ls -l /dev/ttyACM0`, and `lsof /dev/ttyACM0`. Stop monitors and other
port owners before uploading. `lsof` is mandatory: unavailable or failed ownership
checks abort. If ModemManager or another service owns the port, release that device
before proceeding. Do not run the entire build or uploader with sudo.

Upload sends one MSC request. Write timeouts, disconnects, permission failures,
short writes, and missing replies stop with an error. The request may have reached
the device; inspect its state before retrying. There are no automatic serial retries.

## SD-card uploads and eject

Keep both cables connected: USB-C for control, USB-A for the MSC data connection.
The card must have filesystem label `TAB5`. Mount it through the desktop file manager
if automount is disabled. Linux mount paths are discovered from `lsblk` metadata,
including USB parent-disk transport, rather than assuming `/Volumes` or `/media`.

```sh
./tools/tabos tab5 upload --port /dev/ttyACM0 \
    --file build/apps/tester/tester bin/tester
# When already in MSC mode with the card mounted:
./apps/build.sh --msc
```

Use `--mount /run/media/$USER/TAB5` for upload, or
`--msc-mount=/run/media/$USER/TAB5` for the app builder. Both also honor
`TABOS_MSC_MOUNT`. Overrides must identify a real mounted, writable USB volume
labeled `TAB5`; an ordinary directory is rejected. Multiple candidates require an
explicit override. Disks with other mounted filesystems are rejected because eject
acts on the whole device.

Both commands preflight every destination and required tool before copying, retain
replaced files under `.local/msc-backups`, stage and fsync each replacement, and
verify SHA-256 before and after replacement. Existing unfinished staging files stop
the operation. A multi-file upload is not a transaction: earlier verified files may
remain installed if a later copy fails. Inspect failures and safely eject manually.

Use an active local desktop login with UDisks2/Polkit permission to unmount and
power off removable USB drives. The uploader checks the power-off grant before
copying and never opens an interactive authorization prompt. SSH, containers, or
custom Polkit policies may deny access; an administrator must configure permissions
for that session. Writable card files alone do not prove eject permission. Unmount
or power-off errors fail the command; the uploader reports success only after both
complete. macOS uses synchronous `diskutil eject`.

Linux uses `udisksctl unmount` followed by `udisksctl power-off`, which detaches the
USB device. Unmount alone can leave Tab5 in MSC mode. Physical Ubuntu 26.10 testing
confirmed verified copying, safe eject, the firmware's storage-session-ended message,
software reset, normal microSD remount, persistent uploaded bytes, and sole-shell
recovery. A diagnostic serial-open reset required operator FAT dirty-state repair
before the final clean repeat; the validation record includes that incident.
Existing firmware was retained. See
[validation evidence](validation/linux-tooling-2026-10-28.md).
