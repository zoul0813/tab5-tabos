# ELF Loader Experiment

TabOS includes first executable-loader feasibility experiment. It validates and loads independently compiled, stripped RV32 ELF image, then executes same artifact on Tab5 or through bounded RV32 interpretation on host. Experiment intentionally excludes filesystem and shell.

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
- at most 1 MiB loaded image

Current hello fixture is 348-byte stripped ELF containing 125-byte load image, one load segment, and no relocations. Source lives in `apps/hello_elf/hello.c`. Linker configuration lives in `sdk/linker/app-riscv32.ld`.

Application payloads currently target RV32IMA with integer ABI `ilp32`. This subset runs natively on ESP32-P4 and through pinned `mini-rv32ima` interpreter on macOS and Linux. Host execution uses reserved guest call gates to bridge experimental TabOS API table; it does not provide Linux system calls.

Host execution is resumable. Each application update executes at most 10,000 guest
instructions, then yields to normal TabOS input, display, timer, and application work.
Guest registers and memory persist into next update. This is scheduling quantum, not
application lifetime limit: long-running and interactive programs continue until they
return, request exit, fault, or are stopped by lifecycle manager.

Build standalone fixture inside activated ESP-IDF v5.4.4 environment:

```sh
./sdk/tools/build-hello-elf.sh
```

Application uses `<tabos/elf_api.h>`. API version 1 contains ABI version, console-write function, and clean-exit request function. This small table is experimental and not frozen as final TabOS ABI.

## Tab5 Hardware Test

Run project configurator and select `elf-hello` as Tab5 startup application:

```sh
./tools/tabos config
./tools/tabos tab5 debug build
```

Build and flash can also be requested together through the wrapper:

```sh
./tools/tabos tab5 debug flash
```

Successful display output includes:

```text
Hello from independent TabOS ELF
```

Serial output includes loaded file/image sizes and returned status zero. Application then exits and runtime remains active without foreground cursor.

If display reports `ELF load FAILED: NO EXECUTABLE MEMORY`, writable PSRAM allocation failed. `PREPARE FAILED` means cache synchronization, physical-address resolution, or executable MMU alias creation failed.

## Memory Scope

Current Tab5 backend loads bytes through a writable PSRAM mapping, synchronizes cache, then creates a read/execute MMU alias for the same physical pages. API string pointers are translated back to readable data mapping. ESP-IDF documents executable heap capabilities, explicit MMU execute mappings, and instruction-cache synchronization:

- [ESP32-P4 heap capabilities](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/system/mem_alloc.html)
- [ESP32-P4 MMU-supported memory](https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32p4/api-reference/system/mm.html)
- [ESP32-P4 memory synchronization](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/system/mm_sync.html)

## Deferred Work

Experiment does not yet provide:

- filesystem ELF loading
- application arguments
- relocation processing
- imported symbol resolution
- multiple code/data permission regions
- PSRAM execution mapping
- process isolation or crash containment
- host execution of RV32 binaries
- signing or package metadata

Host tests parse and load image bytes into memory but do not execute RV32 code. Real execution remains Tab5 hardware test until CPU emulation exists.
