if(NOT DEFINED TABOS_ROOT)
    message(FATAL_ERROR "TABOS_ROOT must be set before including TabOSSources.cmake")
endif()

set(TABOS_CORE_SOURCES
    "${TABOS_ROOT}/kernel/runtime.c"
    "${TABOS_ROOT}/kernel/application.c"
    "${TABOS_ROOT}/kernel/application_registry.c"
    "${TABOS_ROOT}/kernel/boot_report.c"
    "${TABOS_ROOT}/apps/builtin_apps.c"
    "${TABOS_ROOT}/graphics/display.c"
    "${TABOS_ROOT}/graphics/font.c"
    "${TABOS_ROOT}/graphics/terminal.c"
    "${TABOS_ROOT}/input/input.c"
    "${TABOS_ROOT}/console/console.c"
    "${TABOS_ROOT}/time/time.c"
    "${TABOS_ROOT}/loader/elf_loader.c"
    "${TABOS_ROOT}/loader/fixtures/hello_elf.c"
)

if(TABOS_ENABLE_ELF_LOADER_EXPERIMENT)
    list(APPEND TABOS_CORE_SOURCES
        "${TABOS_ROOT}/loader/elf_application.c"
    )
endif()

if(TABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP)
    list(APPEND TABOS_CORE_SOURCES
        "${TABOS_ROOT}/apps/console_diagnostic/console_diagnostic.c"
    )
endif()

set(TABOS_HOST_PLATFORM_SOURCES
    "${TABOS_ROOT}/platform/host/sdl/platform.c"
)

set(TABOS_ESP32P4_PLATFORM_SOURCES
    "${TABOS_ROOT}/platform/esp32p4/platform.c"
)
