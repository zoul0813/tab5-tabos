# TabOS Application SDK

TabOS ABI v1 applications are independently compiled RV32 C17 executables. Applications
include public headers under `sdk/include/` and do not access the private ELF API table,
ESP-IDF, FreeRTOS, or SDL.

## Supported Runtime

The SDK uses newlib. Standard `main(argc, argv)`, stdio, `malloc`, `calloc`, `realloc`,
`free`, and process exit are supported. File APIs include `open`, `close`, `read`, `write`,
`lseek`, `stat`, `fstat`, `mkdir`, `rmdir`, `unlink`, `rename`, `chdir`, `getcwd`, directory
iteration, and nonblocking flags. TabOS does not claim full POSIX compatibility.

## Time and System Information

`<tabos/runtime_time.h>` provides monotonic milliseconds and cooperative sleep.
`<tabos/clock.h>` provides UTC RTC calendar and Unix-epoch access; normal libc
`time()`, `gettimeofday()`, and realtime/monotonic `clock_gettime()` are supported.
`<tabos/system.h>` provides portable target, device, display, processor, and memory data.

## Reboot and Power-Off

`<sys/reboot.h>` provides `reboot(RB_AUTOBOOT)` and `reboot(RB_POWER_OFF)`. Successful
requests do not return. TabOS flushes application stdio, stops processes, closes and
unmounts storage, and shuts down services before applying the platform action. See
`docs/power.md`.

## Fullscreen Graphics

`<tabos/graphics.h>` provides foreground fullscreen RGB565 clear, clipped rectangle,
bitmap blit, and explicit presentation operations. See `docs/graphics-api.md`.

## Building

Each application includes `sdk/make/application.mk`. `make` produces a stripped,
extensionless executable and installs it under `.local/rootfs/T/bin/`. Run
`./apps/build.sh` to build all applications.

ABI v1 preserves this public SDK contract. Compatible runtime calls may be appended;
incompatible changes require a new application ABI version.
