#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
output=${1:-"$root/.local/rootfs/T/bin/shell.bin"}
unstripped="$root/build/elf-spike/shell-unstripped.elf"

mkdir -p "$(dirname -- "$output")"
mkdir -p "$(dirname -- "$unstripped")"
riscv32-esp-elf-gcc \
    -march=rv32ima_zicsr_zifencei -mabi=ilp32 -Os \
    -ffreestanding -fPIC -fno-stack-protector -fno-asynchronous-unwind-tables \
    -nostdlib -I "$root/sdk/include" \
    -Wl,-T,"$root/sdk/linker/app-riscv32.ld" \
    -Wl,--build-id=none -Wl,--gc-sections -Wl,-N \
    -o "$unstripped" "$root/apps/shell/shell.c"
riscv32-esp-elf-strip --strip-all "$unstripped" -o "$output"
riscv32-esp-elf-size "$output"
