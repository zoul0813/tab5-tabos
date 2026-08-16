# Shell

TabOS shell is independently compiled RV32 ELF application loaded from
`T:/bin/shell.bin` as process 0. It is not linked into kernel firmware. Process 0 must
remain alive; shell return, explicit exit, or execution fault enters kernel panic.

Build shell with project ESP-IDF toolchain activated:

```sh
eval "$(./tools/tabos activate-idf)"
./sdk/tools/build-shell-elf.sh
```

Default output is `.local/rootfs/T/bin/shell.bin`. Unstripped debugging image remains at
`build/elf-spike/shell-unstripped.elf`. Copy `shell.bin` and other applications to TF
card `bin/` directory for Tab5.

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
- `<program>`: execute `T:/bin/<program>.bin`
- `<path>`: execute explicit application path

Shell waits for normalized console text/key events. Enter submits line; Backspace edits
without crossing prompt. Executed application becomes foreground child, owns console and
input, then returns status and focus to shell.

Current shell ABI is experimental. Command arguments, quoting, environment variables,
redirection, pipelines, background jobs, and executable search across multiple drives are
not implemented.
