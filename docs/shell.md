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
- `history`: print retained commands oldest first, numbered from 1
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
without crossing prompt. Command entry accepts and echoes printable ASCII; Up/Down
recalls command history. Other escape sequences and non-ASCII bytes are ignored. Executed application
becomes foreground child, owns console and input, then returns status and focus to shell.
Shell prints `Exit status: N` when an external application returns nonzero.

## Command History

The shell retains the last 32 nonblank commands, including commands that fail.
Consecutive identical commands use one entry; repeats separated by another command
remain separate. Original quotes and spacing are preserved, up to the existing
255-character command limit.

Press Up to recall the newest command, then older entries. Up stops at the oldest
entry without wrapping. Down moves toward newer entries; moving past the newest
restores the unfinished line you were typing. Type or press Backspace to edit a
recalled command without changing its stored entry. Editing leaves browsing mode;
the edited text becomes the draft restored after another Up/Down traversal. Enter
executes the displayed command.

`history` lists entries oldest first, numbered 1 through the current count. It is
recorded like any other command, so the listing includes itself. There are no
history expansion, numbered-execution, or `history -c` shortcuts.
Ctrl+Up/Down and other terminal scrollback shortcuts retain their existing behavior.

History loads from `T:/user/history.txt` before the first prompt and is saved before
each command that changes history executes, including `reboot` and `shutdown`.
The shell creates `T:/user` if needed. On the default host setup, this corresponds
to `.local/rootfs/T/user/history.txt`; on Tab5 it is `user/history.txt` on microSD.

The file is plaintext, one command per line, oldest first. Commands containing
sensitive arguments are saved too. LF and CRLF files are accepted, including a final
line without a newline. Blank, overlong, and invalid nonprintable lines are skipped.
Only the newest 32 valid entries are retained.

Saving writes a temporary file and replaces history only after writing and closing
succeed. This does not guarantee durability during sudden power loss. Missing history
starts an empty list. Other storage errors produce one warning per failure episode;
the shell keeps working with in-memory history and retries on the next changed command.
To remove history manually, delete the file while the shell is stopped, then restart.

Current shell ABI is experimental. Environment variables, redirection, pipelines,
background jobs, and executable search across multiple drives are not implemented.
