#ifndef POOL_TABLE_H
#define POOL_TABLE_H

/* Table-local integer geometry. Positive Y points down. Screen translation is
 * owned by the renderer; these points never include its (56,48) offset. */
enum {
    POOL_TABLE_WIDTH     = 528,
    POOL_TABLE_HEIGHT    = 264,
    POOL_BALL_RADIUS     = 7,
    POOL_BALL_COUNT      = 16,
    POOL_CUSHION_COUNT   = 6,
    POOL_POCKET_COUNT    = 6,
    POOL_JAW_RADIUS      = 2,
    POOL_RAIL_DEPTH      = 10,
    POOL_CORNER_CUT      = 18,
    POOL_SIDE_HALF_MOUTH = 16
};

typedef struct {
        int x;
        int y;
} pool_point_t;

typedef struct {
        /* a,b are the cushion face; c,d are its back edge. Rounded jaw centers
         * are a,b. Future physics uses this face and the endpoint radius. */
        pool_point_t a, b, c, d;
} pool_cushion_t;

typedef struct {
        pool_point_t center;
        int radius;
        /* Mouth joins two cushion endpoints. Capture circle lies behind it. */
        pool_point_t mouth_a, mouth_b;
} pool_pocket_t;

extern const pool_cushion_t pool_cushions[POOL_CUSHION_COUNT];
extern const pool_pocket_t pool_pockets[POOL_POCKET_COUNT];
/* Indexed by number: 0 is the cue ball, 1..15 are object balls. */
extern const pool_point_t pool_initial_balls[POOL_BALL_COUNT];

#endif
