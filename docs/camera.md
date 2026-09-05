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

For RAW8, RGB565, and JPEG, when producer outruns consumer, TabOS replaces the oldest ready unleased frame and
increments the drop counter. Leased frames are never overwritten. If every pool slot is
leased, the incoming frame is dropped.

H.264 encoding pauses when all three slots are ready or leased, resuming after a release.
Encoded pictures retain their reference order. The sensor may skip frames upstream;
these are not counted by the stream's pool-drop counter. Slow storage therefore reduces
recording throughput. An oversized frame or unexpected H.264 submission to a full pool
faults the stream instead of returning truncated data or dropping a reference picture.

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
cameratest snapshot T:/snap.rgb
cameratest record h264 T:/camera.h264 [FRAMES]
```

RAW output uses grayscale PGM. RGB output is headerless little-endian RGB565. JPEG is
JFIF, and H.264 recording is an Annex-B elementary stream. Host fixtures use a
deterministic dark-top-left to light-bottom-right gradient.

`snapshot PATH` opens live RGB565 preview. Once the subject is clearly visible,
press S to save the frame presented on that iteration and return to the shell;
Q/Escape cancels without writing. The frame remains leased until the write finishes,
so this compares displayed content with the same frame's chunked file-copy path.
Output is headerless little-endian RGB565, like `capture rgb`.

## Phase 6 Validation

Run `tester` from the shell after installing the current `build/apps/tester/tester`
on both host and Tab5. Camera coverage uses the native 1280x720 mode where supported
and the smaller host fixture dimensions otherwise. It exhausts all three leases,
checks nonblocking behavior and drops, rejects reused leases, and repeatedly closes
streams with outstanding leases before reopening.

For physical validation, capture RAW8, RGB565, JPEG, and H.264 files, decode the files
on a workstation, and verify dimensions, orientation, image contents, and frame count.
Run preview twice and check that Q/Escape restores the shell promptly. Record audio,
network, storage, display, and input progress during capture; passing each service
before or after capture alone does not prove concurrent responsiveness.

Tab5 serial logs report `capture start` with format, dimensions, configured FPS, and
free internal/PSRAM bytes after allocation. `capture stop` reports processed frame
count, elapsed microseconds, total RAW8 conversion microseconds, and total encoder
microseconds. Divide totals by processed frames for average stage time, and multiply
frames by 1,000,000 / elapsed_us for observed throughput including startup. These
backend counts precede application pool drops; record utility `dropped` output too.
Zero conversion/encoding time means that stage does not apply to the selected format.
`dequeue_misses` counts capture polls that return no buffer; it is separate from
pool drops. The pinned video driver maps its ordinary 10 ms ready-wait timeout to
`EPERM`. These polls no longer emit individual warnings; two seconds without a
successful dequeue still emits a stall warning, including for possible preprocessing
failures sharing that error code. Other dequeue errors warn immediately.
Free-memory snapshots exclude allocations made after camera open, such as the preview
application's graphics buffer; they are not peak-memory measurements.

Capture polling from runtime and application waits shares a pipeline lock with stream
start/stop. This prevents concurrent dequeue/encode operations and closing DMA buffers
while a capture update still uses them.

## Tab5 Color Tuning and File Writes

Still capture (`capture raw|rgb|jpeg`) drains startup frames before opening the
output file. It requires at least six acquired frames and a capture timestamp at
least two seconds after settling starts, allowing exposure/white balance to adjust.
Unready streams fail with a timeout rather than saving a startup frame. This is a
bounded settling policy, not a sensor-reported convergence guarantee. RAW8 PGM
output is intentionally grayscale. H.264 recording retains every encoded reference
picture and does not use this discard policy.

TabOS bounds the final color-correction matrix after white-balance gains are applied.
For out-of-range coefficients, it blends towards a diagonal matrix with the same row
sums, retaining the neutral RGB response while reducing color correction enough to
fit hardware limits. Already-valid matrices pass through unchanged. Nonfinite values
or neutral gains outside the supported baseline range retain driver error handling.
Serial logs report the first correction and first remaining rejection per boot.
This avoids rejecting the complete CCM update under observed lighting conditions;
it does not establish calibrated color accuracy.

`targets/tab5/sc202cs.json` derives from Espressif `esp_cam_sensor` 2.0.1
`sensors/sc202cs/cfg/sc202cs_default.json`, under Apache-2.0 (license in
`targets/tab5/sc202cs.LICENSE`). TabOS changes the 2292 K color-correction matrix:
`M_new = I + 0.84 * (M_original - I)`. This preserves neutral row sums while
reducing the largest coefficient from 4.5445 to 3.97738, within the ESP32-P4 ISP's
[-4, 4] range. All other tuning values remain unchanged. Physical color accuracy
still needs validation, especially under warm lighting.

Fresh builds select this file through `sdkconfig.defaults`. For an existing ESP-IDF
configuration, select the SC202CS custom IPA file and set its project-relative path
to `sc202cs.json`; verify `CONFIG_CAMERA_SC202CS_CUSTOMIZED_IPA_JSON_CONFIGURATION_FILE=y`
in `targets/tab5/sdkconfig` before rebuilding.

`cameratest` writes in 4 KiB chunks and yields every 64 KiB so long SD transfers
periodically let lower-priority work run. This is a mitigation pending physical
watchdog/responsiveness validation. Still-capture write failures also close the file
before returning. Copy the rebuilt `cameratest` to `T:/bin/cameratest` separately
from flashing firmware.
