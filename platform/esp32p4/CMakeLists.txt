set(TABOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
include("${TABOS_ROOT}/cmake/TabOSSources.cmake")

idf_component_register(
    SRCS ${TABOS_ESP32P4_PLATFORM_SOURCES}
    INCLUDE_DIRS
        "${TABOS_ROOT}/platform/include"
        "${CMAKE_CURRENT_LIST_DIR}/include"
    PRIV_INCLUDE_DIRS
        "${TABOS_ROOT}/input/include"
        "${TABOS_ROOT}/sdk/include"
        "${TABOS_ROOT}/time/include"
        "${CMAKE_BINARY_DIR}/generated"
    REQUIRES esp_codec_dev esp_driver_i2c esp_driver_i2s esp_driver_ppa esp_event esp_hosted esp_hw_support esp_lcd esp_lcd_st7121 esp_lcd_touch_gt911 esp_lcd_touch_st7123 esp_mm esp_netif esp_system esp_timer esp-tls esp_tinyusb esp_wifi esp_wifi_remote fatfs freertos log lwip m5stack_tab5_noglib sdmmc spi_flash vfs
)
