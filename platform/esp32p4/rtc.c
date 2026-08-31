#include "internal.h"

#include <tabos/internal/wall_clock.h>
#include <tabos/platform/platform.h>

#include <tabos/config/identity.h>

#include <bsp/esp-bsp.h>
#include <driver/i2c_master.h>
#include <esp_log.h>

#include <errno.h>
#include <string.h>

enum {
    RX8130_ADDRESS                 = 0x32,
    RX8130_TIME_REGISTER           = 0x10,
    RX8130_FLAG_REGISTER           = 0x1d,
    RX8130_CONTROL0_REGISTER       = 0x1e,
    RX8130_CONTROL1_REGISTER       = 0x1f,
    RX8130_DIGITAL_OFFSET_REGISTER = 0x30,
};

static const char* const TAG = TABOS_PLATFORM_LOG_TAG;
static i2c_master_dev_handle_t rtc_device;
static bool rtc_available;
static bool rtc_detected;
static int rtc_error;

static uint8_t bcd_decode(uint8_t value)
{
    return (uint8_t) ((value >> 4U) * 10U + (value & 0x0fU));
}

static uint8_t bcd_encode(uint8_t value)
{
    return (uint8_t) (((value / 10U) << 4U) | (value % 10U));
}

static bool bcd_valid(uint8_t value)
{
    return (value & 0x0fU) <= 9U && ((value >> 4U) & 0x0fU) <= 9U;
}

static bool rtc_read(uint8_t reg, uint8_t* data, size_t size)
{
    return rtc_device != NULL && data != NULL && size > 0U &&
           i2c_master_transmit_receive(rtc_device, &reg, sizeof(reg), data, size, 100) == ESP_OK;
}

static bool rtc_write(uint8_t reg, const uint8_t* data, size_t size)
{
    if (rtc_device == NULL || data == NULL || size == 0U || size > 7U) {
        return false;
    }
    uint8_t transaction[8];
    transaction[0] = reg;
    memcpy(&transaction[1], data, size);
    return i2c_master_transmit(rtc_device, transaction, size + 1U, 100) == ESP_OK;
}

bool tab5_rtc_init(void)
{
    if (rtc_device != NULL) {
        return rtc_available;
    }
    rtc_detected = false;
    rtc_error    = 0;
    if (bsp_i2c_init() != ESP_OK || i2c_master_probe(bsp_i2c_get_handle(), RX8130_ADDRESS, 100) != ESP_OK) {
        ESP_LOGW(TAG, "RX8130 RTC not detected");
        return false;
    }
    rtc_detected                     = true;
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = RX8130_ADDRESS,
        .scl_speed_hz    = 400000U,
    };
    if (i2c_master_bus_add_device(bsp_i2c_get_handle(), &config, &rtc_device) != ESP_OK) {
        ESP_LOGW(TAG, "Could not attach RX8130 RTC driver");
        rtc_error = EIO;
        return false;
    }
    uint8_t control1       = 0U;
    const uint8_t disabled = 0U;
    if (!rtc_read(RX8130_CONTROL1_REGISTER, &control1, sizeof(control1))) {
        rtc_available = false;
    } else {
        control1      |= 0x30U;
        rtc_available  = rtc_write(RX8130_CONTROL1_REGISTER, &control1, sizeof(control1)) &&
                        rtc_write(RX8130_DIGITAL_OFFSET_REGISTER, &disabled, sizeof(disabled)) &&
                        rtc_write(RX8130_CONTROL0_REGISTER, &disabled, sizeof(disabled));
    }
    uint8_t flags = 0U;
    rtc_available = rtc_available && rtc_read(RX8130_FLAG_REGISTER, &flags, sizeof(flags));
    if (!rtc_available) {
        (void) i2c_master_bus_rm_device(rtc_device);
        rtc_device = NULL;
        ESP_LOGW(TAG, "RX8130 RTC did not respond");
        rtc_error = EIO;
        return false;
    }
    if ((flags & 0x80U) != 0U) {
        ESP_LOGW(TAG, "RX8130 RTC reports low-voltage history; verify wall-clock value");
    }
    ESP_LOGI(TAG, "RX8130 RTC detected");
    return true;
}

void tab5_rtc_shutdown(void)
{
    if (rtc_device != NULL) {
        (void) i2c_master_bus_rm_device(rtc_device);
        rtc_device = NULL;
    }
    rtc_available = false;
}

bool tab5_rtc_present(void)
{
    return rtc_available;
}

bool tab5_rtc_detected(void)
{
    return rtc_detected;
}

int tab5_rtc_error(void)
{
    return rtc_error;
}

bool platform_wall_clock_get(int64_t* seconds)
{
    if (!rtc_available || seconds == NULL) {
        return false;
    }
    uint8_t registers[7];
    if (!rtc_read(RX8130_TIME_REGISTER, registers, sizeof(registers))) {
        rtc_error = EIO;
        return false;
    }
    const uint8_t second  = registers[0] & 0x7fU;
    const uint8_t minute  = registers[1] & 0x7fU;
    const uint8_t hour    = registers[2] & 0x3fU;
    const uint8_t day     = registers[4] & 0x3fU;
    const uint8_t month   = registers[5] & 0x1fU;
    const uint8_t year    = registers[6];
    const uint8_t weekday = registers[3] & 0x7fU;
    if (!bcd_valid(second) || !bcd_valid(minute) || !bcd_valid(hour) || !bcd_valid(day) || !bcd_valid(month) ||
        !bcd_valid(year) || weekday == 0U || (weekday & (weekday - 1U)) != 0U) {
        rtc_error = EINVAL;
        return false;
    }
    tabos_datetime_t datetime = {
        .year    = 2000 + bcd_decode(year),
        .month   = bcd_decode(month),
        .day     = bcd_decode(day),
        .weekday = (uint8_t) __builtin_ctz((unsigned int) weekday),
        .hour    = bcd_decode(hour),
        .minute  = bcd_decode(minute),
        .second  = bcd_decode(second),
    };
    if (!wall_clock_datetime_to_epoch(&datetime, seconds)) {
        rtc_error = EINVAL;
        return false;
    }
    rtc_error = 0;
    return true;
}

bool platform_wall_clock_set(int64_t seconds)
{
    if (!rtc_available) {
        return false;
    }
    tabos_datetime_t datetime;
    if (!wall_clock_epoch_to_datetime(seconds, &datetime) || datetime.year < 2000 || datetime.year > 2099) {
        return false;
    }
    const uint8_t registers[7] = {
        bcd_encode(datetime.second),
        bcd_encode(datetime.minute),
        bcd_encode(datetime.hour),
        (uint8_t) (1U << datetime.weekday),
        bcd_encode(datetime.day),
        bcd_encode(datetime.month),
        bcd_encode((uint8_t) (datetime.year - 2000)),
    };
    if (!rtc_write(RX8130_TIME_REGISTER, registers, sizeof(registers))) {
        rtc_error = EIO;
        return false;
    }
    rtc_error = 0;
    return true;
}

bool platform_wall_clock_status(int* error)
{
    if (error != NULL) {
        *error = rtc_error;
    }
    return rtc_available && rtc_error == 0;
}
