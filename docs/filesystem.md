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

The SDK headers map those source-level names to a TabOS-owned, prefixed ABI. This
avoids exposing host, ESP-IDF, or C-library file descriptors and structures as the
stable application ABI. `<tabos/filesystem.h>` provides the explicit namespaced
API for system code that does not want the compatibility headers.

## Supported Operations

The initial subset supports:

- opening, closing, reading, writing, and seeking regular files
- file and open-file metadata
- creating, removing, and renaming files and directories
- current-working-directory operations
- opening and iterating directories
- fixed TabOS error numbers through `errno`

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

Disk format is FAT filesystem supported by Tab5 BSP. `A:` is unavailable until internal
flash filesystem is implemented, then appears separately in boot diagnostics. Live
card-removal handling is not implemented. Tab5 FAT enables
heap-backed long filename support through 255 characters, matching
`TABOS_FS_NAME_MAX`; the default ESP-IDF 8.3-only mode is not used.

Tab5 reserves an 8 KiB startup-task stack for current built-in applications. This
accommodates nested filesystem and FatFs VFS calls until applications receive
their own runtime tasks and stack budgets.

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
