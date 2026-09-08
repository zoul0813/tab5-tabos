if(NOT DEFINED TABOS_SOURCE_DIR OR NOT DEFINED TABOS_TEST_DIR OR NOT DEFINED TABOS_C_COMPILER)
    message(FATAL_ERROR "application build tracking test requires source, test, and compiler paths")
endif()

find_program(TABOS_TEST_MAKE NAMES gmake make REQUIRED)
file(REMOVE_RECURSE "${TABOS_TEST_DIR}")
file(MAKE_DIRECTORY
    "${TABOS_TEST_DIR}/include"
    "${TABOS_TEST_DIR}/src"
    "${TABOS_TEST_DIR}/sdk/crt"
    "${TABOS_TEST_DIR}/sdk/include/tabos"
    "${TABOS_TEST_DIR}/sdk/lib"
    "${TABOS_TEST_DIR}/sdk/libc"
    "${TABOS_TEST_DIR}/sdk/linker"
)

set(runtime_sources
    crt/crt0.c
    crt/metadata.S
    libc/syscalls.c
    lib/process.c
    lib/graphics.c
    lib/sprite.c
    lib/tilemap.c
    lib/input.c
    lib/network.c
    lib/wait.c
    lib/camera.c
    lib/tls.c
    lib/battery.c
    lib/audio.c
    lib/pointer.c
    lib/clock.c
    lib/reboot.c
    lib/runtime.c
    lib/device.c
    lib/posix_filesystem.c
)
foreach(source IN LISTS runtime_sources)
    file(WRITE "${TABOS_TEST_DIR}/sdk/${source}" "")
endforeach()
file(WRITE "${TABOS_TEST_DIR}/sdk/linker/app-riscv32.ld" "")
file(WRITE "${TABOS_TEST_DIR}/sdk/include/tabos/public.h" "#define SDK_VALUE 1\n")
file(WRITE "${TABOS_TEST_DIR}/include/app_config.h" "#define APP_VALUE 1\n")
file(WRITE "${TABOS_TEST_DIR}/include/generated.h" "#define GENERATED_VALUE 1\n")
file(WRITE "${TABOS_TEST_DIR}/src/main.c" [=[
#include <tabos/public.h>
#include "app_config.h"
#include "generated.h"

int main(void)
{
    return SDK_VALUE + APP_VALUE + GENERATED_VALUE == 3 ? 0 : 1;
}
]=])

file(WRITE "${TABOS_TEST_DIR}/Makefile" [=[
APP_NAME := tracked
APP_DIR := .
SDK_ROOT := sdk
PROJECT_ROOT := .
BUILD_DIR := build
OUTPUT := $(BUILD_DIR)/tracked
UNSTRIPPED := $(BUILD_DIR)/tracked.elf
INSTALL_PATH := install/tracked
SOURCES := src/main.c
TABOS_BUILD_PREREQUISITES := include/generated.h
include ]=] "${TABOS_SOURCE_DIR}/sdk/make/application.mk\n")

file(WRITE "${TABOS_TEST_DIR}/compiler.sh" "#!/bin/sh\nset -eu\ncase \" $* \" in\n    *\" -MM \"*) ;;\n    *) printf 'link\\n' >> '${TABOS_TEST_DIR}/links.log' ;;\nesac\nexec '${TABOS_C_COMPILER}' \"$@\"\n")
file(WRITE "${TABOS_TEST_DIR}/strip.sh" "#!/bin/sh\nset -eu\ninput=\noutput=\nwhile [ \"$#\" -gt 0 ]; do\n    case \"$1\" in\n        --strip-unneeded) shift ;;\n        -o) output=$2; shift 2 ;;\n        *) input=$1; shift ;;\n    esac\ndone\ncp \"$input\" \"$output\"\n")
file(WRITE "${TABOS_TEST_DIR}/size.sh" "#!/bin/sh\nexit 0\n")
file(CHMOD
    "${TABOS_TEST_DIR}/compiler.sh"
    "${TABOS_TEST_DIR}/strip.sh"
    "${TABOS_TEST_DIR}/size.sh"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

function(run_build)
    execute_process(
        COMMAND
            "${TABOS_TEST_MAKE}" --no-print-directory build
            "CC=${TABOS_TEST_DIR}/compiler.sh"
            "STRIP=${TABOS_TEST_DIR}/strip.sh"
            "SIZE=${TABOS_TEST_DIR}/size.sh"
            "TABOS_CFLAGS=-std=c17"
            "TABOS_LDFLAGS="
            ${ARGN}
        WORKING_DIRECTORY "${TABOS_TEST_DIR}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "application build failed (${result}):\n${output}\n${error}")
    endif()
endfunction()

function(expect_links expected reason)
    file(STRINGS "${TABOS_TEST_DIR}/links.log" links)
    list(LENGTH links actual)
    if(NOT actual EQUAL expected)
        message(FATAL_ERROR "${reason}: expected ${expected} links, got ${actual}")
    endif()
endfunction()

run_build()
expect_links(1 "initial build")
run_build()
expect_links(1 "unchanged build")

execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1)
file(APPEND "${TABOS_TEST_DIR}/sdk/include/tabos/public.h" "/* changed */\n")
run_build()
expect_links(2 "SDK header change")

execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1)
file(APPEND "${TABOS_TEST_DIR}/include/app_config.h" "/* changed */\n")
run_build()
expect_links(3 "application header change")

execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1)
file(APPEND "${TABOS_TEST_DIR}/include/generated.h" "/* changed */\n")
run_build()
expect_links(4 "generated prerequisite change")

execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1)
file(APPEND "${TABOS_TEST_DIR}/Makefile" "# changed\n")
run_build()
expect_links(5 "application Makefile change")

run_build("TABOS_APP_HEAP_BYTES=524288")
expect_links(6 "heap setting change")
run_build("TABOS_APP_HEAP_BYTES=524288")
expect_links(6 "unchanged heap setting")
run_build("TABOS_APP_HEAP_BYTES=524288" "TABOS_APP_STACK_BYTES=32768")
expect_links(7 "stack setting change")
run_build("TABOS_APP_HEAP_BYTES=524288" "TABOS_APP_STACK_BYTES=32768" "TABOS_APP_ABI_VERSION=22")
expect_links(8 "ABI setting change")
run_build(
    "TABOS_APP_HEAP_BYTES=524288"
    "TABOS_APP_STACK_BYTES=32768"
    "TABOS_APP_ABI_VERSION=22"
    "TABOS_POINTER_MAX_CONTACTS=7"
)
expect_links(9 "pointer contact setting change")
run_build(
    "TABOS_APP_HEAP_BYTES=524288"
    "TABOS_APP_STACK_BYTES=32768"
    "TABOS_APP_ABI_VERSION=22"
    "TABOS_POINTER_MAX_CONTACTS=7"
    "TABOS_APP_CAPABILITIES=3"
)
expect_links(10 "capability setting change")
