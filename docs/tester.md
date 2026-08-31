# SDK Tester Application

`apps/tester/` is the maintained end-to-end validation application for the public TabOS
SDK, C runtime, and application ABI. Unlike host unit tests, it is compiled as a normal
independent RV32 application and the same extensionless `tester` runs through host emulation and on
Tab5 hardware.

Tests are separate modules under `apps/tester/src/tests/`. The current modules cover:

- `argc` and `argv`
- standard formatted and character output
- `malloc`, `calloc`, `realloc`, and `free`
- files, metadata, binary/CP437 byte preservation, and working directories
- nonblocking stdin through `fcntl`, `O_NONBLOCK`, and `EAGAIN`
- nested child/grandchild status delivery and parent restoration
- process-owned heap and leaked-descriptor cleanup after nonzero child exit
- RTC calendar, Unix time, `time()`, `gettimeofday()`, and realtime/monotonic clocks
- battery registry state, telemetry validity, signed power consistency, and unchanged charger controls

Build and install into the host root filesystem:

```sh
eval "$(./tools/tabos activate-idf)"
make -C apps/tester
```

Run from the shell:

```text
> tester one "two words"
```

For Tab5, copy `build/apps/tester/tester` to `T:/bin/tester`. A successful run
ends with `[PASS] TabOS SDK tester` and status zero. The filesystem test creates
`T:/tabos-tester/` and removes it before completion.

Process test launches tester as its own child; child launches another tester as
grandchild. Known leaf and child exit statuses verify reverse-order status delivery.
Parent repeats entire chain to verify cleanup and reloading without another app binary.
It also repeatedly launches a child that allocates heap memory, deliberately leaves
eight file descriptors open, and exits nonzero. The parent then reopens and removes the
fixture file, proving child teardown reclaimed descriptors before resuming the parent.

Runtime tests read the UTC wall clock through TabOS and libc interfaces. They write
the immediately reread epoch value back unchanged to verify the RTC write path without
intentionally changing the clock.

Battery tests require `battery0` with telemetry and charge-control features, verify
internally consistent signed readings, and write the already reported normal and fast
charger settings back unchanged. They do not deliberately change charging policy.

Add future API coverage as another focused source module and register it in
`apps/tester/src/main.c`. Tests should remain deterministic, clean up persistent state,
and behave identically on host and Tab5.
