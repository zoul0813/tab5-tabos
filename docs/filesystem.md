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

Paths use `/`, may be absolute or relative to the process-wide current directory,
collapse repeated separators and `.`, and resolve `..` without allowing traversal
above the TabOS root. The initial implementation has one mounted root filesystem.
Per-application working directories, permissions, links, and multiple mount points
are not implemented yet.

## Host Storage

macOS and Linux map `/` to the controlled directory selected by
`TABOS_HOST_ROOTFS`. The default is `.local/rootfs`; change it with:

```sh
./tools/tabos config
```

The adapter rejects symbolic-link traversal so a TabOS path cannot escape the
configured root. This storage is persistent between host runs. Automated tests
instead create isolated temporary roots.

## Tab5 Storage

Tab5 uses the BSP microSD mount at `/sdcard` as the backing store for the TabOS
root. A missing or unmountable card is a nonfatal boot condition: filesystem calls
report `ENODEV`, while boot diagnostics report storage as not mounted. When a card
mounts, the boot report includes filesystem capacity and free space.

The initial disk format is the FAT filesystem supported by the Tab5 BSP. Internal
flash storage and live card-removal handling remain future work. Tab5 FAT enables
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

Diagnostic uses only `/tabos-fs-test`. It creates a directory and file, writes and
verifies known data, seeks, closes and reopens, checks metadata, renames, enumerates,
and removes test contents. Each step prints `[OK]` or `[FAIL]` with TabOS error number.
Successful run ends with `Filesystem diagnostic passed`. Failure leaves remaining
test contents available for inspection; next run removes old diagnostic contents
before starting.
