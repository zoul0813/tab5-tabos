#include <tabos/clock.h>
#include <tabos/internal/elf_api.h>
#include <tabos/runtime_time.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

extern const tabos_elf_api_t* tabos_runtime_api;

static bool leap_year(int32_t year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static uint8_t days_in_month(int32_t year, uint8_t month)
{
    static const uint8_t days[] = {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};
    if (month == 2U && leap_year(year)) {
        return 29U;
    }
    return days[month - 1U];
}

static bool datetime_valid(const tabos_datetime_t* datetime)
{
    if (datetime == NULL || datetime->year < 1970 || datetime->year > 9999 || datetime->month < 1U ||
        datetime->month > 12U || datetime->day < 1U || datetime->hour > 23U || datetime->minute > 59U ||
        datetime->second > 59U || datetime->weekday > 6U) {
        return false;
    }
    return datetime->day <= days_in_month(datetime->year, datetime->month);
}

static bool datetime_to_epoch(const tabos_datetime_t* datetime, int64_t* seconds)
{
    if (!datetime_valid(datetime) || seconds == NULL) {
        return false;
    }
    int64_t year = datetime->year;
    if (datetime->month <= 2U) {
        --year;
    }
    const int64_t era          = year >= 0 ? year / 400 : (year - 399) / 400;
    const uint32_t year_of_era = (uint32_t) (year - era * 400);
    const uint32_t adjusted_month =
        datetime->month > 2U ? (uint32_t) datetime->month - 3U : (uint32_t) datetime->month + 9U;
    const uint32_t day_of_year = (153U * adjusted_month + 2U) / 5U + datetime->day - 1U;
    const uint32_t day_of_era  = year_of_era * 365U + year_of_era / 4U - year_of_era / 100U + day_of_year;
    const int64_t days         = era * 146097 + (int64_t) day_of_era - 719468;
    *seconds = days * 86400 + (int64_t) datetime->hour * 3600 + (int64_t) datetime->minute * 60 + datetime->second;
    return true;
}

static bool epoch_to_datetime(int64_t seconds, tabos_datetime_t* datetime)
{
    if (seconds < 0 || datetime == NULL) {
        return false;
    }
    int64_t days                = seconds / 86400;
    const int64_t daily         = seconds % 86400;
    const int64_t weekday_day   = days + 4;
    days                       += 719468;
    const int64_t era           = days >= 0 ? days / 146097 : (days - 146096) / 146097;
    const uint32_t day_of_era   = (uint32_t) (days - era * 146097);
    const uint32_t year_of_era  = (day_of_era - day_of_era / 1460U + day_of_era / 36524U - day_of_era / 146096U) / 365U;
    int32_t year                = (int32_t) year_of_era + (int32_t) era * 400;
    const uint32_t day_of_year  = day_of_era - (365U * year_of_era + year_of_era / 4U - year_of_era / 100U);
    const uint32_t month_prime  = (5U * day_of_year + 2U) / 153U;
    const uint8_t day           = (uint8_t) (day_of_year - (153U * month_prime + 2U) / 5U + 1U);
    const uint8_t month         = (uint8_t) (month_prime < 10U ? month_prime + 3U : month_prime - 9U);
    if (month <= 2U) {
        ++year;
    }
    if (year < 1970 || year > 9999) {
        return false;
    }
    *datetime = (tabos_datetime_t) {
        .year    = year,
        .month   = month,
        .day     = day,
        .weekday = (uint8_t) (weekday_day % 7),
        .hour    = (uint8_t) (daily / 3600),
        .minute  = (uint8_t) ((daily % 3600) / 60),
        .second  = (uint8_t) (daily % 60),
    };
    return true;
}

int tabos_clock_get_epoch(int64_t* seconds)
{
    if (seconds == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (tabos_runtime_api == NULL || tabos_runtime_api->wall_time_get == NULL) {
        errno = ENOSYS;
        return -1;
    }
    tabos_elf_wall_time_t runtime_time;
    const int result = tabos_runtime_api->wall_time_get(&runtime_time);
    if (result < 0) {
        errno = -result;
        return -1;
    }
    *seconds = (int64_t) ((uint64_t) runtime_time.seconds_low | (uint64_t) (uint32_t) runtime_time.seconds_high << 32U);
    return 0;
}

int tabos_clock_set_epoch(int64_t seconds)
{
    if (seconds < 0) {
        errno = EINVAL;
        return -1;
    }
    if (tabos_runtime_api == NULL || tabos_runtime_api->wall_time_set == NULL) {
        errno = ENOSYS;
        return -1;
    }
    const tabos_elf_wall_time_t runtime_time = {
        .seconds_low  = (uint32_t) seconds,
        .seconds_high = (int32_t) (seconds >> 32U),
    };
    const int result = tabos_runtime_api->wall_time_set(&runtime_time);
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return 0;
}

int tabos_clock_get(tabos_datetime_t* datetime)
{
    int64_t seconds = 0;
    if (datetime == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (tabos_clock_get_epoch(&seconds) != 0) {
        return -1;
    }
    if (!epoch_to_datetime(seconds, datetime)) {
        errno = EOVERFLOW;
        return -1;
    }
    return 0;
}

int tabos_clock_set(const tabos_datetime_t* datetime)
{
    int64_t seconds = 0;
    if (!datetime_to_epoch(datetime, &seconds)) {
        errno = EINVAL;
        return -1;
    }
    return tabos_clock_set_epoch(seconds);
}

int clock_gettime(clockid_t clock_id, struct timespec* time_value)
{
    if (time_value == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (clock_id == CLOCK_MONOTONIC) {
        const uint64_t milliseconds = tabos_monotonic_ms();
        time_value->tv_sec          = (time_t) (milliseconds / 1000U);
        time_value->tv_nsec         = (long) (milliseconds % 1000U) * 1000000L;
        return 0;
    }
    if (clock_id == CLOCK_REALTIME) {
        int64_t seconds = 0;
        if (tabos_clock_get_epoch(&seconds) != 0) {
            return -1;
        }
        time_value->tv_sec  = (time_t) seconds;
        time_value->tv_nsec = 0L;
        return 0;
    }
    errno = EINVAL;
    return -1;
}

int clock_settime(clockid_t clock_id, const struct timespec* time_value)
{
    if (clock_id != CLOCK_REALTIME || time_value == NULL || time_value->tv_sec < 0 || time_value->tv_nsec < 0L ||
        time_value->tv_nsec >= 1000000000L) {
        errno = EINVAL;
        return -1;
    }
    return tabos_clock_set_epoch((int64_t) time_value->tv_sec);
}
