#ifndef SOCCER_RENDER_H
#define SOCCER_RENDER_H
#include <soccer/camera.h>
#include <soccer/visuals.h>
#include <tabos/graphics.h>
int soccer_render(tabos_graphics_t* graphics, const soccer_game_t* game, const soccer_camera_t* camera,
                  bool sound_muted);
int soccer_render_trial(tabos_graphics_t* graphics, const soccer_game_t* game, const soccer_camera_t* camera,
                        bool sound_muted, const soccer_visuals_t* visuals);
#endif
