/* Original procedural trial artwork. Local (0,0) is the planted ground anchor.
   Left views mirror their right equivalent; upright bodies are never rotated. */
static void trial_box(tabos_graphics_t* g, int x, int y, int mirror, int dx, int dy, unsigned int width,
                      unsigned int height, tabos_color_t color)
{
    if (mirror < 0) {
        dx = -dx - (int) width + 1;
    }
    box(g, x + dx, y + dy, width, height, color);
}

static void draw_trial_slide(tabos_graphics_t* g, const soccer_game_t* game, const soccer_camera_t* camera,
                             unsigned int index, tabos_color_t shirt, tabos_color_t shade)
{
    const soccer_player_t* player = &game->players[index];
    const int x = sx(camera, player->position.x), ground = sy(camera, player->position.y);
    const int dx = player->slide_x, dy = player->slide_y;
    const int side_x = -dy, side_y = dx;
    const int trail_count = (int) ((player->slide_ticks + 5U) / 6U);
    for (int trail = 0; trail < trail_count; ++trail) {
        const int back = 9 + trail * 4;
        box(g, x - dx * back + side_x * (trail - 1) * 2 - 1,
            ground - 2 - dy * back + side_y * (trail - 1) * 2, 3U, 2U, TABOS_RGB565(110, 156, 72));
    }
    box(g, x - 8, ground + 1, 17U, 3U, DARK);
    /* The last third braces the torso and trailing knee for a readable recovery. */
    const bool recovery = player->slide_ticks <= 6U;
    const int body_back = recovery ? 2 : 5;
    for (int leg = 2; leg <= 8; ++leg) {
        box(g, x + dx * leg + side_x - 1, ground - 3 + dy * leg + side_y - 1, 3U, 3U,
            TABOS_RGB565(248, 180, 120));
    }
    box(g, x + dx * 9 + side_x - 2, ground - 4 + dy * 9 + side_y - 1, 5U, 3U, INK);
    box(g, x - dx * body_back - 4, ground - 8 - dy * body_back, 9U, 6U, shirt);
    box(g, x - dx * body_back - 4, ground - 8 - dy * body_back, 2U, 5U, shade);
    box(g, x - dx * (body_back + 6) - 3, ground - 8 - dy * (body_back + 6), 6U, 5U,
        TABOS_RGB565(248, 180, 120));
    box(g, x - dx * (body_back + 6) - 3, ground - 9 - dy * (body_back + 6), 6U, 2U, INK);
    box(g, x - dx * 2 + side_x * 5 - 2, ground - 6 - dy * 2 + side_y * 5 - 2, 4U, 4U,
        TABOS_RGB565(248, 180, 120));
    for (int i = 0; index < SOCCER_TEAM_SIZE && i < 4; ++i) {
        box(g, x - 3 + i, ground - 29 + i, (unsigned int) (7 - 2 * i), 1U,
            index == game->controlled ? GOLD : TABOS_RGB565(100, 224, 255));
    }
}

static void draw_trial_dive(tabos_graphics_t* g, const soccer_game_t* game, const soccer_camera_t* camera,
                            unsigned int team, tabos_color_t shirt, tabos_color_t shade)
{
    const soccer_player_t* keeper = &game->keepers[team];
    const soccer_dive_t* dive     = &game->dives[team];
    const int x = sx(camera, keeper->position.x), ground = sy(camera, keeper->position.y);
    const int reach = dive->direction;
    box(g, x - 7, ground + 1, 15U, 3U, DARK);
    box(g, x - 4, ground - 11, 8U, 8U, shirt);
    box(g, x - 4, ground - 11, 2U, 7U, shade);
    box(g, x - 3, ground - 16 - reach * 2, 6U, 5U, TABOS_RGB565(248, 180, 120));
    box(g, x - 3, ground - 17 - reach * 2, 6U, 2U, INK);
    for (int arm = 4; arm <= 10; ++arm) {
        box(g, x + reach * arm - 1, ground - 10 - arm / 2, 3U, 3U, TABOS_RGB565(248, 180, 120));
    }
    box(g, x + reach * 11 - 2, ground - 16, 4U, 4U, WHITE);
    box(g, x + reach * 14 - 2, ground - 17, 4U, 4U, WHITE);
    box(g, x - reach * 4 - 2, ground - 3, 4U, 3U, TABOS_RGB565(248, 180, 120));
    box(g, x - reach * 7 - 2, ground - 2, 5U, 2U, INK);
}

static void draw_trial_player(tabos_graphics_t* g, const soccer_game_t* game, const soccer_camera_t* camera,
                              const soccer_visuals_t* visuals, unsigned int index)
{
    const bool goalkeeper         = index >= SOCCER_PLAYER_COUNT;
    const unsigned int team       = goalkeeper ? index - SOCCER_PLAYER_COUNT : index / SOCCER_TEAM_SIZE;
    const soccer_player_t* player = goalkeeper ? &game->keepers[team] : &game->players[index];
    const int x = sx(camera, player->position.x), ground = sy(camera, player->position.y);
    if (x < -24 || x > 344 || ground < -16 || ground > 272) {
        return;
    }
    const tabos_color_t shirt      = goalkeeper ? (team == 0U ? TABOS_RGB565(100, 224, 255) : GOLD)
                                                 : (team == 0U ? TABOS_RGB565(48, 124, 244) : RED);
    const tabos_color_t shade      = goalkeeper ? (team == 0U ? TABOS_RGB565(42, 142, 168)
                                                              : TABOS_RGB565(170, 116, 30))
                                                 : (team == 0U ? TABOS_RGB565(28, 76, 168)
                                                               : TABOS_RGB565(156, 39, 38));
    const tabos_color_t highlight  = goalkeeper ? (team == 0U ? TABOS_RGB565(178, 250, 255)
                                                              : TABOS_RGB565(255, 234, 128))
                                                 : (team == 0U ? TABOS_RGB565(88, 160, 255)
                                                               : TABOS_RGB565(255, 124, 108));
    if (!goalkeeper && player->slide_ticks > 0U) {
        draw_trial_slide(g, game, camera, index, shirt, shade);
        return;
    }
    if (goalkeeper && game->dives[team].ticks > 10U && game->dives[team].ticks <= 45U) {
        draw_trial_dive(g, game, camera, team, shirt, shade);
        return;
    }
    int dx = player->facing_x, dy = player->facing_y;
    const bool kicking = player->kick_ticks > 0U;
    if (kicking) {
        dx = visuals->action_x[index];
        dy = visuals->action_y[index];
    }
    const bool anticipation = !goalkeeper && !kicking &&
                              ((game->shot_charge > 0U && game->charge_player == index) ||
                               (game->pass_charge > 0U && game->pass_player == index));
    const bool running       = visuals->moving[index] && !kicking && !anticipation;
    const unsigned int frame = (visuals->run_distance[index] / (16U * SOCCER_ONE)) % 4U;
    static const int strides[]     = {0, 3, 0, -3};
    const int stride               = running ? strides[frame] : 0;
    const int mirror               = dx < 0 ? -1 : 1;
    const bool side                = dx != 0 && dy == 0;
    const bool diagonal            = dx != 0 && dy != 0;
    const bool rear                = dy < 0;
    const int bob = running && (frame == 1U || frame == 3U) ? 1 : 0;
    unsigned int jump_phase = 0U;
    if (player->jump_ticks > 0U) {
        jump_phase = player->jump_ticks > 12U ? 24U - player->jump_ticks : player->jump_ticks;
    }
    const int y = ground - bob - (int) jump_phase;
    const tabos_color_t skin       = TABOS_RGB565(248, 180, 120);
    const tabos_color_t skin_shade = TABOS_RGB565(194, 122, 76);
    box(g, x - 6, ground + 1, 13U, 3U, DARK);

    /* Far arm and leg first; direction-specific foreshortening keeps side views narrow. */
    int far_x = -3, near_x = 2;
    int far_y = 0, near_y = 0;
    if (side || diagonal) {
        far_x  = -2 - stride;
        near_x = 1 + stride;
        far_y  = stride > 0 ? -1 : 0;
        near_y = stride < 0 ? -2 : 0;
    } else {
        far_y  = -stride;
        near_y = stride;
    }
    if (running && frame == 2U) {
        far_y -= 2;
    }
    int extension = 0;
    if (kicking) {
        const bool pass = visuals->action[index] == SOCCER_EVENT_PASS;
        extension       = player->kick_ticks >= 8U ? (pass ? 4 : 7) : (int) player->kick_ticks * (pass ? 4 : 6) / 8;
    } else if (anticipation) {
        extension = -3;
    }
    if (kicking || anticipation) {
        near_x += abs(dx) * extension;
        near_y += dy * extension;
        if (dy == 0) {
            near_y -= abs(extension) / 2;
        }
    }
    /* Connect hips to knees during the extended kick, rather than moving a
       detached rectangular leg. Shorts cover the upper ends of these strokes. */
    for (int joint = 0; joint <= 6; ++joint) {
        trial_box(g, x, ground, mirror, -3 + (far_x + 3) * joint / 6, -7 + (far_y + 3) * joint / 6, 3U, 3U, skin_shade);
        trial_box(g, x, ground, mirror, 2 + (near_x - 2) * joint / 6, -7 + (near_y + 3) * joint / 6, 3U, 3U, skin);
    }
    trial_box(g, x, ground, mirror, far_x, far_y - 6, 3U, 5U, skin_shade);
    trial_box(g, x, ground, mirror, far_x, far_y - 2, 3U, 2U, WHITE);
    trial_box(g, x, ground, mirror, far_x - 1, far_y, 4U, 2U, INK);
    const int arm_lift = player->jump_ticks > 0U ? 4 : 0;
    trial_box(g, x, y, mirror, -5 + (dx != 0 ? stride / 2 : 0), -14 + stride / 2 - arm_lift, 3U, 5U,
              skin_shade);
    trial_box(g, x, ground, mirror, near_x, near_y - 6, 3U, 5U, skin);
    trial_box(g, x, ground, mirror, near_x, near_y - 2, 3U, 2U, WHITE);
    trial_box(g, x, ground, mirror, near_x - 1, near_y, 4U, 2U, INK);

    const unsigned int torso = side ? 6U : 9U;
    const int torso_x        = side ? -2 : -4;
    trial_box(g, x, y, mirror, torso_x, -9, torso, 4U, WHITE);
    trial_box(g, x, y, mirror, torso_x, -16, torso, 8U, shirt);
    trial_box(g, x, y, mirror, torso_x, -15, 2U, 7U, shade);
    trial_box(g, x, y, mirror, torso_x + 2, -16, torso - 3U, 1U, highlight);
    trial_box(g, x, y, mirror, (side ? 1 : 4) - (dx != 0 ? stride / 2 : 0),
              -14 - stride / 2 - (anticipation ? 1 : 0) - arm_lift,
              3U, 5U, skin);
    trial_box(g, x, y, mirror, side ? 1 : 4, -15 - stride / 2, 3U, 2U, shirt);
    trial_box(g, x, y, mirror, -1, -18, 3U, 2U, skin_shade);

    /* Five canonical views: front/back/side plus front/back diagonals; mirror for eight. */
    const int head_x = side || diagonal ? 0 : -2;
    trial_box(g, x, y, mirror, head_x, -23, 5U, 6U, skin);
    trial_box(g, x, y, mirror, head_x, -24, 5U, rear ? 4U : 2U, INK);
    if (rear) {
        trial_box(g, x, y, mirror, head_x, -21, diagonal ? 3U : 5U, 2U, INK);
        /* Small shirt number, reserved for the clear rear view. */
        if (!diagonal) {
            static const char* numbers[] = {"9", "7", "8", "4", "5"};
            const char* number = goalkeeper ? "1" : numbers[index % SOCCER_TEAM_SIZE];
            text(g, x - 2, y - 14, number, 1U, WHITE);
        }
    } else if (side || diagonal) {
        trial_box(g, x, y, mirror, head_x + 4, -21, 2U, 2U, skin);
        trial_box(g, x, y, mirror, head_x + 3, -21, 1U, 1U, INK);
        trial_box(g, x, y, mirror, head_x, -20, 1U, 2U, skin_shade);
    } else {
        trial_box(g, x, y, mirror, -1, -21, 1U, 1U, INK);
        trial_box(g, x, y, mirror, 2, -21, 1U, 1U, INK);
        trial_box(g, x, y, mirror, 0, -18, 2U, 1U, skin_shade);
    }
    if (goalkeeper) {
        const bool holding = game->keeper_owner == (int) team;
        trial_box(g, x, y, mirror, -7, holding ? -18 : -12, 3U, 3U, WHITE);
        trial_box(g, x, y, mirror, 5, holding ? -18 : -12, 3U, 3U, WHITE);
    }
    for (int i = 0; !goalkeeper && team == 0U && i < 4; ++i) {
        box(g, x - 3 + i, ground - 29 + i, (unsigned int) (7 - 2 * i), 1U,
            game->controlled == index ? GOLD : TABOS_RGB565(100, 224, 255));
    }
}

static void draw_trial_ball(tabos_graphics_t* g, const soccer_game_t* game, const soccer_camera_t* camera,
                            const soccer_visuals_t* visuals)
{
    const int x = sx(camera, game->ball.x);
    const int y = sy(camera, game->ball.y) - game->ball_height / (2 * SOCCER_ONE);
    /* Constant 5x5 clipped silhouette; asymmetric interior panels, never a central pupil. */
    const tabos_color_t leather = TABOS_RGB565(248, 244, 218);
    const tabos_color_t seam    = TABOS_RGB565(54, 66, 66);
    box(g, x - 1, y - 2, 3U, 5U, leather);
    box(g, x - 2, y - 1, 5U, 3U, leather);
    box(g, x - 1, y + 2, 3U, 1U, TABOS_RGB565(180, 190, 169));
    static const int panels[4][6] = {
        {-1, -1,  0, -1,  1,  1},
        { 1, -1,  1,  0, -1,  1},
        {-1,  1,  0,  1,  1, -1},
        {-1,  0, -1,  1,  1,  0}
    };
    const unsigned int frame = (visuals->ball_distance / (8U * SOCCER_ONE)) % 4U;
    for (unsigned int panel = 0U; panel < 3U; ++panel) {
        box(g, x + panels[frame][panel * 2U], y + panels[frame][panel * 2U + 1U], 1U, 1U, seam);
    }
}
