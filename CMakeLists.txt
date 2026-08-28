cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/config/Identity.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/config/Display.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/config/Font.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/config/Filesystem.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/config/Input.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/config/Console.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/config/Acceleration.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/config/Loader.cmake")
project(${TABOS_CMAKE_PROJECT_NAME} VERSION ${TABOS_PROJECT_VERSION} LANGUAGES C ASM)

set(TABOS_GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated")
set(TABOS_GENERATED_FONT_FILE "${CMAKE_BINARY_DIR}/generated/font.bin")
set(TABOS_GENERATED_FONT_SOURCE "${CMAKE_BINARY_DIR}/generated/font_data.S")
configure_file("${TABOS_FONT_FILE}" "${TABOS_GENERATED_FONT_FILE}" COPYONLY)
set(TABOS_FONT_EMBED_FILE "${TABOS_GENERATED_FONT_FILE}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/graphics/font_data.S.in"
    "${TABOS_GENERATED_FONT_SOURCE}"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/identity.h.in"
    "${TABOS_GENERATED_INCLUDE_DIR}/tabos/config/identity.h"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/font.h.in"
    "${TABOS_GENERATED_INCLUDE_DIR}/tabos/config/font.h"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/filesystem.h.in"
    "${TABOS_GENERATED_INCLUDE_DIR}/tabos/config/filesystem.h"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/loader.h.in"
    "${TABOS_GENERATED_INCLUDE_DIR}/tabos/config/loader.h"
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/display.h.in"
    "${TABOS_GENERATED_INCLUDE_DIR}/tabos/config/display.h"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/input.h.in"
    "${TABOS_GENERATED_INCLUDE_DIR}/tabos/config/input.h"
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/console.h.in"
    "${TABOS_GENERATED_INCLUDE_DIR}/tabos/config/console.h"
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/acceleration.h.in"
    "${TABOS_GENERATED_INCLUDE_DIR}/tabos/config/acceleration.h"
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

include(FetchContent)
FetchContent_Declare(
    mini_rv32ima
    GIT_REPOSITORY https://github.com/cnlohr/mini-rv32ima.git
    GIT_TAG 84858f58cb41899705e2ff2d6ee3b2d5c0795bfe
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(mini_rv32ima)

add_library(tabos_core STATIC ${TABOS_CORE_SOURCES})
target_include_directories(tabos_core
    PUBLIC "${TABOS_ROOT}/sdk/include"
    PRIVATE
        "${TABOS_ROOT}/apps/diag/include"
        "${TABOS_ROOT}/apps/diag/console/include"
        "${TABOS_ROOT}/apps/diag/filesystem/include"
        "${TABOS_ROOT}/apps/diag/elf_loader/include"
        "${TABOS_ROOT}/console/include"
        "${TABOS_ROOT}/fs/include"
        "${TABOS_ROOT}/graphics/include"
        "${TABOS_ROOT}/input/include"
        "${TABOS_ROOT}/kernel/include"
        "${TABOS_ROOT}/loader/include"
        "${TABOS_ROOT}/platform/include"
        "${TABOS_ROOT}/time/include"
        "${TABOS_GENERATED_INCLUDE_DIR}"
)
tabos_enable_warnings(tabos_core)
tabos_enable_sanitizers(tabos_core)

add_library(tabos_platform_host STATIC ${TABOS_HOST_PLATFORM_SOURCES})
# RV32 interpretation is the host application's execution engine. Keep this hot
# translation unit optimized even in debug builds; other host code retains the
# selected build type and sanitizer instrumentation.
set_property(SOURCE "${TABOS_ROOT}/platform/host/sdl/executable.c"
    APPEND PROPERTY COMPILE_OPTIONS -O3)
target_include_directories(tabos_platform_host
    PUBLIC "${TABOS_ROOT}/platform/include"
    PRIVATE
        "${TABOS_ROOT}/input/include"
        "${TABOS_ROOT}/sdk/include"
        "${TABOS_ROOT}/time/include"
        "${TABOS_GENERATED_INCLUDE_DIR}"
        "${mini_rv32ima_SOURCE_DIR}/mini-rv32ima"
)
target_link_libraries(tabos_platform_host PRIVATE tabos_core SDL3::SDL3)
target_compile_definitions(tabos_platform_host PRIVATE TABOS_TARGET_HOST=1)

if(TABOS_TARGET STREQUAL "macos")
    target_compile_definitions(tabos_platform_host PRIVATE TABOS_HOST_MACOS=1)
else()
    target_compile_definitions(tabos_platform_host PRIVATE
        TABOS_HOST_LINUX=1
        _POSIX_C_SOURCE=200809L
    )
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
