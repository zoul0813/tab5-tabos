#ifndef SOCCER_CAMERA_H
#define SOCCER_CAMERA_H
#include <soccer/game.h>
enum {
    SOCCER_VIEW_WIDTH  = 640,
    SOCCER_VIEW_HEIGHT = 480
};
typedef struct {
        soccer_vec_t position, velocity;
} soccer_camera_t;
void soccer_camera_reset(soccer_camera_t* camera, const soccer_game_t* game);
void soccer_camera_step(soccer_camera_t* camera, const soccer_game_t* game);
#endif
