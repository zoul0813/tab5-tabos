#include <tabos/platform/platform.h>

#include <tabos/config/identity.h>
#include <tabos/platform/display_transform.h>

#include <bsp/esp-bsp.h>
#include <driver/i2c_master.h>
#include <esp_cache.h>
#include <esp_heap_caps.h>
#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_st7121.h>
#include <esp_lcd_touch_gt911.h>
#include <esp_lcd_touch_st7123.h>
#include <esp_ldo_regulator.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <stddef.h>
#include <stdlib.h>

static const char *const TAG = TABOS_PLATFORM_LOG_TAG;

/* M5Stack's current ST712x Tab5 reference uses 965 Mbps. */
#define TABOS_TAB5_DSI_LANE_BITRATE_MBPS 965

static bsp_lcd_handles_t display_handles;
static esp_ldo_channel_handle_t display_phy_power;
static tab_pixel_t *logical_pixels;
static tab_pixel_t *native_pixels;
static bool display_created;
static bool display_uses_bsp;
static bool backlight_enabled;
static const char *detected_display_name = "unknown";

typedef enum {
    TAB5_PANEL_UNKNOWN = 0,
    TAB5_PANEL_ILI9881C,
    TAB5_PANEL_ST7121,
    TAB5_PANEL_ST7123,
} tab5_panel_type_t;

static tab5_panel_type_t tab5_detect_panel(void)
{
    esp_lcd_panel_io_handle_t touch_io = NULL;
    esp_lcd_panel_io_i2c_config_t touch_io_config = ESP_LCD_TOUCH_IO_I2C_ST7123_CONFIG();
    uint8_t firmware_version = 0;

    esp_err_t result = bsp_feature_enable(BSP_FEATURE_LCD, false);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Could not reset Tab5 LCD controller: %s", esp_err_to_name(result));
        return TAB5_PANEL_UNKNOWN;
    }
    result = bsp_feature_enable(BSP_FEATURE_TOUCH, false);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Could not reset Tab5 touch controller: %s", esp_err_to_name(result));
        return TAB5_PANEL_UNKNOWN;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    result = bsp_feature_enable(BSP_FEATURE_LCD, true);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Could not power Tab5 LCD controller: %s", esp_err_to_name(result));
        return TAB5_PANEL_UNKNOWN;
    }
    result = bsp_feature_enable(BSP_FEATURE_TOUCH, true);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Could not power Tab5 touch controller: %s", esp_err_to_name(result));
        return TAB5_PANEL_UNKNOWN;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    if (i2c_master_probe(
            bsp_i2c_get_handle(),
            ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP,
            100
        ) == ESP_OK) {
        detected_display_name = "ILI9881C";
        return TAB5_PANEL_ILI9881C;
    }
    if (i2c_master_probe(
            bsp_i2c_get_handle(),
            ESP_LCD_TOUCH_IO_I2C_ST7123_ADDRESS,
            100
        ) != ESP_OK) {
        ESP_LOGE(TAG, "Could not detect supported Tab5 panel touch controller");
        return TAB5_PANEL_UNKNOWN;
    }

    result = esp_lcd_new_panel_io_i2c(bsp_i2c_get_handle(), &touch_io_config, &touch_io);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Could not create Tab5 touch probe: %s", esp_err_to_name(result));
        return TAB5_PANEL_UNKNOWN;
    }

    const esp_err_t read_result = esp_lcd_panel_io_rx_param(
        touch_io,
        0x0000,
        &firmware_version,
        sizeof(firmware_version)
    );
    (void)esp_lcd_panel_io_del(touch_io);

    if (read_result != ESP_OK) {
        ESP_LOGW(TAG, "Could not read ST712x firmware version: %s", esp_err_to_name(read_result));
        detected_display_name = "ST7123 (assumed)";
        return TAB5_PANEL_ST7123;
    }
    if (firmware_version == 1) {
        detected_display_name = "ST7121";
        return TAB5_PANEL_ST7121;
    }
    if (firmware_version == 3) {
        detected_display_name = "ST7123";
        return TAB5_PANEL_ST7123;
    }

    ESP_LOGW(TAG, "Unknown ST712x touch firmware version %u; trying ST7123", firmware_version);
    detected_display_name = "ST7123 (assumed)";
    return TAB5_PANEL_ST7123;
}

static esp_err_t tab5_display_new_st7121(void)
{
    const esp_ldo_channel_config_t ldo_config = {
        .chan_id = BSP_MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv = BSP_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
    };
    esp_err_t result = esp_ldo_acquire_channel(&ldo_config, &display_phy_power);
    if (result != ESP_OK) {
        return result;
    }

    const esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = 2,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = TABOS_TAB5_DSI_LANE_BITRATE_MBPS,
    };
    result = esp_lcd_new_dsi_bus(&bus_config, &display_handles.mipi_dsi_bus);
    if (result != ESP_OK) {
        return result;
    }

    const esp_lcd_dbi_io_config_t dbi_config = ST7121_PANEL_IO_DBI_CONFIG();
    result = esp_lcd_new_panel_io_dbi(
        display_handles.mipi_dsi_bus,
        &dbi_config,
        &display_handles.io
    );
    if (result != ESP_OK) {
        return result;
    }

    const esp_lcd_dpi_panel_config_t dpi_config =
        ST7121_1280_720_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
    const st7121_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus = display_handles.mipi_dsi_bus,
            .dpi_config = &dpi_config,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = GPIO_NUM_NC,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,
        .bits_per_pixel = 24,
        .vendor_config = (void *)&vendor_config,
    };
    result = esp_lcd_new_panel_st7121(
        display_handles.io,
        &panel_config,
        &display_handles.panel
    );
    if (result != ESP_OK) {
        return result;
    }
    result = esp_lcd_panel_reset(display_handles.panel);
    if (result != ESP_OK) {
        return result;
    }
    result = esp_lcd_panel_init(display_handles.panel);
    if (result != ESP_OK) {
        return result;
    }
    result = esp_lcd_panel_disp_on_off(display_handles.panel, true);
    if (result != ESP_OK) {
        return result;
    }

    result = bsp_display_brightness_init();
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "ST7121 display initialized with native resolution %dx%d", BSP_LCD_H_RES, BSP_LCD_V_RES);
    }
    return result;
}

bool tab_platform_init(bool headless)
{
    (void)headless;
    return true;
}

int tab_platform_run(void)
{
    ESP_LOGI(TAG, "Tab5 platform run loop started");
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void tab_platform_shutdown(void)
{
    tab_platform_display_shutdown();
}

const char *tab_platform_name(void)
{
    return TABOS_TARGET_NAME_TAB5;
}

const char *tab_platform_display_name(void)
{
    return detected_display_name;
}

bool tab_platform_display_init(tab_framebuffer_t *framebuffer)
{
    if (framebuffer == NULL) {
        return false;
    }

    const tab5_panel_type_t panel_type = tab5_detect_panel();
    if (panel_type == TAB5_PANEL_UNKNOWN) {
        return false;
    }
    ESP_LOGI(TAG, "Detected display: %s", tab_platform_display_name());

    const size_t pixel_count = (size_t)TABOS_DISPLAY_WIDTH * TABOS_DISPLAY_HEIGHT;
    logical_pixels = heap_caps_calloc(pixel_count, sizeof(*logical_pixels), MALLOC_CAP_SPIRAM);
    if (logical_pixels == NULL) {
        ESP_LOGE(TAG, "Could not allocate logical display buffer in PSRAM");
        tab_platform_display_shutdown();
        return false;
    }

    esp_err_t create_result;
    if (panel_type == TAB5_PANEL_ST7121) {
        display_uses_bsp = false;
        display_created = true;
        create_result = tab5_display_new_st7121();
    } else {
        const bsp_display_config_t config = {
            .dsi_bus = {
                .phy_clk_src = 0,
                .lane_bit_rate_mbps = panel_type == TAB5_PANEL_ILI9881C
                    ? BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS
                    : TABOS_TAB5_DSI_LANE_BITRATE_MBPS,
            },
        };
        display_uses_bsp = true;
        create_result = bsp_display_new_with_handles(&config, &display_handles);
        display_created = create_result == ESP_OK;
    }
    if (create_result != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize Tab5 display: %s", esp_err_to_name(create_result));
        tab_platform_display_shutdown();
        return false;
    }
    const esp_err_t framebuffer_result = esp_lcd_dpi_panel_get_frame_buffer(
        display_handles.panel,
        1,
        (void **)&native_pixels
    );
    if (framebuffer_result != ESP_OK || native_pixels == NULL) {
        ESP_LOGE(
            TAG,
            "Could not access Tab5 scanout framebuffer: %s",
            esp_err_to_name(framebuffer_result)
        );
        tab_platform_display_shutdown();
        return false;
    }

    const esp_err_t panel_result = esp_lcd_panel_disp_on_off(display_handles.panel, true);
    if (panel_result != ESP_OK) {
        ESP_LOGE(TAG, "Could not enable Tab5 display panel: %s", esp_err_to_name(panel_result));
        tab_platform_display_shutdown();
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    *framebuffer = (tab_framebuffer_t){
        .pixels = logical_pixels,
        .width = TABOS_DISPLAY_WIDTH,
        .height = TABOS_DISPLAY_HEIGHT,
        .stride_pixels = TABOS_DISPLAY_WIDTH,
    };
    ESP_LOGI(TAG, "Tab5 display initialized at %dx%d RGB565", TABOS_DISPLAY_WIDTH, TABOS_DISPLAY_HEIGHT);
    return true;
}

bool tab_platform_display_present(const tab_framebuffer_t *framebuffer)
{
    if (!display_created || framebuffer == NULL || framebuffer->pixels != logical_pixels) {
        return false;
    }

    if (!tab_framebuffer_rotate_counter_clockwise(
            framebuffer,
            native_pixels,
            TABOS_DISPLAY_HEIGHT,
            TABOS_DISPLAY_WIDTH
        )) {
        ESP_LOGE(TAG, "Could not rotate Tab5 framebuffer");
        return false;
    }

    const size_t buffer_size =
        (size_t)TABOS_DISPLAY_WIDTH * TABOS_DISPLAY_HEIGHT * sizeof(*native_pixels);
    const esp_err_t sync_result = esp_cache_msync(
        native_pixels,
        buffer_size,
        ESP_CACHE_MSYNC_FLAG_DIR_C2M
    );
    if (sync_result != ESP_OK) {
        ESP_LOGE(TAG, "Could not synchronize Tab5 framebuffer: %s", esp_err_to_name(sync_result));
        return false;
    }

    const esp_err_t draw_result = esp_lcd_panel_draw_bitmap(
        display_handles.panel,
        0,
        0,
        BSP_LCD_H_RES,
        BSP_LCD_V_RES,
        native_pixels
    );
    if (draw_result != ESP_OK) {
        ESP_LOGE(TAG, "Could not present Tab5 framebuffer: %s", esp_err_to_name(draw_result));
        return false;
    }

    if (!backlight_enabled) {
        const esp_err_t backlight_result = bsp_display_backlight_on();
        if (backlight_result != ESP_OK) {
            ESP_LOGE(TAG, "Could not enable Tab5 backlight: %s", esp_err_to_name(backlight_result));
            return false;
        }
        backlight_enabled = true;
    }

    return true;
}

void tab_platform_display_shutdown(void)
{
    if (backlight_enabled) {
        (void)bsp_display_backlight_off();
        backlight_enabled = false;
    }

    if (display_created) {
        if (display_uses_bsp) {
            bsp_display_delete();
        } else {
            if (display_handles.panel != NULL) {
                (void)esp_lcd_panel_del(display_handles.panel);
            }
            if (display_handles.io != NULL) {
                (void)esp_lcd_panel_io_del(display_handles.io);
            }
            if (display_handles.mipi_dsi_bus != NULL) {
                (void)esp_lcd_del_dsi_bus(display_handles.mipi_dsi_bus);
            }
            if (display_phy_power != NULL) {
                (void)esp_ldo_release_channel(display_phy_power);
                display_phy_power = NULL;
            }
            (void)bsp_display_brightness_deinit();
        }
        display_handles = (bsp_lcd_handles_t){0};
        display_created = false;
        display_uses_bsp = false;
    }

    native_pixels = NULL;
    free(logical_pixels);
    logical_pixels = NULL;
}
