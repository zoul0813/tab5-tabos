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

add_executable(tabos_raster unit/raster.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_raster PRIVATE
    "${TABOS_ROOT}/graphics/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
)
target_link_libraries(tabos_raster PRIVATE tabos_core)
tabos_enable_warnings(tabos_raster)
tabos_enable_sanitizers(tabos_raster)
add_test(NAME unit.raster COMMAND tabos_raster)

add_executable(tabos_sdk_graphics
    unit/sdk_graphics.c
    "${TABOS_ROOT}/sdk/lib/graphics.c"
)
target_include_directories(tabos_sdk_graphics PRIVATE "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_sdk_graphics)
tabos_enable_sanitizers(tabos_sdk_graphics)
add_test(NAME unit.sdk_graphics COMMAND tabos_sdk_graphics)

add_executable(tabos_sdk_input
    unit/sdk_input.c
    "${TABOS_ROOT}/sdk/lib/input.c"
)
target_include_directories(tabos_sdk_input PRIVATE "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_sdk_input)
tabos_enable_sanitizers(tabos_sdk_input)
add_test(NAME unit.sdk_input COMMAND tabos_sdk_input)

add_executable(tabos_sdk_reboot
    unit/sdk_reboot.c
    "${TABOS_ROOT}/sdk/lib/reboot.c"
)
target_include_directories(tabos_sdk_reboot PRIVATE "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_sdk_reboot)
tabos_enable_sanitizers(tabos_sdk_reboot)
add_test(NAME unit.sdk_reboot COMMAND tabos_sdk_reboot)

add_executable(tabos_sdk_network
    unit/sdk_network.c
    "${TABOS_ROOT}/sdk/lib/network.c"
    "${TABOS_ROOT}/sdk/lib/wait.c"
)
target_include_directories(tabos_sdk_network PRIVATE "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_sdk_network)
tabos_enable_sanitizers(tabos_sdk_network)
add_test(NAME unit.sdk_network COMMAND tabos_sdk_network)

add_executable(tabos_sdk_tls
    unit/sdk_tls.c
    "${TABOS_ROOT}/sdk/lib/tls.c"
)
target_include_directories(tabos_sdk_tls PRIVATE "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_sdk_tls)
tabos_enable_sanitizers(tabos_sdk_tls)
add_test(NAME unit.sdk_tls COMMAND tabos_sdk_tls)

add_executable(tabos_starfall_game
    unit/starfall_game.c
    "${TABOS_ROOT}/apps/starfall/src/game.c"
)
target_include_directories(tabos_starfall_game PRIVATE
    "${TABOS_ROOT}/apps/starfall/include"
)
tabos_enable_warnings(tabos_starfall_game)
tabos_enable_sanitizers(tabos_starfall_game)
add_test(NAME unit.starfall_game COMMAND tabos_starfall_game)

add_executable(tabos_doom_video
    unit/doom_video.c
    "${TABOS_ROOT}/apps/doom/src/doom_tabos_video.c"
)
target_include_directories(tabos_doom_video PRIVATE
    "${TABOS_ROOT}/apps/doom/include"
    "${TABOS_ROOT}/sdk/include"
)
tabos_enable_warnings(tabos_doom_video)
tabos_enable_sanitizers(tabos_doom_video)
add_test(NAME unit.doom_video COMMAND tabos_doom_video)

add_executable(tabos_doom_input
    unit/doom_input.c
    "${TABOS_ROOT}/apps/doom/src/doom_tabos_input.c"
)
target_include_directories(tabos_doom_input PRIVATE
    "${TABOS_ROOT}/apps/doom/include"
    "${TABOS_ROOT}/sdk/include"
)
tabos_enable_warnings(tabos_doom_input)
tabos_enable_sanitizers(tabos_doom_input)
add_test(NAME unit.doom_input COMMAND tabos_doom_input)

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
    "${TABOS_GENERATED_INCLUDE_DIR}"
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

add_executable(tabos_terminal_ansi unit/terminal_ansi.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_terminal_ansi PRIVATE
    "${TABOS_ROOT}/graphics/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_GENERATED_INCLUDE_DIR}"
)
target_link_libraries(tabos_terminal_ansi PRIVATE tabos_core)
tabos_enable_warnings(tabos_terminal_ansi)
tabos_enable_sanitizers(tabos_terminal_ansi)

add_test(NAME unit.terminal_ansi COMMAND tabos_terminal_ansi)

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

add_executable(tabos_wall_clock unit/wall_clock.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_wall_clock PRIVATE
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${TABOS_ROOT}/time/include"
)
target_link_libraries(tabos_wall_clock PRIVATE tabos_core)
tabos_enable_warnings(tabos_wall_clock)
tabos_enable_sanitizers(tabos_wall_clock)
add_test(NAME unit.wall_clock COMMAND tabos_wall_clock)

add_executable(tabos_network_config unit/network_config.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_network_config PRIVATE
    "${TABOS_ROOT}/net/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_network_config PRIVATE tabos_core)
tabos_enable_warnings(tabos_network_config)
tabos_enable_sanitizers(tabos_network_config)
add_test(NAME unit.network_config COMMAND tabos_network_config)

add_executable(tabos_network_service unit/network_service.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_network_service PRIVATE
    "${TABOS_ROOT}/fs/include"
    "${TABOS_ROOT}/net/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_network_service PRIVATE tabos_core)
tabos_enable_warnings(tabos_network_service)
tabos_enable_sanitizers(tabos_network_service)
add_test(NAME unit.network_service COMMAND tabos_network_service)

add_executable(tabos_shell_parser
    unit/shell_parser.c
    "${TABOS_ROOT}/apps/shell/src/parser.c"
)
target_include_directories(tabos_shell_parser PRIVATE
    "${TABOS_ROOT}/apps/shell/include"
)
tabos_enable_warnings(tabos_shell_parser)
tabos_enable_sanitizers(tabos_shell_parser)

add_test(NAME unit.shell_parser COMMAND tabos_shell_parser)

add_executable(tabos_shell_input
    unit/shell_input.c
    "${TABOS_ROOT}/apps/shell/src/input.c"
)
target_include_directories(tabos_shell_input PRIVATE
    "${TABOS_ROOT}/apps/shell/include"
)
tabos_enable_warnings(tabos_shell_input)
tabos_enable_sanitizers(tabos_shell_input)

add_test(NAME unit.shell_input COMMAND tabos_shell_input)

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
    "${CMAKE_CURRENT_LIST_DIR}/fixtures/elf/hello_elf.c"
    ${TABOS_TEST_PLATFORM_FAKES}
)
target_include_directories(tabos_elf_loader PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/fixtures/elf"
    "${TABOS_ROOT}/loader/include"
    "${TABOS_ROOT}/platform/include"
)
target_link_libraries(tabos_elf_loader PRIVATE tabos_core)
tabos_enable_warnings(tabos_elf_loader)
tabos_enable_sanitizers(tabos_elf_loader)

add_test(NAME unit.elf_loader COMMAND tabos_elf_loader)

add_executable(tabos_file_elf_loader
    component/file_elf_loader.c
    "${CMAKE_CURRENT_LIST_DIR}/fixtures/elf/hello_elf.c"
    fakes/platform_display.c
    fakes/storage_backend.c
    "${TABOS_ROOT}/platform/posix/storage.c"
)
target_include_directories(tabos_file_elf_loader PRIVATE
    "${TABOS_ROOT}/fs/include"
    "${CMAKE_CURRENT_LIST_DIR}/fixtures/elf"
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
    "${CMAKE_CURRENT_LIST_DIR}/fixtures/elf/hello_elf.c"
)
target_include_directories(tabos_rv32_execution PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/fixtures/elf"
    "${TABOS_ROOT}/loader/include"
    "${TABOS_ROOT}/platform/include"
)
target_link_libraries(tabos_rv32_execution PRIVATE
    tabos_core tabos_platform_host SDL3::SDL3
)
tabos_enable_warnings(tabos_rv32_execution)
tabos_enable_sanitizers(tabos_rv32_execution)

add_test(NAME component.rv32_execution COMMAND tabos_rv32_execution)

add_executable(tabos_network_socket
    component/network_socket.c
    "${TABOS_ROOT}/platform/posix/socket.c"
)
find_package(Threads REQUIRED)
target_link_libraries(tabos_network_socket PRIVATE Threads::Threads)
target_include_directories(tabos_network_socket PRIVATE
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
)
tabos_enable_warnings(tabos_network_socket)
tabos_enable_sanitizers(tabos_network_socket)
add_test(NAME component.network_socket COMMAND tabos_network_socket)

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
    NAME architecture.shell_api_boundary
    COMMAND "${CMAKE_COMMAND}"
        "-DTABOS_SOURCE_DIR=${TABOS_ROOT}"
        -P "${CMAKE_CURRENT_LIST_DIR}/check_shell_boundary.cmake"
)

add_test(
    NAME architecture.application_api_boundary
    COMMAND "${CMAKE_COMMAND}"
        "-DTABOS_SOURCE_DIR=${TABOS_ROOT}"
        -P "${CMAKE_CURRENT_LIST_DIR}/check_application_api_boundary.cmake"
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
