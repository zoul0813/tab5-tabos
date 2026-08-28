#include <tabos/internal/wall_clock.h>

#include <assert.h>

static void expect_round_trip(const tabos_datetime_t* expected, int64_t epoch)
{
    int64_t converted_epoch = -1;
    tabos_datetime_t converted;
    assert(wall_clock_datetime_to_epoch(expected, &converted_epoch));
    assert(converted_epoch == epoch);
    assert(wall_clock_epoch_to_datetime(epoch, &converted));
    assert(converted.year == expected->year);
    assert(converted.month == expected->month);
    assert(converted.day == expected->day);
    assert(converted.weekday == expected->weekday);
    assert(converted.hour == expected->hour);
    assert(converted.minute == expected->minute);
    assert(converted.second == expected->second);
}

int main(void)
{
    const tabos_datetime_t epoch = {
        .year    = 1970,
        .month   = 1U,
        .day     = 1U,
        .weekday = 4U,
        .hour    = 0U,
        .minute  = 0U,
        .second  = 0U,
    };
    const tabos_datetime_t leap_day = {
        .year    = 2000,
        .month   = 2U,
        .day     = 29U,
        .weekday = 2U,
        .hour    = 12U,
        .minute  = 34U,
        .second  = 56U,
    };
    const tabos_datetime_t modern = {
        .year    = 2024,
        .month   = 1U,
        .day     = 1U,
        .weekday = 1U,
        .hour    = 0U,
        .minute  = 0U,
        .second  = 0U,
    };
    expect_round_trip(&epoch, 0);
    expect_round_trip(&leap_day, 951827696);
    expect_round_trip(&modern, 1704067200);

    tabos_datetime_t invalid = leap_day;
    invalid.year             = 2100;
    invalid.day              = 28U;
    assert(wall_clock_datetime_valid(&invalid));
    invalid.day = 29U;
    assert(!wall_clock_datetime_valid(&invalid));
    invalid       = modern;
    invalid.month = 13U;
    assert(!wall_clock_datetime_valid(&invalid));
    assert(!wall_clock_epoch_to_datetime(-1, &invalid));
    return 0;
}
