# Keyboard Input

TabOS provides one portable keyboard event path across Tab5, macOS, and Linux. Touch
and pointer input uses separate process-owned streams described in `docs/pointer.md`.

## Public API

Include `<tabos/input.h>`. Applications can poll without blocking or wait for the next event:

```c
tabos_input_event_t event;

if (tabos_input_poll(&event)) {
    /* Handle one queued event. */
}

if (tabos_input_wait(&event)) {
    /* Handle the next event after waiting. */
}
```

Events are `TABOS_INPUT_KEY_DOWN`, `TABOS_INPUT_KEY_UP`, or `TABOS_INPUT_TEXT`.
Key events carry a portable physical key, modifier mask, and repeat flag. The public
header defines codes such as `TABOS_KEY_UP`, `TABOS_KEY_ENTER`, `TABOS_KEY_CTRL`,
`TABOS_KEY_SHIFT`, `TABOS_KEY_ALT`, `TABOS_KEY_GUI`, and `TABOS_KEY_SYM`. Test modifiers accompanying
any event with `TABOS_MODIFIER_CONTROL`, `TABOS_MODIFIER_SHIFT`,
`TABOS_MODIFIER_ALT`, `TABOS_MODIFIER_GUI`, and `TABOS_MODIFIER_SYM`. Text events carry up to nine CP437
bytes plus a null terminator.

Raw input and standard input consume the same foreground event stream. An application
should choose one interface rather than mix them. Terminal applications may use
`read(STDIN_FILENO, ...)`; arrow keys are encoded as ANSI CSI `A`, `B`, `C`, and `D`
sequences. Games and other interactive applications should use this raw event API to
observe key-up, key-down, held-key repeats, and modifiers directly.

Input policy is process-owned and inherited by children. Cooked input is default.
Set `TABOS_TTY_MODE_RAW_INPUT` through `TABOS_TTY_SET_MODE` when physical key
events are required; raw polling omits translated text events. Clear that bit for
cooked text input. Preserve unrelated TTY mode bits when changing policy.

The queue holds 64 events and is protected for host-thread and FreeRTOS-task access by
the platform mutex abstraction. Tab5 uses a priority-inheriting FreeRTOS mutex, so an
application waiting for input cannot spin and starve the runtime task that owns the
queue. If producers outrun consumers, the oldest event is discarded so current input
remains responsive.

Held-key repeat uses an exact monotonic deadline: initial delay starts on key-down and
matching key-up or input reset cancels it immediately. If runtime handles a repeat late,
it emits one repeat and schedules the next future interval instead of replaying missed
events in a burst.

## Host Backend

SDL3 physical keyboard events become TabOS key-down/key-up events. The host derives
representable CP437 text directly from those normalized key events, including Enter,
Tab, modifiers, and portable held-key repeats. Native SDL text-input/IME composition is
disabled because TabOS accepts CP437 rather than composed Unicode; this also prevents
macOS press-and-hold accent UI from intercepting game controls.

macOS maps either Option key to TabOS Sym and keeps Command as GUI. Windows and Linux
map either Win/Super key to Sym and retain their Alt keys as TabOS Alt. Key events and
modifier masks use same mapping.

Cmd+Shift+F12 on macOS (Super+Shift+F12 on Linux) saves the current logical
1280x720 framebuffer as a timestamped PNG under `screenshots/`. The host backend
consumes both shortcut key events, while unmodified F12 remains available to TabOS
applications.

## Tab5 Keyboard Backend

The Tab5 Keyboard is connected through ExtPort1 using SDA GPIO0, SCL GPIO1, and default I²C address `0x6D`. TabOS uses ESP32-P4 I²C controller 0 at 400 kHz; the Tab5 BSP's internal device bus remains on controller 1.

At platform initialization, TabOS probes the keyboard, reads firmware register `0xFE`, selects Normal mode through register `0x10`, and drains boot-time queued events. GPIO50 is configured as the active-low keyboard interrupt and Normal-mode interrupt delivery is enabled through register `0x00`. The ISR only marks work pending and wakes the runtime task. Task context then reads status register `0x01`, drains the event count and matrix press/release data from registers `0x02` and `0x20`, clears status by writing zero to register `0x01`, and rechecks both status and GPIO50 to close the arrival race. Idle runtime updates perform no keyboard I²C traffic. Normal mode preserves independent held state for simultaneous keys; TabOS translates matrix positions into its portable key codes and text events.

Matrix events are normalized into the same physical key, modifier, and text events used by host builds. Current hardware text translation uses a US ANSI mapping. Ctrl or Alt-modified key combinations produce physical key events but no text event.

Tab5 `Aa` is exposed as `TABOS_KEY_SHIFT`/`TABOS_MODIFIER_SHIFT`; `Sym` is exposed
as `TABOS_KEY_SYM`/`TABOS_MODIFIER_SYM`. Raw mode reports physical presses,
releases, and held state. Cooked translation also implements keyboard-style one-shot
behavior: tapping `Aa` or `Sym` modifies next ordinary key, then clears. Holding either
modifier applies it until release. Modified key repeats with its cooked text.

Keyboard presence, firmware version, and Normal mode appear in both serial and on-screen boot diagnostics. Missing keyboard hardware or interrupt setup failure is a warning and does not prevent TabOS from booting. A runtime keyboard I/O failure marks `keyboard0` faulted in the device registry.

## Keyboard Diagnostic Monitor

Configure a build with `TABOS_ENABLE_KEYBOARD_DIAGNOSTICS=ON` to log every normalized keyboard event through the platform log or serial port:

```text
keyboard: KEY_DOWN key=A usage=4 modifiers=SHIFT repeat=no
keyboard: TEXT text="A"
keyboard: KEY_UP key=A usage=4 modifiers=SHIFT repeat=no
```

Option defaults to `OFF`. Monitor observes submitted events without removing them from public queue. Disable it for normal builds to avoid per-key logging overhead.

Host example:

```sh
cmake --preset macos-debug -DTABOS_ENABLE_KEYBOARD_DIAGNOSTICS=ON
cmake --build --preset macos-debug
```

Tab5 example with activated ESP-IDF v5.4.4 environment:

```sh
idf.py -C targets/tab5 -B build/tab5-debug \
  -DIDF_TARGET=esp32p4 -DCMAKE_BUILD_TYPE=Debug \
  -DTABOS_ENABLE_KEYBOARD_DIAGNOSTICS=ON build
```

## Current Limits

Only the foreground application can consume input. The shell uses terminal standard
input, while graphics applications may use raw events. The Tab5 keyboard is interrupt-driven
through GPIO50; built-in touch is interrupt-driven through GPIO23 and documented in
`docs/pointer.md`.

USB keyboards connected to Tab5 are not supported yet. A future ESP-IDF USB-host HID backend can submit events to the same portable queue and coexist with the I²C keyboard without changing applications.

## Keyboard readiness waits

`tabos_input_wait_source()` is declared in `<tabos/wait.h>`. It returns a stable,
process-owned generation-tagged source for the foreground keyboard queue. Request
`TABOS_WAIT_READABLE` through `tabos_wait()`. Readiness observes queued events without
consuming them; drain with `tabos_input_poll()`. Raw and stdin reads share the queue,
so an application must choose one consumption path. Readiness may include events
subsequently filtered by TTY policy.

Keyboard-only waits use a retained, coalescing wake signal on native Tab5 and suspend
the actual RV32 guest on host. Empty infinite waits create no periodic application
work. Finite waits use absolute monotonic deadlines. Teardown cancels the wait before
execution and resources are destroyed. Sources from another process or a destroyed
child return `EBADF`; writable readiness is invalid. Repeated source lookup does not
consume additional source slots. This change does not replace legacy polling behavior
for mixed or unrelated generic service waits, or the existing `tabos_input_wait()`
convenience wrapper.
