#include <tabos/platform/platform.h>

#include <tabos/battery.h>

#include <bsp/esp-bsp.h>
#include <bsp/m5stack_tab5.h>
#include <esp_io_expander.h>
#include <esp_log.h>
#include <esp_system.h>
#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <errno.h>

enum {
    INA226_ADDRESS             = 0x41,
    INA226_CONFIGURATION       = 0x00,
    INA226_BUS_VOLTAGE         = 0x02,
    INA226_CURRENT             = 0x04,
    INA226_CALIBRATION         = 0x05,
    INA226_CONFIGURATION_VALUE = 0x4527,
    INA226_CALIBRATION_VALUE   = 4096,
    BATTERY_EMPTY_MV           = 6000,
    BATTERY_FULL_MV            = 8230,
    BATTERY_FULL_THRESHOLD_MV  = 8200,
    BATTERY_ACTIVE_CURRENT_MA  = 20,
};

static const char* const TAG = "tabos_power";
static i2c_master_dev_handle_t battery_monitor;
static bool battery_charging_enabled = true;
static bool battery_fast_charging_enabled;
static bool battery_monitor_detected;
static int battery_monitor_error;
static int battery_charging_error;
static int battery_fast_charging_error;
static bool battery_charging_state_valid;
static bool battery_fast_charging_state_valid;

static bool battery_register(uint8_t reg, uint16_t value)
{
    uint8_t data[] = {reg, (uint8_t) (value >> 8U), (uint8_t) value};
    return battery_monitor != NULL && i2c_master_transmit(battery_monitor, data, sizeof(data), 100) == ESP_OK;
}

static bool battery_read(uint8_t reg, uint16_t* value)
{
    uint8_t data[2];
    if (battery_monitor == NULL || value == NULL ||
        i2c_master_transmit_receive(battery_monitor, &reg, 1U, data, sizeof(data), 100) != ESP_OK) {
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
    battery_monitor_detected = false;
    battery_monitor_error    = 0;
    if (bsp_i2c_init() != ESP_OK || i2c_master_probe(bsp_i2c_get_handle(), INA226_ADDRESS, 100) != ESP_OK) {
        return false;
    }
    battery_monitor_detected         = true;
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = INA226_ADDRESS,
        .scl_speed_hz    = 400000U,
    };
    if (i2c_master_bus_add_device(bsp_i2c_get_handle(), &config, &battery_monitor) != ESP_OK) {
        battery_monitor_error = EIO;
        return false;
    }
    if (!battery_register(INA226_CONFIGURATION, INA226_CONFIGURATION_VALUE) ||
        !battery_register(INA226_CALIBRATION, INA226_CALIBRATION_VALUE)) {
        battery_monitor_error = EIO;
        (void) i2c_master_bus_rm_device(battery_monitor);
        battery_monitor = NULL;
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(20U));
    battery_monitor_error = 0;
    return true;
}

bool platform_battery_monitor_detected(void)
{
    return battery_monitor_detected;
}

int platform_battery_monitor_error(void)
{
    int error = 0;
    (void) platform_battery_health(&error);
    return error;
}

bool platform_battery_set_charging(bool enabled)
{
    esp_io_expander_handle_t expander = bsp_io_expander1_init();
    if (expander == NULL) {
        ESP_LOGW(TAG, "Could not initialize charger control expander");
        battery_charging_error = EIO;
        return false;
    }

    const uint32_t charger_enable = IO_EXPANDER_PIN_NUM_7;
    if (esp_io_expander_set_dir(expander, charger_enable, IO_EXPANDER_OUTPUT) != ESP_OK ||
        esp_io_expander_set_output_mode(expander, charger_enable, IO_EXPANDER_OUTPUT_MODE_PUSH_PULL) != ESP_OK ||
        esp_io_expander_set_level(expander, charger_enable, enabled ? 1U : 0U) != ESP_OK) {
        ESP_LOGW(TAG, "Could not set Tab5 battery charger state");
        battery_charging_error = EIO;
        return false;
    }
    ESP_LOGI(TAG, "Tab5 battery charger %s", enabled ? "enabled" : "disabled");
    battery_charging_enabled     = enabled;
    battery_charging_state_valid = true;
    battery_charging_error       = 0;
    return true;
}

static int32_t quarter_ma_to_ma(int16_t raw)
{
    const int32_t value = raw;
    return value >= 0 ? (value + 2) / 4 : -((-value + 2) / 4);
}

bool platform_battery_status(platform_battery_status_t* status)
{
    if (status == NULL || !platform_battery_monitor_init()) {
        return false;
    }
    uint16_t voltage_raw;
    uint16_t current_raw;
    if (!battery_read(INA226_BUS_VOLTAGE, &voltage_raw) || !battery_read(INA226_CURRENT, &current_raw)) {
        battery_monitor_error = EIO;
        return false;
    }
    battery_monitor_error    = 0;
    const int32_t voltage_mv = ((int32_t) voltage_raw * 5 + 2) / 4;
    const int32_t current_ma = quarter_ma_to_ma((int16_t) current_raw);
    int32_t percentage       = (voltage_mv - BATTERY_EMPTY_MV) * 100 / (BATTERY_FULL_MV - BATTERY_EMPTY_MV);
    if (percentage < 0) {
        percentage = 0;
    } else if (percentage > 100) {
        percentage = 100;
    }
    uint32_t charge_state = TABOS_BATTERY_STATE_UNKNOWN;
    uint32_t valid        = TABOS_BATTERY_VALID_PERCENTAGE | TABOS_BATTERY_VALID_VOLTAGE | TABOS_BATTERY_VALID_CURRENT |
                     TABOS_BATTERY_VALID_POWER;
    bool external_power_present = false;
    if (current_ma < -BATTERY_ACTIVE_CURRENT_MA) {
        charge_state            = TABOS_BATTERY_STATE_CHARGING;
        external_power_present  = true;
        valid                  |= TABOS_BATTERY_VALID_STATE | TABOS_BATTERY_VALID_SOURCE;
    } else if (current_ma > BATTERY_ACTIVE_CURRENT_MA) {
        charge_state  = TABOS_BATTERY_STATE_DISCHARGING;
        valid        |= TABOS_BATTERY_VALID_STATE | TABOS_BATTERY_VALID_SOURCE;
    } else if (voltage_mv >= BATTERY_FULL_THRESHOLD_MV) {
        charge_state  = TABOS_BATTERY_STATE_FULL;
        valid        |= TABOS_BATTERY_VALID_STATE;
    }
    if (battery_charging_state_valid) {
        valid |= TABOS_BATTERY_VALID_CHARGING_CONTROL;
    }
    if (battery_fast_charging_state_valid) {
        valid |= TABOS_BATTERY_VALID_FAST_CHARGING_CONTROL;
    }
    *status = (platform_battery_status_t) {
        .available              = true,
        .external_power_present = external_power_present,
        .charging_enabled       = battery_charging_enabled,
        .fast_charging_enabled  = battery_fast_charging_enabled,
        .valid                  = valid,
        .voltage_mv             = (uint32_t) voltage_mv,
        .current_ma             = current_ma,
        .power_mw               = (int32_t) ((int64_t) voltage_mv * current_ma / 1000),
        .percentage             = (uint32_t) percentage,
        .charge_state           = charge_state,
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
        battery_fast_charging_error = EIO;
        return false;
    }
    const uint32_t quick_charge_enable = IO_EXPANDER_PIN_NUM_5;
    if (esp_io_expander_set_dir(expander, quick_charge_enable, IO_EXPANDER_OUTPUT) != ESP_OK ||
        esp_io_expander_set_output_mode(expander, quick_charge_enable, IO_EXPANDER_OUTPUT_MODE_PUSH_PULL) != ESP_OK ||
        esp_io_expander_set_level(expander, quick_charge_enable, enabled ? 0U : 1U) != ESP_OK) {
        ESP_LOGW(TAG, "Could not set Tab5 fast-charge state");
        battery_fast_charging_error = EIO;
        return false;
    }
    battery_fast_charging_enabled     = enabled;
    battery_fast_charging_state_valid = true;
    battery_fast_charging_error       = 0;
    return true;
}

bool platform_battery_health(int* error)
{
    const int current_error = battery_monitor_error != 0  ? battery_monitor_error :
                              battery_charging_error != 0 ? battery_charging_error :
                                                            battery_fast_charging_error;
    if (error != NULL) {
        *error = current_error;
    }
    return battery_monitor != NULL && current_error == 0;
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
