# ELF Loader Experiment

TabOS validates and loads independently compiled, stripped RV32 ELF images from TabOS
storage, then executes the same artifact on Tab5 or through bounded RV32 interpretation
on host. Shell launches these images as nested foreground processes.

## Current ELF Contract

Accepted image must be:

- ELF32
- little-endian
- RISC-V machine type
- `ET_EXEC`
- one or more valid `PT_LOAD` segments
- entry point inside executable load segment
- no dynamic segment
- no `SHT_REL` or `SHT_RELA` relocations
- at most 2 MiB ELF file size
- at most 1 MiB loaded image

Current hello fixture is 420-byte stripped ELF containing a 199-byte load image, one load segment, and no relocations. Source lives in `apps/hello_elf/src/main.c`. Linker configuration lives in `sdk/linker/app-riscv32.ld`.

Application payloads currently target RV32IMA with integer ABI `ilp32`. This subset runs natively on ESP32-P4 and through pinned `mini-rv32ima` interpreter on macOS and Linux. Host execution uses reserved guest call gates to bridge experimental TabOS API table; it does not provide Linux system calls.

Host execution is resumable. Each application update executes at most 10,000 guest
instructions, then yields to normal TabOS input, display, timer, and application work.
Guest registers and memory persist into next update. This is scheduling quantum, not
application lifetime limit: long-running and interactive programs continue until they
return, request exit, fault, or are stopped by lifecycle manager.

Build the standalone application inside the activated ESP-IDF v5.4.4 environment:

```sh
make -C apps/hello_elf
```

The stripped runnable output is `build/apps/hello/hello.bin`; the default target copies
it to `.local/rootfs/T/bin/hello.bin`. Use the `build` target to skip installation.
Unstripped linker output remains at
`build/apps/hello/hello.elf` for debugging.

Applications use `<tabos/elf_api.h>`. Experimental ABI version 2 invokes entry with API
table, `argc`, and `argv`. API table provides console output and
input, terminal clearing, working-directory and directory-list operations, child
execution, cooperative yielding, and clean exit request. ABI is not frozen.

## Tab5 Hardware Test

Filesystem-backed loader diagnostic reads `T:/bin/hello.bin` by default. On host, copy
file to `.local/rootfs/T/bin/hello.bin`. For Tab5, copy same file to `bin/hello.bin` on
TF card, using MSC mode or another card reader. Extension is `.bin` for current workflow,
but contents remain ELF.

Run project configurator, select `elf-hello` as startup application, and change startup
path if needed:

```sh
./tools/tabos config
./tools/tabos tab5 debug build
```

Build and flash can also be requested together through the wrapper:

```sh
./tools/tabos tab5 debug flash
```

Filesystem-backed launcher remains process 0 and starts `hello.bin` twice as child
processes. Successful display output includes two hello messages followed by:

```text
Hello TabOS!
argv: T:/bin/hello.bin
ELF child exited with status 0
ELF launcher diagnostic passed
```

Serial output includes source path, loaded image size, and returned status zero.
Launcher remains active after validation, so successful child exits do not panic process 0.

If display reports `ELF load FAILED: NO EXECUTABLE MEMORY`, writable PSRAM allocation failed. `PREPARE FAILED` means cache synchronization, physical-address resolution, or executable MMU alias creation failed.

## Memory Scope

Current Tab5 backend loads bytes through a writable PSRAM mapping, synchronizes cache, then creates a read/execute MMU alias for the same physical pages. API string pointers are translated back to readable data mapping. ESP-IDF documents executable heap capabilities, explicit MMU execute mappings, and instruction-cache synchronization:

- [ESP32-P4 heap capabilities](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/system/mem_alloc.html)
- [ESP32-P4 MMU-supported memory](https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32p4/api-reference/system/mm.html)
- [ESP32-P4 memory synchronization](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/system/mm_sync.html)

## Deferred Work

Loader does not yet provide:

- relocation processing
- imported symbol resolution
- multiple code/data permission regions
- process isolation or crash containment
- signing, discovery, or package metadata

Host tests parse, load, and execute the same RV32 application artifact used by Tab5 through a resumable RV32IMA interpreter. Guest state persists across bounded instruction slices so host tests cover loader, ABI, output, exit status, and execution faults without replacing RV32 code with a host-native build.
