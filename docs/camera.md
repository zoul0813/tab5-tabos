# Camera Capture

TabOS exposes detected cameras as `camera0`. Applications use `<tabos/camera.h>`;
native camera, DMA, ISP, and frame-buffer pointers remain private.

## Discovery and Configuration

Find `camera0` through `<tabos/device.h>`, then query formats and limits with
`tabos_camera_get_info()`. Capture configuration selects device ID, format, width,
height, and frame rate. Host simulation and Tab5 expose bounded RAW8, RGB565, JPEG,
and H.264. Tab5 uses its MIPI-CSI/ISP path for RGB565 and YUV420 capture, converts
RGB565 to grayscale RAW8, and uses ESP32-P4 hardware encoders for JPEG and H.264.

Host builds provide deterministic 64x48 RAW8 frames at up to 10 FPS. Pixel value is
`(x + y + frame_sequence) & 0xff`, making capture tests reproducible. Tab5 detects and
registers its SC2356-compatible sensor through Espressif camera stack. Physical image
orientation, output validity, throughput, and responsiveness still require validation.
The Tab5 `cameratest` utility captures the sensor's native 1280x720 mode at 30 FPS.
Its application metadata reserves a 3 MiB heap so fullscreen RGB565 preview has room
for the 1.8 MiB application framebuffer and normal runtime allocations.
Tab5 enables the ISP pipeline controller and bundled SC202CS tuning data for automatic
exposure and white balance.

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

## Camera Utility

`cameratest` exercises discovery, still capture, fullscreen preview, file output, and
dropped-frame reporting:

```sh
cameratest info
cameratest capture raw T:/camera.pgm
cameratest capture rgb T:/camera.rgb565
cameratest capture jpeg T:/camera.jpg
cameratest preview [FRAMES]
cameratest record h264 T:/camera.h264 [FRAMES]
```

RAW output uses grayscale PGM. RGB output is headerless little-endian RGB565. JPEG is
JFIF, and H.264 recording is an Annex-B elementary stream. Host fixtures use a
deterministic dark-top-left to light-bottom-right gradient.
