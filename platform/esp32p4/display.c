#include <tabos/platform/platform.h>

#include <tabos/config/identity.h>
#include <tabos/platform/display_transform.h>

#include <bsp/esp-bsp.h>
#include <driver/i2c_master.h>
#include <driver/ppa.h>
#include "include/pie.h"
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
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <stdlib.h>
#include <string.h>

#define TAB5_DSI_LANE_BITRATE_MBPS 965

#define TAB5_PPA_BLIT_MIN_PIXELS (16U * 1024U)

static const char *const TAG = TABOS_PLATFORM_LOG_TAG;
static bsp_lcd_handles_t display_handles;
static esp_ldo_channel_handle_t display_phy_power;
static platform_pixel_t *logical_pixels;
static platform_pixel_t *native_pixels;
static platform_pixel_t *native_front_pixels;
static platform_pixel_t *native_framebuffers[2];
static bool display_created;
static bool display_uses_bsp;
static bool backlight_enabled;
static const char *detected_display_name = "unknown";
static ppa_client_handle_t ppa_srm_client;
static ppa_client_handle_t ppa_fill_client;
static ppa_client_handle_t ppa_blend_client;
static SemaphoreHandle_t ppa_done;
static SemaphoreHandle_t vsync_done;
static bool ppa_ready;
static platform_pixel_t *ppa_scratch;
static bool direct_graphics_active;
static bool direct_frame_prepared;
static bool direct_frame_dirty;

static bool IRAM_ATTR display_refresh_done(esp_lcd_panel_handle_t panel,
                                           esp_lcd_dpi_panel_event_data_t *event_data,
                                           void *user_data)
{
    (void)panel;
    (void)event_data;
    BaseType_t task_woken = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)user_data, &task_woken);
    return task_woken == pdTRUE;
}

static bool wait_for_vsync(void)
{
    return vsync_done != NULL && xSemaphoreTake(vsync_done, portMAX_DELAY) == pdTRUE;
}

static bool submit_native_frame(void)
{
    while (xSemaphoreTake(vsync_done, 0) == pdTRUE) {}
    const esp_err_t result = esp_lcd_panel_draw_bitmap(
        display_handles.panel, 0, 0, BSP_LCD_H_RES, BSP_LCD_V_RES, native_pixels);
    if (result != ESP_OK || !wait_for_vsync()) return false;
    platform_pixel_t *presented = native_pixels;
    native_pixels = native_front_pixels;
    native_front_pixels = presented;
    return true;
}

static bool ppa_transaction_done(ppa_client_handle_t client,
                                 ppa_event_data_t *event_data, void *user_data)
{
    (void)client;
    (void)event_data;
    BaseType_t task_woken = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)user_data, &task_woken);
    return task_woken == pdTRUE;
}

static bool initialize_ppa(void)
{
    const size_t pixel_count = (size_t)TABOS_DISPLAY_WIDTH * TABOS_DISPLAY_HEIGHT;
    ppa_scratch = heap_caps_aligned_calloc(
        64U, pixel_count, sizeof(*ppa_scratch), MALLOC_CAP_SPIRAM);
    if (ppa_scratch == NULL) return false;
    ppa_done = xSemaphoreCreateBinary();
    if (ppa_done == NULL) {
        free(ppa_scratch);
        ppa_scratch = NULL;
        return false;
    }
    ppa_client_config_t client_config = {
        .oper_type = PPA_OPERATION_SRM,
        .max_pending_trans_num = 1,
    };
    esp_err_t result = ppa_register_client(&client_config, &ppa_srm_client);
    if (result == ESP_OK) {
        const ppa_event_callbacks_t callbacks = {.on_trans_done = ppa_transaction_done};
        result = ppa_client_register_event_callbacks(ppa_srm_client, &callbacks);
        client_config.oper_type = PPA_OPERATION_FILL;
        if (result == ESP_OK) result = ppa_register_client(&client_config, &ppa_fill_client);
        if (result == ESP_OK) {
            result = ppa_client_register_event_callbacks(ppa_fill_client, &callbacks);
        }
        client_config.oper_type = PPA_OPERATION_BLEND;
        if (result == ESP_OK) result = ppa_register_client(&client_config, &ppa_blend_client);
        if (result == ESP_OK) {
            result = ppa_client_register_event_callbacks(ppa_blend_client, &callbacks);
        }
    }
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "PPA unavailable; using software graphics: %s", esp_err_to_name(result));
        if (ppa_srm_client != NULL) {
            (void)ppa_unregister_client(ppa_srm_client);
            ppa_srm_client = NULL;
        }
        if (ppa_fill_client != NULL) {
            (void)ppa_unregister_client(ppa_fill_client);
            ppa_fill_client = NULL;
        }
        if (ppa_blend_client != NULL) {
            (void)ppa_unregister_client(ppa_blend_client);
            ppa_blend_client = NULL;
        }
        vSemaphoreDelete(ppa_done);
        ppa_done = NULL;
        free(ppa_scratch);
        ppa_scratch = NULL;
        return false;
    }
    ESP_LOGI(TAG, "PPA framebuffer rotation enabled");
    return true;
}

static bool present_with_ppa(const platform_framebuffer_t *framebuffer)
{
    const size_t buffer_size = framebuffer->width * framebuffer->height *
        sizeof(*framebuffer->pixels);
    while (xSemaphoreTake(ppa_done, 0) == pdTRUE) {}
    const ppa_srm_oper_config_t config = {
        .in = {
            .buffer = framebuffer->pixels,
            .pic_w = (uint32_t)framebuffer->width,
            .pic_h = (uint32_t)framebuffer->height,
            .block_w = (uint32_t)framebuffer->width,
            .block_h = (uint32_t)framebuffer->height,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .out = {
            .buffer = native_pixels,
            .buffer_size = (uint32_t)buffer_size,
            .pic_w = TABOS_DISPLAY_HEIGHT,
            .pic_h = TABOS_DISPLAY_WIDTH,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .rotation_angle = PPA_SRM_ROTATION_ANGLE_90,
        .scale_x = 1.0F,
        .scale_y = 1.0F,
        .mode = PPA_TRANS_MODE_NON_BLOCKING,
        .user_data = ppa_done,
    };
    return ppa_do_scale_rotate_mirror(ppa_srm_client, &config) == ESP_OK &&
        xSemaphoreTake(ppa_done, portMAX_DELAY) == pdTRUE;
}

typedef enum {
    TAB5_PANEL_UNKNOWN = 0,
    TAB5_PANEL_ILI9881C,
    TAB5_PANEL_ST7121,
    TAB5_PANEL_ST7123,
} tab5_panel_type_t;

static tab5_panel_type_t detect_panel(void)
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

    if (i2c_master_probe(bsp_i2c_get_handle(), ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP, 100) == ESP_OK) {
        detected_display_name = "ILI9881C";
        return TAB5_PANEL_ILI9881C;
    }
    if (i2c_master_probe(bsp_i2c_get_handle(), ESP_LCD_TOUCH_IO_I2C_ST7123_ADDRESS, 100) != ESP_OK) {
        ESP_LOGE(TAG, "Could not detect supported Tab5 panel touch controller");
        return TAB5_PANEL_UNKNOWN;
    }
    result = esp_lcd_new_panel_io_i2c(bsp_i2c_get_handle(), &touch_io_config, &touch_io);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Could not create Tab5 touch probe: %s", esp_err_to_name(result));
        return TAB5_PANEL_UNKNOWN;
    }
    const esp_err_t read_result = esp_lcd_panel_io_rx_param(
        touch_io, 0x0000, &firmware_version, sizeof(firmware_version));
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

static esp_err_t create_st7121(void)
{
    const esp_ldo_channel_config_t ldo_config = {
        .chan_id = BSP_MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv = BSP_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
    };
    esp_err_t result = esp_ldo_acquire_channel(&ldo_config, &display_phy_power);
    if (result != ESP_OK) return result;
    const esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = 2,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = TAB5_DSI_LANE_BITRATE_MBPS,
    };
    result = esp_lcd_new_dsi_bus(&bus_config, &display_handles.mipi_dsi_bus);
    if (result != ESP_OK) return result;
    const esp_lcd_dbi_io_config_t dbi_config = ST7121_PANEL_IO_DBI_CONFIG();
    result = esp_lcd_new_panel_io_dbi(display_handles.mipi_dsi_bus, &dbi_config, &display_handles.io);
    if (result != ESP_OK) return result;
    esp_lcd_dpi_panel_config_t dpi_config =
        ST7121_1280_720_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
    dpi_config.num_fbs = 2;
    const st7121_vendor_config_t vendor_config = {
        .mipi_config = {.dsi_bus = display_handles.mipi_dsi_bus, .dpi_config = &dpi_config},
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = GPIO_NUM_NC,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,
        .bits_per_pixel = 24,
        .vendor_config = (void *)&vendor_config,
    };
    result = esp_lcd_new_panel_st7121(display_handles.io, &panel_config, &display_handles.panel);
    if (result != ESP_OK) return result;
    result = esp_lcd_panel_reset(display_handles.panel);
    if (result != ESP_OK) return result;
    result = esp_lcd_panel_init(display_handles.panel);
    if (result != ESP_OK) return result;
    result = esp_lcd_panel_disp_on_off(display_handles.panel, true);
    if (result != ESP_OK) return result;
    result = bsp_display_brightness_init();
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "ST7121 display initialized with native resolution %dx%d", BSP_LCD_H_RES, BSP_LCD_V_RES);
    }
    return result;
}

const char *platform_display_name(void)
{
    return detected_display_name;
}

uint32_t platform_graphics_capabilities(void)
{
    return ppa_ready ? TABOS_GRAPHICS_CAP_HARDWARE_ACCELERATED : 0U;
}

bool platform_graphics_begin(void)
{
    direct_graphics_active = native_pixels != NULL;
    direct_frame_prepared = false;
    direct_frame_dirty = false;
    return direct_graphics_active;
}

void platform_graphics_end(void)
{
    direct_graphics_active = false;
}

bool platform_graphics_present(platform_framebuffer_t *framebuffer)
{
    if (!direct_graphics_active) return platform_display_present(framebuffer);
    if (!direct_frame_dirty) return wait_for_vsync();
    if (!submit_native_frame()) return false;
    direct_frame_prepared = false;
    direct_frame_dirty = false;
    return true;
}

static bool wait_for_ppa(esp_err_t result)
{
    if (result == ESP_OK && xSemaphoreTake(ppa_done, portMAX_DELAY) == pdTRUE) {
        return true;
    }
    if (ppa_ready) ESP_LOGW(TAG, "PPA operation failed; switching to software graphics");
    ppa_ready = false;
    return false;
}

static color_pixel_rgb888_data_t rgb565_to_rgb888(platform_pixel_t color)
{
    return (color_pixel_rgb888_data_t){
        .r = (uint8_t)(((color >> 11U) & 0x1fU) << 3U),
        .g = (uint8_t)(((color >> 5U) & 0x3fU) << 2U),
        .b = (uint8_t)((color & 0x1fU) << 3U),
    };
}

static void native_fill_cpu(int32_t left, int32_t top, int32_t right, int32_t bottom,
                            platform_pixel_t color)
{
    const int32_t native_x = top;
    const int32_t native_y = TABOS_DISPLAY_WIDTH - right;
    const int32_t native_right = bottom;
    const int32_t native_bottom = TABOS_DISPLAY_WIDTH - left;
    for (int32_t row = native_y; row < native_bottom; ++row) {
        platform_pixel_t *destination = native_pixels +
            (size_t)row * TABOS_DISPLAY_HEIGHT + (size_t)native_x;
        const size_t count = (size_t)(native_right - native_x);
        if (!esp32p4_pie_fill16(destination, count, &color))
            for (size_t column = 0U; column < count; ++column) destination[column] = color;
    }
    const size_t first = (size_t)native_y * TABOS_DISPLAY_HEIGHT + (size_t)native_x;
    const size_t last = (size_t)(native_bottom - 1) * TABOS_DISPLAY_HEIGHT +
        (size_t)native_right;
    (void)esp_cache_msync(native_pixels + first, (last - first) * sizeof(*native_pixels),
                          ESP_CACHE_MSYNC_FLAG_DIR_C2M |
                          ESP_CACHE_MSYNC_FLAG_UNALIGNED);
}

static bool native_color_keyed(platform_pixel_t color, platform_pixel_t low,
                               platform_pixel_t high)
{
    return ((color >> 11U) & 0x1fU) >= ((low >> 11U) & 0x1fU) &&
        ((color >> 11U) & 0x1fU) <= ((high >> 11U) & 0x1fU) &&
        ((color >> 5U) & 0x3fU) >= ((low >> 5U) & 0x3fU) &&
        ((color >> 5U) & 0x3fU) <= ((high >> 5U) & 0x3fU) &&
        (color & 0x1fU) >= (low & 0x1fU) && (color & 0x1fU) <= (high & 0x1fU);
}

static platform_pixel_t native_blend(platform_pixel_t source,
                                     platform_pixel_t destination, uint8_t opacity)
{
    const unsigned int inverse = 255U - opacity;
    const unsigned int red = ((((source >> 11U) & 0x1fU) * opacity) +
        (((destination >> 11U) & 0x1fU) * inverse) + 127U) / 255U;
    const unsigned int green = ((((source >> 5U) & 0x3fU) * opacity) +
        (((destination >> 5U) & 0x3fU) * inverse) + 127U) / 255U;
    const unsigned int blue = (((source & 0x1fU) * opacity) +
        ((destination & 0x1fU) * inverse) + 127U) / 255U;
    return (platform_pixel_t)((red << 11U) | (green << 5U) | blue);
}

static bool prepare_direct_back_buffer(bool replaces_entire_frame)
{
    if (direct_frame_prepared) return true;
    direct_frame_prepared = true;
    if (replaces_entire_frame) return true;
    const size_t bytes = (size_t)TABOS_DISPLAY_WIDTH * TABOS_DISPLAY_HEIGHT *
        sizeof(*native_pixels);
    const size_t pixels = bytes / sizeof(*native_pixels);
    if (!esp32p4_pie_copy16(native_pixels, native_front_pixels, pixels))
        memcpy(native_pixels, native_front_pixels, bytes);
    return esp_cache_msync(native_pixels, bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M) == ESP_OK;
}

static bool native_blit_cpu(const tabos_graphics_blit_options_t *options)
{
    if (options == NULL || options->pixels == NULL || options->source.x < 0 ||
        options->source.y < 0 || options->source.width == 0U ||
        options->source.height == 0U || options->destination.width == 0U ||
        options->destination.height == 0U ||
        (uint64_t)(uint32_t)options->source.x + options->source.width >
            options->bitmap_width ||
        (uint64_t)(uint32_t)options->source.y + options->source.height >
            options->bitmap_height || options->rotation > TABOS_GRAPHICS_ROTATE_270) {
        return false;
    }
    if (!prepare_direct_back_buffer(false)) return false;
    const uint32_t rotated_width = options->rotation == TABOS_GRAPHICS_ROTATE_90 ||
        options->rotation == TABOS_GRAPHICS_ROTATE_270
        ? options->source.height : options->source.width;
    const uint32_t rotated_height = options->rotation == TABOS_GRAPHICS_ROTATE_90 ||
        options->rotation == TABOS_GRAPHICS_ROTATE_270
        ? options->source.width : options->source.height;
    for (uint32_t dy = 0U; dy < options->destination.height; ++dy) {
        const int64_t logical_y = (int64_t)options->destination.y + dy;
        if (logical_y < 0 || logical_y >= TABOS_DISPLAY_HEIGHT) continue;
        const uint32_t ry = (uint32_t)((uint64_t)dy * rotated_height /
                                      options->destination.height);
        for (uint32_t dx = 0U; dx < options->destination.width; ++dx) {
            const int64_t logical_x = (int64_t)options->destination.x + dx;
            if (logical_x < 0 || logical_x >= TABOS_DISPLAY_WIDTH) continue;
            const uint32_t rx = (uint32_t)((uint64_t)dx * rotated_width /
                                          options->destination.width);
            uint32_t sx = rx, sy = ry;
            if (options->rotation == TABOS_GRAPHICS_ROTATE_90) {
                sx = options->source.width - 1U - ry; sy = rx;
            } else if (options->rotation == TABOS_GRAPHICS_ROTATE_180) {
                sx = options->source.width - 1U - rx;
                sy = options->source.height - 1U - ry;
            } else if (options->rotation == TABOS_GRAPHICS_ROTATE_270) {
                sx = ry; sy = options->source.height - 1U - rx;
            }
            if (options->mirror_x) sx = options->source.width - 1U - sx;
            if (options->mirror_y) sy = options->source.height - 1U - sy;
            sx += (uint32_t)options->source.x;
            sy += (uint32_t)options->source.y;
            const platform_pixel_t source =
                options->pixels[(size_t)sy * options->bitmap_width + sx];
            if (options->color_key_enabled && native_color_keyed(
                    source, options->color_key_low, options->color_key_high)) continue;
            platform_pixel_t *destination = native_pixels +
                (size_t)(TABOS_DISPLAY_WIDTH - 1 - logical_x) * TABOS_DISPLAY_HEIGHT +
                (size_t)logical_y;
            *destination = options->opacity == 255U
                ? source : native_blend(source, *destination, options->opacity);
        }
    }
    (void)esp_cache_msync(native_pixels,
                          (size_t)TABOS_DISPLAY_WIDTH * TABOS_DISPLAY_HEIGHT *
                          sizeof(*native_pixels),
                          ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    direct_frame_dirty = true;
    return true;
}

static bool native_blit_ppa(const tabos_graphics_blit_options_t *options)
{
    if (!ppa_ready || options == NULL || options->pixels == NULL ||
        options->source.x < 0 || options->source.y < 0 ||
        options->source.width == 0U || options->source.height == 0U ||
        options->destination.width == 0U || options->destination.height == 0U ||
        options->destination.x < 0 || options->destination.y < 0 ||
        (uint64_t)(uint32_t)options->source.x + options->source.width >
            options->bitmap_width ||
        (uint64_t)(uint32_t)options->source.y + options->source.height >
            options->bitmap_height ||
        (uint64_t)(uint32_t)options->destination.x + options->destination.width >
            TABOS_DISPLAY_WIDTH ||
        (uint64_t)(uint32_t)options->destination.y + options->destination.height >
            TABOS_DISPLAY_HEIGHT ||
        options->rotation != TABOS_GRAPHICS_ROTATE_0 || options->mirror_x ||
        options->mirror_y || options->opacity != 255U || options->color_key_enabled ||
        (uint64_t)options->destination.width * options->destination.height <
            TAB5_PPA_BLIT_MIN_PIXELS) return false;
    if (!prepare_direct_back_buffer(false)) return false;

    const uint32_t native_x = (uint32_t)options->destination.y;
    const uint32_t native_y = TABOS_DISPLAY_WIDTH -
        ((uint32_t)options->destination.x + options->destination.width);
    while (xSemaphoreTake(ppa_done, 0) == pdTRUE) {}
    const ppa_srm_oper_config_t config = {
        .in = {
            .buffer = options->pixels, .pic_w = options->bitmap_width,
            .pic_h = options->bitmap_height, .block_w = options->source.width,
            .block_h = options->source.height,
            .block_offset_x = (uint32_t)options->source.x,
            .block_offset_y = (uint32_t)options->source.y,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .out = {
            .buffer = native_pixels,
            .buffer_size = TABOS_DISPLAY_WIDTH * TABOS_DISPLAY_HEIGHT *
                sizeof(*native_pixels),
            .pic_w = TABOS_DISPLAY_HEIGHT, .pic_h = TABOS_DISPLAY_WIDTH,
            .block_offset_x = native_x, .block_offset_y = native_y,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .rotation_angle = PPA_SRM_ROTATION_ANGLE_90,
        .scale_x = (float)options->destination.width / options->source.width,
        .scale_y = (float)options->destination.height / options->source.height,
        .mode = PPA_TRANS_MODE_NON_BLOCKING, .user_data = ppa_done,
    };
    if (!wait_for_ppa(ppa_do_scale_rotate_mirror(ppa_srm_client, &config))) return false;
    direct_frame_dirty = true;
    return true;
}

bool platform_graphics_fill(platform_framebuffer_t *framebuffer, int32_t x, int32_t y,
                            uint32_t width, uint32_t height, platform_pixel_t color)
{
    if (framebuffer == NULL || framebuffer->pixels != logical_pixels ||
        width == 0U || height == 0U) return false;
    const int64_t right = (int64_t)x + width, bottom = (int64_t)y + height;
    const int32_t left = x < 0 ? 0 : x, top = y < 0 ? 0 : y;
    const int32_t clipped_right = right > (int64_t)framebuffer->width
        ? (int32_t)framebuffer->width : (int32_t)right;
    const int32_t clipped_bottom = bottom > (int64_t)framebuffer->height
        ? (int32_t)framebuffer->height : (int32_t)bottom;
    if (left >= clipped_right || top >= clipped_bottom) return true;
    if (direct_graphics_active) {
        const bool fullscreen = left == 0 && top == 0 &&
            clipped_right == TABOS_DISPLAY_WIDTH &&
            clipped_bottom == TABOS_DISPLAY_HEIGHT;
        if (!prepare_direct_back_buffer(fullscreen)) return false;
        const uint32_t native_x = (uint32_t)top;
        const uint32_t native_y = TABOS_DISPLAY_WIDTH - (uint32_t)clipped_right;
        const uint32_t native_width = (uint32_t)(clipped_bottom - top);
        const uint32_t native_height = (uint32_t)(clipped_right - left);
        if (ppa_ready) {
            while (xSemaphoreTake(ppa_done, 0) == pdTRUE) {}
            const ppa_fill_oper_config_t direct = {
                .out = {
                    .buffer = native_pixels,
                    .buffer_size = TABOS_DISPLAY_WIDTH * TABOS_DISPLAY_HEIGHT *
                        sizeof(*native_pixels),
                    .pic_w = TABOS_DISPLAY_HEIGHT, .pic_h = TABOS_DISPLAY_WIDTH,
                    .block_offset_x = native_x, .block_offset_y = native_y,
                    .fill_cm = PPA_FILL_COLOR_MODE_RGB565,
                },
                .fill_block_w = native_width, .fill_block_h = native_height,
                .fill_argb_color = {
                    .a = 255U,
                    .r = (uint8_t)(((color >> 11U) & 0x1fU) << 3U),
                    .g = (uint8_t)(((color >> 5U) & 0x3fU) << 2U),
                    .b = (uint8_t)((color & 0x1fU) << 3U),
                },
                .mode = PPA_TRANS_MODE_NON_BLOCKING, .user_data = ppa_done,
            };
            if (wait_for_ppa(ppa_do_fill(ppa_fill_client, &direct))) {
                direct_frame_dirty = true;
                return true;
            }
        }
        native_fill_cpu(left, top, clipped_right, clipped_bottom, color);
        direct_frame_dirty = true;
        return true;
    }
    if (!ppa_ready) return false;
    while (xSemaphoreTake(ppa_done, 0) == pdTRUE) {}
    const ppa_fill_oper_config_t config = {
        .out = {
            .buffer = framebuffer->pixels,
            .buffer_size = (uint32_t)(framebuffer->width * framebuffer->height *
                                      sizeof(*framebuffer->pixels)),
            .pic_w = (uint32_t)framebuffer->width,
            .pic_h = (uint32_t)framebuffer->height,
            .block_offset_x = (uint32_t)left,
            .block_offset_y = (uint32_t)top,
            .fill_cm = PPA_FILL_COLOR_MODE_RGB565,
        },
        .fill_block_w = (uint32_t)(clipped_right - left),
        .fill_block_h = (uint32_t)(clipped_bottom - top),
        .fill_argb_color = {
            .a = 255U,
            .r = (uint8_t)(((color >> 11U) & 0x1fU) << 3U),
            .g = (uint8_t)(((color >> 5U) & 0x3fU) << 2U),
            .b = (uint8_t)((color & 0x1fU) << 3U),
        },
        .mode = PPA_TRANS_MODE_NON_BLOCKING,
        .user_data = ppa_done,
    };
    return wait_for_ppa(ppa_do_fill(ppa_fill_client, &config));
}

bool platform_graphics_blit(platform_framebuffer_t *framebuffer,
                            const tabos_graphics_blit_options_t *options)
{
    if (direct_graphics_active) {
        return native_blit_ppa(options) || native_blit_cpu(options);
    }
    if (!ppa_ready || framebuffer == NULL || options == NULL || options->pixels == NULL ||
        options->source.x < 0 ||
        options->source.y < 0 || options->destination.x < 0 || options->destination.y < 0 ||
        (uint64_t)(uint32_t)options->source.x + options->source.width > options->bitmap_width ||
        (uint64_t)(uint32_t)options->source.y + options->source.height > options->bitmap_height ||
        (uint64_t)(uint32_t)options->destination.x + options->destination.width > framebuffer->width ||
        (uint64_t)(uint32_t)options->destination.y + options->destination.height > framebuffer->height ||
        options->source.width == 0U || options->source.height == 0U ||
        options->destination.width == 0U || options->destination.height == 0U ||
        (uint64_t)options->destination.width * options->destination.height <
            TAB5_PPA_BLIT_MIN_PIXELS) return false;

    if ((options->opacity != 255U || options->color_key_enabled) &&
        options->rotation == TABOS_GRAPHICS_ROTATE_0 && !options->mirror_x &&
        !options->mirror_y && options->source.width == options->destination.width &&
        options->source.height == options->destination.height) {
        while (xSemaphoreTake(ppa_done, 0) == pdTRUE) {}
        const uint32_t buffer_size = (uint32_t)(framebuffer->width * framebuffer->height *
                                                sizeof(*framebuffer->pixels));
        const ppa_blend_oper_config_t blend = {
            .in_bg = {
                .buffer = framebuffer->pixels, .pic_w = (uint32_t)framebuffer->width,
                .pic_h = (uint32_t)framebuffer->height,
                .block_w = options->destination.width,
                .block_h = options->destination.height,
                .block_offset_x = (uint32_t)options->destination.x,
                .block_offset_y = (uint32_t)options->destination.y,
                .blend_cm = PPA_BLEND_COLOR_MODE_RGB565,
            },
            .in_fg = {
                .buffer = options->pixels, .pic_w = options->bitmap_width,
                .pic_h = options->bitmap_height, .block_w = options->source.width,
                .block_h = options->source.height,
                .block_offset_x = (uint32_t)options->source.x,
                .block_offset_y = (uint32_t)options->source.y,
                .blend_cm = PPA_BLEND_COLOR_MODE_RGB565,
            },
            .out = {
                .buffer = ppa_scratch, .buffer_size = buffer_size,
                .pic_w = (uint32_t)framebuffer->width,
                .pic_h = (uint32_t)framebuffer->height,
                .block_offset_x = (uint32_t)options->destination.x,
                .block_offset_y = (uint32_t)options->destination.y,
                .blend_cm = PPA_BLEND_COLOR_MODE_RGB565,
            },
            .bg_alpha_update_mode = PPA_ALPHA_FIX_VALUE, .bg_alpha_fix_val = 255U,
            .fg_alpha_update_mode = PPA_ALPHA_FIX_VALUE,
            .fg_alpha_fix_val = options->opacity,
            .fg_ck_en = options->color_key_enabled,
            .fg_ck_rgb_low_thres = rgb565_to_rgb888(options->color_key_low),
            .fg_ck_rgb_high_thres = rgb565_to_rgb888(options->color_key_high),
            .mode = PPA_TRANS_MODE_NON_BLOCKING, .user_data = ppa_done,
        };
        if (!wait_for_ppa(ppa_do_blend(ppa_blend_client, &blend))) return false;
        for (uint32_t row = 0U; row < options->destination.height; ++row) {
            const size_t offset = ((size_t)(uint32_t)options->destination.y + row) *
                framebuffer->stride_pixels + (uint32_t)options->destination.x;
            memcpy(framebuffer->pixels + offset, ppa_scratch + offset,
                   options->destination.width * sizeof(*framebuffer->pixels));
        }
        return true;
    }
    if (options->opacity != 255U || options->color_key_enabled) return false;
    ppa_srm_rotation_angle_t angle = PPA_SRM_ROTATION_ANGLE_0;
    float scale_x = (float)options->destination.width / options->source.width;
    float scale_y = (float)options->destination.height / options->source.height;
    if (options->rotation == TABOS_GRAPHICS_ROTATE_90) {
        angle = PPA_SRM_ROTATION_ANGLE_90;
        scale_x = (float)options->destination.height / options->source.width;
        scale_y = (float)options->destination.width / options->source.height;
    } else if (options->rotation == TABOS_GRAPHICS_ROTATE_180) {
        angle = PPA_SRM_ROTATION_ANGLE_180;
    } else if (options->rotation == TABOS_GRAPHICS_ROTATE_270) {
        angle = PPA_SRM_ROTATION_ANGLE_270;
        scale_x = (float)options->destination.height / options->source.width;
        scale_y = (float)options->destination.width / options->source.height;
    } else if (options->rotation != TABOS_GRAPHICS_ROTATE_0) {
        return false;
    }
    while (xSemaphoreTake(ppa_done, 0) == pdTRUE) {}
    const ppa_srm_oper_config_t config = {
        .in = {
            .buffer = options->pixels, .pic_w = options->bitmap_width,
            .pic_h = options->bitmap_height, .block_w = options->source.width,
            .block_h = options->source.height,
            .block_offset_x = (uint32_t)options->source.x,
            .block_offset_y = (uint32_t)options->source.y,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .out = {
            .buffer = framebuffer->pixels,
            .buffer_size = (uint32_t)(framebuffer->width * framebuffer->height *
                                      sizeof(*framebuffer->pixels)),
            .pic_w = (uint32_t)framebuffer->width, .pic_h = (uint32_t)framebuffer->height,
            .block_offset_x = (uint32_t)options->destination.x,
            .block_offset_y = (uint32_t)options->destination.y,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .rotation_angle = angle, .scale_x = scale_x, .scale_y = scale_y,
        .mirror_x = options->mirror_x, .mirror_y = options->mirror_y,
        .mode = PPA_TRANS_MODE_NON_BLOCKING, .user_data = ppa_done,
    };
    return wait_for_ppa(ppa_do_scale_rotate_mirror(ppa_srm_client, &config));
}

bool platform_display_init(platform_framebuffer_t *framebuffer)
{
    if (framebuffer == NULL) return false;
    const tab5_panel_type_t panel_type = detect_panel();
    if (panel_type == TAB5_PANEL_UNKNOWN) return false;
    ESP_LOGI(TAG, "Detected display: %s", platform_display_name());
    const size_t pixel_count = (size_t)TABOS_DISPLAY_WIDTH * TABOS_DISPLAY_HEIGHT;
    logical_pixels = heap_caps_aligned_calloc(
        64U, pixel_count, sizeof(*logical_pixels), MALLOC_CAP_SPIRAM);
    if (logical_pixels == NULL) {
        ESP_LOGE(TAG, "Could not allocate logical display buffer in PSRAM");
        platform_display_shutdown();
        return false;
    }
    esp_err_t create_result;
    if (panel_type == TAB5_PANEL_ST7121) {
        display_uses_bsp = false;
        display_created = true;
        create_result = create_st7121();
    } else {
        const bsp_display_config_t config = {
            .dsi_bus = {
                .phy_clk_src = 0,
                .lane_bit_rate_mbps = panel_type == TAB5_PANEL_ILI9881C
                    ? BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS : TAB5_DSI_LANE_BITRATE_MBPS,
            },
        };
        display_uses_bsp = true;
        create_result = bsp_display_new_with_handles(&config, &display_handles);
        display_created = create_result == ESP_OK;
    }
    if (create_result != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize Tab5 display: %s", esp_err_to_name(create_result));
        platform_display_shutdown();
        return false;
    }
    const esp_err_t framebuffer_result = esp_lcd_dpi_panel_get_frame_buffer(
        display_handles.panel, 2, (void **)&native_framebuffers[0],
        (void **)&native_framebuffers[1]);
    if (framebuffer_result != ESP_OK || native_framebuffers[0] == NULL ||
        native_framebuffers[1] == NULL) {
        ESP_LOGE(TAG, "Could not access Tab5 scanout framebuffer: %s", esp_err_to_name(framebuffer_result));
        platform_display_shutdown();
        return false;
    }
    const esp_err_t panel_result = esp_lcd_panel_disp_on_off(display_handles.panel, true);
    if (panel_result != ESP_OK) {
        ESP_LOGE(TAG, "Could not enable Tab5 display panel: %s", esp_err_to_name(panel_result));
        platform_display_shutdown();
        return false;
    }
    vsync_done = xSemaphoreCreateBinary();
    if (vsync_done == NULL) {
        platform_display_shutdown();
        return false;
    }
    const esp_lcd_dpi_panel_event_callbacks_t callbacks = {
        .on_refresh_done = display_refresh_done,
    };
    const esp_err_t callback_result = esp_lcd_dpi_panel_register_event_callbacks(
        display_handles.panel, &callbacks, vsync_done);
    if (callback_result != ESP_OK) {
        ESP_LOGE(TAG, "Could not register display VSYNC callback: %s",
                 esp_err_to_name(callback_result));
        platform_display_shutdown();
        return false;
    }
    native_front_pixels = native_framebuffers[0];
    native_pixels = native_framebuffers[1];
    vTaskDelay(pdMS_TO_TICKS(100));
    ppa_ready = initialize_ppa();
    *framebuffer = (platform_framebuffer_t){
        .pixels = logical_pixels,
        .width = TABOS_DISPLAY_WIDTH,
        .height = TABOS_DISPLAY_HEIGHT,
        .stride_pixels = TABOS_DISPLAY_WIDTH,
    };
    ESP_LOGI(TAG, "Tab5 display initialized at %dx%d RGB565", TABOS_DISPLAY_WIDTH, TABOS_DISPLAY_HEIGHT);
    platform_raster_diagnostics();
    return true;
}

bool platform_display_present(const platform_framebuffer_t *framebuffer)
{
    if (!display_created || framebuffer == NULL || framebuffer->pixels != logical_pixels) return false;
    bool hardware_presented = false;
    if (ppa_ready && !present_with_ppa(framebuffer)) {
        ESP_LOGW(TAG, "PPA presentation failed; switching to software graphics");
        ppa_ready = false;
    } else if (ppa_ready) {
        hardware_presented = true;
    }
    if (!ppa_ready && !platform_framebuffer_rotate_counter_clockwise(
                          framebuffer, native_pixels, TABOS_DISPLAY_HEIGHT,
                          TABOS_DISPLAY_WIDTH)) {
        ESP_LOGE(TAG, "Could not rotate Tab5 framebuffer");
        return false;
    }
    const size_t buffer_size =
        (size_t)TABOS_DISPLAY_WIDTH * TABOS_DISPLAY_HEIGHT * sizeof(*native_pixels);
    if (!hardware_presented) {
        const esp_err_t sync_result = esp_cache_msync(
            native_pixels, buffer_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
        if (sync_result != ESP_OK) {
            ESP_LOGE(TAG, "Could not synchronize Tab5 framebuffer: %s",
                     esp_err_to_name(sync_result));
            return false;
        }
    }
    if (!submit_native_frame()) {
        ESP_LOGE(TAG, "Could not submit Tab5 framebuffer at VSYNC");
        return false;
    }
    if (!backlight_enabled) {
        const esp_err_t result = bsp_display_backlight_on();
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Could not enable Tab5 backlight: %s", esp_err_to_name(result));
            return false;
        }
        backlight_enabled = true;
    }
    return true;
}

void platform_display_shutdown(void)
{
    ppa_ready = false;
    if (ppa_srm_client != NULL) {
        (void)ppa_unregister_client(ppa_srm_client);
        ppa_srm_client = NULL;
    }
    if (ppa_fill_client != NULL) {
        (void)ppa_unregister_client(ppa_fill_client);
        ppa_fill_client = NULL;
    }
    if (ppa_blend_client != NULL) {
        (void)ppa_unregister_client(ppa_blend_client);
        ppa_blend_client = NULL;
    }
    if (ppa_done != NULL) {
        vSemaphoreDelete(ppa_done);
        ppa_done = NULL;
    }
    free(ppa_scratch);
    ppa_scratch = NULL;
    if (backlight_enabled) {
        (void)bsp_display_backlight_off();
        backlight_enabled = false;
    }
    if (display_created) {
        if (display_uses_bsp) {
            bsp_display_delete();
        } else {
            if (display_handles.panel != NULL) (void)esp_lcd_panel_del(display_handles.panel);
            if (display_handles.io != NULL) (void)esp_lcd_panel_io_del(display_handles.io);
            if (display_handles.mipi_dsi_bus != NULL) (void)esp_lcd_del_dsi_bus(display_handles.mipi_dsi_bus);
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
    native_front_pixels = NULL;
    native_framebuffers[0] = NULL;
    native_framebuffers[1] = NULL;
    if (vsync_done != NULL) {
        vSemaphoreDelete(vsync_done);
        vsync_done = NULL;
    }
    free(logical_pixels);
    logical_pixels = NULL;
}
