# Filesystem and Storage

TabOS exposes a small POSIX-style filesystem surface so portable C programs can
use familiar calls such as `open`, `read`, `write`, `lseek`, `stat`, `opendir`,
and `readdir`. Include the normal POSIX headers when building an application with
the TabOS SDK:

```c
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
```

Independently loaded applications use the toolchain's normal newlib headers. SDK syscall
stubs map those functions to a TabOS-owned versioned ABI, so host and ESP-IDF descriptors
never become application ABI. Kernel and built-in diagnostic code can use
`<tabos/filesystem.h>` directly; `sdk/posix/include/` provides source compatibility for
native TabOS components that do not link the loaded-application C runtime.

## Supported Operations

The initial subset supports:

- opening, closing, reading, writing, and seeking regular files
- file and open-file metadata
- creating, removing, and renaming files and directories
- current-working-directory operations
- opening and iterating directories
- fixed TabOS error numbers through `errno`
- blocking console stdin plus `fcntl(..., O_NONBLOCK)` and `EAGAIN`

Files and standard streams carry bytes without newline translation. TabOS system text is
single-byte CP437; ASCII bytes retain their usual values and bytes 0x80-0xFF select CP437
glyphs. Files may contain arbitrary binary data.

Storage uses drive letters. Canonical absolute paths use `A:/path` form. `/path` is
absolute on current drive and `path` is relative to current working directory. A bare
`A:` selects that drive's root. Drive letters are case-insensitive and normalized to
uppercase. DOS-style `A:relative` paths are rejected. Repeated separators and `.` are
collapsed; `..` cannot traverse above drive root. Cross-drive rename returns `EXDEV`.

Use `tabos_fs_drive_count()` and `tabos_fs_drive_info()` to enumerate available drives,
their letters, names, removable status, capacity, and free space.
Boot diagnostics list every available drive on its own line with the same letter,
filesystem/backend name, and capacity. Unavailable drives are omitted.

## Host Storage

macOS and Linux expose `A:` and `T:` beneath controlled directory selected by
`TABOS_HOST_ROOTFS`. Defaults are `.local/rootfs/A/` and `.local/rootfs/T/`; change
container directory with:

```sh
./tools/tabos config
```

The adapter rejects symbolic-link traversal so a TabOS path cannot escape the
configured root. This storage is persistent between host runs. Automated tests
instead create isolated temporary roots.

## Tab5 Storage

Tab5 exposes BSP-mounted TF/microSD card as `T:`. `T:/` maps internally to `/sdcard`.
A missing or unmountable card is nonfatal: filesystem calls report `ENODEV`. When a
card mounts, boot diagnostics list `T:/`, its FAT filesystem, capacity, and free space.
The shared `TABOS_FILESYSTEM_MAX_FILES` project setting controls both the TabOS
filesystem table and the Tab5 FAT mount limit, so host and hardware builds expose the
same system-wide open-file capacity. It defaults to 32 and can be changed through
`./tools/tabos config`.
The corresponding `TABOS_FILESYSTEM_MAX_DIRECTORIES` setting controls the
system-wide open-directory table on every target and defaults to 8. Each loaded
application also has a separate eight-entry `DIR*` wrapper pool; this per-process SDK
limit is independent of the shared kernel capacity.

Disk format is FAT filesystem supported by Tab5 BSP. `A:` is unavailable until internal
flash filesystem is implemented, then appears separately in boot diagnostics. Live
card-removal handling is not implemented. Tab5 FAT enables
heap-backed long filename support through 255 characters, matching
`TABOS_FS_NAME_MAX`; the default ESP-IDF 8.3-only mode is not used.

Each native loaded application runs with a 16 KiB task stack. Its default heap limit is
1 MiB, allocated and advanced on demand rather than inferred from ELF static segments.

### Boot USB Storage Mode

Hold the built-in keyboard Delete key while starting or resetting the Tab5. TabOS
checks for the key during a 750 ms early-boot window, before mounting `T:`, and
enters USB mass-storage mode. Connect the **Tab5 USB-A port** to a USB-C port on
the host with a USB-A-to-USB-C data cable. The TF/microSD card appears as a
removable USB disk, and the Tab5 screen reports that USB storage mode is active.
The Tab5 USB-C power/programming port does not carry the mass-storage device.
TabOS disables USB-A host power in this mode so the connected host safely
supplies VBUS.

USB-A host power is also disabled during every normal TabOS startup. It is only
enabled when a USB-host service deliberately takes ownership of the port. This
makes a restart after eject safe while both the USB-C power/programming cable
and USB-A data cable are still attached.

TabOS does not mount or access `T:` while the host owns it. Eject the removable
disk through the host operating system before disconnecting it. A safe eject or
USB disconnect ends storage mode and restarts the Tab5; normal boot then mounts
the card as `T:` again. Internal flash is not exported.

## Platform Diagnostic

Select `filesystem-test` as startup application with:

```sh
./tools/tabos config
```

Then run host target or build and flash Tab5:

```sh
./tools/tabos macos run
./tools/tabos tab5 build
./tools/tabos tab5 flash
```

Diagnostic uses `tabos-fs-test` on current drive (`A:` host, `T:` Tab5). It creates a directory and file, writes and
verifies known data, seeks, closes and reopens, checks metadata, renames, enumerates,
and removes test contents. Each step prints `[OK]` or `[FAIL]` with TabOS error number.
Successful run ends with `Filesystem diagnostic passed`. Failure leaves remaining
test contents available for inspection; next run removes old diagnostic contents
before starting.
