#include <tabos/clock.h>
#include <tabos/internal/elf_api.h>
#include <tabos/runtime_time.h>

#include <assert.h>
#include <errno.h>
#include <string.h>

const tabos_elf_api_t* tabos_runtime_api;
static int64_t epoch;

uint64_t tabos_monotonic_ms(void)
{
    return 0U;
}

static int get_epoch(tabos_elf_wall_time_t* value)
{
    value->seconds_low  = (uint32_t) epoch;
    value->seconds_high = (int32_t) ((uint64_t) epoch >> 32U);
    return 0;
}

int main(void)
{
    const tabos_elf_api_t api = {.wall_time_get = get_epoch};
    tabos_runtime_api         = &api;
    tabos_datetime_t value;
    epoch = INT64_C(253402300799);
    assert(tabos_clock_get(&value) == 0);
    assert(value.year == 9999 && value.month == 12U && value.day == 31U);
    assert(value.weekday == 5U && value.hour == 23U && value.minute == 59U && value.second == 59U);
    const tabos_datetime_t retained = value;
    const int64_t invalid[]         = {INT64_C(253402300800), INT64_MAX, INT64_MIN, -1};
    for (size_t index = 0U; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        epoch = invalid[index];
        assert(tabos_clock_get(&value) == -1);
        assert(errno == EOVERFLOW);
        assert(memcmp(&value, &retained, sizeof(value)) == 0);
    }
    epoch = 0;
    assert(tabos_clock_get(&value) == 0);
    assert(value.year == 1970 && value.month == 1U && value.day == 1U && value.weekday == 4U);
    return 0;
}
