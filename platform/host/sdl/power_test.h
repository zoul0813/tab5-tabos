#ifndef TABOS_HOST_POWER_TEST_H
#define TABOS_HOST_POWER_TEST_H
#include <tabos/platform/platform.h>
typedef enum {
    HOST_POWER_FAIL_NONE,
    HOST_POWER_FAIL_BRIGHTNESS,
    HOST_POWER_FAIL_PREPARE,
    HOST_POWER_FAIL_SLEEP,
    HOST_POWER_FAIL_RESTORE
} host_power_failure_t;
void host_power_test_reset(void);
void host_power_test_advance_time(uint64_t milliseconds);
void host_power_test_activity(void (*inject)(void* context, uint64_t now_ms), void* context);
void host_power_test_wake(platform_power_wake_cause_t causes);
void host_power_test_fail(host_power_failure_t failure);
uint64_t host_power_time_offset(void);
uint8_t host_power_test_brightness(void);
#endif
