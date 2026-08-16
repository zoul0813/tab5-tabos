# Hello ELF

Hello ELF is a minimal independent TabOS application used to validate filesystem ELF
loading and execution.

Activate the project toolchain, then build and install it into the host TF-card root:

```sh
eval "$(./tools/tabos activate-idf)"
make -C apps/hello_elf install
```

The stripped runnable image is written to `build/apps/hello/hello.bin` and installed as
`.local/rootfs/T/bin/hello.bin`. The unstripped ELF remains at
`build/apps/hello/hello.elf` for debugging.
