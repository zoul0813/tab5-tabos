#include <tabos/internal/pointer.h>
#include <tabos/platform/platform.h>

#include <bsp/esp-bsp.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_touch.h>
#include <esp_lcd_touch_gt911.h>
#include <esp_lcd_touch_st7123.h>
#include <esp_log.h>

#include <limits.h>
#include <string.h>

typedef struct {
        uint16_t x;
        uint16_t y;
        bool active;
} tab5_contact_t;

static const char* const TAG = "tabos-touch";
static esp_lcd_panel_io_handle_t touch_io;
static esp_lcd_touch_handle_t touch_handle;
static tab5_contact_t contacts[TABOS_POINTER_MAX_CONTACTS];
static bool touch_ready;

static void submit_contact(tabos_pointer_event_type_t type, uint32_t contact, uint16_t native_x, uint16_t native_y)
{
    int32_t x = (TABOS_DISPLAY_WIDTH - 1) - native_y;
    int32_t y = native_x;
    if (x < 0) {
        x = 0;
    } else if (x >= TABOS_DISPLAY_WIDTH) {
        x = TABOS_DISPLAY_WIDTH - 1;
    }
    if (y < 0) {
        y = 0;
    } else if (y >= TABOS_DISPLAY_HEIGHT) {
        y = TABOS_DISPLAY_HEIGHT - 1;
    }
    const tabos_pointer_event_t event = {
        .type       = type,
        .contact_id = contact,
        .x          = x,
        .y          = y,
        .buttons    = type == TABOS_POINTER_UP || type == TABOS_POINTER_CANCEL ? 0U : TABOS_POINTER_BUTTON_PRIMARY,
    };
    pointer_service_submit(&event);
}

bool platform_pointer_init(const char** driver, int* error)
{
    memset(contacts, 0, sizeof(contacts));
    const i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    uint16_t gt911_address            = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP;
    bool gt911                        = i2c_master_probe(bus, gt911_address, 100) == ESP_OK;
    if (!gt911) {
        gt911_address = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS;
        gt911         = i2c_master_probe(bus, gt911_address, 100) == ESP_OK;
    }
    const bool st712x = !gt911 && i2c_master_probe(bus, ESP_LCD_TOUCH_IO_I2C_ST7123_ADDRESS, 100) == ESP_OK;
    if (!gt911 && !st712x) {
        if (driver != NULL) {
            *driver = NULL;
        }
        if (error != NULL) {
            *error = 0;
        }
        return false;
    }
    esp_lcd_panel_io_i2c_config_t io_config;
    esp_err_t result;
    if (gt911) {
        io_config          = (esp_lcd_panel_io_i2c_config_t) ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
        io_config.dev_addr = gt911_address;
    } else {
        io_config = (esp_lcd_panel_io_i2c_config_t) ESP_LCD_TOUCH_IO_I2C_ST7123_CONFIG();
    }
    result = esp_lcd_new_panel_io_i2c(bus, &io_config, &touch_io);
    if (result == ESP_OK) {
        const esp_lcd_touch_config_t config = {
            .x_max        = BSP_LCD_H_RES,
            .y_max        = BSP_LCD_V_RES,
            .rst_gpio_num = GPIO_NUM_NC,
            .int_gpio_num = GPIO_NUM_NC,
        };
        result = gt911 ? esp_lcd_touch_new_i2c_gt911(touch_io, &config, &touch_handle) :
                         esp_lcd_touch_new_i2c_st7123(touch_io, &config, &touch_handle);
    }
    if (driver != NULL) {
        *driver = gt911 ? "GT911" : "ST712x";
    }
    if (error != NULL) {
        *error = result == ESP_OK ? 0 : result;
    }
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize touch controller: %s", esp_err_to_name(result));
        platform_pointer_shutdown();
        return true;
    }
    touch_ready = true;
    ESP_LOGI(TAG, "Touch controller initialized: %s", gt911 ? "GT911" : "ST712x");
    return true;
}

void platform_pointer_update(void)
{
    if (!touch_ready) {
        return;
    }
    esp_lcd_touch_point_data_t points[CONFIG_ESP_LCD_TOUCH_MAX_POINTS];
    uint8_t point_count = 0U;
    if (esp_lcd_touch_read_data(touch_handle) != ESP_OK ||
        esp_lcd_touch_get_data(touch_handle, points, &point_count, CONFIG_ESP_LCD_TOUCH_MAX_POINTS) != ESP_OK) {
        return;
    }
    bool matched_contacts[TABOS_POINTER_MAX_CONTACTS]    = {false};
    bool matched_points[CONFIG_ESP_LCD_TOUCH_MAX_POINTS] = {false};
    for (uint8_t point = 0U; point < point_count; ++point) {
        uint32_t best_contact  = TABOS_POINTER_MAX_CONTACTS;
        uint32_t best_distance = UINT_MAX;
        for (uint32_t contact = 0U; contact < TABOS_POINTER_MAX_CONTACTS; ++contact) {
            if (!contacts[contact].active || matched_contacts[contact]) {
                continue;
            }
            const int32_t dx        = (int32_t) contacts[contact].x - points[point].x;
            const int32_t dy        = (int32_t) contacts[contact].y - points[point].y;
            const uint32_t distance = (uint32_t) (dx * dx + dy * dy);
            if (distance < best_distance) {
                best_distance = distance;
                best_contact  = contact;
            }
        }
        if (best_contact < TABOS_POINTER_MAX_CONTACTS) {
            matched_contacts[best_contact] = true;
            matched_points[point]          = true;
            contacts[best_contact].x       = points[point].x;
            contacts[best_contact].y       = points[point].y;
            submit_contact(TABOS_POINTER_MOVE, best_contact, points[point].x, points[point].y);
        }
    }
    for (uint32_t contact = 0U; contact < TABOS_POINTER_MAX_CONTACTS; ++contact) {
        if (contacts[contact].active && !matched_contacts[contact]) {
            submit_contact(TABOS_POINTER_UP, contact, contacts[contact].x, contacts[contact].y);
            contacts[contact].active = false;
        }
    }
    for (uint8_t point = 0U; point < point_count; ++point) {
        if (matched_points[point]) {
            continue;
        }
        for (uint32_t contact = 0U; contact < TABOS_POINTER_MAX_CONTACTS; ++contact) {
            if (contacts[contact].active) {
                continue;
            }
            contacts[contact] = (tab5_contact_t) {
                .x      = points[point].x,
                .y      = points[point].y,
                .active = true,
            };
            submit_contact(TABOS_POINTER_DOWN, contact, points[point].x, points[point].y);
            break;
        }
    }
}

void platform_pointer_shutdown(void)
{
    if (touch_handle != NULL) {
        (void) esp_lcd_touch_del(touch_handle);
        touch_handle = NULL;
    }
    if (touch_io != NULL) {
        (void) esp_lcd_panel_io_del(touch_io);
        touch_io = NULL;
    }
    memset(contacts, 0, sizeof(contacts));
    touch_ready = false;
}
