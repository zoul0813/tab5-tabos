#include <tabos/internal/wall_clock.h>

#include <stddef.h>

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

static int64_t days_from_civil(int32_t year, uint8_t month, uint8_t day)
{
    int64_t adjusted_year = year;
    if (month <= 2U) {
        --adjusted_year;
    }
    const int64_t era             = adjusted_year >= 0 ? adjusted_year / 400 : (adjusted_year - 399) / 400;
    const uint32_t year_of_era    = (uint32_t) (adjusted_year - era * 400);
    const uint32_t adjusted_month = month > 2U ? (uint32_t) month - 3U : (uint32_t) month + 9U;
    const uint32_t day_of_year    = (153U * adjusted_month + 2U) / 5U + (uint32_t) day - 1U;
    const uint32_t day_of_era     = year_of_era * 365U + year_of_era / 4U - year_of_era / 100U + day_of_year;
    return era * 146097 + (int64_t) day_of_era - 719468;
}

bool wall_clock_datetime_valid(const tabos_datetime_t* datetime)
{
    if (datetime == NULL || datetime->year < 1970 || datetime->year > 9999 || datetime->month < 1U ||
        datetime->month > 12U || datetime->day < 1U || datetime->hour > 23U || datetime->minute > 59U ||
        datetime->second > 59U || datetime->weekday > 6U) {
        return false;
    }
    return datetime->day <= days_in_month(datetime->year, datetime->month);
}

bool wall_clock_datetime_to_epoch(const tabos_datetime_t* datetime, int64_t* seconds)
{
    if (!wall_clock_datetime_valid(datetime) || seconds == NULL) {
        return false;
    }
    const int64_t days = days_from_civil(datetime->year, datetime->month, datetime->day);
    *seconds = days * 86400 + (int64_t) datetime->hour * 3600 + (int64_t) datetime->minute * 60 + datetime->second;
    return true;
}

bool wall_clock_epoch_to_datetime(int64_t seconds, tabos_datetime_t* datetime)
{
    if (datetime == NULL || seconds < 0) {
        return false;
    }
    int64_t days                = seconds / 86400;
    const int64_t daily         = seconds % 86400;
    const int64_t weekday_days  = days + 4;
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
        .weekday = (uint8_t) (weekday_days % 7),
        .hour    = (uint8_t) (daily / 3600),
        .minute  = (uint8_t) ((daily % 3600) / 60),
        .second  = (uint8_t) (daily % 60),
    };
    return true;
}
