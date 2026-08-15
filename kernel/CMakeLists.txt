set(TABOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
include("${TABOS_ROOT}/cmake/TabOSSources.cmake")

idf_component_register(
    SRCS ${TABOS_CORE_SOURCES}
    INCLUDE_DIRS
        "${TABOS_ROOT}/apps/include"
        "${TABOS_ROOT}/console/include"
        "${TABOS_ROOT}/fs/include"
        "${TABOS_ROOT}/graphics/include"
        "${TABOS_ROOT}/input/include"
        "${TABOS_ROOT}/loader/include"
        "${CMAKE_CURRENT_LIST_DIR}/include"
        "${TABOS_ROOT}/sdk/include"
    PRIV_INCLUDE_DIRS
        "${TABOS_ROOT}/apps/console_diagnostic/include"
        "${TABOS_ROOT}/apps/filesystem_diagnostic/include"
        "${TABOS_ROOT}/platform/include"
        "${CMAKE_BINARY_DIR}/generated"
    PRIV_REQUIRES esp32p4
)
