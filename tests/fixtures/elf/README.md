# ELF Test Fixture

`guest.c` is a deliberately small freestanding RV32 application used by ELF
parser, filesystem-loader, and host-execution tests. It is separate from the
user-facing `apps/hello_elf` newlib application so ordinary application growth
does not create a large or unstable embedded test fixture.

`hello_elf.c` is the stripped ELF converted to a C byte array. Regenerate it
whenever the application ABI changes, then update only parser assertions tied
to the resulting ELF layout.

The fixture uses the same RV32 compiler flags and `sdk/linker/app-riscv32.ld`
as applications, but links only `guest.c` with `-nostdlib`. Strip debug data,
convert the result with `xxd -i`, and retain the public array names declared in
`hello_elf.h`.
