#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int digit(char character)
{
    return character >= '0' && character <= '9' ? character - '0' : -1;
}

static int parse_number(const char* text, size_t count)
{
    int value = 0;
    for (size_t index = 0U; index < count; ++index) {
        const int current = digit(text[index]);
        if (current < 0) {
            return -1;
        }
        value = value * 10 + current;
    }
    return value;
}

static bool leap_year(int year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static int month_days(int year, int month)
{
    static const uint8_t days[] = {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};
    return month == 2 && leap_year(year) ? 29 : days[month - 1];
}

static bool parse_datetime(const char* text, struct timespec* result)
{
    if (text == NULL || result == NULL || strlen(text) != 19U || text[4] != '-' || text[7] != '-' ||
        (text[10] != ' ' && text[10] != 'T') || text[13] != ':' || text[16] != ':') {
        return false;
    }
    const int year   = parse_number(text, 4U);
    const int month  = parse_number(text + 5, 2U);
    const int day    = parse_number(text + 8, 2U);
    const int hour   = parse_number(text + 11, 2U);
    const int minute = parse_number(text + 14, 2U);
    const int second = parse_number(text + 17, 2U);
    if (year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > month_days(year, month) || hour < 0 ||
        hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
        return false;
    }
    int64_t days = 0;
    for (int current = 1970; current < year; ++current) {
        days += leap_year(current) ? 366 : 365;
    }
    for (int current = 1; current < month; ++current) {
        days += month_days(year, current);
    }
    days            += day - 1;
    result->tv_sec   = (time_t) (days * 86400 + hour * 3600 + minute * 60 + second);
    result->tv_nsec  = 0L;
    return true;
}

static int show_date(void)
{
    static const char* const weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* const months[]   = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    const time_t now                    = time(NULL);
    if (now == (time_t) -1) {
        fprintf(stderr, "date: cannot read clock: %s\n", strerror(errno));
        return 1;
    }
    const struct tm* calendar = gmtime(&now);
    if (calendar == NULL) {
        fprintf(stderr, "date: cannot convert clock\n");
        return 1;
    }
    printf("%s %s %02d %02d:%02d:%02d UTC %04d\n", weekdays[calendar->tm_wday], months[calendar->tm_mon],
           calendar->tm_mday, calendar->tm_hour, calendar->tm_min, calendar->tm_sec, calendar->tm_year + 1900);
    return 0;
}

int main(int argc, char** argv)
{
    if (argc == 1 || (argc == 2 && strcmp(argv[1], "-u") == 0)) {
        return show_date();
    }
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        printf("Usage: date [-u]\n       date -s 'YYYY-MM-DD HH:MM:SS'\n");
        return 0;
    }
    if (argc == 3 && (strcmp(argv[1], "-s") == 0 || strcmp(argv[1], "--set") == 0)) {
        struct timespec value;
        if (!parse_datetime(argv[2], &value)) {
            fprintf(stderr, "date: invalid date; expected YYYY-MM-DD HH:MM:SS (2000-2099)\n");
            return 1;
        }
        if (clock_settime(CLOCK_REALTIME, &value) != 0) {
            fprintf(stderr, "date: cannot set clock: %s\n", strerror(errno));
            return 1;
        }
        return show_date();
    }
    fprintf(stderr, "Usage: date [-u]\n       date -s 'YYYY-MM-DD HH:MM:SS'\n");
    return 1;
}
