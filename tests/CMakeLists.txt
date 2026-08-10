add_executable(tabos_core_smoke unit/core_smoke.c fakes/platform_display.c)
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

add_executable(tabos_display_diagnostic unit/display_diagnostic.c fakes/platform_display.c)
target_include_directories(tabos_display_diagnostic PRIVATE
    "${TABOS_ROOT}/graphics/include"
    "${TABOS_ROOT}/kernel/include"
    "${TABOS_ROOT}/platform/include"
)
target_link_libraries(tabos_display_diagnostic PRIVATE tabos_core)
tabos_enable_warnings(tabos_display_diagnostic)
tabos_enable_sanitizers(tabos_display_diagnostic)

add_test(NAME unit.display_diagnostic COMMAND tabos_display_diagnostic)

add_executable(tabos_input_queue unit/input_queue.c fakes/platform_display.c)
target_include_directories(tabos_input_queue PRIVATE
    "${TABOS_ROOT}/input/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
)
target_link_libraries(tabos_input_queue PRIVATE tabos_core)
tabos_enable_warnings(tabos_input_queue)
tabos_enable_sanitizers(tabos_input_queue)

add_test(NAME unit.input_queue COMMAND tabos_input_queue)

add_executable(tabos_console_service unit/console_service.c fakes/platform_display.c)
target_include_directories(tabos_console_service PRIVATE
    "${TABOS_ROOT}/console/include"
    "${TABOS_ROOT}/graphics/include"
    "${TABOS_ROOT}/input/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_console_service PRIVATE tabos_core)
tabos_enable_warnings(tabos_console_service)
tabos_enable_sanitizers(tabos_console_service)

add_test(NAME unit.console_service COMMAND tabos_console_service)

add_executable(tabos_timer unit/timer.c fakes/platform_display.c)
target_include_directories(tabos_timer PRIVATE
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_timer PRIVATE tabos_core)
tabos_enable_warnings(tabos_timer)
tabos_enable_sanitizers(tabos_timer)

add_test(NAME unit.timer COMMAND tabos_timer)

add_executable(tabos_application_lifecycle unit/application_lifecycle.c fakes/platform_display.c)
target_include_directories(tabos_application_lifecycle PRIVATE
    "${TABOS_ROOT}/console/include"
    "${TABOS_ROOT}/graphics/include"
    "${TABOS_ROOT}/input/include"
    "${TABOS_ROOT}/kernel/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
)
target_link_libraries(tabos_application_lifecycle PRIVATE tabos_core)
tabos_enable_warnings(tabos_application_lifecycle)
tabos_enable_sanitizers(tabos_application_lifecycle)

add_test(NAME unit.application_lifecycle COMMAND tabos_application_lifecycle)

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
    NAME build.invalid_host_target
    COMMAND "${CMAKE_COMMAND}"
        "-DTABOS_SOURCE_DIR=${TABOS_ROOT}"
        "-DTABOS_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/invalid-target"
        -P "${CMAKE_CURRENT_LIST_DIR}/check_invalid_target.cmake"
)
