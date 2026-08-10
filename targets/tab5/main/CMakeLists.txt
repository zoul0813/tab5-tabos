idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS ""
    PRIV_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/generated"
    REQUIRES kernel esp32p4
)
