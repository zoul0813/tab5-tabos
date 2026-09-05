#ifndef TABOS_SPRITE_H
#define TABOS_SPRITE_H

#include <tabos/graphics.h>

#include <stdbool.h>
#include <stdint.h>

#define TABOS_SPRITE_NONE UINT32_MAX

typedef struct {
        const tabos_color_t* pixels;
        uint32_t width;
        uint32_t height;
        tabos_color_t color_key;
        bool color_key_enabled;
} tabos_sprite_image_t;

typedef struct {
        uint32_t image;
        int32_t x;
        int32_t y;
        uint32_t width;
        uint32_t height;
        int32_t pivot_x;
        int32_t pivot_y;
        uint32_t flags;
} tabos_sprite_t;

typedef struct {
        uint32_t sprite;
        uint32_t duration_ms;
} tabos_sprite_frame_t;

typedef struct {
        const tabos_sprite_frame_t* frames;
        uint32_t frame_count;
        uint32_t repeat_count;
        uint32_t trigger_sprite;
} tabos_sprite_animation_t;

typedef struct {
        uint32_t sprite;
        int32_t x;
        int32_t y;
        tabos_graphics_rotation_t rotation;
        bool mirror_x;
        bool mirror_y;
        uint8_t opacity;
} tabos_metasprite_part_t;

typedef struct {
        const tabos_metasprite_part_t* parts;
        uint32_t part_count;
} tabos_metasprite_t;

typedef struct {
        const tabos_sprite_image_t* images;
        uint32_t image_count;
        const tabos_sprite_t* sprites;
        uint32_t sprite_count;
        const tabos_sprite_animation_t* animations;
        uint32_t animation_count;
        const tabos_metasprite_t* metasprites;
        uint32_t metasprite_count;
        void* _storage;
} tabos_sprite_set_t;

typedef struct {
        uint32_t width;
        uint32_t height;
        tabos_graphics_rotation_t rotation;
        bool mirror_x;
        bool mirror_y;
        uint8_t opacity;
        tabos_graphics_rect_t clip;
        bool clip_enabled;
} tabos_sprite_draw_options_t;

/* Natural size, no transform, full opacity, and no clipping. */
#define TABOS_SPRITE_DRAW_OPTIONS_DEFAULT ((tabos_sprite_draw_options_t) {.opacity = UINT8_MAX})

int tabos_sprite_draw(tabos_graphics_t* graphics, const tabos_sprite_set_t* set, uint32_t sprite, int32_t x, int32_t y);
int tabos_sprite_draw_ex(tabos_graphics_t* graphics, const tabos_sprite_set_t* set, uint32_t sprite, int32_t x,
                         int32_t y, const tabos_sprite_draw_options_t* options);
uint32_t tabos_sprite_animation_frame(const tabos_sprite_set_t* set, uint32_t animation, uint64_t elapsed_ms);
/* Elapsed time is measured from this actor's animation start, not the previous frame. */
int tabos_sprite_animation_draw(tabos_graphics_t* graphics, const tabos_sprite_set_t* set, uint32_t animation,
                                int32_t x, int32_t y, uint64_t elapsed_ms);
int tabos_sprite_animation_draw_ex(tabos_graphics_t* graphics, const tabos_sprite_set_t* set, uint32_t animation,
                                   int32_t x, int32_t y, uint64_t elapsed_ms,
                                   const tabos_sprite_draw_options_t* options);
/* Finite clips finish after the final frame's duration. Loops never finish.
 * Returns 0/-1 with errno; leaves *finished unchanged on failure. */
int tabos_sprite_animation_finished(const tabos_sprite_set_t* set, uint32_t animation, uint64_t elapsed_ms,
                                    bool* finished);
int tabos_metasprite_draw(tabos_graphics_t* graphics, const tabos_sprite_set_t* set, uint32_t metasprite, int32_t x,
                          int32_t y, bool mirror_x, bool mirror_y, uint8_t opacity);
uint32_t tabos_sprite_flags(const tabos_sprite_set_t* set, uint32_t sprite);
int tabos_sprite_set_load(const char* path, tabos_sprite_set_t* set);
void tabos_sprite_set_unload(tabos_sprite_set_t* set);

#endif
