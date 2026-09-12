# Lua

TabOS provides PUC Lua 5.5.1 as the independent `T:/bin/lua` application. macOS
and Tab5 run the same RV32 executable. Scripts and pure-Lua modules can be copied
onto storage and run without rebuilding firmware.

## Build and run

Build/install the normal application set from the repository root:

```sh
./apps/build.sh
```

This installs `lua` into `.local/rootfs/T/bin/`, example scripts into
`.local/rootfs/T/data/lua/`, and MIT/provenance notices into
`.local/rootfs/T/share/licenses/lua/`. Existing `--msc` installation also copies
these assets and notices to the device. After the toolchain is active, inspect
resource metadata with `make -C apps/lua metadata`.

In the TabOS shell:

```text
lua --help
lua -v
lua -e 'print(1 + 2)'
lua T:/data/lua/hello.lua first 'two words'
lua T:/data/lua/system.lua
lua T:/data/lua/files.lua T:/lua-example.dat
lua -i T:/data/lua/hello.lua
lua -l module script.lua
lua -l helper=module -e 'print(helper)'
lua -- -filename.lua
lua
```

`files.lua` writes the output path supplied by the user and prints its bytes.
`-e` and `-l` execute in argument order before the script. `-l module` assigns
the result of `require` to a global named `module`; `-l name=module` uses `name`.
`arg[0]` is the script name, positive entries are script arguments, and negative
entries retain preceding command-line arguments. Script varargs contain the same
positive arguments. Without a script, `arg[0]` is the executable name.

No arguments enters the REPL. `-i` enters it after successful actions/script,
preserving globals. `-v` prints the version and exits unless other work is supplied.
Unknown options and `lua -` return a nonzero status. Piped/redirected stdin scripts,
`loadfile()` without a filename, and `dofile()` without a filename are unsupported.
The shell's existing argument-count and byte limits still apply.

## Prompt and cancellation

Enter submits a line; Backspace edits it. Tab inserts one space. Incomplete Lua
syntax gets a `>> ` continuation prompt. Expressions display their results;
`=expression` is also accepted. Ctrl-D exits an empty input line. Ctrl-C discards
current console input or raises an `interrupted` error in a running Lua chunk.
Syntax/runtime errors and ordinary Lua allocation failures leave the prompt usable.
Script failures print an error/traceback to stderr and return status 1.

The application uses one cooked-event broker for REPL input, console reads and
interruption. Aa/Sym translation stays enabled. Inherited scroll-key interception
and raw mode are disabled while Lua owns the console, then the original mode is
restored. Physical Enter/Backspace events are not echoed twice. Navigation/history
editing is not implemented. Idle input blocks on the existing keyboard wait source.

The broker retains up to 128 non-release events while scripts run. On overflow it
drops newest events, continues detecting Ctrl-C, and rejects the next input line
with a visible overflow error instead of executing truncated input. Console lines
hold at most 4094 bytes; a continued chunk holds at most 4095 bytes total.
Overlong input is discarded through Enter. File-based scripts are bounded by the
Lua memory ceiling rather than these console limits.

Cancellation uses a count hook every 1000 VM instructions, with service work at
most once per 10 elapsed milliseconds. New coroutines inherit the hook. Long sleeps
are split into at most 20 ms SDK sleep calls with cancellation checks. A script can
catch an interruption error; long C-library operations execute no VM hooks. This
is cooperative cancellation, not a hard time limit or security sandbox. Hardware
latency and hook overhead still require measurement.

## Standard-library profile

| Surface | Behavior |
| --- | --- |
| Base, table, string, math, coroutine | Ordinary Lua behavior within resource limits; `string.dump` omitted. |
| Numbers | Standard signed 64-bit integers and double precision. RV32 uses software arithmetic where needed. |
| `load`, `loadfile`, `dofile`, `require` | Text source only; binary chunks are rejected, including reader-function loads. No `luac` supplied. |
| UTF-8 | Byte-processing library retained. Console/system text remains CP437; file data can contain arbitrary UTF-8 or binary bytes. |
| Regular files | Standard `io.open`, read formats, write, seek, flush, close and file line iteration through newlib. NUL bytes in file contents are preserved. |
| Console reads | `io.read`, `io.stdin:read`, default-input aliases and `io.lines()` support `l`/`*l` and `L`/`*L` only. Ctrl-D on an empty line returns nil. Other formats raise a Lua error before consuming input. |
| `io.popen`, `io.tmpfile` | Raise unsupported-operation errors. |
| `os.remove`, `os.rename` | Filesystem operations return Lua-style success or nil/message/error-code results. |
| `os.getenv` | Always nil; no host environment is inherited. |
| `os.exit` | Accepts boolean or C-int status, closes the Lua state and owned files, restores TTY mode and exits. State cleanup is always performed, including when the optional close argument is false. Calling it recursively during state finalization raises an error. |
| `os.clock`, `os.execute`, `os.tmpname`, `os.setlocale` | Raise unsupported-operation errors. No shell parsing, CPU-time substitution or workstation locale changes. |
| `package` | Preloaded and pure-source modules; two searchers, empty cpath. `package.loadlib` returns nil/message/`"absent"`. |
| `debug` | Public library omitted; internal traceback and interruption hooks remain available. |

Embedded NUL in filenames, module names, search paths and filesystem modes is
rejected before crossing a C-string API. Host-only Lua configuration variables
(`LUA_INIT`, `LUA_PATH`, `LUA_CPATH`, including versioned variants) are ignored.
Errors from supported filesystem/system services use nil/message/numeric-error
results; invalid arguments and explicitly unsupported language operations raise
Lua errors. Standard `load` failures retain nil/message results.

### UTC dates and time

`os.time()` reads the portable wall clock. `os.date()` always formats UTC,
including when `!` is omitted. The supported epoch range is 1970-01-01 through
9999-12-31 23:59:59. `os.date('*t', seconds)` returns a copied calendar table;
`isdst` is false. `os.difftime` subtracts timestamps.

`os.time(table)` requires valid `year`, `month`, and `day`; hour defaults to 12,
minute/second to zero. Calendar fields must already be in range: overflowing
months/days/hours and invalid leap days raise errors rather than being normalized.
No leap seconds or DST adjustment are applied. The input table is updated with
calendar fields, weekday, year-day and false `isdst`.

Supported date conversions are `%a %A %b %B %c %d %H %I %j %m %M %p %S %U %w %W
%x %X %y %Y %z %Z %%`; `%z` is `+0000` and `%Z` is `UTC`. Other specifiers and
modifiers raise an error. Formatting uses the runtime's initial C locale.

## Modules and TabOS helpers

Default `package.path`:

```text
./?.lua;./?/init.lua;T:/lib/lua/5.5/?.lua;T:/lib/lua/5.5/?/init.lua
```

Paths are separated by semicolons because drive names contain colons. Relative
paths use the process working directory, inherited from the shell, not the script's
directory. Dots in module names become slashes. For example, manually install
`util/format.lua` under the working directory or `T:/lib/lua/5.5/`, then call
`require('util.format')`. Module caching, `package.preload` and `package.loaded`
retain upstream semantics. Missing system module directories need no startup
creation. Scripts may explicitly change `package.path`. Examples use the existing
flat asset staging mechanism; it does not recursively copy module trees.

`local tabos = require('tabos')` exposes:

- `tabos.info()`: owned table with target/device/display strings, display width/height,
  CPU cores/frequency, total memory bytes and external memory bytes.
- `tabos.monotonic_ms()`: monotonic elapsed milliseconds as a Lua integer.
- `tabos.sleep_ms(ms)`: integer duration from 0 through 4294967295, returns true or
  nil/message/error-code. Uses the SDK's cooperative yield-based sleep; it does not
  request low-power sleep. Wrong types, fractions, infinities and negatives are errors.

Graphics, direct input-event, audio, child execution, native C modules, networking,
camera bindings, LuaJIT and LuaRocks are not part of this CLI milestone.

## Resource limits and validation

Metadata requests a 4 MiB application heap and 64 KiB native stack. The Lua allocator
caps managed allocations at 3 MiB, reserving the rest for newlib and adapter state.
C-call and pattern recursion limits are 48. These are conservative starting budgets;
physical stack/heap high-water measurements remain required. Deep parsing, recursion,
large allocations or long patterns may therefore fail earlier than desktop Lua.
Closing the state releases Lua allocations and file handles. Native Tab5 execution
faults remain device-fatal under the current platform architecture.

Host tests compile the target Lua profile with ASan/UBSan. They inject allocation
failure during registration, check allocator cleanup, console reads, numeric semantics,
UTC bounds, unsupported operations, file bytes, module loading and coroutine interruption.
Three unchanged official 5.5.1 suites (UTF-8, strings, math) run with their portable/
low-resource flags. This is an audited subset, not a full upstream-suite pass.
See [provenance and exclusions](../apps/lua/UPSTREAM.md).

After building the host and applications, run the optional real-RV32 session harness:

```sh
build/macos-debug/tests/tabos_lua_rv32 build/apps/shell/shell build/apps/lua/lua
```

It uses temporary drive roots and the real loader, interpreter, shell, terminal,
filesystem and wait services. It checks stdout/status, files, modules, argument
passing, REPL state, error/OOM recovery, Ctrl-C, exit cleanup and repeated launch.
Ordinary CTest does not require separately built application artifacts. Linux
validation is excluded for this implementation at the user's request.

See the [validation record](validation/lua-cli-2026-09-12.md) for build and test evidence.

Physical Tab5 acceptance is still pending. Before marking the CLI milestone accepted:

1. Run the examples from microSD; verify Aa/Sym typing, Backspace, multiline input,
   Ctrl-D, and a usable shell after repeated launches and `os.exit(7)`.
2. Interrupt `while true do end` and the same loop in `coroutine.wrap`; measure
   response latency and verify other runtime services remain responsive.
3. Read/write binary files and require a nested module from microSD.
4. Recover from `string.rep('x', 4000000)`, parser-depth and recursion failures.
5. Record application stack/heap high-water use and representative computation/
   allocation performance. Host interpreter timing is not a native benchmark.
