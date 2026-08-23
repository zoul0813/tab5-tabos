# Shared display and terminal presentation settings.
# All targets use the same values so host output matches Tab5 output.

set(TABOS_TERMINAL_SCALE 2 CACHE STRING "Terminal glyph scale from 1 through 8")
set(TABOS_TERMINAL_SCROLLBACK_LINES 256 CACHE STRING
    "Number of retained terminal scrollback lines")
set(TABOS_HOST_REFRESH_RATE_HZ 58 CACHE STRING
    "Host graphics presentation rate used to emulate Tab5")

if(NOT TABOS_TERMINAL_SCALE MATCHES "^[1-8]$")
    message(FATAL_ERROR "TABOS_TERMINAL_SCALE must be an integer from 1 through 8")
endif()

if(NOT TABOS_TERMINAL_SCROLLBACK_LINES MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "TABOS_TERMINAL_SCROLLBACK_LINES must be a positive integer")
endif()

if(NOT TABOS_HOST_REFRESH_RATE_HZ MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "TABOS_HOST_REFRESH_RATE_HZ must be a positive integer")
endif()
