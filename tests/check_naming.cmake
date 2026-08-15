if(NOT DEFINED TABOS_SOURCE_DIR)
    message(FATAL_ERROR "TABOS_SOURCE_DIR is required")
endif()

set(PRODUCTION_DIRECTORIES
    apps
    console
    fs
    graphics
    input
    kernel
    loader
    platform
    process
    sdk
    targets
    time
)

set(INTERNAL_HEADER_DIRECTORIES
    apps
    console
    fs
    graphics
    input
    kernel
    loader
    process
)

foreach(directory IN LISTS PRODUCTION_DIRECTORIES)
    file(GLOB_RECURSE production_files
        "${TABOS_SOURCE_DIR}/${directory}/*.c"
        "${TABOS_SOURCE_DIR}/${directory}/*.h"
        "${TABOS_SOURCE_DIR}/${directory}/*.S"
        "${TABOS_SOURCE_DIR}/${directory}/*.S.in"
    )
    foreach(production_file IN LISTS production_files)
        if(production_file MATCHES "/managed_components/")
            continue()
        endif()
        file(READ "${production_file}" contents)
        if(contents MATCHES "(^|[^A-Za-z0-9_])tab_[A-Za-z0-9_]+")
            message(FATAL_ERROR "Legacy internal tab_ symbol in ${production_file}")
        endif()
    endforeach()
endforeach()

foreach(directory IN LISTS INTERNAL_HEADER_DIRECTORIES)
    file(GLOB_RECURSE internal_headers
        "${TABOS_SOURCE_DIR}/${directory}/include/tabos/internal/*.h"
    )
    foreach(internal_header IN LISTS internal_headers)
        file(STRINGS "${internal_header}" lines)
        foreach(line IN LISTS lines)
            if(line MATCHES "^[A-Za-z_][A-Za-z0-9_ \\*]*[ \\*]tabos_[A-Za-z0-9_]+[ \\t]*\\(")
                message(FATAL_ERROR
                    "Internal header declares public-prefixed function in ${internal_header}: ${line}")
            endif()
        endforeach()
    endforeach()
endforeach()
