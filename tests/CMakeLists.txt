set(TABOS_TEST_PLATFORM_FAKES fakes/platform_display.c fakes/platform_storage.c)

add_executable(tabos_core_smoke unit/core_smoke.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_core_smoke PRIVATE
    "${TABOS_ROOT}/graphics/include"
    "${TABOS_ROOT}/input/include"
    "${TABOS_ROOT}/kernel/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_GENERATED_INCLUDE_DIR}"
)
target_link_libraries(tabos_core_smoke PRIVATE tabos_core)
tabos_enable_warnings(tabos_core_smoke)
tabos_enable_sanitizers(tabos_core_smoke)

add_test(NAME unit.core_smoke COMMAND tabos_core_smoke)

add_executable(tabos_display_diagnostic unit/display_diagnostic.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_display_diagnostic PRIVATE
    "${TABOS_ROOT}/graphics/include"
    "${TABOS_ROOT}/kernel/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_GENERATED_INCLUDE_DIR}"
)
target_link_libraries(tabos_display_diagnostic PRIVATE tabos_core)
tabos_enable_warnings(tabos_display_diagnostic)
tabos_enable_sanitizers(tabos_display_diagnostic)

add_test(NAME unit.display_diagnostic COMMAND tabos_display_diagnostic)

add_executable(tabos_input_queue unit/input_queue.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_input_queue PRIVATE
    "${TABOS_ROOT}/input/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_input_queue PRIVATE tabos_core)
tabos_enable_warnings(tabos_input_queue)
tabos_enable_sanitizers(tabos_input_queue)

add_test(NAME unit.input_queue COMMAND tabos_input_queue)

add_executable(tabos_console_service unit/console_service.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_console_service PRIVATE
    "${TABOS_ROOT}/console/include"
    "${TABOS_ROOT}/graphics/include"
    "${TABOS_ROOT}/input/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
    "${TABOS_GENERATED_INCLUDE_DIR}"
)
target_link_libraries(tabos_console_service PRIVATE tabos_core)
tabos_enable_warnings(tabos_console_service)
tabos_enable_sanitizers(tabos_console_service)

add_test(NAME unit.console_service COMMAND tabos_console_service)

add_executable(tabos_timer unit/timer.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_timer PRIVATE
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_timer PRIVATE tabos_core)
tabos_enable_warnings(tabos_timer)
tabos_enable_sanitizers(tabos_timer)

add_test(NAME unit.timer COMMAND tabos_timer)

add_executable(tabos_application_lifecycle unit/application_lifecycle.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_application_lifecycle PRIVATE
    "${TABOS_ROOT}/console/include"
    "${TABOS_ROOT}/graphics/include"
    "${TABOS_ROOT}/input/include"
    "${TABOS_ROOT}/kernel/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_application_lifecycle PRIVATE tabos_core)
tabos_enable_warnings(tabos_application_lifecycle)
tabos_enable_sanitizers(tabos_application_lifecycle)

add_test(NAME unit.application_lifecycle COMMAND tabos_application_lifecycle)

add_executable(tabos_elf_loader
    unit/elf_loader.c
    "${TABOS_ROOT}/loader/fixtures/hello_elf.c"
    ${TABOS_TEST_PLATFORM_FAKES}
)
target_include_directories(tabos_elf_loader PRIVATE
    "${TABOS_ROOT}/loader/fixtures"
    "${TABOS_ROOT}/loader/include"
    "${TABOS_ROOT}/platform/include"
)
target_link_libraries(tabos_elf_loader PRIVATE tabos_core)
tabos_enable_warnings(tabos_elf_loader)
tabos_enable_sanitizers(tabos_elf_loader)

add_test(NAME unit.elf_loader COMMAND tabos_elf_loader)

add_executable(tabos_file_elf_loader
    component/file_elf_loader.c
    "${TABOS_ROOT}/loader/fixtures/hello_elf.c"
    fakes/platform_display.c
    fakes/storage_backend.c
    "${TABOS_ROOT}/platform/posix/storage.c"
)
target_include_directories(tabos_file_elf_loader PRIVATE
    "${TABOS_ROOT}/fs/include"
    "${TABOS_ROOT}/loader/fixtures"
    "${TABOS_ROOT}/loader/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_file_elf_loader PRIVATE tabos_core)
tabos_enable_warnings(tabos_file_elf_loader)
tabos_enable_sanitizers(tabos_file_elf_loader)

add_test(NAME component.file_elf_loader COMMAND tabos_file_elf_loader)

add_executable(tabos_rv32_execution
    component/rv32_execution.c
    "${TABOS_ROOT}/loader/fixtures/hello_elf.c"
)
target_include_directories(tabos_rv32_execution PRIVATE
    "${TABOS_ROOT}/loader/fixtures"
    "${TABOS_ROOT}/loader/include"
    "${TABOS_ROOT}/platform/include"
)
target_link_libraries(tabos_rv32_execution PRIVATE
    tabos_core tabos_platform_host SDL3::SDL3
)
tabos_enable_warnings(tabos_rv32_execution)
tabos_enable_sanitizers(tabos_rv32_execution)

add_test(NAME component.rv32_execution COMMAND tabos_rv32_execution)

add_executable(tabos_filesystem
    component/filesystem.c
    fakes/storage_backend.c
    "${TABOS_ROOT}/platform/posix/storage.c"
)
target_include_directories(tabos_filesystem PRIVATE
    "${TABOS_ROOT}/fs/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_filesystem PRIVATE tabos_core)
tabos_enable_warnings(tabos_filesystem)
tabos_enable_sanitizers(tabos_filesystem)

add_test(NAME component.filesystem COMMAND tabos_filesystem)

add_library(tabos_posix_source OBJECT
    component/posix_filesystem.c
)
target_include_directories(tabos_posix_source PRIVATE
    "${TABOS_ROOT}/sdk/posix/include"
    "${TABOS_ROOT}/fs/include"
    "${TABOS_ROOT}/sdk/include"
)
tabos_enable_warnings(tabos_posix_source)
tabos_enable_sanitizers(tabos_posix_source)

add_executable(tabos_posix_filesystem
    $<TARGET_OBJECTS:tabos_posix_source>
    fakes/storage_backend.c
    "${TABOS_ROOT}/platform/posix/storage.c"
)
target_include_directories(tabos_posix_filesystem PRIVATE
    "${TABOS_ROOT}/fs/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_posix_filesystem PRIVATE tabos_core)
tabos_enable_warnings(tabos_posix_filesystem)
tabos_enable_sanitizers(tabos_posix_filesystem)

add_test(NAME component.posix_filesystem COMMAND tabos_posix_filesystem)

add_test(NAME integration.host_smoke COMMAND tabos_host --smoke)
set_tests_properties(integration.host_smoke PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
)

add_test(
    NAME architecture.portable_header_boundary
    COMMAND "${CMAKE_COMMAND}"
        "-DTABOS_SOURCE_DIR=${TABOS_ROOT}"
        -P "${CMAKE_CURRENT_LIST_DIR}/check_boundaries.cmake"
)

add_test(
    NAME architecture.symbol_naming
    COMMAND "${CMAKE_COMMAND}"
        "-DTABOS_SOURCE_DIR=${TABOS_ROOT}"
        -P "${CMAKE_CURRENT_LIST_DIR}/check_naming.cmake"
)

add_test(
    NAME build.invalid_host_target
    COMMAND "${CMAKE_COMMAND}"
        "-DTABOS_SOURCE_DIR=${TABOS_ROOT}"
        "-DTABOS_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/invalid-target"
        -P "${CMAKE_CURRENT_LIST_DIR}/check_invalid_target.cmake"
)
