set(TABOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
include("${TABOS_ROOT}/cmake/TabOSSources.cmake")

idf_component_register(
    SRCS ${TABOS_CORE_SOURCES}
    INCLUDE_DIRS
        "${TABOS_ROOT}/apps/diag/include"
        "${TABOS_ROOT}/audio/include"
        "${TABOS_ROOT}/camera/include"
        "${TABOS_ROOT}/console/include"
        "${TABOS_ROOT}/fs/include"
        "${TABOS_ROOT}/graphics/include"
        "${TABOS_ROOT}/input/include"
        "${TABOS_ROOT}/loader/include"
        "${TABOS_ROOT}/net/include"
        "${TABOS_ROOT}/pointer/include"
        "${CMAKE_CURRENT_LIST_DIR}/include"
        "${TABOS_ROOT}/sdk/include"
        "${TABOS_ROOT}/time/include"
    PRIV_INCLUDE_DIRS
        "${TABOS_ROOT}/apps/diag/console/include"
        "${TABOS_ROOT}/apps/diag/filesystem/include"
        "${TABOS_ROOT}/apps/diag/elf_loader/include"
        "${TABOS_ROOT}/platform/include"
        "${CMAKE_BINARY_DIR}/generated"
    PRIV_REQUIRES esp32p4
)
