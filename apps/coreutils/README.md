# TabOS Core Utilities

Core utilities are small, independently loaded TabOS programs. Each utility has its own
source directory and produces its own ELF binary.

Build every utility:

```sh
make -C apps/coreutils
```

Build one utility:

```sh
make -C apps/coreutils ls
make -C apps/coreutils mkdir
```

Use `build-ls` or `build-mkdir` to compile without installing. Runnable binaries install
to `.local/rootfs/T/bin/ls.bin` and `.local/rootfs/T/bin/mkdir.bin`.
