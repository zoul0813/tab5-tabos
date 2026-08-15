set(TABOS_HOST_ROOTFS "${CMAKE_CURRENT_LIST_DIR}/../.local/rootfs" CACHE PATH
    "Controlled host directory exposed inside TabOS as /")

option(
    TABOS_ENABLE_FILESYSTEM_DIAGNOSTIC_APP
    "Run filesystem read/write diagnostic application after boot"
    OFF
)
