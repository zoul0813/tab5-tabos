# TabOS Shell

The shell is TabOS's persistent process 0 and command-line interface. It uses only the
public experimental TabOS application runtime. It has a standard C `main()`; its parser
owns spaces, quoting, and escaping;
kernel receives finalized argument strings.

Activate the project toolchain, then build and install it into the host TF-card root:

```sh
eval "$(./tools/tabos activate-idf)"
make -C apps/shell
```

The default build writes the stripped runnable image to `build/apps/shell/shell` and
installs it as `.local/rootfs/T/bin/shell`. Use `make -C apps/shell build` to build
without installing. The unstripped ELF remains at
`build/apps/shell/shell.elf` for debugging.

Command entry accepts and echoes printable ASCII. Up/Down recalls the last 32 commands;
Down past the newest entry restores the unfinished line. The `history` built-in
prints retained commands. History persists in `T:/user/history.txt`; consecutive
exact duplicates and blank lines are skipped. Other escape sequences and non-ASCII
input bytes are ignored. Ctrl+Arrow still navigates terminal scrollback.

See [Shell](../../docs/shell.md) for commands, persistence, and runtime behavior.
