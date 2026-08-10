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
)

set(FORBIDDEN_INCLUDE_PATTERN
    "#[ \t]*include[ \t]*[<\"]((SDL3/)|(freertos/)|(esp_[A-Za-z0-9_]*\\.h))"
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
