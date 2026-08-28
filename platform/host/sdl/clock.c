#include <tabos/platform/platform.h>

#include <limits.h>
#include <time.h>

static int64_t wall_clock_offset;

bool platform_wall_clock_get(int64_t* seconds)
{
    if (seconds == NULL) {
        return false;
    }
    const time_t now = time(NULL);
    if (now == (time_t) -1) {
        return false;
    }
    const int64_t current = (int64_t) now;
    if ((wall_clock_offset > 0 && current > INT64_MAX - wall_clock_offset) ||
        (wall_clock_offset < 0 && current < INT64_MIN - wall_clock_offset)) {
        return false;
    }
    *seconds = current + wall_clock_offset;
    return true;
}

bool platform_wall_clock_set(int64_t seconds)
{
    if (seconds < 0) {
        return false;
    }
    const time_t now = time(NULL);
    if (now == (time_t) -1) {
        return false;
    }
    wall_clock_offset = seconds - (int64_t) now;
    return true;
}
