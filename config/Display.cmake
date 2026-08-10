# Shared display and terminal presentation settings.
# All targets use the same values so host output matches Tab5 output.

set(TABOS_TERMINAL_SCALE 4)
set(TABOS_TERMINAL_SCROLLBACK_LINES 256)

if(NOT TABOS_TERMINAL_SCALE MATCHES "^[1-8]$")
    message(FATAL_ERROR "TABOS_TERMINAL_SCALE must be an integer from 1 through 8")
endif()

if(NOT TABOS_TERMINAL_SCROLLBACK_LINES MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "TABOS_TERMINAL_SCROLLBACK_LINES must be a positive integer")
endif()
