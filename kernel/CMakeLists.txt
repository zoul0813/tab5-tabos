set(TABOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
include("${TABOS_ROOT}/cmake/TabOSSources.cmake")

idf_component_register(
    SRCS ${TABOS_CORE_SOURCES}
    INCLUDE_DIRS
        "${TABOS_ROOT}/graphics/include"
        "${CMAKE_CURRENT_LIST_DIR}/include"
        "${TABOS_ROOT}/sdk/include"
    PRIV_INCLUDE_DIRS
        "${TABOS_ROOT}/platform/include"
        "${CMAKE_BINARY_DIR}/generated"
    PRIV_REQUIRES esp32p4
)
