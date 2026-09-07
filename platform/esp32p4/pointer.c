#include "gpio_interrupt.h"
#include "touch_interrupt.h"

#include <tabos/internal/pointer.h>
#include <tabos/platform/platform.h>

#include <bsp/esp-bsp.h>
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_attr.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_touch.h>
#include <esp_lcd_touch_gt911.h>
#include <esp_lcd_touch_st7123.h>
#include <esp_log.h>

#include <errno.h>
#include <stdatomic.h>

static const char* const TAG = "tabos-touch";
static esp_lcd_panel_io_handle_t touch_io;
static esp_lcd_touch_handle_t touch_handle;
static tab5_touch_interrupt_state_t touch_state;
static atomic_bool touch_interrupt_pending;
static bool touch_isr_installed;
static bool touch_ready;
static int touch_error;

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

static bool read_points(void* context, tab5_touch_point_t* points, uint8_t* point_count, uint8_t maximum_points)
{
    (void) context;
    esp_lcd_touch_point_data_t driver_points[CONFIG_ESP_LCD_TOUCH_MAX_POINTS];
    uint8_t driver_point_count = 0U;
    const uint8_t driver_maximum =
        maximum_points < CONFIG_ESP_LCD_TOUCH_MAX_POINTS ? maximum_points : CONFIG_ESP_LCD_TOUCH_MAX_POINTS;
    if (esp_lcd_touch_read_data(touch_handle) != ESP_OK ||
        esp_lcd_touch_get_data(touch_handle, driver_points, &driver_point_count, driver_maximum) != ESP_OK) {
        return false;
    }
    for (uint8_t point = 0U; point < driver_point_count; ++point) {
        points[point] = (tab5_touch_point_t) {
            .x = driver_points[point].x,
            .y = driver_points[point].y,
        };
    }
    *point_count = driver_point_count;
    return true;
}

static bool interrupt_asserted(void* context)
{
    (void) context;
    return gpio_get_level(BSP_LCD_TOUCH_INT) == 0;
}

static void interrupt_submit_contact(void* context, tabos_pointer_event_type_t type, uint32_t contact, uint16_t x,
                                     uint16_t y)
{
    (void) context;
    submit_contact(type, contact, x, y);
}

static tab5_touch_interrupt_ops_t touch_operations(void)
{
    return (tab5_touch_interrupt_ops_t) {
        .read_points        = read_points,
        .interrupt_asserted = interrupt_asserted,
        .submit_contact     = interrupt_submit_contact,
    };
}

static void IRAM_ATTR touch_interrupt(void* context)
{
    (void) context;
    atomic_store_explicit(&touch_interrupt_pending, true, memory_order_release);
    platform_runtime_notify_from_isr(PLATFORM_RUNTIME_EVENT_POINTER);
}

static void touch_interrupt_disable(void)
{
    (void) gpio_intr_disable(BSP_LCD_TOUCH_INT);
    if (touch_isr_installed) {
        const esp_err_t result = gpio_isr_handler_remove(BSP_LCD_TOUCH_INT);
        if (result != ESP_OK) {
            ESP_LOGW(TAG, "Could not remove touch interrupt handler: %s", esp_err_to_name(result));
        }
        touch_isr_installed = false;
    }
    atomic_store_explicit(&touch_interrupt_pending, false, memory_order_release);
}

static void touch_fault(void)
{
    touch_error = EIO;
    touch_ready = false;
    touch_interrupt_disable();
    const tab5_touch_interrupt_ops_t ops = touch_operations();
    tab5_touch_interrupt_cancel(&touch_state, &ops);
    pointer_service_remove_device();
}

bool platform_pointer_init(const char** driver, int* error)
{
    tab5_touch_interrupt_state_init(&touch_state);
    atomic_store_explicit(&touch_interrupt_pending, false, memory_order_release);
    touch_isr_installed               = false;
    touch_ready                       = false;
    touch_error                       = 0;
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
            .int_gpio_num = BSP_LCD_TOUCH_INT,
            /* TabOS owns GPIO registration; component callback registration
             * attempts another global ISR-service install and ignores failures. */
            .interrupt_callback = NULL,
        };
        result = gt911 ? esp_lcd_touch_new_i2c_gt911(touch_io, &config, &touch_handle) :
                         esp_lcd_touch_new_i2c_st7123(touch_io, &config, &touch_handle);
        if (result == ESP_OK) {
            result              = tab5_gpio_interrupt_add(BSP_LCD_TOUCH_INT, touch_interrupt, NULL);
            touch_isr_installed = result == ESP_OK;
        }
    }
    if (driver != NULL) {
        *driver = gt911 ? "GT911" : "ST712x";
    }
    if (error != NULL) {
        *error = result == ESP_OK ? 0 : result;
    }
    if (result != ESP_OK) {
        touch_error = EIO;
        ESP_LOGE(TAG, "Could not initialize touch controller: %s", esp_err_to_name(result));
        platform_pointer_shutdown();
        return true;
    }
    touch_ready = true;
    if (atomic_load_explicit(&touch_interrupt_pending, memory_order_acquire) ||
        gpio_get_level(BSP_LCD_TOUCH_INT) == 0) {
        atomic_store_explicit(&touch_interrupt_pending, true, memory_order_release);
        platform_runtime_notify(PLATFORM_RUNTIME_EVENT_POINTER);
    }
    ESP_LOGI(TAG, "Touch controller initialized: %s", gt911 ? "GT911" : "ST712x");
    return true;
}

void platform_pointer_update(void)
{
    if (!touch_ready) {
        return;
    }
    if (!atomic_exchange_explicit(&touch_interrupt_pending, false, memory_order_acq_rel) &&
        gpio_get_level(BSP_LCD_TOUCH_INT) != 0) {
        return;
    }
    const tab5_touch_interrupt_ops_t ops = touch_operations();
    bool still_pending                   = false;
    if (!tab5_touch_interrupt_drain(&touch_state, &ops, 4U, &still_pending)) {
        ESP_LOGW(TAG, "Touch interrupt drain failed");
        touch_fault();
        return;
    }
    if (still_pending || atomic_exchange_explicit(&touch_interrupt_pending, false, memory_order_acq_rel)) {
        atomic_store_explicit(&touch_interrupt_pending, true, memory_order_release);
        platform_runtime_notify(PLATFORM_RUNTIME_EVENT_POINTER);
    }
}

void platform_pointer_shutdown(void)
{
    touch_ready = false;
    touch_interrupt_disable();
    const tab5_touch_interrupt_ops_t ops = touch_operations();
    tab5_touch_interrupt_cancel(&touch_state, &ops);
    if (touch_handle != NULL) {
        (void) esp_lcd_touch_del(touch_handle);
        touch_handle = NULL;
    }
    if (touch_io != NULL) {
        (void) esp_lcd_panel_io_del(touch_io);
        touch_io = NULL;
    }
    tab5_touch_interrupt_state_init(&touch_state);
}

bool platform_pointer_health(int* error)
{
    if (error != NULL) {
        *error = touch_error;
    }
    return touch_ready && touch_error == 0;
}
