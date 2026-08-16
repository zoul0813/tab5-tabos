# Hello ELF

Hello ELF is a minimal independent TabOS application used to validate filesystem ELF
loading, argument delivery, and execution. It prints its received argument vector.

Activate the project toolchain, then build and install it into the host TF-card root:

```sh
eval "$(./tools/tabos activate-idf)"
make -C apps/hello_elf
```

The default build writes the stripped runnable image to `build/apps/hello/hello.bin` and
installs it as `.local/rootfs/T/bin/hello.bin`. Use `make -C apps/hello_elf build` to
build without installing. The unstripped ELF remains at
`build/apps/hello/hello.elf` for debugging.
