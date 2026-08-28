# Shell

TabOS shell is independently compiled RV32 ELF application loaded from
`T:/bin/shell` as process 0. It is not linked into kernel firmware. Process 0 must
remain alive; shell return, explicit exit, or execution fault enters kernel panic.

Build shell with project ESP-IDF toolchain activated:

```sh
eval "$(./tools/tabos activate-idf)"
make -C apps/shell
```

Build output is `build/apps/shell/shell`; the default target copies it to
`.local/rootfs/T/bin/shell`. Use the `build` target to skip installation. The
unstripped debugging image remains at
`build/apps/shell/shell.elf`. Copy `shell` and other applications to the TF-card
`bin/` directory for Tab5.

Select `shell` as host and Tab5 startup application with:

```sh
./tools/tabos config
```

Initial commands are:

- `help`: list commands
- `clear`: clear terminal
- `pwd`: show current drive and directory
- `cd <path>`: change current directory
- `ls [path]`: list directory entries
- `<program>`: search the shell PATH, defaulting to `T:/bin/<program>`
- `<path>`: execute explicit application path

Shell splits application commands into at most 16 arguments. Spaces delimit arguments;
single quotes, double quotes, and backslash escaping preserve spaces or quote characters:

```text
hello one "two words" escaped\ value
```

Quote and escape processing belongs exclusively to shell. Kernel receives only finalized
argument strings and does not interpret command-line syntax.

Shell waits for normalized console text/key events. Enter submits line; Backspace edits
without crossing prompt. Command entry accepts and echoes printable ASCII; ANSI escape
sequences, including arrow keys, and non-ASCII bytes are ignored. Executed application
becomes foreground child, owns console and input, then returns status and focus to shell.
Shell prints `Exit status: N` after every external application completes.

Current shell ABI is experimental. Environment variables, redirection, pipelines,
background jobs, and executable search across multiple drives are not implemented.
