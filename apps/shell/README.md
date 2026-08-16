# TabOS Shell

The shell is TabOS's persistent process 0 and command-line interface. It uses only the
public experimental TabOS application ABI.

Activate the project toolchain, then build and install it into the host TF-card root:

```sh
eval "$(./tools/tabos activate-idf)"
make -C apps/shell
```

The default build writes the stripped runnable image to `build/apps/shell/shell.bin` and
installs it as `.local/rootfs/T/bin/shell.bin`. Use `make -C apps/shell build` to build
without installing. The unstripped ELF remains at
`build/apps/shell/shell.elf` for debugging.

See `docs/shell.md` for commands and runtime behavior.
