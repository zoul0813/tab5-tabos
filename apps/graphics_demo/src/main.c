#include <errno.h>
#include <stdio.h>
#include <tabos/graphics.h>
#include <tabos/input.h>
#include <tabos/runtime_time.h>

#define DEMO_SCREEN_WIDTH 320U
#define DEMO_SCREEN_HEIGHT 240U
#define DEMO_MOVE_STEP 8
#define DEMO_FRAME_INTERVAL_MS 16U

#define DEMO_PANEL_MARGIN 24U
#define DEMO_PLAYER_SIZE 64U
#define DEMO_PLAYER_INSET 12
#define DEMO_PLAYER_INNER_SIZE 40U

#define DEMO_SPRITE_WIDTH 16U
#define DEMO_SPRITE_HEIGHT 16U
#define DEMO_SPRITE_CHECKER_SIZE 4U
#define DEMO_SPRITE_OFFSET 16
#define DEMO_SPRITE_DRAW_WIDTH 32U
#define DEMO_SPRITE_DRAW_HEIGHT 32U
#define DEMO_SPRITE_OPACITY 192U
#define DEMO_ROTATION_COUNT 4
#define DEMO_VGA_COLOR_COUNT 16U

#define DEMO_COLOR_BACKGROUND TABOS_RGB565(8, 16, 32)
#define DEMO_COLOR_PANEL TABOS_RGB565(20, 40, 72)
#define DEMO_COLOR_PLAYER TABOS_RGB565(255, 176, 32)
#define DEMO_COLOR_PLAYER_INNER TABOS_RGB565(48, 128, 255)
#define DEMO_COLOR_SPRITE_KEY TABOS_RGB565(255, 255, 255)
#define DEMO_COLOR_SPRITE_FILL TABOS_RGB565(255, 64, 96)

static const tabos_color_t demo_vga_palette[DEMO_VGA_COLOR_COUNT] = {
    TABOS_RGB565(0, 0, 0),
    TABOS_RGB565(0, 0, 170),
    TABOS_RGB565(0, 170, 0),
    TABOS_RGB565(0, 170, 170),
    TABOS_RGB565(170, 0, 0),
    TABOS_RGB565(170, 0, 170),
    TABOS_RGB565(170, 85, 0),
    TABOS_RGB565(170, 170, 170),
    TABOS_RGB565(85, 85, 85),
    TABOS_RGB565(85, 85, 255),
    TABOS_RGB565(85, 255, 85),
    TABOS_RGB565(85, 255, 255),
    TABOS_RGB565(255, 85, 85),
    TABOS_RGB565(255, 85, 255),
    TABOS_RGB565(255, 255, 85),
    TABOS_RGB565(255, 255, 255),
};

#if DEMO_SCREEN_WIDTH < DEMO_PLAYER_SIZE || DEMO_SCREEN_HEIGHT < DEMO_PLAYER_SIZE
#error "Demo screen must fit the player"
#endif

#if DEMO_SCREEN_WIDTH < DEMO_PANEL_MARGIN * 2U || \
    DEMO_SCREEN_HEIGHT < DEMO_PANEL_MARGIN * 2U
#error "Demo screen must fit the panel margins"
#endif

#if DEMO_PLAYER_INSET < 0 || \
    DEMO_PLAYER_INSET + DEMO_PLAYER_INNER_SIZE > DEMO_PLAYER_SIZE
#error "Player inner rectangle must fit the player"
#endif

#if DEMO_SPRITE_OFFSET < 0 ||                                         \
    DEMO_SPRITE_OFFSET + DEMO_SPRITE_DRAW_WIDTH > DEMO_PLAYER_SIZE || \
    DEMO_SPRITE_OFFSET + DEMO_SPRITE_DRAW_HEIGHT > DEMO_PLAYER_SIZE
#error "Rendered sprite must fit the player"
#endif

int main(void) {
    tabos_graphics_t graphics = {
        .width = DEMO_SCREEN_WIDTH,
        .height = DEMO_SCREEN_HEIGHT,
    };
    if (tabos_graphics_open(&graphics) != 0) {
        fprintf(stderr, "graphics-demo: open failed (errno %d)\n", errno);
        return 1;
    }

    int32_t x = (int32_t)(graphics.width - DEMO_PLAYER_SIZE) / 2;
    int32_t y = (int32_t)(graphics.height - DEMO_PLAYER_SIZE) / 2;
    tabos_color_t sprite[DEMO_SPRITE_WIDTH * DEMO_SPRITE_HEIGHT];
    for (uint32_t row = 0U; row < DEMO_SPRITE_HEIGHT; ++row) {
        for (uint32_t column = 0U; column < DEMO_SPRITE_WIDTH; ++column) {
            sprite[row * DEMO_SPRITE_WIDTH + column] =
                ((row / DEMO_SPRITE_CHECKER_SIZE) +
                 (column / DEMO_SPRITE_CHECKER_SIZE)) %
                            2U ==
                        0U
                    ? DEMO_COLOR_SPRITE_KEY
                    : DEMO_COLOR_SPRITE_FILL;
        }
    }
    bool running = true;
    tabos_graphics_rotation_t rotation = TABOS_GRAPHICS_ROTATE_0;
    unsigned int palette_index = 0U;

    while (running) {
        tabos_input_event_t event;
        while (tabos_input_poll(&event)) {
            if (event.type != TABOS_INPUT_KEY_DOWN) continue;
            switch (event.key) {
                case TABOS_KEY_UP:
                    palette_index = (palette_index + 1U) % DEMO_VGA_COLOR_COUNT;
                    (void)tabos_graphics_set_letterbox_color(
                        &graphics, demo_vga_palette[palette_index]);
                    break;
                case TABOS_KEY_DOWN:
                    palette_index = (palette_index + DEMO_VGA_COLOR_COUNT - 1U) %
                                    DEMO_VGA_COLOR_COUNT;
                    (void)tabos_graphics_set_letterbox_color(
                        &graphics, demo_vga_palette[palette_index]);
                    break;
                // EASD because Tab5 keyboards WASD isn't naturally spaced
                case TABOS_KEY_E:
                    y -= DEMO_MOVE_STEP;
                    break;
                case TABOS_KEY_S:
                    y += DEMO_MOVE_STEP;
                    break;
                case TABOS_KEY_A:
                    x -= DEMO_MOVE_STEP;
                    break;
                case TABOS_KEY_D:
                    x += DEMO_MOVE_STEP;
                    break;
                case TABOS_KEY_Q:
                    running = false;
                    break;
                case TABOS_KEY_R:
                    rotation = (tabos_graphics_rotation_t)((rotation + 1) %
                                                           DEMO_ROTATION_COUNT);
                    break;
                default:
                    break;
            }
        }
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x > (int32_t)(graphics.width - DEMO_PLAYER_SIZE))
            x = (int32_t)(graphics.width - DEMO_PLAYER_SIZE);
        if (y > (int32_t)(graphics.height - DEMO_PLAYER_SIZE))
            y = (int32_t)(graphics.height - DEMO_PLAYER_SIZE);

        (void)tabos_graphics_clear(&graphics, DEMO_COLOR_BACKGROUND);
        (void)tabos_graphics_fill_rect(&graphics, DEMO_PANEL_MARGIN, DEMO_PANEL_MARGIN,
                                       graphics.width - DEMO_PANEL_MARGIN * 2U,
                                       graphics.height - DEMO_PANEL_MARGIN * 2U, DEMO_COLOR_PANEL);
        (void)tabos_graphics_fill_rect(&graphics, x, y, DEMO_PLAYER_SIZE,
                                       DEMO_PLAYER_SIZE, DEMO_COLOR_PLAYER);
        (void)tabos_graphics_fill_rect(&graphics, x + DEMO_PLAYER_INSET,
                                       y + DEMO_PLAYER_INSET, DEMO_PLAYER_INNER_SIZE,
                                       DEMO_PLAYER_INNER_SIZE, DEMO_COLOR_PLAYER_INNER);
        const tabos_graphics_blit_options_t transformed = {
            .pixels = sprite,
            .bitmap_width = DEMO_SPRITE_WIDTH,
            .bitmap_height = DEMO_SPRITE_HEIGHT,
            .source = {.width = DEMO_SPRITE_WIDTH, .height = DEMO_SPRITE_HEIGHT},
            .destination = {
                .x = x + DEMO_SPRITE_OFFSET,
                .y = y + DEMO_SPRITE_OFFSET,
                .width = DEMO_SPRITE_DRAW_WIDTH,
                .height = DEMO_SPRITE_DRAW_HEIGHT,
            },
            .rotation = rotation,
            .opacity = DEMO_SPRITE_OPACITY,
            .color_key_enabled = true,
            .color_key_low = DEMO_COLOR_SPRITE_KEY,
            .color_key_high = DEMO_COLOR_SPRITE_KEY,
        };
        (void)tabos_graphics_blit_ex(&graphics, &transformed);
        (void)tabos_graphics_present(&graphics);
    }

    return tabos_graphics_close(&graphics) == 0 ? 0 : 1;
}
