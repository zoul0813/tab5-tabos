#include <pool/table.h>

enum {
    W = POOL_TABLE_WIDTH,
    H = POOL_TABLE_HEIGHT,
    C = POOL_CORNER_CUT,
    L = W / 2 - POOL_SIDE_HALF_MOUTH,
    R = W / 2 + POOL_SIDE_HALF_MOUTH,
    D = POOL_RAIL_DEPTH
};

// clang-format off
const pool_cushion_t pool_cushions[POOL_CUSHION_COUNT] = {
    {{C, 0}, {L, 0}, {L + 5, -D}, {C - D, -D}},
    {{R, 0}, {W - C, 0}, {W - C + D, -D}, {R - 5, -D}},
    {{C, H}, {L, H}, {L + 5, H + D}, {C - D, H + D}},
    {{R, H}, {W - C, H}, {W - C + D, H + D}, {R - 5, H + D}},
    {{0, C}, {0, H - C}, {-D, H - C + D}, {-D, C - D}},
    {{W, C}, {W, H - C}, {W + D, H - C + D}, {W + D, C - D}},
};

const pool_pocket_t pool_pockets[POOL_POCKET_COUNT] = {
    {{-4, -4}, 11, {0, C}, {C, 0}},
    {{W / 2, -6}, 10, {L, 0}, {R, 0}},
    {{W + 4, -4}, 11, {W - C, 0}, {W, C}},
    {{-4, H + 4}, 11, {0, H - C}, {C, H}},
    {{W / 2, H + 6}, 10, {L, H}, {R, H}},
    {{W + 4, H + 4}, 11, {W - C, H}, {W, H - C}},
};

/* Rack points are deliberately integer and non-overlapping: rows advance 13
 * units and stagger by 7, giving sqrt(218) between diagonally adjacent centers.
 * Stage 4 can introduce a tighter fractional rack with explicit review/tests. */
const pool_point_t pool_initial_balls[POOL_BALL_COUNT] = {
    {132, 132}, /* cue */
    {396, 132}, /* 1, apex */
    {409, 139}, /* 2 */
    {422, 118}, /* 3 */
    {435, 125}, /* 4 */
    {435, 153}, /* 5 */
    {448, 104}, /* 6, solid rear corner */
    {448, 132}, /* 7 */
    {422, 132}, /* 8, center of third row */
    {409, 125}, /* 9 */
    {422, 146}, /* 10 */
    {435, 111}, /* 11 */
    {435, 139}, /* 12 */
    {448, 118}, /* 13 */
    {448, 146}, /* 14 */
    {448, 160}, /* 15, striped rear corner */
};
// clang-format on
