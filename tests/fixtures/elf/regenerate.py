#!/usr/bin/env python3
"""Regenerate the private-transport fixture using the SDK RISC-V compiler."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix="tabos-elf-fixture-") as temporary:
    image = Path(temporary) / "guest.elf"
    subprocess.run([
        "riscv32-esp-elf-gcc", "-march=rv32i_zicsr_zifencei", "-mabi=ilp32", "-Os",
        "-std=c17", "-fno-pic", "-mno-relax", "-msmall-data-limit=0", "-nostdlib",
        "-fno-stack-protector", "-fno-asynchronous-unwind-tables",
        "-Wl,--build-id=none", "-Wl,--emit-relocs", "-Wl,-N",
        "-T", str(root / "sdk/linker/app-riscv32.ld"),
        "-I", str(root / "sdk/include"), str(Path(__file__).with_name("guest.c")),
        "-o", str(image),
    ], check=True)
    subprocess.run(["riscv32-esp-elf-strip", "--strip-debug", str(image)], check=True)
    data = image.read_bytes()
lines = ['#include "hello_elf.h"', '', 'const uint8_t loader_hello_elf[] = {']
for start in range(0, len(data), 12):
    lines.append('    ' + ', '.join(f'0x{byte:02x}' for byte in data[start:start + 12]) + ',')
lines += ['};', '', 'const size_t loader_hello_elf_size = sizeof(loader_hello_elf);', '']
Path(__file__).with_name("hello_elf.c").write_text('\n'.join(lines))
