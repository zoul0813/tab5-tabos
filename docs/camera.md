# Camera Capture

TabOS exposes detected cameras as `camera0`. Applications use `<tabos/camera.h>`;
native camera, DMA, ISP, and frame-buffer pointers remain private.

## Discovery and Configuration

Find `camera0` through `<tabos/device.h>`, then query formats and limits with
`tabos_camera_get_info()`. Capture configuration selects device ID, format, width,
height, and frame rate. Capture foundation currently exposes bounded RAW8. RGB565
preview, JPEG, and H.264 remain later Phase 6 work.

Host builds provide deterministic 64x48 RAW8 frames at up to 10 FPS. Pixel value is
`(x + y + frame_sequence) & 0xff`, making capture tests reproducible. Tab5 detects and
registers its SC2356 through Espressif camera stack; physical raw capture delivery remains
next Phase 6 step.

## Frames and Leases

`tabos_camera_open()` creates a process-owned stream with three kernel-owned frame slots.
`tabos_camera_acquire()` is nonblocking and returns `EAGAIN` when no frame is ready.
Returned metadata contains an opaque generation-tagged lease, format, dimensions, stride,
byte size, monotonic timestamp, sequence, and cumulative dropped-frame count.

Copy bytes from an active lease with `tabos_camera_copy()`. Copy supports offsets, allowing
applications to use bounded buffers. Release every frame with `tabos_camera_release()`.
Stale, released, and foreign leases fail with `EBADF`. Stream close and process teardown
reclaim all leaked leases and buffers.

When producer outruns consumer, TabOS replaces the oldest ready unleased frame and
increments the drop counter. Leased frames are never overwritten. If every pool slot is
leased, the incoming frame is dropped.

## Waiting and Failure

`tabos_camera_wait_source()` adapts a stream to `tabos_wait()`. Sources report readable
when a frame can be acquired, error after backend failure, and hangup after device removal.
Closing a stream invalidates its wait source.
