# Shared display and terminal presentation settings.
# All targets use the same values so host output matches Tab5 output.

set(TABOS_TERMINAL_SCALE 4)

if(NOT TABOS_TERMINAL_SCALE MATCHES "^[1-8]$")
    message(FATAL_ERROR "TABOS_TERMINAL_SCALE must be an integer from 1 through 8")
endif()
