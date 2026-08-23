#include "internal.h"

#include <tabos/config/identity.h>
#include <tabos/internal/input.h>

#include <driver/i2c_master.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <stdint.h>
#include <stdio.h>

#define KEYBOARD_I2C_ADDRESS          0x6d
#define KEYBOARD_I2C_FREQUENCY_HZ     400000
#define KEYBOARD_REG_INTERRUPT_CONFIG 0x00
#define KEYBOARD_REG_EVENT_COUNT      0x02
#define KEYBOARD_REG_MODE             0x10
#define KEYBOARD_REG_KEY_EVENT        0x20
#define KEYBOARD_REG_FIRMWARE_VERSION 0xfe
#define KEYBOARD_MODE_NORMAL          0
#define KEYBOARD_ROWS                 5U
#define KEYBOARD_COLUMNS              14U
#define KEYBOARD_KEY_COUNT            (KEYBOARD_ROWS * KEYBOARD_COLUMNS)

#define KEYBOARD_INDEX(row, column) ((row) * KEYBOARD_COLUMNS + (column))
#define KEYBOARD_SYM_INDEX          KEYBOARD_INDEX(3U, 0U)
#define KEYBOARD_SHIFT_INDEX        KEYBOARD_INDEX(3U, 1U)
#define KEYBOARD_CONTROL_INDEX      KEYBOARD_INDEX(4U, 0U)
#define KEYBOARD_ALT_INDEX          KEYBOARD_INDEX(4U, 1U)

typedef struct {
        uint8_t usage;
        uint8_t modifiers;
} key_mapping_t;

/* M5Stack's MIT-licensed Tab5 keyboard matrix mapping, row-major. */
// clang-format off
static const key_mapping_t base_mapping[KEYBOARD_KEY_COUNT] = {
    {0x29, 0}, {0x1e, 0}, {0x1f, 0}, {0x20, 0}, {0x21, 0}, {0x22, 0}, {0x23, 0},
    {0x24, 0}, {0x25, 0}, {0x26, 0}, {0x27, 0}, {0x2d, 0}, {0x2e, 2}, {0x4c, 0},
    {0x35, 0}, {0x1e, 2}, {0x1f, 2}, {0x20, 2}, {0x21, 2}, {0x22, 2}, {0x23, 2},
    {0x24, 2}, {0x25, 2}, {0x26, 2}, {0x27, 2}, {0x2f, 0}, {0x30, 0}, {0x31, 0},
    {0x2b, 0}, {0x14, 0}, {0x1a, 0}, {0x08, 0}, {0x15, 0}, {0x17, 0}, {0x1c, 0},
    {0x18, 0}, {0x0c, 0}, {0x12, 0}, {0x13, 0}, {0x33, 0}, {0x34, 0}, {0x2a, 0},
    {0, 0},    {0, 0},    {0x04, 0}, {0x16, 0}, {0x07, 0}, {0x09, 0}, {0x0a, 0},
    {0x0b, 0}, {0x0d, 0}, {0x0e, 0}, {0x0f, 0}, {0x52, 0}, {0x2d, 2}, {0x28, 0},
    {0, 0},    {0, 0},    {0x1d, 0}, {0x1b, 0}, {0x06, 0}, {0x19, 0}, {0x05, 0},
    {0x11, 0}, {0x10, 0}, {0x37, 0}, {0x50, 0}, {0x51, 0}, {0x4f, 0}, {0x2c, 0},
};

static const key_mapping_t sym_mapping[KEYBOARD_KEY_COUNT] = {
    {0x29, 0}, {0x1e, 0}, {0x1f, 0}, {0x20, 0}, {0x21, 0}, {0x22, 0}, {0x23, 0},
    {0x24, 0}, {0x25, 0}, {0x26, 0}, {0x27, 0}, {0x2d, 0}, {0x2e, 2}, {0x4c, 0},
    {0x35, 2}, {0x38, 2}, {0x1f, 2}, {0x20, 2}, {0x21, 2}, {0x22, 2}, {0x23, 2},
    {0x24, 2}, {0x38, 0}, {0x36, 2}, {0x37, 2}, {0x2f, 2}, {0x30, 2}, {0x31, 2},
    {0x2b, 0}, {0x14, 0}, {0x1a, 0}, {0x08, 0}, {0x15, 0}, {0x17, 0}, {0x1c, 0},
    {0x18, 0}, {0x0c, 0}, {0x12, 0}, {0x13, 0}, {0x33, 2}, {0x34, 2}, {0x2a, 0},
    {0, 0},    {0, 0},    {0x04, 0}, {0x16, 0}, {0x07, 0}, {0x09, 0}, {0x0a, 0},
    {0x0b, 0}, {0x0d, 0}, {0x0e, 0}, {0x0f, 0}, {0x52, 0}, {0x2e, 0}, {0x28, 0},
    {0, 0},    {0, 0},    {0x1d, 0}, {0x1b, 0}, {0x06, 0}, {0x19, 0}, {0x05, 0},
    {0x11, 0}, {0x10, 0}, {0x36, 0}, {0x50, 0}, {0x51, 0}, {0x4f, 0}, {0x2c, 0},
};
// clang-format on

static const char* const TAG = TABOS_PLATFORM_LOG_TAG;
static i2c_master_bus_handle_t keyboard_bus;
static i2c_master_dev_handle_t keyboard_device;
static bool keyboard_present;
static bool pressed_keys[KEYBOARD_KEY_COUNT];
static uint8_t pressed_usages[KEYBOARD_KEY_COUNT];
static uint8_t pressed_modifiers[KEYBOARD_KEY_COUNT];
static bool sym_latched;
static bool shift_latched;
static bool sym_used;
static bool shift_used;
static bool delete_held;
static char keyboard_name[40] = "Tab5 keyboard not detected";

static esp_err_t keyboard_read(uint8_t reg, void* data, size_t size)
{
    return i2c_master_transmit_receive(keyboard_device, &reg, sizeof(reg), data, size, 100);
}

static esp_err_t keyboard_write(uint8_t reg, uint8_t value)
{
    const uint8_t command[] = {reg, value};
    return i2c_master_transmit(keyboard_device, command, sizeof(command), 100);
}

static uint8_t current_modifiers(void)
{
    uint8_t modifiers = 0U;
    if (pressed_keys[KEYBOARD_CONTROL_INDEX]) {
        modifiers |= TABOS_MODIFIER_CONTROL;
    }
    if (pressed_keys[KEYBOARD_SHIFT_INDEX]) {
        modifiers |= TABOS_MODIFIER_SHIFT;
    }
    if (pressed_keys[KEYBOARD_ALT_INDEX]) {
        modifiers |= TABOS_MODIFIER_ALT;
    }
    if (pressed_keys[KEYBOARD_SYM_INDEX]) {
        modifiers |= TABOS_MODIFIER_SYM;
    }
    return modifiers;
}

static tabos_key_t normalized_key(uint8_t usage)
{
    if ((usage >= TABOS_KEY_A && usage <= TABOS_KEY_CAPS_LOCK) || (usage >= TABOS_KEY_F1 && usage <= TABOS_KEY_F12) ||
        (usage >= TABOS_KEY_INSERT && usage <= TABOS_KEY_UP)) {
        return (tabos_key_t) usage;
    }
    return TABOS_KEY_UNKNOWN;
}

static bool modifier_key(size_t index, tabos_key_t* key)
{
    if (index == KEYBOARD_SYM_INDEX) {
        *key = TABOS_KEY_SYM;
    } else if (index == KEYBOARD_SHIFT_INDEX) {
        *key = TABOS_KEY_SHIFT;
    } else if (index == KEYBOARD_CONTROL_INDEX) {
        *key = TABOS_KEY_CTRL;
    } else if (index == KEYBOARD_ALT_INDEX) {
        *key = TABOS_KEY_ALT;
    } else {
        return false;
    }
    return true;
}

static void submit_matrix_event(uint8_t report)
{
    const bool pressed   = (report & 0x80U) != 0U;
    const uint8_t row    = (report >> 4U) & 0x07U;
    const uint8_t column = report & 0x0fU;
    if (row >= KEYBOARD_ROWS || column >= KEYBOARD_COLUMNS) {
        return;
    }
    const size_t index = KEYBOARD_INDEX(row, column);
    if (pressed_keys[index] == pressed) {
        return;
    }
    pressed_keys[index] = pressed;

    tabos_key_t modifier;
    if (modifier_key(index, &modifier)) {
        if (index == KEYBOARD_SYM_INDEX) {
            if (pressed) {
                sym_used = false;
            } else if (!sym_used) {
                sym_latched = true;
            }
        } else if (index == KEYBOARD_SHIFT_INDEX) {
            if (pressed) {
                shift_used = false;
            } else if (!shift_used) {
                shift_latched = true;
            }
        }
        const tabos_input_event_t event = {
            .type      = pressed ? TABOS_INPUT_KEY_DOWN : TABOS_INPUT_KEY_UP,
            .key       = modifier,
            .modifiers = current_modifiers(),
        };
        (void) input_submit(&event);
        return;
    }

    if (pressed) {
        pressed_usages[index]    = base_mapping[index].usage;
        pressed_modifiers[index] = current_modifiers();
    }
    const uint8_t usage     = pressed_usages[index];
    const uint8_t modifiers = pressed_modifiers[index];
    if (usage == 0U) {
        return;
    }
    const tabos_input_event_t key_event = {
        .type      = pressed ? TABOS_INPUT_KEY_DOWN : TABOS_INPUT_KEY_UP,
        .key       = normalized_key(usage),
        .modifiers = modifiers,
    };
    (void) input_submit(&key_event);
    if (pressed) {
        const bool cooked_sym       = pressed_keys[KEYBOARD_SYM_INDEX] || sym_latched;
        const bool cooked_shift     = pressed_keys[KEYBOARD_SHIFT_INDEX] || shift_latched;
        const key_mapping_t mapping = cooked_sym ? sym_mapping[index] : base_mapping[index];
        uint8_t cooked_modifiers    = modifiers;
        if (cooked_shift || (mapping.modifiers & 0x02U) != 0U) {
            cooked_modifiers |= TABOS_MODIFIER_SHIFT;
        }
        if (cooked_sym) {
            cooked_modifiers |= TABOS_MODIFIER_SYM;
        }
        tabos_input_event_t text = {.type = TABOS_INPUT_TEXT, .modifiers = cooked_modifiers};
        if (input_text_from_hid(mapping.usage, cooked_modifiers, text.text, sizeof(text.text)) > 0U) {
            (void) input_submit(&text);
        }
        if (pressed_keys[KEYBOARD_SYM_INDEX]) {
            sym_used = true;
        }
        if (pressed_keys[KEYBOARD_SHIFT_INDEX]) {
            shift_used = true;
        }
        sym_latched   = false;
        shift_latched = false;
    } else {
        pressed_usages[index]    = 0U;
        pressed_modifiers[index] = 0U;
    }
    if (usage == TABOS_KEY_DELETE) {
        delete_held = pressed;
    }
}

static void capture_boot_delete(void)
{
    uint8_t count = 0U;
    if (keyboard_read(KEYBOARD_REG_EVENT_COUNT, &count, sizeof(count)) != ESP_OK) {
        return;
    }
    if (count > 32U) {
        count = 32U;
    }
    for (uint8_t index = 0U; index < count; ++index) {
        uint8_t report = 0xffU;
        if (keyboard_read(KEYBOARD_REG_KEY_EVENT, &report, sizeof(report)) != ESP_OK || report == 0xffU) {
            break;
        }
        submit_matrix_event(report);
    }
}

void tab5_keyboard_shutdown(void)
{
    if (keyboard_device != NULL) {
        (void) i2c_master_bus_rm_device(keyboard_device);
        keyboard_device = NULL;
    }
    if (keyboard_bus != NULL) {
        (void) i2c_del_master_bus(keyboard_bus);
        keyboard_bus = NULL;
    }
    keyboard_present = false;
    for (size_t index = 0U; index < KEYBOARD_KEY_COUNT; ++index) {
        pressed_keys[index]      = false;
        pressed_usages[index]    = 0U;
        pressed_modifiers[index] = 0U;
    }
    delete_held   = false;
    sym_latched   = false;
    shift_latched = false;
    sym_used      = false;
    shift_used    = false;
}

bool tab5_keyboard_init(void)
{
    (void) snprintf(keyboard_name, sizeof(keyboard_name), "Tab5 keyboard not detected");
    const i2c_master_bus_config_t bus_config = {
        .i2c_port                     = I2C_NUM_0,
        .sda_io_num                   = GPIO_NUM_0,
        .scl_io_num                   = GPIO_NUM_1,
        .clk_source                   = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt            = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t result = i2c_new_master_bus(&bus_config, &keyboard_bus);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Could not initialize Tab5 Keyboard I2C bus: %s", esp_err_to_name(result));
        return false;
    }
    result = i2c_master_probe(keyboard_bus, KEYBOARD_I2C_ADDRESS, 100);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Tab5 Keyboard not detected at I2C address 0x%02x", KEYBOARD_I2C_ADDRESS);
        return false;
    }
    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = KEYBOARD_I2C_ADDRESS,
        .scl_speed_hz    = KEYBOARD_I2C_FREQUENCY_HZ,
    };
    result = i2c_master_bus_add_device(keyboard_bus, &device_config, &keyboard_device);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Could not attach Tab5 Keyboard I2C device: %s", esp_err_to_name(result));
        return false;
    }
    uint8_t firmware_version = 0U;
    if (keyboard_read(KEYBOARD_REG_FIRMWARE_VERSION, &firmware_version, sizeof(firmware_version)) != ESP_OK ||
        keyboard_write(KEYBOARD_REG_MODE, KEYBOARD_MODE_NORMAL) != ESP_OK) {
        ESP_LOGW(TAG, "Could not configure Tab5 Keyboard");
        tab5_keyboard_shutdown();
        return false;
    }
    capture_boot_delete();
    if (keyboard_write(KEYBOARD_REG_EVENT_COUNT, 0U) != ESP_OK ||
        keyboard_write(KEYBOARD_REG_INTERRUPT_CONFIG, 0U) != ESP_OK) {
        ESP_LOGW(TAG, "Could not configure Tab5 Keyboard");
        tab5_keyboard_shutdown();
        return false;
    }
    keyboard_present = true;
    (void) snprintf(keyboard_name, sizeof(keyboard_name), "Tab5 keyboard FW %u; Normal mode", firmware_version);
    ESP_LOGI(TAG, "Detected keyboard: %s", keyboard_name);
    return true;
}

void tab5_keyboard_poll(void)
{
    if (!keyboard_present) {
        return;
    }
    uint8_t count = 0U;
    if (keyboard_read(KEYBOARD_REG_EVENT_COUNT, &count, sizeof(count)) != ESP_OK) {
        return;
    }
    if (count > 32U) {
        count = 32U;
    }
    for (uint8_t index = 0U; index < count; ++index) {
        uint8_t report = 0xffU;
        if (keyboard_read(KEYBOARD_REG_KEY_EVENT, &report, sizeof(report)) != ESP_OK || report == 0xffU) {
            break;
        }
        submit_matrix_event(report);
    }
}

const char* tab5_keyboard_name(void)
{
    return keyboard_name;
}

bool tab5_keyboard_present(void)
{
    return keyboard_present;
}

bool tab5_keyboard_delete_held(uint32_t window_ms)
{
    if (!keyboard_present) {
        return false;
    }
    const TickType_t delay    = pdMS_TO_TICKS(10U);
    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(window_ms);
    do {
        tab5_keyboard_poll();
        if (delete_held) {
            return true;
        }
        vTaskDelay(delay);
    } while ((int32_t) (deadline - xTaskGetTickCount()) > 0);
    return false;
}
