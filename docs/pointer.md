# Touch and Pointer Input

TabOS exposes detected touch and host pointer devices as `touch0`. Applications use
the process-owned `<tabos/pointer.h>` API; SDL and ESP-IDF touch handles remain private.

## Events and Coordinates

Pointer streams deliver `TABOS_POINTER_DOWN`, `TABOS_POINTER_MOVE`,
`TABOS_POINTER_UP`, and `TABOS_POINTER_CANCEL`. Every event contains the boot-local
device ID, stable contact ID, logical 1280x720 coordinates, button mask, and optional
normalized pressure from 0 through `TABOS_POINTER_PRESSURE_MAX`.

Builds support five simultaneous contacts by default, matching current Tab5 hardware.
`TABOS_POINTER_MAX_CONTACTS` is configurable from 1 through 32 with
`tools/tabos config`, or as a CMake/Make setting for direct builds. Firmware and
applications must use the same value.

Tab5 converts native 720x1280 controller coordinates into the logical display
orientation. GT911, ST7123, and ST7121-family controllers use the same application
coordinate system. Host mouse input uses contact 0 while a primary button is held;
SDL touch contacts receive stable IDs for their active lifetime.

## Streams and Waiting

Find `touch0` with `tabos_device_find()`, then open its ID:

```c
tabos_device_info_t device;
tabos_device_find(TABOS_DEVICE_NAME_TOUCH, &device);
tabos_pointer_stream_t stream = tabos_pointer_open(device.id);
tabos_wait_source_t source = tabos_pointer_wait_source(stream);
```

`tabos_pointer_read()` is nonblocking and returns `EAGAIN` when its bounded queue is
empty. Pointer wait sources report readable and hangup events through `tabos_wait()`.
Close streams with `tabos_pointer_close()`; process teardown closes leaked streams and
invalidates their wait sources.

Only the foreground process consumes pointer events. Focus changes cancel active
contacts. Device removal also queues cancellation and reports hangup. If producer
traffic fills a queue, TabOS resets active contact state with cancel events instead of
leaving applications with stuck contacts.

## Diagnostic Utility

Run `touchtest` from the shell. It prints event type, contact ID, logical coordinates,
buttons, and pressure when available. Press Q or Escape to return to the shell.

Physical coordinate validation remains required on ILI9881C/GT911, ST7123, and ST7121
Tab5 revisions.
