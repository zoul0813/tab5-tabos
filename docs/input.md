# Keyboard Input

TabOS provides one portable keyboard event path across Tab5, macOS, and Linux. Touch and pointer events remain deferred.

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
`TABOS_KEY_SHIFT`, `TABOS_KEY_ALT`, and `TABOS_KEY_GUI`. Test modifiers accompanying
any event with `TABOS_MODIFIER_CONTROL`, `TABOS_MODIFIER_SHIFT`,
`TABOS_MODIFIER_ALT`, and `TABOS_MODIFIER_GUI`. Text events carry up to nine CP437
bytes plus a null terminator.

Raw input and standard input consume the same foreground event stream. An application
should choose one interface rather than mix them. Terminal applications may use
`read(STDIN_FILENO, ...)`; arrow keys are encoded as ANSI CSI `A`, `B`, `C`, and `D`
sequences. Games and other interactive applications should use this raw event API to
observe key-up, key-down, held-key repeats, and modifiers directly.

The queue holds 64 events and is protected for host-thread and FreeRTOS-task access. If producers outrun consumers, the oldest event is discarded so current input remains responsive.

## Host Backend

SDL3 physical keyboard events become TabOS key-down/key-up events. Representable SDL3
text input becomes CP437 text events. SDL does not consistently provide text events for
Enter, Tab, or operating-system key repeat, so the host backend synthesizes missing
normalized text events and suppresses matching SDL duplicates.

Cmd+Shift+F12 on macOS (Super+Shift+F12 on Linux) saves the current logical
1280x720 framebuffer as a timestamped PNG under `screenshots/`. The host backend
consumes both shortcut key events, while unmodified F12 remains available to TabOS
applications.

## Tab5 Keyboard Backend

The Tab5 Keyboard is connected through ExtPort1 using SDA GPIO0, SCL GPIO1, and default I²C address `0x6D`. TabOS uses ESP32-P4 I²C controller 0 at 400 kHz; the Tab5 BSP's internal device bus remains on controller 1.

At platform initialization, TabOS probes the keyboard, reads firmware register `0xFE`, selects HID mode through register `0x10`, and clears the device event queue. The run loop polls event-count register `0x02` every 10 ms and drains two-byte HID reports from register `0x30`.

HID reports are normalized into the same physical key, modifier, and text events used by host builds. Current hardware text translation uses a US ANSI mapping. Ctrl, Alt, or GUI-modified key combinations produce physical key events but no text event.

Keyboard presence, firmware version, and HID mode appear in both serial and on-screen boot diagnostics. Missing keyboard hardware is a warning and does not prevent TabOS from booting.

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
input, while graphics applications may use raw events. The current Tab5 driver uses
low-overhead polling; GPIO50 interrupt support can replace polling later without changing
application code.

USB keyboards connected to Tab5 are not supported yet. A future ESP-IDF USB-host HID backend can submit events to the same portable queue and coexist with the I²C keyboard without changing applications.
