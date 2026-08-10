set(TABOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
include("${TABOS_ROOT}/cmake/TabOSSources.cmake")

idf_component_register(
    SRCS ${TABOS_ESP32P4_PLATFORM_SOURCES}
    INCLUDE_DIRS "${TABOS_ROOT}/platform/include"
    PRIV_INCLUDE_DIRS
        "${TABOS_ROOT}/input/include"
        "${TABOS_ROOT}/sdk/include"
        "${CMAKE_BINARY_DIR}/generated"
    REQUIRES esp_driver_i2c esp_hw_support esp_lcd esp_lcd_st7121 esp_lcd_touch_gt911 esp_lcd_touch_st7123 esp_mm esp_timer log m5stack_tab5_noglib spi_flash
)
