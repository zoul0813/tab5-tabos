set(TABOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
include("${TABOS_ROOT}/cmake/TabOSSources.cmake")

idf_component_register(
    SRCS ${TABOS_CORE_SOURCES}
    INCLUDE_DIRS
        "${CMAKE_CURRENT_LIST_DIR}/include"
        "${TABOS_ROOT}/sdk/include"
    PRIV_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/generated"
)
