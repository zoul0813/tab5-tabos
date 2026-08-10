# Console Service

TabOS provides one portable foreground text console shared by Tab5, macOS, and Linux. It joins normalized keyboard input to the framebuffer terminal without embedding shell behavior in the kernel.

## Foreground Session

An application must acquire the console before reading or writing it:

```c
#include <tabos/console.h>

tabos_console_session_t console = {0};

if (tabos_console_acquire(&console)) {
    tabos_console_write_line(&console, "Hello from an application");
    tabos_console_release(&console);
}
```

Only one session can own the foreground console. Calls made with missing, background, released, or stale sessions fail. A rejected input read does not remove an event from the queue. This is cooperative ownership, not a security boundary; future process management will assign and revoke foreground sessions.

## Output and Cursor Controls

Terminal stores character, foreground color, and background color in a cell/history model. Framebuffer is rendered view, not only copy of terminal state. `tabos_console_write()` updates cells, redraws viewport, and presents changed framebuffer immediately. Supported controls are:

- `\n`: move to first column of next row.
- `\r`: move to first column of current row.
- `\b` or DEL: move backward and erase one cell.
- `\t`: advance with spaces to the next four-column tab stop.

Text wraps at the right edge. Output at the bottom scrolls framebuffer upward by one terminal row. `tabos_console_clear()` clears display and resets cursor to column 0, row 0. `tabos_console_get_cursor()` reports current cell position.

Foreground session shows a steady inverted-cell cursor. Cursor disappears while viewport is above live output and when foreground session is released.

## Scrollback

Terminal retains visible rows plus `TABOS_TERMINAL_SCROLLBACK_LINES` history rows in a ring buffer. Shared default is 256 and can be changed in `config/Display.cmake`.

- `tabos_console_scroll_lines()` moves viewport by an exact signed row count.
- `tabos_console_page_up()` and `tabos_console_page_down()` move by one screen minus one row.
- `tabos_console_scroll_to_start()` moves to oldest retained row.
- `tabos_console_scroll_to_end()` returns to live output.
- `tabos_console_is_at_end()` reports whether viewport follows live output.
- `tabos_console_get_history_line_count()` reports currently retained physical rows.

Oldest history row is discarded when ring fills. Any new console output automatically returns viewport to live end. Full keyboards use Page Up, Page Down, Home, and End. Tab5 Keyboard has no physical keys for these HID usages, so diagnostic application maps Ctrl+Up/Down to Page Up/Down and Ctrl+Left/Right to Home/End. Plain arrows remain available for future cursor editing.

Changing terminal scale rebuilds geometry, reflows retained hard and soft-wrapped lines, preserves colors and cursor, and redraws current viewport. Console content no longer falls back to boot-only redraw after scale change.

## Input

`tabos_console_poll()` reads one queued event without blocking. `tabos_console_wait()` waits for one. Both use the normalized events documented in [Keyboard Input](input.md) and only work for foreground session.

## Diagnostic Application

Set `TABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP=ON` when configuring a build to start a small interactive test application after boot. Option defaults to `OFF`.

Diagnostic application displays a prompt, echoes text through public console API, handles Enter, Tab, and repeating Backspace, protects prompt from deletion, clears screen with Ctrl+L, and navigates history with full-keyboard navigation keys or Tab5 Ctrl+Arrow shortcuts. SDL host backend synthesizes normalized text for Enter, Tab, and repeated keys when desktop text input does not provide it. It contains no command parser and is not shell.

Host example:

```sh
cmake --preset macos-debug -DTABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP=ON
cmake --build --preset macos-debug
```

Tab5 example with activated ESP-IDF v5.4.4 environment:

```sh
idf.py -C targets/tab5 -B build/tab5-debug \
  -DIDF_TARGET=esp32p4 -DCMAKE_BUILD_TYPE=Debug \
  -DTABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP=ON build
```
