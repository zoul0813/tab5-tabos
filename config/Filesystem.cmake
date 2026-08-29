set(TABOS_HOST_ROOTFS "${CMAKE_CURRENT_LIST_DIR}/../.local/rootfs" CACHE PATH
    "Controlled host directory exposed inside TabOS as /")

set(TABOS_FILESYSTEM_MAX_FILES 32 CACHE STRING
    "Maximum number of files open across TabOS")
if(NOT TABOS_FILESYSTEM_MAX_FILES MATCHES "^[1-9][0-9]*$" OR
   TABOS_FILESYSTEM_MAX_FILES GREATER 255)
    message(FATAL_ERROR "TABOS_FILESYSTEM_MAX_FILES must be an integer from 1 through 255")
endif()

set(TABOS_FILESYSTEM_MAX_DIRECTORIES 8 CACHE STRING
    "Maximum number of directories open across TabOS")
if(NOT TABOS_FILESYSTEM_MAX_DIRECTORIES MATCHES "^[1-9][0-9]*$" OR
   TABOS_FILESYSTEM_MAX_DIRECTORIES GREATER 255)
    message(FATAL_ERROR "TABOS_FILESYSTEM_MAX_DIRECTORIES must be an integer from 1 through 255")
endif()

option(
    TABOS_ENABLE_FILESYSTEM_DIAGNOSTIC_APP
    "Run filesystem read/write diagnostic application after boot"
    OFF
)
