option(
    TABOS_ENABLE_ELF_LOADER_EXPERIMENT
    "Load and execute embedded standalone RISC-V hello ELF after boot"
    OFF
)

if(TABOS_ENABLE_ELF_LOADER_EXPERIMENT AND
   (TABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP OR TABOS_ENABLE_FILESYSTEM_DIAGNOSTIC_APP))
    message(FATAL_ERROR
        "Only one startup diagnostic or ELF experiment can be enabled")
endif()

if(TABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP AND TABOS_ENABLE_FILESYSTEM_DIAGNOSTIC_APP)
    message(FATAL_ERROR "Only one startup diagnostic application can be enabled")
endif()
