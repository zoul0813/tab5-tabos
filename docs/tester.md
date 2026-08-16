# SDK Tester Application

`apps/tester/` is the maintained end-to-end validation application for the public TabOS
SDK, C runtime, and application ABI. Unlike host unit tests, it is compiled as a normal
independent RV32 application and the same `tester.bin` runs through host emulation and on
Tab5 hardware.

Tests are separate modules under `apps/tester/src/tests/`. The current modules cover:

- `argc` and `argv`
- standard formatted and character output
- `malloc`, `calloc`, `realloc`, and `free`
- files, metadata, binary/CP437 byte preservation, and working directories
- nonblocking stdin through `fcntl`, `O_NONBLOCK`, and `EAGAIN`

Build and install into the host root filesystem:

```sh
eval "$(./tools/tabos activate-idf)"
make -C apps/tester
```

Run from the shell:

```text
> tester one "two words"
```

For Tab5, copy `build/apps/tester/tester.bin` to `T:/bin/tester.bin`. A successful run
ends with `[PASS] TabOS SDK tester` and status zero. The filesystem test creates
`T:/tabos-tester/` and removes it before completion.

Add future API coverage as another focused source module and register it in
`apps/tester/src/main.c`. Tests should remain deterministic, clean up persistent state,
and behave identically on host and Tab5.
