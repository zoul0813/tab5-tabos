cmake_minimum_required(VERSION 3.16)

set(TABOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
include("${TABOS_ROOT}/config/Identity.cmake")
include("${TABOS_ROOT}/config/Display.cmake")
include("${TABOS_ROOT}/config/Input.cmake")
include("${TABOS_ROOT}/config/Console.cmake")

set(TABOS_GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated")
configure_file(
    "${TABOS_ROOT}/config/identity.h.in"
    "${TABOS_GENERATED_INCLUDE_DIR}/tabos/config/identity.h"
    @ONLY
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
