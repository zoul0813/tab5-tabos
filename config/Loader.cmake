option(
    TABOS_ENABLE_ELF_LOADER_EXPERIMENT
    "Load and execute embedded standalone RISC-V hello ELF after boot"
    OFF
)

set(TABOS_ELF_STARTUP_PATH "T:/bin/hello.bin" CACHE STRING
    "TabOS path loaded by the filesystem-backed ELF diagnostic")
set(TABOS_SHELL_PATH "T:/bin/shell.bin" CACHE STRING
    "TabOS filesystem path for the PID 0 shell")

option(TABOS_ENABLE_SHELL_STARTUP "Start filesystem-backed shell as process 0" OFF)

if(TABOS_ENABLE_SHELL_STARTUP AND
   (TABOS_ENABLE_ELF_LOADER_EXPERIMENT OR TABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP OR
    TABOS_ENABLE_FILESYSTEM_DIAGNOSTIC_APP))
    message(FATAL_ERROR "Shell startup cannot be combined with diagnostic startup applications")
endif()

if(TABOS_ENABLE_ELF_LOADER_EXPERIMENT AND
   (TABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP OR TABOS_ENABLE_FILESYSTEM_DIAGNOSTIC_APP))
    message(FATAL_ERROR
        "Only one startup diagnostic or ELF experiment can be enabled")
endif()

if(TABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP AND TABOS_ENABLE_FILESYSTEM_DIAGNOSTIC_APP)
    message(FATAL_ERROR "Only one startup diagnostic application can be enabled")
endif()
