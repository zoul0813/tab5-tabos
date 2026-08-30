#include <tabos/platform/platform.h>

#include <bsp/esp-bsp.h>
#include <bsp/m5stack_tab5.h>
#include <esp_io_expander.h>
#include <esp_log.h>
#include <esp_system.h>
#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <errno.h>

static const char* const TAG = "tabos_power";
static i2c_master_dev_handle_t battery_monitor;
static bool battery_charging_enabled = true;
static bool battery_fast_charging_enabled;
static bool battery_monitor_detected;
static int battery_monitor_error;

static bool battery_register(uint8_t reg, uint16_t value)
{
    uint8_t data[] = {reg, (uint8_t) (value >> 8U), (uint8_t) value};
    return i2c_master_transmit(battery_monitor, data, sizeof(data), 100) == ESP_OK;
}

static bool battery_read(uint8_t reg, uint16_t* value)
{
    uint8_t data[2];
    if (i2c_master_transmit_receive(battery_monitor, &reg, 1U, data, sizeof(data), 100) != ESP_OK) {
        return false;
    }
    *value = (uint16_t) data[0] << 8U | data[1];
    return true;
}

bool platform_battery_monitor_init(void)
{
    if (battery_monitor != NULL) {
        return true;
    }
    battery_monitor_detected         = false;
    battery_monitor_error            = 0;
    const i2c_device_config_t config = {.device_address = 0x41, .scl_speed_hz = 400000};
    if (i2c_master_bus_add_device(bsp_i2c_get_handle(), &config, &battery_monitor) != ESP_OK) {
        return false;
    }
    battery_monitor_detected = true;
    if (!battery_register(0x00, 0x4527U) || !battery_register(0x05, 1024U)) {
        battery_monitor_error = EIO;
        (void) i2c_master_bus_rm_device(battery_monitor);
        battery_monitor = NULL;
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(20U));
    return true;
}

bool platform_battery_monitor_detected(void)
{
    return battery_monitor_detected;
}

int platform_battery_monitor_error(void)
{
    return battery_monitor_error;
}

bool platform_battery_set_charging(bool enabled)
{
    esp_io_expander_handle_t expander = bsp_io_expander1_init();
    if (expander == NULL) {
        ESP_LOGW(TAG, "Could not initialize charger control expander");
        return false;
    }

    const uint32_t charger_enable = IO_EXPANDER_PIN_NUM_7;
    if (esp_io_expander_set_dir(expander, charger_enable, IO_EXPANDER_OUTPUT) != ESP_OK ||
        esp_io_expander_set_output_mode(expander, charger_enable, IO_EXPANDER_OUTPUT_MODE_PUSH_PULL) != ESP_OK ||
        esp_io_expander_set_level(expander, charger_enable, enabled ? 1U : 0U) != ESP_OK) {
        ESP_LOGW(TAG, "Could not set Tab5 battery charger state");
        return false;
    }
    ESP_LOGI(TAG, "Tab5 battery charger %s", enabled ? "enabled" : "disabled");
    battery_charging_enabled = enabled;
    return true;
}

bool platform_battery_status(platform_battery_status_t* status)
{
    if (status == NULL || !platform_battery_monitor_init()) {
        return false;
    }
    uint16_t voltage_raw;
    uint16_t current_raw;
    if (!battery_read(0x02, &voltage_raw) || !battery_read(0x04, &current_raw)) {
        return false;
    }
    const int32_t voltage_mv = (int32_t) voltage_raw * 125 / 100;
    const int32_t current_ma = (int16_t) current_raw;
    int32_t percentage       = (voltage_mv - 6000) * 100 / 2400;
    if (percentage < 0) {
        percentage = 0;
    } else if (percentage > 100) {
        percentage = 100;
    }
    uint32_t charge_state = 0U;
    if (current_ma < -20) {
        charge_state = 1U;
    } else if (current_ma > 20) {
        charge_state = 2U;
    } else if (voltage_mv >= 8200) {
        charge_state = 3U;
    }
    *status = (platform_battery_status_t) {
        .available             = true,
        .charging_enabled      = battery_charging_enabled,
        .fast_charging_enabled = battery_fast_charging_enabled,
        .voltage_mv            = (uint32_t) voltage_mv,
        .current_ma            = current_ma,
        .power_mw              = voltage_mv * current_ma / 1000,
        .percentage            = (uint32_t) percentage,
        .charge_state          = charge_state,
    };
    return true;
}

bool platform_battery_charging_enable(void)
{
    return platform_battery_set_charging(true);
}

bool platform_battery_set_fast_charging(bool enabled)
{
    esp_io_expander_handle_t expander = bsp_io_expander1_init();
    if (expander == NULL) {
        return false;
    }
    const uint32_t quick_charge_enable = IO_EXPANDER_PIN_NUM_5;
    if (esp_io_expander_set_dir(expander, quick_charge_enable, IO_EXPANDER_OUTPUT) != ESP_OK ||
        esp_io_expander_set_output_mode(expander, quick_charge_enable, IO_EXPANDER_OUTPUT_MODE_PUSH_PULL) != ESP_OK ||
        esp_io_expander_set_level(expander, quick_charge_enable, enabled ? 0U : 1U) != ESP_OK) {
        return false;
    }
    battery_fast_charging_enabled = enabled;
    return true;
}

static void power_off(void)
{
    esp_io_expander_handle_t expander = bsp_io_expander1_init();
    if (expander != NULL) {
        const uint32_t signal = IO_EXPANDER_PIN_NUM_4;
        (void) esp_io_expander_set_dir(expander, signal, IO_EXPANDER_OUTPUT);
        (void) esp_io_expander_set_output_mode(expander, signal, IO_EXPANDER_OUTPUT_MODE_PUSH_PULL);
        for (unsigned int attempt = 0U; attempt < 3U; ++attempt) {
            (void) esp_io_expander_set_level(expander, signal, 1U);
            vTaskDelay(pdMS_TO_TICKS(100U));
            (void) esp_io_expander_set_level(expander, signal, 0U);
            vTaskDelay(pdMS_TO_TICKS(100U));
        }
    } else {
        ESP_LOGE(TAG, "Could not initialize Tab5 power control");
    }
    ESP_LOGW(TAG, "Power remains present; Tab5 halted with services stopped");
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

void platform_perform_system_action(platform_system_action_t action)
{
    if (action == PLATFORM_SYSTEM_ACTION_REBOOT) {
        ESP_LOGI(TAG, "Restarting Tab5");
        esp_restart();
    }
    if (action == PLATFORM_SYSTEM_ACTION_POWER_OFF) {
        ESP_LOGI(TAG, "Powering off Tab5");
        power_off();
    }
}
