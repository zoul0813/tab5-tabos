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
make -C apps/coreutils date
```

Use `build-ls`, `build-mkdir`, or `build-date` to compile without installing. Runnable
binaries install to `.local/rootfs/T/bin/` without filename extensions.
