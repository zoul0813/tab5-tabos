#include <soccer/render.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define WHITE TABOS_RGB565(224, 240, 212)
#define INK   TABOS_RGB565(14, 28, 40)
#define GOLD  TABOS_RGB565(255, 212, 80)
#define RED   TABOS_RGB565(240, 76, 68)
#define DARK  TABOS_RGB565(27, 93, 67)
typedef struct {
        char character;
        uint8_t rows[7];
} glyph_t;
static const glyph_t glyphs[] = {
#include "../assets/font5x7.inc"
    {'/',    {1U, 2U, 2U, 4U, 8U, 8U, 16U}},
    {'%', {25U, 25U, 2U, 4U, 8U, 19U, 19U}},
};

static void text(tabos_graphics_t* graphics, int x, int y, const char* value, unsigned int scale, tabos_color_t color)
{
    while (*value != '\0') {
        const uint8_t* rows = glyphs[0].rows;
        for (size_t i = 0U; i < sizeof(glyphs) / sizeof(glyphs[0]); ++i) {
            if (glyphs[i].character == *value) {
                rows = glyphs[i].rows;
                break;
            }
        }
        for (unsigned int row = 0U; row < 7U; ++row) {
            for (unsigned int column = 0U; column < 5U; ++column) {
                if ((rows[row] & (1U << (4U - column))) != 0U) {
                    (void) tabos_graphics_fill_rect(graphics, x + (int) (column * scale), y + (int) (row * scale),
                                                    scale, scale, color);
                }
            }
        }
        x += (int) (6U * scale);
        ++value;
    }
}

static void centered(tabos_graphics_t* graphics, int y, const char* value, unsigned int scale, tabos_color_t color)
{
    text(graphics, (320 - (int) (strlen(value) * 6U * scale)) / 2, y, value, scale, color);
}


static void box(tabos_graphics_t* g, int x, int y, unsigned int w, unsigned int h, tabos_color_t color)
{
    (void) tabos_graphics_fill_rect(g, x, y, w, h, color);
}
static void ellipse(tabos_graphics_t* g, int x, int y, int rx, int ry, tabos_color_t color)
{
    for (int dy = -ry; dy <= ry; ++dy) {
        for (int dx = -rx; dx <= rx; ++dx) {
            const int distance = dx * dx * ry * ry + dy * dy * rx * rx;
            const int radius   = rx * rx * ry * ry;
            if (distance <= radius && distance >= radius - 2 * rx * ry * ry) {
                (void) tabos_graphics_pixel(g, x + dx, y + dy, color);
            }
        }
    }
}
static int sx(const soccer_camera_t* camera, int32_t x)
{
    return (x - camera->position.x) / (2 * SOCCER_ONE);
}
static int sy(const soccer_camera_t* camera, int32_t y)
{
    return (y - camera->position.y) / (2 * SOCCER_ONE);
}

#include "visual_trial.inc"

static void draw_ball(tabos_graphics_t* g, const soccer_game_t* game, const soccer_camera_t* camera)
{
    const int lift = game->ball_height / (2 * SOCCER_ONE);
    const int x = sx(camera, game->ball.x), y = sy(camera, game->ball.y) - lift;
    if (game->owner < 0 && game->keeper_owner < 0 && (abs(game->velocity.x) + abs(game->velocity.y) > 3 * SOCCER_ONE)) {
        for (int i = 2; i > 0; --i) {
            const int tail_x = sx(camera, game->ball.x - game->velocity.x * i);
            const int tail_y = sy(camera, game->ball.y - game->velocity.y * i) - lift;
            box(g, tail_x - 1, tail_y - 1, 2U, 2U, TABOS_RGB565(140, 185, 130));
        }
    }

    box(g, x - 2, y - 2, 5U, 5U, WHITE);
    box(g, x - 1, y - 3, 3U, 1U, WHITE);
    const int spin = (game->ball.x / SOCCER_ONE + game->ball.y / SOCCER_ONE) % 3;
    box(g, x - 1 + spin / 2, y - 1, 2U, 2U, INK);
}

static void draw_player(tabos_graphics_t* g, const soccer_game_t* game, const soccer_camera_t* camera,
                        unsigned int index)
{
    const bool goalkeeper         = index >= SOCCER_PLAYER_COUNT;
    const soccer_player_t* player = goalkeeper ? &game->keepers[index - SOCCER_PLAYER_COUNT] : &game->players[index];
    const int ground_y            = sy(camera, player->position.y);
    unsigned int jump_phase       = 0U;
    if (player->jump_ticks > 0U) {
        jump_phase = player->jump_ticks > 12U ? 24U - player->jump_ticks : player->jump_ticks;
    }
    const int x = sx(camera, player->position.x);
    int y       = ground_y - (int) jump_phase;

    /* Include extended boots, diving gloves and slide trails in the bounds;
       the graphics API clips any portion outside the viewport. */
    if (x < -26 || x > 346 || y < -26 || y - 30 >= 240) {
        return;
    }
    /* Poses use simulation ticks, so pause freezes them and rendering never advances play. */
    static const int strides[] = {0, 1, 2, 1, 0, -1, -2, -1};
    const unsigned int phase   = (player->animation / 4U) % 8U;
    const bool tackling        = !goalkeeper && player->kick_ticks > 0U && player->tackle_cooldown > 28U;
    const bool running         = player->moving && player->kick_ticks == 0U && player->jump_ticks == 0U;
    const int stride           = running ? strides[phase] : 0;
    int kick                   = 0;
    if (player->kick_ticks > 0U) {
        kick = player->kick_ticks > 6U ? 6 : (int) player->kick_ticks;
    }
    if (running && (phase == 2U || phase == 6U)) {
        --y;
    }
    if (tackling) {
        y += 4;
    }
    const int lean     = running || tackling ? player->facing_x : 0;
    const int leg_x    = stride * player->facing_x;
    const int leg_y    = stride * player->facing_y;
    const int arm_lift = player->jump_ticks > 0U ? 4 : 0;
    tabos_color_t blue = index < SOCCER_TEAM_SIZE ? TABOS_RGB565(48, 124, 244) : RED;
    if (goalkeeper) {
        blue = index == SOCCER_PLAYER_COUNT ? TABOS_RGB565(100, 224, 255) : GOLD;
    }
    const tabos_color_t skin = TABOS_RGB565(248, 180, 120);
    if (!goalkeeper && player->slide_ticks > 0U) {
        const int dx = player->slide_x, dy = player->slide_y;
        const int cy          = ground_y - 3;
        const int trail_count = (int) ((player->slide_ticks + 5U) / 6U);
        for (int trail = 0; trail < trail_count; ++trail) {
            const int back = 12 + trail * 4;
            box(g, x - dx * back - dy * (trail - 1) * 3 - 1, cy - dy * back + dx * (trail - 1) * 3, 3U, 2U,
                TABOS_RGB565(110, 156, 72));
        }
        box(g, x - 9, ground_y, 19U, 3U, DARK);
        for (int leg = 2; leg <= 9; ++leg) {
            box(g, x + dx * leg - 2, cy + dy * leg - 2, 4U, 4U, skin);
        }
        box(g, x + dx * 10 - 3, cy + dy * 10 - 2, 6U, 4U, INK);
        box(g, x + dx * 2 - 3, cy + dy * 2 - 3, 6U, 6U, WHITE);
        box(g, x - dx * 3 - 4, cy - dy * 3 - 4, 8U, 8U, blue);
        box(g, x - dx * 3 - dy * 5 - 2, cy - dy * 3 + dx * 5 - 2, 4U, 4U, skin);
        box(g, x - dx * 10 - 3, cy - dy * 10 - 3, 6U, 6U, skin);
        box(g, x - dx * 11 - 3, cy - dy * 11 - 3, 6U, 2U, INK);
        for (int i = 0; index < SOCCER_TEAM_SIZE && i < 4; ++i) {
            box(g, x - 3 + i, ground_y - 29 + i, (unsigned int) (7 - 2 * i), 1U,
                index == game->controlled ? GOLD : TABOS_RGB565(100, 224, 255));
        }
        return;
    }

    if (goalkeeper) {
        const soccer_dive_t* dive = &game->dives[index - SOCCER_PLAYER_COUNT];
        if (dive->ticks > 10U && dive->ticks <= 45U) {
            const int reach  = dive->direction;
            const int body_y = y - 4;
            box(g, x - 6, y - 10, 13U, 20U, DARK);
            box(g, x - 4, body_y - 4, 9U, 9U, blue);
            box(g, x - 3, body_y + reach * 7 - 2, 7U, 5U, skin);
            box(g, x - 4, body_y - reach * 6 - 2, 9U, 4U, WHITE);
            box(g, x - 4, body_y - reach * 10 - 1, 3U, 3U, INK);
            box(g, x + 2, body_y - reach * 10 - 1, 3U, 3U, INK);
            box(g, x - 7, body_y + reach * 5 - 3, 3U, 7U, skin);
            box(g, x + 5, body_y + reach * 5 - 3, 3U, 7U, skin);
            box(g, x - 7, body_y + reach * 10 - 2, 3U, 4U, WHITE);
            box(g, x + 5, body_y + reach * 10 - 2, 3U, 4U, WHITE);
            return;
        }
    }
    box(g, x - 6, ground_y + 1, 13U, 3U, DARK);
    /* Opposing feet follow all eight movement directions; the support foot stays planted on kicks. */
    box(g, x - 3 - leg_x, y - 4 - leg_y, 3U, 5U, skin);
    box(g, x + 1 + leg_x + player->facing_x * kick, y - 4 + leg_y + player->facing_y * kick, 3U, 5U, skin);
    box(g, x - 4 - leg_x, y - leg_y, 4U, 2U, INK);
    box(g, x + leg_x + player->facing_x * kick, y + leg_y + player->facing_y * kick, 5U, 2U, INK);
    box(g, x - 4, y - 8, 9U, 4U, WHITE);
    box(g, x - 5, y - 15, 11U, 8U, blue);
    box(g, x - 5, y - 15, 2U, 7U, TABOS_RGB565(24, 64, 100));
    box(g, x - 7, y - 13 + stride - arm_lift, 3U, 6U, skin);
    box(g, x + 5, y - 13 - stride - arm_lift, 3U, 6U, skin);
    box(g, x - 3 + lean, y - 21, 7U, 6U, skin);
    box(g, x - 3 + lean, y - 22, 7U, 2U, INK);
    if (player->facing_y >= 0) {
        box(g, x + lean + (player->facing_x < 0 ? -2 : 2), y - 18, 1U, 1U, INK);
        static const char* numbers[] = {"9", "7", "8", "4", "5"};
        _Static_assert(sizeof(numbers) / sizeof(numbers[0]) == SOCCER_TEAM_SIZE, "shirt numbers");
        const char* number = goalkeeper ? "1" : numbers[index % SOCCER_TEAM_SIZE];
        text(g, x - 2, y - 14, number, 1U, WHITE);
    } else {
        box(g, x - 2 + lean, y - 20, 5U, 2U, INK);
        box(g, x, y - 13, 1U, 5U, WHITE);
    }
    if (goalkeeper) {
        const int gloves = game->keeper_owner == (int) (index - SOCCER_PLAYER_COUNT) ? -17 : -9;
        box(g, x - 8, y + gloves, 4U, 3U, WHITE);
        box(g, x + 5, y + gloves, 4U, 3U, WHITE);
    }
    for (int i = 0; index < SOCCER_TEAM_SIZE && i < 4; ++i) {
        box(g, x - 3 + i, y - 29 + i, (unsigned int) (7 - 2 * i), 1U,
            index == game->controlled ? GOLD : TABOS_RGB565(100, 224, 255));
    }
}

static const char* const difficulty_names[] = {"EASY", "NORMAL", "HARD"};

static void difficulty_label(tabos_graphics_t* g, const soccer_game_t* game, int y)
{
    char label[40];
    (void) snprintf(label, sizeof(label), "LEFT/RIGHT LEVEL: %s", difficulty_names[game->difficulty]);
    centered(g, y, label, 1U, WHITE);
}

static void result_row(tabos_graphics_t* g, int y, const char* label, uint32_t blue, uint32_t red, bool percent)
{
    char value[16];
    text(g, 56, y, label, 1U, WHITE);
    (void) snprintf(value, sizeof(value), "%lu%s", (unsigned long) blue, percent ? "%" : "");
    text(g, 174, y, value, 1U, WHITE);
    (void) snprintf(value, sizeof(value), "%lu%s", (unsigned long) red, percent ? "%" : "");
    text(g, 230, y, value, 1U, WHITE);
}

/* Three-pixel enamel tube with a cool shaded edge and an offset shadow. */
static void goal_tube(tabos_graphics_t* g, int x, int y, int end_x, int end_y, int outward)
{
    (void) tabos_graphics_line(g, x + outward * 2, y + 1, end_x + outward * 2, end_y + 1, TABOS_RGB565(18, 48, 40));
    (void) tabos_graphics_line(g, x + outward, y, end_x + outward, end_y, TABOS_RGB565(125, 153, 150));
    (void) tabos_graphics_line(g, x, y, end_x, end_y, TABOS_RGB565(229, 233, 211));
    (void) tabos_graphics_line(g, x - outward, y, end_x - outward, end_y, TABOS_RGB565(255, 253, 233));
}

/* Original elevated goals: bright open frame, softer roof and darker side mesh. */
static void draw_goal(tabos_graphics_t* g, int front, int top, int bottom, int outward)
{
    enum {
        FRAME_INSET = 8,
        NET_DEPTH   = 11,
        TIE_DEPTH   = 8
    };
    if (front < -20 || front > 340 || bottom < -6 || top > 260) {
        return;
    }
    const int raised         = front - outward * FRAME_INSET;
    const int rear           = front + outward * NET_DEPTH;
    const tabos_color_t roof = TABOS_RGB565(178, 196, 170);
    const tabos_color_t mesh = TABOS_RGB565(113, 151, 130);
    /* Different face tones and a slight roof sag make the bag read as a volume. */
    for (int depth = -FRAME_INSET; depth <= NET_DEPTH; ++depth) {
        const int x           = front + outward * depth;
        const int slope       = (depth + FRAME_INSET) * 10 / (FRAME_INSET + NET_DEPTH);
        const int sag         = depth > -4 && depth < TIE_DEPTH ? 1 : 0;
        const int roof_top    = top - 18 + slope + sag;
        const int roof_bottom = bottom - 18 + slope + sag;
        const int floor       = bottom + (depth > 0 ? depth / 3 : 0);
        /* Sparse shadow bands leave grass visible through the roof without a solid panel. */
        if ((depth + FRAME_INSET) % 3 == 0) {
            (void) tabos_graphics_line(g, x, roof_top, x, roof_bottom, TABOS_RGB565(28, 89, 59));
        }
        (void) tabos_graphics_line(g, x, roof_bottom + 1, x, floor, TABOS_RGB565(20, 65, 47));
        if ((depth + FRAME_INSET) % 5 == 0 || depth == NET_DEPTH) {
            (void) tabos_graphics_line(g, x, roof_top, x, roof_bottom, roof);
            (void) tabos_graphics_line(g, x, roof_bottom, x, floor, mesh);
        }
    }
    for (int y = top; y <= bottom; y += 6) {
        const int middle = front + outward * 3;
        (void) tabos_graphics_line(g, raised, y - 18, middle, y - 12, roof);
        (void) tabos_graphics_line(g, middle, y - 12, rear, y - 8, roof);
    }
    for (int row = 1; row < 4; ++row) {
        (void) tabos_graphics_line(g, raised, bottom - 18 + row * 4, rear, bottom - 8 + row * 3, mesh);
    }
    /* Rear seams and ground shadow are deliberately less bright than the opening. */
    (void) tabos_graphics_line(g, rear, top - 8, rear, bottom + 4, mesh);
    (void) tabos_graphics_line(g, rear, bottom + 5, front, bottom + 2, TABOS_RGB565(17, 58, 42));
    (void) tabos_graphics_line(g, raised, top - 18, rear, top - 8, roof);
    (void) tabos_graphics_line(g, raised, bottom - 18, rear, bottom - 8, roof);
    /* Back stays and tied corners distinguish the net bag from the front frame. */
    (void) tabos_graphics_line(g, rear, top - 8, front + outward * TIE_DEPTH, top + 3, mesh);
    (void) tabos_graphics_line(g, rear, bottom - 8, front + outward * TIE_DEPTH, bottom + 4, mesh);
    goal_tube(g, raised, top - 18, raised, bottom - 18, outward);
    goal_tube(g, raised, top - 18, front, top, outward);
    box(g, front - 2, top, 5U, 2U, TABOS_RGB565(73, 101, 85));
    box(g, front - 1, top - 1, 3U, 2U, WHITE);
}

static void draw_near_goalpost(tabos_graphics_t* g, int front, int bottom, int outward)
{
    const int raised = front - outward * 8;
    box(g, front - 2, bottom + 1, 5U, 2U, TABOS_RGB565(17, 58, 42));
    goal_tube(g, raised, bottom - 18, front, bottom, outward);
    box(g, front - 2, bottom, 5U, 2U, TABOS_RGB565(73, 101, 85));
    box(g, front - 1, bottom - 1, 3U, 2U, WHITE);
}

int soccer_render_trial(tabos_graphics_t* g, const soccer_game_t* game, const soccer_camera_t* camera, bool sound_muted,
                        const soccer_visuals_t* visuals)
{
    if (tabos_graphics_clear(g, INK) != 0) {
        return -1;
    }
    box(g, 0, 0, 320U, SOCCER_VIEW_HEIGHT / 2U, DARK);
    const int left   = sx(camera, SOCCER_LEFT * SOCCER_ONE);
    const int right  = sx(camera, SOCCER_RIGHT * SOCCER_ONE);
    const int top    = sy(camera, SOCCER_TOP * SOCCER_ONE);
    const int bottom = sy(camera, SOCCER_BOTTOM * SOCCER_ONE);
    const int middle = sy(camera, 480 * SOCCER_ONE);
    const int centre = sx(camera, 800 * SOCCER_ONE);
    /* World-anchored stripes and markings scroll together; only visible stripes draw. */
    for (int strip = 0; strip < 16; ++strip) {
        const int x = left + strip * 48;
        if (x >= 320 || x + 48 <= 0) {
            continue;
        }
        box(g, x, top, 48U, (unsigned int) (bottom - top),
            strip % 2 == 0 ? TABOS_RGB565(45, 142, 82) : TABOS_RGB565(40, 131, 76));
    }
    (void) tabos_graphics_rect(g, left, top, (uint32_t) (right - left + 1), (uint32_t) (bottom - top + 1), WHITE);
    (void) tabos_graphics_line(g, centre, top, centre, bottom, WHITE);
    if (centre > -48 && centre < 368 && middle > -48 && middle < 288) {
        ellipse(g, centre, middle, 46, 46, WHITE);
        box(g, centre - 1, middle - 1, 3U, 3U, WHITE);
    }
    (void) tabos_graphics_rect(g, right - 132, middle - 108, 133U, 217U, WHITE);
    (void) tabos_graphics_rect(g, right - 44, middle - 56, 45U, 113U, WHITE);
    box(g, right - 88, middle - 1, 3U, 3U, WHITE);
    (void) tabos_graphics_rect(g, left, middle - 108, 133U, 217U, WHITE);
    (void) tabos_graphics_rect(g, left, middle - 56, 45U, 113U, WHITE);
    box(g, left + 88, middle - 1, 3U, 3U, WHITE);
    const int goal_top    = sy(camera, SOCCER_GOAL_TOP * SOCCER_ONE);
    const int goal_bottom = sy(camera, SOCCER_GOAL_BOTTOM * SOCCER_ONE);
    draw_goal(g, left, goal_top, goal_bottom, -1);
    draw_goal(g, right, goal_top, goal_bottom, 1);
    enum {
        ACTORS = SOCCER_PLAYER_COUNT + 2,
        BALL   = ACTORS
    };
    box(g, sx(camera, game->ball.x) - 3, sy(camera, game->ball.y) + 2, 7U, 2U, DARK);
    unsigned int order[ACTORS + 1];
    int32_t feet[ACTORS + 1];
    for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
        order[i] = i;
        feet[i]  = game->players[i].position.y;
    }
    for (unsigned int i = 0U; i < 2U; ++i) {
        order[SOCCER_PLAYER_COUNT + i] = SOCCER_PLAYER_COUNT + i;
        feet[SOCCER_PLAYER_COUNT + i]  = game->keepers[i].position.y;
    }
    order[BALL] = BALL;
    feet[BALL]  = game->ball_height > 20 * SOCCER_ONE ? INT32_MAX : game->ball.y;
    for (unsigned int i = 1U; i < ACTORS + 1U; ++i) {
        unsigned int j = i;
        while (j > 0U && feet[order[j - 1U]] > feet[order[j]]) {
            const unsigned int swap = order[j];
            order[j]                = order[j - 1U];
            order[j - 1U]           = swap;
            --j;
        }
    }
    for (unsigned int i = 0U; i < ACTORS + 1U; ++i) {
        if (order[i] == BALL) {
            if (visuals != NULL) {
                draw_trial_ball(g, game, camera, visuals);
            } else {
                draw_ball(g, game, camera);
            }
        } else {
            if (visuals != NULL) {
                draw_trial_player(g, game, camera, visuals, order[i]);
            } else {
                draw_player(g, game, camera, order[i]);
            }
        }
    }
    draw_near_goalpost(g, left, goal_bottom, -1);
    draw_near_goalpost(g, right, goal_bottom, 1);
    const unsigned int seconds = (game->match_ticks + 59U) / 60U;
    char clock[16];
    (void) snprintf(clock, sizeof(clock), "%u:%02u", seconds / 60U, seconds % 60U);
    centered(g, 7, clock, 2U, GOLD);
    if (game->mode == SOCCER_TITLE || game->paused) {
        box(g, 38, 60, 244U, 148U, INK);
        (void) tabos_graphics_rect(g, 38, 60, 244U, 148U, GOLD);
        centered(g, 72, game->paused ? "PAUSED" : "SOCCER", 2U, GOLD);
        centered(g, 96, "6-A-SIDE / THREE MINUTES", 1U, WHITE);
        if (game->paused) {
            char score[48];
            (void) snprintf(score, sizeof(score), "B %lu R %lu PASS %lu", (unsigned long) game->goals,
                            (unsigned long) game->opponent_goals, (unsigned long) game->completed_passes);
            centered(g, 116, score, 1U, WHITE);
        } else {
            difficulty_label(g, game, 116);
        }
        centered(g, 140, "WASD MOVE  J PASS/LOB  K SHOT/ACTION", 1U, WHITE);
        centered(g, 162,
                 sound_muted ? "P PAUSE R NEW MATCH M SOUND OFF Q EXIT" : "P PAUSE R NEW MATCH M SOUND ON Q EXIT", 1U,
                 GOLD);
        centered(g, 188, game->paused ? "P TO RESUME" : "ENTER TO START", 1U, GOLD);
    } else if (game->mode == SOCCER_FULL_TIME) {
        box(g, 38, 48, 244U, 162U, INK);
        (void) tabos_graphics_rect(g, 38, 48, 244U, 162U, GOLD);
        centered(g, 57, "FULL TIME", 2U, GOLD);
        const char* result = "DRAW";
        if (game->goals > game->opponent_goals) {
            result = "BLUE WINS";
        } else if (game->goals < game->opponent_goals) {
            result = "RED WINS";
        }
        centered(g, 80, result, 2U, WHITE);
        text(g, 174, 104, "BLUE", 1U, TABOS_RGB565(100, 224, 255));
        text(g, 230, 104, "RED", 1U, RED);
        result_row(g, 120, "SHOTS", game->stats[0].shots, game->stats[1].shots, false);
        result_row(g, 136, "ON TARGET", game->stats[0].on_target, game->stats[1].on_target, false);
        result_row(g, 152, "SAVES", game->stats[0].saves, game->stats[1].saves, false);
        const uint32_t total = game->stats[0].possession + game->stats[1].possession;
        const uint32_t blue  = total > 0U ? game->stats[0].possession * 100U / total : 0U;
        const uint32_t red   = total > 0U ? 100U - blue : 0U;
        result_row(g, 168, "POSSESSION", blue, red, true);
        difficulty_label(g, game, 181);
        centered(g, 197, "ENTER TO PLAY AGAIN", 1U, GOLD);
    } else if (game->mode == SOCCER_RESTART) {
        static const char* kinds[] = {"THROW IN", "CORNER", "GOAL KICK"};
        char restart[32];
        (void) snprintf(restart, sizeof(restart), "%s %s", game->restart_team == 0U ? "BLUE" : "RED",
                        kinds[game->restart_kind]);
        box(g, 38, 77, 244U, 55U, INK);
        centered(g, 87, restart, 2U, GOLD);
        const char* instruction = "GET READY";
        if (game->restart_team == 0U && game->restart_ticks <= 90U && game->restart_kind != SOCCER_GOAL_KICK) {
            instruction = game->restart_kind == SOCCER_CORNER ? "J PASS/HOLD LOB  K SHOT" : "J THROW IN";
        }
        centered(g, 115, instruction, 1U, WHITE);
    } else if (game->mode == SOCCER_KICKOFF) {
        box(g, 62, 84, 196U, 35U, INK);
        centered(g, 94, game->kickoff_team == 0U ? "BLUE KICK OFF" : "RED KICK OFF", 2U, GOLD);
    } else if (game->mode == SOCCER_GOAL) {
        box(g, 102, 76, 116U, 31U, INK);
        centered(g, 84, "GOAL", 2U, GOLD);
    } else {
        if (game->shot_charge > 0U) {
            box(g, 20, 56, 64U, 7U, INK);
            box(g, 22, 58, game->shot_charge * 2U, 3U, GOLD);
        }
    }
    return tabos_graphics_present(g);
}

int soccer_render(tabos_graphics_t* g, const soccer_game_t* game, const soccer_camera_t* camera, bool sound_muted)
{
    return soccer_render_trial(g, game, camera, sound_muted, NULL);
}
