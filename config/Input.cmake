# Optional input diagnostics shared by host and hardware targets.
option(
    TABOS_ENABLE_KEYBOARD_DIAGNOSTICS
    "Log every normalized keyboard event without consuming it"
    OFF
)

set(TABOS_KEY_REPEAT_DELAY_MS 400 CACHE STRING
    "Delay before a held key begins repeating, in milliseconds")
set(TABOS_KEY_REPEAT_INTERVAL_MS 50 CACHE STRING
    "Interval between held-key repeats, in milliseconds")
foreach(TABOS_INPUT_TIME_SETTING TABOS_KEY_REPEAT_DELAY_MS TABOS_KEY_REPEAT_INTERVAL_MS)
    if(NOT ${TABOS_INPUT_TIME_SETTING} MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR "${TABOS_INPUT_TIME_SETTING} must be a positive integer")
    endif()
endforeach()
