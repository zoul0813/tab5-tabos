# TabOS Shell

The shell is TabOS's persistent process 0 and command-line interface. It uses only the
public experimental TabOS application ABI.

Activate the project toolchain, then build and install it into the host TF-card root:

```sh
eval "$(./tools/tabos activate-idf)"
make -C apps/shell install
```

The stripped runnable image is written to `build/apps/shell/shell.bin` and installed as
`.local/rootfs/T/bin/shell.bin`. The unstripped ELF remains at
`build/apps/shell/shell.elf` for debugging.

See `docs/shell.md` for commands and runtime behavior.
