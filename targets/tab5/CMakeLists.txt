cmake_minimum_required(VERSION 3.16)

set(TABOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
include("${TABOS_ROOT}/config/Identity.cmake")
include("${TABOS_ROOT}/config/Display.cmake")
include("${TABOS_ROOT}/config/Input.cmake")
include("${TABOS_ROOT}/config/Console.cmake")
include("${TABOS_ROOT}/config/Loader.cmake")

set(TABOS_GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated")
set(TABOS_FONT_FILE "${TABOS_ROOT}/graphics/blueterm.f12")
set(TABOS_GENERATED_FONT_SOURCE "${CMAKE_BINARY_DIR}/generated/font_data.S")
file(SIZE "${TABOS_FONT_FILE}" TABOS_FONT_FILE_SIZE)
if(NOT TABOS_FONT_FILE_SIZE EQUAL 3072)
    message(FATAL_ERROR "blueterm.f12 must contain 256 glyphs of 12 bytes each")
endif()
configure_file(
    "${TABOS_ROOT}/graphics/font_data.S.in"
    "${TABOS_GENERATED_FONT_SOURCE}"
    @ONLY
)
configure_file(
    "${TABOS_ROOT}/config/identity.h.in"
    "${TABOS_GENERATED_INCLUDE_DIR}/tabos/config/identity.h"
    @ONLY
)
configure_file(
    "${TABOS_ROOT}/config/loader.h.in"
    "${TABOS_GENERATED_INCLUDE_DIR}/tabos/config/loader.h"
)
configure_file(
    "${TABOS_ROOT}/config/display.h.in"
    "${TABOS_GENERATED_INCLUDE_DIR}/tabos/config/display.h"
    @ONLY
)
configure_file(
    "${TABOS_ROOT}/config/input.h.in"
    "${TABOS_GENERATED_INCLUDE_DIR}/tabos/config/input.h"
)
configure_file(
    "${TABOS_ROOT}/config/console.h.in"
    "${TABOS_GENERATED_INCLUDE_DIR}/tabos/config/console.h"
)

set(EXTRA_COMPONENT_DIRS
    "${TABOS_ROOT}/kernel"
    "${TABOS_ROOT}/platform/esp32p4"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(${TABOS_CMAKE_PROJECT_NAME})

if(NOT IDF_VERSION_MAJOR EQUAL 5 OR
   NOT IDF_VERSION_MINOR EQUAL 4 OR
   NOT IDF_VERSION_PATCH EQUAL 4)
    message(FATAL_ERROR "${TABOS_SYSTEM_NAME} currently requires ESP-IDF v5.4.4")
endif()
