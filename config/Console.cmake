# Optional built-in application used to validate console input and rendering.
option(
    TABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP
    "Run the interactive console diagnostic application after boot"
    OFF
)

set(TABOS_CURSOR_BLINK_INTERVAL_MS 500 CACHE STRING
    "Console cursor blink half-period in milliseconds")
if(NOT TABOS_CURSOR_BLINK_INTERVAL_MS MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "TABOS_CURSOR_BLINK_INTERVAL_MS must be a positive integer")
endif()
