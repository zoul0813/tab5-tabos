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
lua T:/data/lua/snake.lua
lua T:/data/lua/starfall.lua
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

Interactive startup displays the bundled Lua version and copyright banner:

```text
Lua 5.5.1  Copyright (C) 1994-2026 Lua.org, PUC-Rio
```

Enter submits a line. Backspace removes the character before the cursor; Delete
removes the character at it. Left/Right move the cursor, Home/End jump to the
start/end, and Up/Down recall the last 16 input lines and restore the current draft.
Long lines scroll horizontally while editing. Tab inserts one space. Incomplete Lua
syntax gets a `>> ` continuation prompt. Expressions display their results;
`=expression` is also accepted. Ctrl-C and Ctrl-D exit the REPL and return to the parent shell, even with
unfinished input or while an interactive chunk is running. `os.exit()` also exits.
Ctrl-U discards the current input, including a pending multiline chunk, and starts
a fresh prompt. Noninteractive scripts retain Ctrl-C interruption as a Lua error.
Syntax/runtime errors and ordinary Lua allocation failures leave the prompt usable.
Script failures print an error/traceback to stderr and return status 1.

The application uses one cooked-event broker for REPL input, console reads and
interruption. Aa/Sym translation stays enabled. Inherited scroll-key interception
and raw mode are disabled while Lua owns the console, then the original mode is
restored. Physical Backspace events edit input; Enter uses cooked text. History is
session-local and is not saved to disk. Console `io.read` retains append/Backspace
editing; navigation applies to REPL prompts. Idle input blocks on the
existing keyboard wait source.

The broker retains up to 128 non-release events while scripts run. On overflow it
drops newest events, continues detecting exit/cancel shortcuts, and rejects the next input line
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
| Console reads | `io.read`, `io.stdin:read`, default-input aliases and `io.lines()` support `l`/`*l` and `L`/`*L` only. During the REPL, Ctrl-C/Ctrl-D exit even during a console read. Outside the REPL, Ctrl-D on an empty line returns nil. Ctrl-U clears pending console input. Other formats raise a Lua error before consuming input. |
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

Graphics, keyboard game input, and PCM playback are available as described below. Pointer,
child execution, native C modules, networking, camera bindings, LuaJIT and LuaRocks
remain unavailable.

## Games written in Lua

A game is an ordinary `.lua` file run with `lua game.lua`. Developers do not compile
C or embed Lua. The script defines artwork, movement, collisions, input, and scoring;
drawing methods call the existing portable TabOS SDK. No tile engine is required.
The installed [Snake example](../apps/lua/examples/snake.lua) includes its own shapes
and score digits, with no external assets:

```text
lua T:/data/lua/snake.lua
```

Arrows or WASD steer, Space pauses, Enter restarts, M toggles sound, and Q quits. A white border means
paused; a red border means game over. Score appears at the top and prints on normal
exit. Start, eat, lose, and win use the same quiet triangle-wave melodies as native
Snake (`apps/snake/src/sound.c` at commit `809b65f`). PCM is generated once in Lua
and cached; new effects flush and reuse the playback stream. Pause stops queued
audio, mute closes the stream, and unavailable audio never stops gameplay.
Ctrl-C/Ctrl-D interrupt a graphics script and restore the terminal.

The [Starfall example](../apps/lua/examples/starfall.lua) ports `apps/starfall` into
one Lua file, including its 640×360 artwork, 5×7 font, scrolling stars, three enemy
types, shots, particles, lives, waves, and title/pause/game-over screens:

```text
lua T:/data/lua/starfall.lua
```

A/S move left/right, K starts or restarts and fires while held, P pauses/resumes,
and Q or Escape returns to the shell. The simulation uses fixed 60 Hz steps with
bounded catch-up; actual presentation speed depends on the device. Like native
Starfall, this example has no sound.

Starfall saves its best score on game over or normal exit to
`T:/data/lua/starfall-highscore.dat`, capped at 999999. Installation creates this
directory. Scores are separate from native Starfall's `T:/data/starfall/highscore.dat`.
A temporary file is renamed after a successful write/close; failed replacement
preserves the previous score. Missing or read-only storage never prevents play.
Ctrl-C/Ctrl-D restore the terminal but do not save the current session's score.
Copying the script alone is enough to play; no font, image, or native game library
is needed.

A minimal script:

```lua
local tabos = require("tabos")
local screen <close> = assert(tabos.graphics.open(320, 180))
local cyan = tabos.rgb(0, 255, 255)
local x = 150
while true do
    local event = screen:poll()
    if event and event.type == "key_down" and event.key == "q" then break end
    if screen:is_down("left") then x = math.max(0, x - 2) end
    if screen:is_down("right") then x = math.min(300, x + 2) end
    assert(screen:clear(tabos.rgb(0, 0, 20)))
    assert(screen:fill_rect(x, 140, 20, 8, cyan))
    assert(screen:present())
    assert(tabos.sleep_ms(16))
end
```

For a real game, use `tabos.monotonic_ms()` for elapsed time or a fixed simulation
step; rendering and presentation can take longer than the requested sleep.

### Canvas and drawing API

`tabos.graphics.open(width, height)` returns one screen userdata or
`nil, message, errno`. Dimensions must be positive integers, fit the physical
display, and use at most 262144 pixels (512 KiB of RGB565 canvas memory). For example,
320×180, 426×240, and 640×360 fit. Native framebuffer access is not exposed. TabOS
centers the canvas at the largest fitting integer scale with black borders.
Only one screen can be open per Lua process; another open returns `EBUSY`.

`tabos.rgb(red, green, blue)` converts integer channels in 0..255 to an RGB565
integer. Drawing colors are RGB565 integers in 0..65535.

| Method | Behavior |
| --- | --- |
| `screen:size()` | Returns logical width, height. |
| `screen:clear(color)` | Fills the canvas. |
| `screen:pixel(x, y, color)` | Draws one pixel. |
| `screen:line(x0, y0, x1, y1, color)` | Draws a line including both endpoints. |
| `screen:rect(x, y, width, height, color)` | Draws a rectangle outline. |
| `screen:fill_rect(x, y, width, height, color)` | Draws a filled rectangle. |
| `screen:blit(x, y, width, height, bytes)` | Copies row-major, little-endian RGB565 bytes. String length must be exactly `width * height * 2`; bitmap size has the same 512 KiB ceiling. |
| `screen:set_letterbox_color(color)` | Changes borders for the next presentation. |
| `screen:present()` | Presents the complete canvas using the SDK's integer upscale. Checks cancellation before and after the SDK call. |
| `screen:close()` | Closes the screen and restores console input/display. Repeated close succeeds. |

Drawing coordinates are integers in -32768..32767, with origin at the top left,
positive X right, and positive Y down. Rectangle sizes are integers in 0..32767;
zero width or height draws nothing. Bitmap sizes must be positive. Drawing clips
to the canvas, so offscreen shapes are valid. Bounded coordinates limit arithmetic
and C-call work. Arguments reject fractions, NaN, infinity, numeric strings, invalid
buffers, and out-of-range values. Methods on a closed screen raise a Lua error,
except `close()`. Mutating methods return true or `nil, message, errno`; use `assert`
when an error should stop the game.

Packed images can also be defined entirely in Lua:

```lua
local red = tabos.rgb(255, 0, 0)
local blue = tabos.rgb(0, 0, 255)
assert(screen:blit(10, 10, 2, 1, string.pack("<I2I2", red, blue)))
```

Blits decode into aligned temporary Lua memory and copy synchronously into the
logical canvas. The source string may be released immediately after return.
Presentation retains the canvas through the SDK call; a failed close retains it
for a later retry instead of freeing memory still borrowed by graphics.

Use `local screen <close> = assert(...)` for deterministic scope cleanup, including
Lua error unwinding. Unreachable screen userdata also closes during garbage
collection. Script completion, uncaught errors, `os.exit`, and process teardown
clean up graphics. The REPL closes any remaining screen before its next prompt;
a game should run its loop inside one chunk or script. Errors caught by the game
retain resources still in scope. A stale screen cannot close a newer one.

### Keyboard game input

The same input broker used by the REPL owns graphics keyboard input. Opening a
screen switches to raw physical events; closing restores the preceding mode.
Console `io.read` is rejected while graphics is open. Transitioning modes clears
buffered events and held-key state; keys already held at open may need release
and another press. Files remain usable while graphics is open.

- `screen:poll()` returns the next event table or nil, without waiting.
- Keyboard event fields are `type` (`"key_down"` or `"key_up"`), `key` (name),
  `code` (SDK key number), `modifiers` (SDK bitmask), and `repeat` (boolean;
  access as `event["repeat"]`, since `repeat` is a Lua keyword).
- `screen:is_down(name)` returns the latest physical state observed by the broker,
  without consuming buffered events. Both methods service input and cancellation.
- Names include lowercase `a`..`z`, `0`..`9`, `left`, `right`, `up`, `down`, `space`,
  `enter`, `escape`, `tab`, `backspace`, `delete`, `home`, `end`, `page_up`, `page_down`,
  `ctrl`, `shift`, `alt`, `gui`, and `sym`. Other physical keys still deliver events
  with `key="unknown"` and their numeric `code`; `is_down` rejects unknown names.
- The broker buffers 128 press/release events. Overflow drops newest events and
  delivers `{type="overflow"}` on the next poll. Held-key state still updates for
  dropped events. Games needing each tap should drain the queue every frame.
- Ctrl-C and Ctrl-D are reserved for cancellation while graphics is open. They
  raise an interruption error in scripts; in interactive sessions they also request
  REPL exit. Ctrl-U has its normal clear-line meaning only in console mode; games
  receive it as a key event. Escape is available to games with no automatic action.

No text rendering, image decoder, transformed blit, touch, sprite, or tilemap
binding is included in this initial graphics slice. Text can be drawn from Lua-defined pixel
patterns, as Snake demonstrates. Those capabilities can be added independently.

## PCM audio playback

`tabos.audio.open(sample_rate, channels, route)` returns a playback stream userdata
or `nil, message, errno`. Arguments may be omitted or nil: defaults are 44100 Hz,
mono, and `"speaker"`. Rates are 8000, 11025, 12000, 16000, 22050, 24000, 32000,
44100, 48000, 88200, or 96000 Hz; channels are 1 or 2; route is `"speaker"` or
`"headphone"`. Actual availability comes from the audio service. Concurrent streams
share one sample rate; a conflicting rate returns `EBUSY`. Up to eight Lua streams
may be open, subject to the system-wide eight-stream limit. Each service stream has
a 32 KiB ring buffer. No resampling or sample callback is hidden in the binding.

| API | Behavior |
| --- | --- |
| `tabos.audio.info()` | Copied table with SDK `features`, `routes`, `sample_rates` bitmasks, `default_sample_rate`, and `capture_channels`. This binding exposes playback only. |
| `stream:write(bytes, offset)` | Nonblocking write of signed-16-bit little-endian PCM. Returns accepted byte count or nil/message/errno. Optional zero-based byte offset defaults to 0. |
| `stream:flush()` | Immediately discards queued playback without closing. Useful for replacing a sound effect. |
| `stream:set_volume(value)` | Sets stream gain in 0..1000. |
| `stream:status()` | Copied table with `buffered_bytes`, `buffer_capacity`, `underruns`, and `overruns`. |
| `stream:close()` | Stops/discards queued playback and closes. Repeated close succeeds. |

PCM strings must contain 1..16384 bytes (`tabos.audio.MAX_WRITE_BYTES`) and complete
frames: 2 bytes for mono, 4 interleaved left/right bytes for stereo. Offset must be
inside the string and frame-aligned. Partial writes are normal; advance the offset
by the returned count. A full buffer returns `nil, message, tabos.audio.EAGAIN`.
Games can defer remaining data to another frame or drop an optional effect. No
method waits for ring space. Other errors should be handled or stop playback.

```lua
local tabos = require("tabos")
local sound <close> = assert(tabos.audio.open())
local pcm = string.rep(string.pack("<i2", 0), 4410) -- 100 ms mono silence
local offset = 0
while offset < #pcm do
    local count, message, code = sound:write(pcm, offset)
    if count then
        offset = offset + count
    elseif code == tabos.audio.EAGAIN then
        assert(tabos.sleep_ms(5)) -- explicit caller-selected retry policy
    else
        error(message)
    end
end
while assert(sound:status()).buffered_bytes > 0 do
    assert(tabos.sleep_ms(5))
end
```

The SDK copies accepted bytes before `write` returns; source strings may then be
released. Backend/device buffering means an empty ring is not an exact audible
completion fence. `close` does not wait for queued sound to finish. Stream methods
other than write/status return true or nil/message/errno. Invalid types, fractional
numbers, out-of-range values, malformed PCM and use of closed streams raise errors.

Use `<close>` for prompt cleanup on scope exit and errors. GC is a fallback;
uncaught script errors, script termination, `os.exit`, and process teardown also
close streams. The REPL closes remaining streams before its next prompt. Caught
errors retain resources still in scope. Generation checks protect later streams
from stale userdata/finalizers. Failed closes remain available for retry.

Writes check cooperative cancellation before submitting any bytes. Hardware
open/close operations use the SDK's synchronous device lifecycle; Lua cannot cancel
in the middle of a driver call. Explicit retry loops using `sleep_ms` retain its
20 ms cancellation checks. Capture, audio wait-source bindings, runtime route
changes, file decoding, and streaming callbacks are not exposed in this slice.
See [Audio Service](audio.md) for backend routing and shared-clock details.

## Resource limits and validation

Metadata requests a 4 MiB application heap and 64 KiB native stack. The Lua allocator
caps managed allocations at 3 MiB, reserving the rest for newlib, adapter state,
and the separate SDK canvas (at most 512 KiB). Packed blit scratch buffers count
toward the 3 MiB Lua limit.
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
build/macos-debug/tests/tabos_lua_rv32 build/apps/shell/shell build/apps/lua/lua apps/lua/examples/snake.lua
```

It uses temporary drive roots and the real loader, interpreter, shell, terminal,
filesystem and wait services. It checks stdout/status, files, modules, argument
passing, REPL state, error/OOM recovery, Ctrl-C, exit cleanup and repeated launch.
It also checks Lua-rendered pixels, packed blits, raw key taps, graphics cleanup,
and (when the optional third path is supplied) the actual Snake script.
Ordinary CTest does not require separately built application artifacts. Linux
validation is excluded for this implementation at the user's request.

See the [CLI validation record](validation/lua-cli-2026-09-12.md) and
[graphics validation record](validation/lua-graphics-2026-09-12.md) for build and test evidence.
Audio validation is recorded in [Lua audio validation](validation/lua-audio-2026-09-12.md).

Physical Tab5 acceptance is still pending. Before marking the CLI milestone accepted:

1. Run the examples from microSD; verify Aa/Sym typing, Backspace, multiline input,
   Ctrl-D, and a usable shell after repeated launches and `os.exit(7)`.
2. Interrupt `while true do end` and the same loop in `coroutine.wrap`; measure
   response latency and verify other runtime services remain responsive.
3. Read/write binary files and require a nested module from microSD.
4. Recover from `string.rep('x', 4000000)`, parser-depth and recursion failures.
5. Record application stack/heap high-water use and representative computation/
   allocation performance. Host interpreter timing is not a native benchmark.
