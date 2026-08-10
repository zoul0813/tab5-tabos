set(TABOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
include("${TABOS_ROOT}/cmake/TabOSSources.cmake")

idf_component_register(
    SRCS ${TABOS_ESP32P4_PLATFORM_SOURCES}
    INCLUDE_DIRS "${TABOS_ROOT}/platform/include"
    PRIV_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/generated"
    REQUIRES log
)
