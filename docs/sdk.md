# TabOS Application SDK

TabOS applications are independently compiled RV32 C17 executables. The current
pre-release ABI version value is 1. Applications
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

## Device Types

`<tabos/device.h>` defines the pre-release portable device ID, class, state, feature,
and copied-information types used by the hardware-service registry. Device IDs are opaque;
applications must not decode or persist them across reboot.

Current built-in logical names are `display0`, `keyboard0`, `storage0`, `rtc0`,
`battery0`, and `wifi0`. TabOS registers only devices detected during startup. Host builds
publish deterministic virtual equivalents. Feature flags describe framebuffer, input,
removable storage, wall-clock, battery telemetry/control, and Wi-Fi support; flags are
informational and do not restrict application access.

`tabos_device_count()`, `tabos_device_at()`, `tabos_device_get()`, and
`tabos_device_find()` enumerate present devices and return copied metadata. Indexed lookup
is a snapshot operation: applications must tolerate devices changing between count and
lookup calls.

`tabos_device_subscribe()` creates a process-owned lifecycle subscription.
`tabos_device_event_read()` nonblockingly returns added, removed, ready, offline, and fault
events; it fails with `EAGAIN` when no event is queued. Each subscription retains 32 events.
On overflow, TabOS drops the oldest event and sets `TABOS_DEVICE_EVENT_OVERFLOW` on the next
event read. Close subscriptions with `tabos_device_subscription_close()`; TabOS also closes
them automatically when the owning process exits.

Call `tabos_device_subscription_wait_source()` to obtain a generic wait source for a
subscription. It reports readable or state-changed while an event is queued and can be
combined with socket sources in one `tabos_wait()` call. Closing the subscription invalidates
its source; later waits fail with `EBADF`.

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
`./apps/build.sh` to build default applications. Optional applications such as DOOM
require their documented opt-in flag.

TabOS has not released or frozen its application ABI. SDK and transport changes may be
incompatible during development, and all bundled applications must be rebuilt with the
matching system build.
