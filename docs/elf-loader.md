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
- static `SHT_RELA` relocation records retained for loaded sections
- no dynamic segment or `SHT_REL` relocations
- at most 2 MiB ELF file size
- at most 1 MiB loaded image

The hello application is a normal C17 program with `main(argc, argv)`. Source lives in
`apps/hello_elf/src/main.c`; SDK startup, libc syscall stubs, and linker configuration
live under `sdk/`. Runnable image size depends on the newlib functions used by the app.

Application payloads currently select the toolchain's non-compressed RV32I multilib with
integer ABI `ilp32`. This keeps newlib-generated code executable both natively on
ESP32-P4 and through the pinned `mini-rv32ima` interpreter on macOS and Linux. Compressed
RVC output requires host-interpreter support before it can become the shared default.
Host execution uses reserved guest call gates to bridge the TabOS API table; it does not
provide Linux system calls.

### Optional TabOS Metadata

Applications may include one ELF `SHT_NOTE` record named `TABOS`, type `1`, with a
32-byte little-endian descriptor:

```text
offset  size  field
0       4     metadata version (1)
4       4     descriptor size (32)
8       4     Application ABI version (1)
12      4     requested heap bytes
16      4     requested stack bytes
20      4     requested capability bits
24      8     reserved (must be zero)
```

The loader validates metadata before process creation. Heap requests must range from
256 KiB through 16 MiB and align to 4 KiB. Stack requests must range from 16 KiB
through 256 KiB and align to 16 bytes. Current supported capability is console.
Missing metadata uses legacy defaults of 256 KiB heap and 16 KiB stack. Resource
allocation from these values is completed by the process-memory work that follows
this parser.

Host execution is resumable. Each application update executes at most 10,000 guest
instructions, then yields to normal TabOS input, display, timer, and application work.
Guest registers and memory persist into next update. This is scheduling quantum, not
application lifetime limit: long-running and interactive programs continue until they
return, request exit, fault, or are stopped by lifecycle manager.

Build the standalone application inside the activated ESP-IDF v5.4.4 environment:

```sh
make -C apps/hello_elf
```

The runnable output is `build/apps/hello/hello.bin`; the default target copies
it to `.local/rootfs/T/bin/hello.bin`. Use the `build` target to skip installation.
The runnable image has debug sections removed but retains its static symbol and relocation
tables. Unstripped linker output remains at
`build/apps/hello/hello.elf` for debugging.

Applications normally implement `int main(int argc, char **argv)` and include standard C
headers. SDK `crt0` owns the ELF entry, initializes newlib, calls `main`, flushes streams,
and converts its return value into process exit. Internal ABI version 6 remains
behind SDK stubs. It provides console, file descriptor, filesystem, heap, child execution,
yield, TTY-mode, and clean-exit services. Applications use the public SDK interfaces,
not this internal call table.

Each process starts with console descriptors 0, 1, and 2 and allocates file/device
descriptors from 3 upward. It owns an inherited working directory, errno state, 16 KiB
stack, and a heap that grows on demand to 1 MiB by default. Process cleanup closes open
descriptors and releases heap and executable memory.

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

If display reports `ELF load FAILED: NO EXECUTABLE MEMORY`, writable PSRAM allocation
failed. `PREPARE FAILED` means cache synchronization, physical-address resolution, or the
executable PSRAM alias failed.

## Memory Scope

Current Tab5 backend allocates the complete ELF image in writable PSRAM, synchronizes it,
then maps the same physical pages into the ESP32-P4 executable PSRAM linear region. The
MMU request uses `EXEC | READ`; ESP-IDF explicitly rejects adding `WRITE`. On ESP32-P4 the
selected linear region is connected to both instruction and data buses, so the executable
alias supports instruction fetches while application data pointers are statically rebased
to that alias. The loader applies `R_RISCV_32`, `HI20`, `LO12_I`, and `LO12_S`
relocations before its final data/instruction cache synchronization. PC-relative control
flow and address relocations require no load-bias adjustment.

The writable alias remains available to the loader and kernel for data-pointer
translation. Application images therefore use PSRAM rather than being capped by internal
RAM. If hardware validation disproves writable application access through the executable
alias, the loader will need separate relocated code and data mappings. ESP-IDF documents
executable heap capabilities and MMU restrictions:

- [ESP32-P4 heap capabilities](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/system/mem_alloc.html)
- [ESP32-P4 MMU-supported memory](https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32p4/api-reference/system/mm.html)
- [ESP32-P4 memory synchronization](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/system/mm_sync.html)

## Deferred Work

Loader does not yet provide:

- dynamic relocation and imported-symbol processing
- imported symbol resolution
- multiple code/data permission regions
- Tab5 native process memory isolation or crash containment; host interpreter faults are contained
- signing, discovery, or package metadata

Host tests parse, load, and execute the same RV32 application artifact used by Tab5 through a resumable RV32IMA interpreter. Guest state persists across bounded instruction slices so host tests cover loader, ABI, output, exit status, and execution faults without replacing RV32 code with a host-native build.
