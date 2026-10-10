idf_component_register(
    SRCS "app_main.c" "msc_control.c"
    INCLUDE_DIRS ""
    PRIV_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/generated"
    REQUIRES kernel esp32p4 esp_driver_usb_serial_jtag
)

if(TABOS_ENABLE_DEVICE_TEST_CONTROL)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE TABOS_ENABLE_DEVICE_TEST_CONTROL=1)
endif()
