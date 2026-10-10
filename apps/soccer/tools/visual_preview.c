/* Host-only fixture: executes the production renderer into a clipped RGB565 buffer.
   No alternative artwork or animation implementation is used for the previews. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/render.c"
static tabos_color_t pixels[320 * 240];

int tabos_graphics_fill_rect(tabos_graphics_t* g, int32_t x, int32_t y, uint32_t w, uint32_t h, tabos_color_t c)
{
    (void) g;
    for (int yy = y; yy < y + (int) h; ++yy) {
        for (int xx = x; xx < x + (int) w; ++xx) {
            if (xx >= 0 && xx < 320 && yy >= 0 && yy < 240) {
                pixels[yy * 320 + xx] = c;
            }
        }
    }
    return 0;
}
int tabos_graphics_clear(tabos_graphics_t* g, tabos_color_t c)
{
    return tabos_graphics_fill_rect(g, 0, 0, 320, 240, c);
}
int tabos_graphics_pixel(tabos_graphics_t* g, int32_t x, int32_t y, tabos_color_t c)
{
    return tabos_graphics_fill_rect(g, x, y, 1, 1, c);
}
int tabos_graphics_line(tabos_graphics_t* g, int32_t x, int32_t y, int32_t xx, int32_t yy, tabos_color_t c)
{
    int dx = abs(xx - x), stepx = x < xx ? 1 : -1, dy = -abs(yy - y), stepy = y < yy ? 1 : -1, err = dx + dy;
    for (;;) {
        tabos_graphics_pixel(g, x, y, c);
        if (x == xx && y == yy) {
            break;
        }
        const int e = 2 * err;
        if (e >= dy) {
            err += dy;
            x   += stepx;
        }
        if (e <= dx) {
            err += dx;
            y   += stepy;
        }
    }
    return 0;
}
int tabos_graphics_rect(tabos_graphics_t* g, int32_t x, int32_t y, uint32_t w, uint32_t h, tabos_color_t c)
{
    box(g, x, y, w, 1, c);
    box(g, x, y + (int) h - 1, w, 1, c);
    box(g, x, y, 1, h, c);
    box(g, x + (int) w - 1, y, 1, h, c);
    return 0;
}
int tabos_graphics_present(tabos_graphics_t* g)
{
    (void) g;
    return 0;
}
static void save(const char* directory, const char* name)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.ppm", directory, name);
    FILE* file = fopen(path, "wb");
    if (!file) {
        perror(path);
        exit(1);
    }
    fprintf(file, "P6\n320 240\n255\n");
    for (unsigned i = 0; i < 320 * 240; ++i) {
        unsigned char rgb[3] = {(unsigned char) ((pixels[i] >> 11) * 255 / 31),
                                (unsigned char) (((pixels[i] >> 5) & 63) * 255 / 63),
                                (unsigned char) ((pixels[i] & 31) * 255 / 31)};
        fwrite(rgb, 1, 3, file);
    }
    fclose(file);
}
static const int directions[8][2] = {
    { 0, -1},
    { 1, -1},
    { 1,  0},
    { 1,  1},
    { 0,  1},
    {-1,  1},
    {-1,  0},
    {-1, -1}
};
static const char* labels[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
static void pose(soccer_game_t* game, soccer_visuals_t* visuals, unsigned direction, unsigned row, int x, int y)
{
    soccer_player_t* p = &game->players[0];
    p->position        = (soccer_vec_t) {x * 2 * SOCCER_ONE, y * 2 * SOCCER_ONE};
    p->facing_x        = directions[direction][0];
    p->facing_y        = directions[direction][1];
    p->moving          = row < 4;
    p->animation       = row * 8U;
    p->kick_ticks      = row == 5 ? 10U : 0U;
    if (row == 6) {
        p->kick_ticks = 3U;
    }
    game->shot_charge        = row == 4 ? 12U : 0U;
    visuals->moving[0]       = p->moving;
    visuals->run_distance[0] = row * 16U * SOCCER_ONE;
    visuals->action_x[0]     = p->facing_x;
    visuals->action_y[0]     = p->facing_y;
    visuals->action[0]       = row == 5U ? SOCCER_EVENT_SHOT : SOCCER_EVENT_PASS;
}
int main(int argc, char** argv)
{
    if (argc != 2) {
        return 1;
    }
    tabos_graphics_t g = {0};
    soccer_game_t game;
    soccer_game_reset(&game);
    game.mode              = SOCCER_PLAY;
    soccer_camera_t camera = {0};
    soccer_visuals_t visuals;
    soccer_visuals_reset(&visuals, &game);
    for (unsigned group = 0; group < 2; ++group) {
        for (unsigned version = 0; version < 2; ++version) {
            tabos_graphics_clear(&g, TABOS_RGB565(40, 131, 76));
            for (unsigned d = 0; d < 8; ++d) {
                text(&g, 14 + (int) d * 40, 2, labels[d], 1, WHITE);
                const unsigned rows = group == 0U ? 4U : 3U;
                for (unsigned row = 0; row < rows; ++row) {
                    pose(&game, &visuals, d, row + group * 4U, 20 + (int) d * 40,
                         group == 0U ? 50 + (int) row * 55 : 60 + (int) row * 70);
                    if (version) {
                        draw_trial_player(&g, &game, &camera, &visuals, 0U);
                    } else {
                        draw_player(&g, &game, &camera, 0);
                    }
                }
            }
            if (group == 0U) {
                save(argv[1], version ? "players-after" : "players-before");
            } else {
                save(argv[1], version ? "actions-after" : "actions-before");
            }
        }
    }
    for (unsigned version = 0; version < 2; ++version) {
        tabos_graphics_clear(&g, INK);
        for (unsigned stripe = 0; stripe < 3; ++stripe) {
            const tabos_color_t bg =
                stripe == 2 ? WHITE : TABOS_RGB565(40 + stripe * 5, 131 + stripe * 11, 76 + stripe * 6);
            box(&g, 0, 25 + (int) stripe * 65, 320, 60, bg);
            for (unsigned frame = 0; frame < 4; ++frame) {
                game.ball             = (soccer_vec_t) {(40 + (int) frame * 70) * 2 * SOCCER_ONE,
                                                        (60 + (int) stripe * 65) * 2 * SOCCER_ONE};
                game.ball_height      = 0;
                game.velocity         = (soccer_vec_t) {0, 0};
                visuals.ball_distance = frame * 8U * SOCCER_ONE;
                if (version) {
                    draw_trial_ball(&g, &game, &camera, &visuals);
                } else {
                    game.ball.x += (int) frame * SOCCER_ONE;
                    draw_ball(&g, &game, &camera);
                }
            }
        }
        centered(&g, 6, version ? "TRIAL BALL - FOUR PANELS" : "ORIGINAL BALL", 1, WHITE);
        save(argv[1], version ? "ball-after" : "ball-before");
    }
    for (unsigned frame = 0; frame < 64; ++frame) {
        tabos_graphics_clear(&g, TABOS_RGB565(40, 131, 76));
        centered(&g, 3, "TRIAL RUN / CHARGE / CONTACT", 1, WHITE);
        for (unsigned d = 0; d < 8; ++d) {
            unsigned row = (frame / 4U) % 4U;
            if (frame >= 32U && frame < 44U) {
                row = 4;
            } else if (frame >= 44U && frame < 48U) {
                row = 5;
            } else if (frame >= 48U && frame < 56U) {
                row = 6;
            }
            pose(&game, &visuals, d, row, 40 + (int) (d % 4) * 80, 65 + (int) (d / 4) * 70);
            draw_trial_player(&g, &game, &camera, &visuals, 0U);
            text(&g, 35 + (int) (d % 4) * 80, 75 + (int) (d / 4) * 70, labels[d], 1, WHITE);
        }
        text(&g, 5, 164, "STILL       ROLL       AIR", 1, WHITE);
        for (unsigned state = 0; state < 3; ++state) {
            game.ball             = (soccer_vec_t) {(40 + (int) state * 110) * 2 * SOCCER_ONE, 215 * 2 * SOCCER_ONE};
            game.ball_height      = state == 2 ? (int32_t) (frame < 32 ? frame : 63 - frame) * SOCCER_ONE : 0;
            visuals.ball_distance = state == 1 ? (frame % 4) * 8U * SOCCER_ONE : 0;
            box(&g, sx(&camera, game.ball.x) - 3, sy(&camera, game.ball.y) + 2, 7, 2, DARK);
            draw_trial_ball(&g, &game, &camera, &visuals);
        }
        char name[40];
        snprintf(name, sizeof(name), "motion-%03u", frame);
        save(argv[1], name);
    }
    soccer_game_reset(&game);
    game.mode                = SOCCER_PLAY;
    game.possession_ticks    = 1000;
    game.players[0].position = (soccer_vec_t) {780 * SOCCER_ONE, 480 * SOCCER_ONE};
    game.players[0].facing_x = 1;
    game.players[0].facing_y = -1;
    game.players[0].moving   = true;
    game.ball                = (soccer_vec_t) {798 * SOCCER_ONE, 472 * SOCCER_ONE};
    camera.position          = (soccer_vec_t) {480 * SOCCER_ONE, 240 * SOCCER_ONE};
    soccer_visuals_reset(&visuals, &game);
    visuals.moving[0]       = true;
    visuals.run_distance[0] = 16U * SOCCER_ONE;
    soccer_render(&g, &game, &camera, true);
    save(argv[1], "pitch-before");
    soccer_render_trial(&g, &game, &camera, true, &visuals);
    save(argv[1], "pitch-trial");
    soccer_game_reset(&game);
    game.mode                   = SOCCER_PLAY;
    camera.position             = (soccer_vec_t) {480 * SOCCER_ONE, 240 * SOCCER_ONE};
    game.players[0].position    = (soccer_vec_t) {680 * SOCCER_ONE, 380 * SOCCER_ONE};
    game.players[0].slide_ticks = 16U;
    game.players[0].slide_x     = 1;
    game.players[0].slide_y     = 1;
    game.players[5].position    = (soccer_vec_t) {920 * SOCCER_ONE, 380 * SOCCER_ONE};
    game.players[5].slide_ticks = 4U;
    game.players[5].slide_x     = -1;
    game.players[5].slide_y     = 1;
    game.keepers[0].position    = (soccer_vec_t) {680 * SOCCER_ONE, 620 * SOCCER_ONE};
    game.keepers[1].position    = (soccer_vec_t) {920 * SOCCER_ONE, 620 * SOCCER_ONE};
    game.dives[0]               = (soccer_dive_t) {.ticks = 40U, .direction = 1};
    game.dives[1]               = (soccer_dive_t) {.ticks = 40U, .direction = -1};
    soccer_visuals_reset(&visuals, &game);
    soccer_render_trial(&g, &game, &camera, true, &visuals);
    save(argv[1], "rollout-actions");
    soccer_game_reset(&game);
    game.mode             = SOCCER_PLAY;
    game.possession_ticks = 1000U; /* Keep possession for this presentation fixture. */
    soccer_camera_reset(&camera, &game);
    soccer_visuals_reset(&visuals, &game);
    for (unsigned frame = 0U; frame < 64U; ++frame) {
        for (unsigned tick = 0U; tick < 4U; ++tick) {
            soccer_input_t input = {0};
            if (frame < 48U) {
                input.dx = 1;
                input.dy = frame < 16U ? -1 : 0;
                if (frame >= 32U) {
                    input.dy = 1;
                }
            }
            input.shoot = frame == 48U && tick == 0U;
            soccer_game_arcade_step(&game, input);
            soccer_visuals_step(&visuals, &game);
            soccer_camera_step(&camera, &game);
        }
        char name[40];
        soccer_render(&g, &game, &camera, true);
        snprintf(name, sizeof(name), "play-before-%03u", frame);
        save(argv[1], name);
        soccer_render_trial(&g, &game, &camera, true, &visuals);
        snprintf(name, sizeof(name), "play-trial-%03u", frame);
        save(argv[1], name);
    }
    return 0;
}
