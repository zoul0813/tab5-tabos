# Automated Tab5 testing

Local USB test control is an opt-in development firmware feature. Normal builds
leave it disabled. It extends the existing USB-C serial MSC controller, uses
normal foreground input delivery, and requires no network service or debugger UI.
Both cables remain connected: USB-C for serial/firmware, USB-A for MSC uploads.

Enable it once in the selected build directory:

```sh
eval "$(./tools/tabos activate-idf)"
idf.py -C targets/tab5 -B build/tab5-release -DTABOS_ENABLE_DEVICE_TEST_CONTROL=ON build
ESPPORT=/dev/cu.usbmodem101 ./tools/tabos tab5 release flash
```

The option persists in that CMake build directory. Set it to `OFF` and rebuild
when removing development controls. Existing firmware sdkconfig optimization and
CPU frequency settings remain separate from the CMake Release configuration.

Run an app's hardware workload through the shared runner:

```sh
python tools/device_test.py graphics_benchmark --screenshots --output /tmp/tabos-hardware-run-1
```

The positional app name selects `apps/<app>/tests/device.py`. Names cannot contain
paths; missing workloads fail before opening serial. Run
`python tools/device_test.py graphics_benchmark --help` for the selected app's options.
The output directory must be new. Stop serial monitors first and leave
the physical keyboard untouched during automated input.

The shared runner owns serial transport, shell launch, key injection, console
capture, screenshot decoding, statistics, actual/configured clock checks and
session/connection cleanup. App modules own commands, result interpretation,
visual assertions and normal app exit. A workload provides `configure(parser)`,
`validate(parser, args)` and `run(device, args, configuration)`; it must attempt
normal exit for any app it launches when its run fails. The runner always attempts
END and closes serial afterward. No forced reset is performed.

Development protocol: exact newline-delimited `TABOS TEST BEGIN`, `END`, `STATUS`,
`READ`, `STATS`, `SHOT`, `MUTE <0|1>`, `TEXT <lowercase ASCII hex>` (up to nine characters), and
`KEY <HID usage> <modifier bits> <down:0|1>`. `TEXT` requires the shell. `KEY` uses
normal physical/logical events; it can drive a foreground guest. BEGIN starts an
8192-byte bounded console capture without serial I/O in application writes;
overflow fails the test. Process status is sampled on the runtime task. Injected
held keys release on END or after two seconds without injected key activity.
Responses use the `TABOS TEST` prefix; unrelated firmware logs remain in the
saved serial log. Screenshot rows use bounded RGB565 raw or run-length records.

`STATS` requires return to the shell and reports hardware fill, blit, overlay and
submission times in microseconds, plus acceleration/fallback and frame counts.
Counters reset at BEGIN. `codec_muted` reports the requested output mute.
`MUTE` requires an active test session and the shell. It selects hardware codec
mute for subsequent audio opens; it does not change stream gain, PCM mixing,
resampling, I2S writes or underrun accounting. The selection persists across app
opens/sample-rate changes and END, but resets on firmware reboot. Test runners
must request it again after MSC uploads. `MUTE 0` restores audible output on the
next launch. Normal firmware without test control keeps its usual codec output. Actual CPU frequency comes from the ESP-IDF clock-tree
API rather than the configured value. The runner records both and rejects a
mismatch before launching a benchmark. Counters describe OS display stages, not application simulation costs. They are diagnostics, not a
replacement for end-to-end application acceptance.

A failure invokes workload cleanup and closes the serial connection. It does not
force-reset the device or overwrite ROMs/configuration. A disconnected or crashed
device can require recovery before a later run.

For firmware optimization, ESP-IDF also accepts the optional
`targets/tab5/sdkconfig.performance.defaults` fragment after the board defaults:

```sh
idf.py -C targets/tab5 -B build/tab5-release -DSDKCONFIG=/tmp/tabos-performance-sdkconfig -DSDKCONFIG_DEFAULTS='sdkconfig.defaults;sdkconfig.performance.defaults' -DTABOS_ENABLE_DEVICE_TEST_CONTROL=ON build
```

Use a new SDKCONFIG path to apply defaults without changing an existing personal
configuration. Keep that file for later builds/flashes; its path is cached in the
build directory. This selects firmware performance optimization and retains the
verified 360 MHz setting. A tested 400 MHz configuration on this Tab5/ESP-IDF 5.4.4
combination booted at an actual 90 MHz and was reverted; do not use it for timing
comparisons. The default board profile is unchanged.

The profile increases L2 cache from 128 KiB to 256 KiB, consuming an additional
128 KiB of internal SRAM. Check free, minimum-free and largest-block diagnostics
with the intended workload before adopting it; standard firmware keeps its
existing cache configuration.

Development STATS reports `cache_kib`, `internal_free`, `internal_min_free`, and
`internal_largest` in bytes (except cache in KiB). Minimum free is cumulative
since firmware boot, not reset by BEGIN. STATS still requires the shell.

Device-test firmware logs `ELF stack: bytes=N unused_min=N` when a native app
returns. ESP-IDF on Tab5 reports unused stack in bytes. This includes the app's
SDK/system-call stack use; it is an observed peak for that run, not a guarantee
for every workload. Logging occurs after the application finishes its timing
report. Stack allocation remains in PSRAM.

## General workloads

`tester` runs the finite filesystem, input and compute SDK checks and requires a zero
failure summary for each. `graphics_benchmark` runs the existing finite graphics
benchmark and saves its phase timings with OS statistics. Both write `result.json`
and console output into the new output directory. `--screenshots` additionally
saves a completed-frame PNG. Use three separate output directories for repeated
measurements; compare identical firmware settings and application inputs.
Workloads attempt normal completion after a failure; they never force reset.
Screenshots take time, so omit them from timed comparison runs. No source-level
debugger or arbitrary network control service is added.
