#include "internal.h"

#include <tabos/config/identity.h>
#include <tabos/internal/input.h>

#include <driver/i2c_master.h>
#include <esp_log.h>

#include <stdint.h>
#include <stdio.h>

#define KEYBOARD_I2C_ADDRESS 0x6d
#define KEYBOARD_I2C_FREQUENCY_HZ 400000
#define KEYBOARD_REG_INTERRUPT_CONFIG 0x00
#define KEYBOARD_REG_EVENT_COUNT 0x02
#define KEYBOARD_REG_MODE 0x10
#define KEYBOARD_REG_HID_EVENT 0x30
#define KEYBOARD_REG_FIRMWARE_VERSION 0xfe
#define KEYBOARD_MODE_HID 1

static const char *const TAG = TABOS_PLATFORM_LOG_TAG;
static i2c_master_bus_handle_t keyboard_bus;
static i2c_master_dev_handle_t keyboard_device;
static bool keyboard_present;
static uint8_t previous_usage;
static uint8_t previous_modifiers;
static char keyboard_name[40] = "Tab5 keyboard not detected";

static esp_err_t keyboard_read(uint8_t reg, void *data, size_t size)
{
    return i2c_master_transmit_receive(keyboard_device, &reg, sizeof(reg), data, size, 100);
}

static esp_err_t keyboard_write(uint8_t reg, uint8_t value)
{
    const uint8_t command[] = {reg, value};
    return i2c_master_transmit(keyboard_device, command, sizeof(command), 100);
}

static uint8_t normalized_modifiers(uint8_t hid)
{
    uint8_t modifiers = 0U;
    if ((hid & 0x11U) != 0U) modifiers |= TABOS_MODIFIER_CONTROL;
    if ((hid & 0x22U) != 0U) modifiers |= TABOS_MODIFIER_SHIFT;
    if ((hid & 0x44U) != 0U) modifiers |= TABOS_MODIFIER_ALT;
    if ((hid & 0x88U) != 0U) modifiers |= TABOS_MODIFIER_GUI;
    return modifiers;
}

static tabos_key_t normalized_key(uint8_t usage)
{
    if ((usage >= TABOS_KEY_A && usage <= TABOS_KEY_CAPS_LOCK) ||
        (usage >= TABOS_KEY_F1 && usage <= TABOS_KEY_F12) ||
        (usage >= TABOS_KEY_INSERT && usage <= TABOS_KEY_UP)) {
        return (tabos_key_t)usage;
    }
    return TABOS_KEY_UNKNOWN;
}

static void submit_report(uint8_t hid_modifiers, uint8_t usage)
{
    const uint8_t modifiers = normalized_modifiers(hid_modifiers);
    if (previous_usage != 0U && previous_usage != usage) {
        const tabos_input_event_t release = {
            .type = TABOS_INPUT_KEY_UP,
            .key = normalized_key(previous_usage),
            .modifiers = previous_modifiers,
        };
        (void)input_submit(&release);
    }
    if (usage != 0U) {
        const bool repeat = usage == previous_usage;
        const tabos_input_event_t press = {
            .type = TABOS_INPUT_KEY_DOWN,
            .key = normalized_key(usage),
            .modifiers = modifiers,
            .repeat = repeat,
        };
        (void)input_submit(&press);
        tabos_input_event_t text = {
            .type = TABOS_INPUT_TEXT,
            .modifiers = modifiers,
            .repeat = repeat,
        };
        if (input_text_from_hid(usage, modifiers, text.text, sizeof(text.text)) > 0U) {
            (void)input_submit(&text);
        }
    }
    previous_usage = usage;
    previous_modifiers = modifiers;
}

void tab5_keyboard_shutdown(void)
{
    if (keyboard_device != NULL) {
        (void)i2c_master_bus_rm_device(keyboard_device);
        keyboard_device = NULL;
    }
    if (keyboard_bus != NULL) {
        (void)i2c_del_master_bus(keyboard_bus);
        keyboard_bus = NULL;
    }
    keyboard_present = false;
    previous_usage = 0U;
    previous_modifiers = 0U;
}

bool tab5_keyboard_init(void)
{
    (void)snprintf(keyboard_name, sizeof(keyboard_name), "Tab5 keyboard not detected");
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_0,
        .scl_io_num = GPIO_NUM_1,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
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
        .device_address = KEYBOARD_I2C_ADDRESS,
        .scl_speed_hz = KEYBOARD_I2C_FREQUENCY_HZ,
    };
    result = i2c_master_bus_add_device(keyboard_bus, &device_config, &keyboard_device);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Could not attach Tab5 Keyboard I2C device: %s", esp_err_to_name(result));
        return false;
    }
    uint8_t firmware_version = 0U;
    if (keyboard_read(KEYBOARD_REG_FIRMWARE_VERSION, &firmware_version, sizeof(firmware_version)) != ESP_OK ||
        keyboard_write(KEYBOARD_REG_MODE, KEYBOARD_MODE_HID) != ESP_OK ||
        keyboard_write(KEYBOARD_REG_EVENT_COUNT, 0U) != ESP_OK ||
        keyboard_write(KEYBOARD_REG_INTERRUPT_CONFIG, 0U) != ESP_OK) {
        ESP_LOGW(TAG, "Could not configure Tab5 Keyboard");
        tab5_keyboard_shutdown();
        return false;
    }
    keyboard_present = true;
    (void)snprintf(keyboard_name, sizeof(keyboard_name), "Tab5 keyboard FW %u; HID mode", firmware_version);
    ESP_LOGI(TAG, "Detected keyboard: %s", keyboard_name);
    return true;
}

void tab5_keyboard_poll(void)
{
    if (!keyboard_present) return;
    uint8_t count = 0U;
    if (keyboard_read(KEYBOARD_REG_EVENT_COUNT, &count, sizeof(count)) != ESP_OK) return;
    if (count > 32U) count = 32U;
    for (uint8_t index = 0U; index < count; ++index) {
        uint8_t report[2] = {0xffU, 0xffU};
        if (keyboard_read(KEYBOARD_REG_HID_EVENT, report, sizeof(report)) != ESP_OK ||
            (report[0] == 0xffU && report[1] == 0xffU)) {
            break;
        }
        submit_report(report[0], report[1]);
    }
}

const char *tab5_keyboard_name(void)
{
    return keyboard_name;
}

bool tab5_keyboard_present(void)
{
    return keyboard_present;
}
