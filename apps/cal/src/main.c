#include <stdio.h>
#include <time.h>

#define ANSI_REVERSE "\x1b[7m"
#define ANSI_BLACK          "\x1b[30m"
#define ANSI_RED            "\x1b[31m"
#define ANSI_GREEN          "\x1b[32m"
#define ANSI_YELLOW         "\x1b[33m"
#define ANSI_BLUE           "\x1b[34m"
#define ANSI_MAGENTA        "\x1b[35m"
#define ANSI_CYAN           "\x1b[36m"
#define ANSI_WHITE          "\x1b[37m"

#define ANSI_BRIGHT_BLACK   "\x1b[90m"
#define ANSI_BRIGHT_RED     "\x1b[91m"
#define ANSI_BRIGHT_GREEN   "\x1b[92m"
#define ANSI_BRIGHT_YELLOW  "\x1b[93m"
#define ANSI_BRIGHT_BLUE    "\x1b[94m"
#define ANSI_BRIGHT_MAGENTA "\x1b[95m"
#define ANSI_BRIGHT_CYAN    "\x1b[96m"
#define ANSI_BRIGHT_WHITE   "\x1b[97m"

#define ANSI_RESET          "\x1b[0m"

#define COLOR ANSI_GREEN

static const char *months[] = {
    "January", "February", "March", "April",
    "May", "June", "July", "August",
    "September", "October", "November", "December"
};

static int days_in_month(int year, int month)
{
    static const int days[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    if (month == 1) {
        int leap = (year % 4 == 0 && year % 100 != 0) ||
                   (year % 400 == 0);

        return leap ? 29 : 28;
    }

    return days[month];
}

int main(void)
{
    time_t now = time(NULL);
    struct tm *today = localtime(&now);

    int year  = today->tm_year + 1900;
    int month = today->tm_mon;
    int day   = today->tm_mday;

    /*
     * Find weekday of the first day of the month.
     * tm_wday: 0 = Sunday ... 6 = Saturday
     */
    struct tm first = *today;
    first.tm_mday = 1;
    mktime(&first);

    printf("     %s %d\n", months[month], year);
    printf("Su Mo Tu We Th Fr Sa\n");

    /* Indent to the first weekday. */
    for (int i = 0; i < first.tm_wday; ++i)
        printf("   ");

    int count = days_in_month(year, month);

    for (int d = 1; d <= count; ++d) {
        if (d == day)
            printf(COLOR "%2d" ANSI_RESET, d);
        else
            printf("%2d", d);

        if ((first.tm_wday + d) % 7 == 0)
            putchar('\n');
        else
            putchar(' ');
    }

    if ((first.tm_wday + count) % 7 != 0)
        putchar('\n');

    return 0;
}