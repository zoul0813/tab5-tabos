# ELF Loader Experiment

TabOS includes first executable-loader feasibility experiment. It validates and loads independently compiled, stripped RV32 ELF image, then executes it on Tab5 through minimal versioned API table. Experiment intentionally excludes filesystem and shell.

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

Current hello fixture is 308-byte stripped ELF containing 85-byte load image, one load segment, and no relocations. Source lives in `apps/hello_elf/hello.c`. Linker configuration lives in `sdk/linker/app-riscv32.ld`.

Build standalone fixture inside activated ESP-IDF v5.4.4 environment:

```sh
./sdk/tools/build-hello-elf.sh
```

Application uses `<tabos/elf_api.h>`. API version 1 contains ABI version, console-write function, and clean-exit request function. This small table is experimental and not frozen as final TabOS ABI.

## Tab5 Hardware Test

Build experimental firmware with console diagnostic disabled:

```sh
podman run --rm --platform linux/amd64 \
  -v "$PWD:/project" -w /project \
  espressif/idf:v5.4.4 \
  idf.py -C targets/tab5 -B build/tab5-debug \
    -DIDF_TARGET=esp32p4 \
    -DCMAKE_BUILD_TYPE=Debug \
    -DTABOS_ENABLE_KEYBOARD_DIAGNOSTICS=OFF \
    -DTABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP=OFF \
    -DTABOS_ENABLE_ELF_LOADER_EXPERIMENT=ON \
    build
```

Flash already-built image:

```sh
./tools/flash.sh debug
```

Successful display output includes:

```text
HELLO FROM INDEPENDENT TABOS ELF
```

Serial output includes loaded file/image sizes and returned status zero. Application then exits and runtime remains active without foreground cursor.

If display reports `ELF LOAD FAILED: NO EXECUTABLE MEMORY`, current internal `MALLOC_CAP_EXEC | MALLOC_CAP_8BIT` allocation path is unavailable under active ESP32-P4 memory-protection configuration. This is useful experiment result, not parser failure. Next experiment must use explicit MMU mapping or another verified executable-memory arrangement.

## Memory Scope

Current Tab5 backend requests internal byte-addressable executable heap and issues compiler instruction-cache synchronization before entry. It does not assume ordinary PSRAM is executable. ESP-IDF documents executable heap capabilities, explicit MMU execute mappings, and instruction-cache synchronization:

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
