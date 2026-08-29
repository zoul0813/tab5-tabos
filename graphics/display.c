#include <tabos/internal/display.h>
#include <tabos/battery.h>

#include <string.h>

enum {
    OVERLAY_MARGIN     = 10,
    OVERLAY_HEIGHT     = 22,
    OVERLAY_WIDTH      = 144,
    OVERLAY_REFRESH_MS = 1000,
    DIGIT_WIDTH        = 3,
    DIGIT_HEIGHT       = 5,
    DIGIT_SCALE        = 3,
    DIGIT_ADVANCE      = 12,
};

static platform_framebuffer_t framebuffer;
static bool display_initialized;
static uint32_t overlay_flags = TABOS_GRAPHICS_OVERLAY_ALL;
static uint64_t overlay_refresh_at;
static platform_battery_status_t overlay_battery;
static platform_network_status_t overlay_network;
static platform_pixel_t overlay_saved[OVERLAY_WIDTH * OVERLAY_HEIGHT];

static const uint8_t digits[10][DIGIT_HEIGHT] = {
    {0x7U, 0x5U, 0x5U, 0x5U, 0x7U},
    {0x2U, 0x6U, 0x2U, 0x2U, 0x7U},
    {0x7U, 0x1U, 0x7U, 0x4U, 0x7U},
    {0x7U, 0x1U, 0x7U, 0x1U, 0x7U},
    {0x5U, 0x5U, 0x7U, 0x1U, 0x1U},
    {0x7U, 0x4U, 0x7U, 0x1U, 0x7U},
    {0x7U, 0x4U, 0x7U, 0x5U, 0x7U},
    {0x7U, 0x1U, 0x1U, 0x1U, 0x1U},
    {0x7U, 0x5U, 0x7U, 0x5U, 0x7U},
    {0x7U, 0x5U, 0x7U, 0x1U, 0x7U},
};
static const uint8_t percent_glyph[DIGIT_HEIGHT] = {0x19U, 0x1aU, 0x04U, 0x0bU, 0x13U};

static void fill(int x, int y, int width, int height, platform_pixel_t color)
{
    for (int row = 0; row < height; ++row) {
        platform_pixel_t* pixel = framebuffer.pixels + (size_t) (y + row) * framebuffer.stride_pixels + (size_t) x;
        for (int column = 0; column < width; ++column) {
            pixel[column] = color;
        }
    }
}

static void draw_digit(int x, int y, unsigned int digit, platform_pixel_t color)
{
    for (unsigned int row = 0U; row < DIGIT_HEIGHT; ++row) {
        for (unsigned int column = 0U; column < DIGIT_WIDTH; ++column) {
            if ((digits[digit][row] & (1U << (DIGIT_WIDTH - 1U - column))) != 0U) {
                fill(x + (int) column * DIGIT_SCALE, y + (int) row * DIGIT_SCALE, DIGIT_SCALE, DIGIT_SCALE, color);
            }
        }
    }
}

static int draw_percentage(int x, int y, uint32_t percentage, platform_pixel_t color)
{
    if (percentage >= 100U) {
        draw_digit(x, y, 1U, color);
        draw_digit(x + DIGIT_ADVANCE, y, 0U, color);
        draw_digit(x + DIGIT_ADVANCE * 2, y, 0U, color);
        x += DIGIT_ADVANCE * 3;
    } else {
        if (percentage >= 10U) {
            draw_digit(x, y, percentage / 10U, color);
            x += DIGIT_ADVANCE;
        }
        draw_digit(x, y, percentage % 10U, color);
        x += DIGIT_ADVANCE;
    }
    for (unsigned int row = 0U; row < DIGIT_HEIGHT; ++row) {
        for (unsigned int column = 0U; column < 5U; ++column) {
            if ((percent_glyph[row] & (1U << (4U - column))) != 0U) {
                fill(x + (int) column * DIGIT_SCALE, y + (int) row * DIGIT_SCALE, DIGIT_SCALE, DIGIT_SCALE, color);
            }
        }
    }
    return x + 5 * DIGIT_SCALE;
}

static int draw_battery(int x, int y, platform_pixel_t color)
{
    const int width = 26;
    fill(x, y, width, 2, color);
    fill(x, y + 14, width, 2, color);
    fill(x, y + 2, 2, 12, color);
    fill(x + width - 2, y + 2, 2, 12, color);
    fill(x + width, y + 5, 3, 6, color);
    const uint32_t level = overlay_battery.percentage > 100U ? 100U : overlay_battery.percentage;
    const int fill_width = (int) (level * 20U / 100U);
    if (fill_width > 0) {
        const platform_pixel_t level_color =
            overlay_battery.charge_state == TABOS_BATTERY_STATE_CHARGING ? TABOS_RGB565(48, 208, 80) : color;
        fill(x + 3, y + 3, fill_width, 10, level_color);
    }
    return x + width + 8;
}

static void draw_bolt_shape(int x, int y, platform_pixel_t color)
{
    static const uint8_t starts[] = {6U, 5U, 5U, 4U, 3U, 2U, 1U, 6U, 5U, 5U, 4U, 3U, 3U, 2U, 1U};
    static const uint8_t widths[] = {4U, 5U, 4U, 4U, 7U, 9U, 9U, 4U, 4U, 3U, 3U, 3U, 2U, 2U, 2U};
    for (size_t row = 0U; row < sizeof(starts); ++row) {
        fill(x + starts[row], y + (int) row, widths[row], 1, color);
    }
}

static void draw_charging_bolt(int x, int y)
{
    draw_bolt_shape(x + 1, y + 1, TABOS_RGB565(0, 0, 0));
    draw_bolt_shape(x, y, TABOS_RGB565(255, 224, 32));
}

static int draw_wifi(int x, int y, platform_pixel_t color)
{
    unsigned int bars = 4U;
    if (overlay_network.state == PLATFORM_NETWORK_ONLINE) {
        if (overlay_network.signal_dbm <= -80) {
            bars = 1U;
        } else if (overlay_network.signal_dbm <= -67) {
            bars = 2U;
        } else if (overlay_network.signal_dbm <= -55) {
            bars = 3U;
        }
    }
    for (unsigned int bar = 0U; bar < 4U; ++bar) {
        if (bar < bars) {
            const int height = 4 + (int) bar * 4;
            fill(x + (int) bar * 6, y + 16 - height, 4, height, color);
        }
    }
    return x + 24;
}

static void refresh_overlay(void)
{
    const uint64_t now = platform_time_ms();
    if (overlay_refresh_at != 0U && now < overlay_refresh_at) {
        return;
    }
    overlay_battery = (platform_battery_status_t) {0};
    overlay_network = (platform_network_status_t) {0};
    (void) platform_battery_status(&overlay_battery);
    (void) platform_network_status(&overlay_network);
    overlay_refresh_at = now + OVERLAY_REFRESH_MS;
}

static bool present_with_overlay(bool graphics)
{
    if (!display_initialized) {
        return false;
    }
    refresh_overlay();
    const bool battery_visible = (overlay_flags & TABOS_GRAPHICS_OVERLAY_BATTERY) != 0U && overlay_battery.available &&
                                 (overlay_battery.charge_state == TABOS_BATTERY_STATE_CHARGING ||
                                  overlay_battery.charge_state == TABOS_BATTERY_STATE_DISCHARGING);
    const bool wifi_visible =
        (overlay_flags & TABOS_GRAPHICS_OVERLAY_WIFI) != 0U &&
        (overlay_network.state == PLATFORM_NETWORK_ONLINE || overlay_network.state == PLATFORM_NETWORK_STARTING ||
         overlay_network.state == PLATFORM_NETWORK_CONNECTING);
    if (!battery_visible && !wifi_visible) {
        return graphics ? platform_graphics_present(&framebuffer) : platform_display_present(&framebuffer);
    }
    const int left = (int) framebuffer.width - OVERLAY_MARGIN - OVERLAY_WIDTH;
    const int top  = OVERLAY_MARGIN;
    for (int row = 0; row < OVERLAY_HEIGHT; ++row) {
        memcpy(overlay_saved + row * OVERLAY_WIDTH,
               framebuffer.pixels + (size_t) (top + row) * framebuffer.stride_pixels + (size_t) left,
               sizeof(platform_pixel_t) * OVERLAY_WIDTH);
    }
    int x                        = left + OVERLAY_WIDTH;
    const platform_pixel_t white = TABOS_RGB565(255, 255, 255);
    if (battery_visible) {
        const uint32_t digits_width  = overlay_battery.percentage >= 100U ? 51U :
                                       overlay_battery.percentage >= 10U  ? 39U :
                                                                            27U;
        x                           -= (int) digits_width;
        (void) draw_percentage(x, top + 3, overlay_battery.percentage, white);
        if (overlay_battery.charge_state == TABOS_BATTERY_STATE_CHARGING) {
            x -= 17;
            draw_charging_bolt(x, top + 3);
        }
        x -= 34;
        (void) draw_battery(x, top + 3, white);
    }
    if (wifi_visible) {
        x -= 30;
        const platform_pixel_t color =
            overlay_network.state == PLATFORM_NETWORK_ONLINE ? white : TABOS_RGB565(112, 112, 112);
        (void) draw_wifi(x, top + 3, color);
    }
    const bool result = graphics ? platform_graphics_present(&framebuffer) : platform_display_present(&framebuffer);
    for (int row = 0; row < OVERLAY_HEIGHT; ++row) {
        memcpy(framebuffer.pixels + (size_t) (top + row) * framebuffer.stride_pixels + (size_t) left,
               overlay_saved + row * OVERLAY_WIDTH, sizeof(platform_pixel_t) * OVERLAY_WIDTH);
    }
    return result;
}

bool display_init(void)
{
    if (display_initialized) {
        return true;
    }

    if (!platform_display_init(&framebuffer)) {
        return false;
    }

    if (framebuffer.pixels == NULL || framebuffer.width != TABOS_DISPLAY_WIDTH ||
        framebuffer.height != TABOS_DISPLAY_HEIGHT || framebuffer.stride_pixels < TABOS_DISPLAY_WIDTH) {
        platform_display_shutdown();
        framebuffer = (platform_framebuffer_t) {0};
        return false;
    }

    display_initialized = true;
    return true;
}

bool display_present(void)
{
    return present_with_overlay(false);
}

bool display_graphics_present(void)
{
    return present_with_overlay(true);
}

void display_overlay_set_flags(uint32_t flags)
{
    overlay_flags = flags & TABOS_GRAPHICS_OVERLAY_ALL;
}

platform_framebuffer_t* display_framebuffer(void)
{
    return display_initialized ? &framebuffer : NULL;
}

void display_shutdown(void)
{
    if (!display_initialized) {
        return;
    }

    platform_display_shutdown();
    framebuffer         = (platform_framebuffer_t) {0};
    display_initialized = false;
    overlay_flags       = TABOS_GRAPHICS_OVERLAY_ALL;
    overlay_refresh_at  = 0U;
}
