if(NOT DEFINED TABOS_SOURCE_DIR)
    message(FATAL_ERROR "TABOS_SOURCE_DIR is required")
endif()

set(PORTABLE_DIRECTORIES
    kernel
    fs
    shell
    graphics
    input
    audio
    net
    loader
    apps
    sdk/include
    sdk/lib
)

set(FORBIDDEN_INCLUDE_PATTERN
    "#[ \t]*include[ \t]*[<\"]((SDL3/)|(freertos/)|(driver/)|(hal/)|(soc/)|(esp_private/)|(esp_[A-Za-z0-9_]*\\.h))"
)

set(FORBIDDEN_PUBLIC_TYPE_PATTERN
    "(^|[^A-Za-z0-9_])((SDL_[A-Za-z0-9_]*)|(esp_[A-Za-z0-9_]*)|(TaskHandle_t)|(QueueHandle_t)|(SemaphoreHandle_t)|(EventGroupHandle_t)|(BaseType_t)|(TickType_t)|(ppa_[A-Za-z0-9_]*)|(pie_[A-Za-z0-9_]*))([^A-Za-z0-9_]|$)"
)

foreach(directory IN LISTS PORTABLE_DIRECTORIES)
    file(GLOB_RECURSE portable_files
        "${TABOS_SOURCE_DIR}/${directory}/*.c"
        "${TABOS_SOURCE_DIR}/${directory}/*.h"
    )

    foreach(portable_file IN LISTS portable_files)
        file(READ "${portable_file}" contents)
        if(contents MATCHES "${FORBIDDEN_INCLUDE_PATTERN}")
            message(FATAL_ERROR "Platform header leaked into portable file: ${portable_file}")
        endif()
    endforeach()
endforeach()

file(GLOB_RECURSE public_headers "${TABOS_SOURCE_DIR}/sdk/include/*.h")
foreach(public_header IN LISTS public_headers)
    file(READ "${public_header}" contents)
    if(contents MATCHES "${FORBIDDEN_PUBLIC_TYPE_PATTERN}")
        message(FATAL_ERROR "Platform type leaked into public header: ${public_header}")
    endif()
endforeach()
