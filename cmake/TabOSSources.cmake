if(NOT DEFINED TABOS_ROOT)
    message(FATAL_ERROR "TABOS_ROOT must be set before including TabOSSources.cmake")
endif()

set(TABOS_CORE_SOURCES
    "${TABOS_ROOT}/kernel/runtime.c"
    "${TABOS_ROOT}/process/process.c"
    "${TABOS_ROOT}/kernel/application_registry.c"
    "${TABOS_ROOT}/kernel/boot_report.c"
    "${TABOS_ROOT}/apps/diag/diagnostic_apps.c"
    "${TABOS_ROOT}/graphics/display.c"
    "${TABOS_ROOT}/graphics/raster.c"
    "${TABOS_ROOT}/graphics/font.c"
    "${TABOS_GENERATED_FONT_SOURCE}"
    "${TABOS_ROOT}/graphics/terminal.c"
    "${TABOS_ROOT}/input/input.c"
    "${TABOS_ROOT}/input/input_diagnostic.c"
    "${TABOS_ROOT}/console/console.c"
    "${TABOS_ROOT}/fs/filesystem.c"
    "${TABOS_ROOT}/fs/path.c"
    "${TABOS_ROOT}/sdk/lib/posix_filesystem.c"
    "${TABOS_ROOT}/time/time.c"
    "${TABOS_ROOT}/time/wall_clock.c"
    "${TABOS_ROOT}/loader/elf_loader.c"
    "${TABOS_ROOT}/loader/elf_application.c"
)

if(TABOS_ENABLE_ELF_LOADER_EXPERIMENT)
    list(APPEND TABOS_CORE_SOURCES
        "${TABOS_ROOT}/apps/diag/elf_loader/elf_loader_diagnostic.c"
    )
endif()

if(TABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP)
    list(APPEND TABOS_CORE_SOURCES
        "${TABOS_ROOT}/apps/diag/console/console_diagnostic.c"
    )
endif()

if(TABOS_ENABLE_FILESYSTEM_DIAGNOSTIC_APP)
    list(APPEND TABOS_CORE_SOURCES
        "${TABOS_ROOT}/apps/diag/filesystem/filesystem_diagnostic.c"
    )
endif()

set(TABOS_HOST_PLATFORM_SOURCES
    "${TABOS_ROOT}/platform/host/sdl/runtime.c"
    "${TABOS_ROOT}/platform/host/sdl/clock.c"
    "${TABOS_ROOT}/platform/host/sdl/mutex.c"
    "${TABOS_ROOT}/platform/host/sdl/input.c"
    "${TABOS_ROOT}/platform/host/sdl/display.c"
    "${TABOS_ROOT}/platform/host/sdl/executable.c"
    "${TABOS_ROOT}/platform/host/posix/storage_backend.c"
    "${TABOS_ROOT}/platform/posix/storage.c"
)

set(TABOS_ESP32P4_PLATFORM_SOURCES
    "${TABOS_ROOT}/platform/esp32p4/runtime.c"
    "${TABOS_ROOT}/platform/esp32p4/rtc.c"
    "${TABOS_ROOT}/platform/esp32p4/mutex.c"
    "${TABOS_ROOT}/platform/esp32p4/keyboard.c"
    "${TABOS_ROOT}/platform/esp32p4/usb_storage.c"
    "${TABOS_ROOT}/platform/esp32p4/display.c"
    "${TABOS_ROOT}/platform/esp32p4/pie.c"
    "${TABOS_ROOT}/platform/esp32p4/pie_kernels.S"
    "${TABOS_ROOT}/platform/esp32p4/executable.c"
    "${TABOS_ROOT}/platform/esp32p4/storage_backend.c"
    "${TABOS_ROOT}/platform/posix/storage.c"
)
