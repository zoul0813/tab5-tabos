if(NOT DEFINED TABOS_ROOT)
    message(FATAL_ERROR "TABOS_ROOT must be set before including TabOSSources.cmake")
endif()

set(TABOS_CORE_SOURCES
    "${TABOS_ROOT}/kernel/runtime.c"
)

set(TABOS_HOST_PLATFORM_SOURCES
    "${TABOS_ROOT}/platform/host/sdl/platform.c"
)

set(TABOS_ESP32P4_PLATFORM_SOURCES
    "${TABOS_ROOT}/platform/esp32p4/platform.c"
)
