if(NOT DEFINED TABOS_SOURCE_DIR OR NOT DEFINED TABOS_BINARY_DIR)
    message(FATAL_ERROR "TABOS_SOURCE_DIR and TABOS_BINARY_DIR are required")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${TABOS_SOURCE_DIR}"
        -B "${TABOS_BINARY_DIR}"
        -DTABOS_TARGET=tab5
    RESULT_VARIABLE configure_result
    OUTPUT_QUIET
    ERROR_QUIET
)

if(configure_result EQUAL 0)
    message(FATAL_ERROR "Host CMake unexpectedly accepted TABOS_TARGET=tab5")
endif()
