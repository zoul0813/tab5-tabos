cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/config/Identity.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/config/Display.cmake")
project(${TABOS_CMAKE_PROJECT_NAME} VERSION ${TABOS_PROJECT_VERSION} LANGUAGES C)

set(TABOS_GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/identity.h.in"
    "${TABOS_GENERATED_INCLUDE_DIR}/tabos/config/identity.h"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/display.h.in"
    "${TABOS_GENERATED_INCLUDE_DIR}/tabos/config/display.h"
    @ONLY
)

set(TABOS_TARGET "" CACHE STRING "${TABOS_SYSTEM_NAME} target platform (macos or linux)")
set_property(CACHE TABOS_TARGET PROPERTY STRINGS macos linux)

if(NOT TABOS_TARGET MATCHES "^(macos|linux)$")
    message(FATAL_ERROR "TABOS_TARGET must be either 'macos' or 'linux' for the host build")
endif()

if(TABOS_TARGET STREQUAL "macos" AND NOT APPLE)
    message(FATAL_ERROR "The macos target must be configured on macOS")
endif()

if(TABOS_TARGET STREQUAL "linux" AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "The linux target must be configured on Linux")
endif()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TABOS_ENABLE_SANITIZERS "Enable host sanitizers" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)

set(TABOS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
include("${TABOS_ROOT}/cmake/TabOSSources.cmake")

find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3)

add_library(tabos_core STATIC ${TABOS_CORE_SOURCES})
target_include_directories(tabos_core
    PUBLIC "${TABOS_ROOT}/sdk/include"
    PRIVATE
        "${TABOS_ROOT}/graphics/include"
        "${TABOS_ROOT}/kernel/include"
        "${TABOS_ROOT}/platform/include"
        "${TABOS_GENERATED_INCLUDE_DIR}"
)
tabos_enable_warnings(tabos_core)
tabos_enable_sanitizers(tabos_core)

add_library(tabos_platform_host STATIC ${TABOS_HOST_PLATFORM_SOURCES})
target_include_directories(tabos_platform_host
    PUBLIC "${TABOS_ROOT}/platform/include"
    PRIVATE "${TABOS_GENERATED_INCLUDE_DIR}"
)
target_link_libraries(tabos_platform_host PRIVATE SDL3::SDL3)
target_compile_definitions(tabos_platform_host PRIVATE TABOS_TARGET_HOST=1)

if(TABOS_TARGET STREQUAL "macos")
    target_compile_definitions(tabos_platform_host PRIVATE TABOS_HOST_MACOS=1)
else()
    target_compile_definitions(tabos_platform_host PRIVATE TABOS_HOST_LINUX=1)
endif()

tabos_enable_warnings(tabos_platform_host)
tabos_enable_sanitizers(tabos_platform_host)

add_executable(tabos_host targets/host/main.c)
target_include_directories(tabos_host PRIVATE
    "${TABOS_ROOT}/graphics/include"
    "${TABOS_ROOT}/kernel/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_GENERATED_INCLUDE_DIR}"
)
target_link_libraries(tabos_host PRIVATE tabos_core tabos_platform_host SDL3::SDL3)
tabos_enable_warnings(tabos_host)
tabos_enable_sanitizers(tabos_host)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
