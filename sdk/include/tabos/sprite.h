#ifndef TABOS_SPRITE_H
#define TABOS_SPRITE_H

#include <tabos/graphics.h>

#include <stdbool.h>
#include <stdint.h>

/*
 * Sprite sets group source images, sprite regions, animation clips, and
 * metasprites. IDs are zero-based indexes into their corresponding arrays.
 * Asset builds generate named constants so applications do not use raw IDs.
 */
#define TABOS_SPRITE_NONE UINT32_MAX

/* RGB565 source image shared by one or more sprite regions. */
typedef struct {
        /* Row-major width * height pixels. Keep valid through present(). */
        const tabos_color_t* pixels;
        uint32_t width;
        uint32_t height;
        /* Exact RGB565 value skipped when color_key_enabled is true. */
        tabos_color_t color_key;
        bool color_key_enabled;
} tabos_sprite_image_t;

/* Rectangular region and drawing metadata for one zero-based sprite ID. */
typedef struct {
        /* Index in tabos_sprite_set_t.images. */
        uint32_t image;
        /* Top-left source-image coordinates. */
        int32_t x;
        int32_t y;
        uint32_t width;
        uint32_t height;
        /*
         * Local geometric origin measured from the region's top-left edge.
         * It may lie on an edge or outside the region. Draw coordinates place
         * this origin in the world; transforms move it with the image.
         */
        int32_t pivot_x;
        int32_t pivot_y;
        /* Application-defined bits; TabOS assigns no gameplay meaning. */
        uint32_t flags;
} tabos_sprite_t;

/* One animation step: draw sprite for duration_ms, which must be positive. */
typedef struct {
        uint32_t sprite;
        uint32_t duration_ms;
} tabos_sprite_frame_t;

/* Stateless animation clip selected using elapsed milliseconds. */
typedef struct {
        /* Ordered, nonempty frame array. */
        const tabos_sprite_frame_t* frames;
        uint32_t frame_count;
        /* Zero loops forever; positive values are the total number of cycles. */
        uint32_t repeat_count;
        /* Sprite ID whose use in a tilemap activates this animation. */
        uint32_t trigger_sprite;
} tabos_sprite_animation_t;

/* One ordered component of a metasprite. */
typedef struct {
        uint32_t sprite;
        /* Signed offset from the metasprite origin. */
        int32_t x;
        int32_t y;
        /* Per-part transform, applied around the referenced sprite's pivot. */
        tabos_graphics_rotation_t rotation;
        bool mirror_x;
        bool mirror_y;
        /* 0 is transparent; 255 is fully opaque. */
        uint8_t opacity;
} tabos_metasprite_part_t;

/* Parts draw in array order; later parts appear over earlier parts. */
typedef struct {
        const tabos_metasprite_part_t* parts;
        uint32_t part_count;
} tabos_metasprite_t;

/*
 * Complete sprite asset set. Descriptor pointers are read-only. A set loaded
 * from .tsp owns _storage until tabos_sprite_set_unload(); generated-C sets
 * have no owned storage. Applications must not access or modify _storage.
 */
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

/* Extended options shared by sprite and animation drawing. */
typedef struct {
        /* Zero selects natural post-rotation width/height; nonzero scales. */
        uint32_t width;
        uint32_t height;
        /* Mirrors act in source space before quarter-turn rotation. */
        tabos_graphics_rotation_t rotation;
        bool mirror_x;
        bool mirror_y;
        /* 0 is transparent; 255 is fully opaque. */
        uint8_t opacity;
        /* Screen-space clip, unaffected by the graphics camera. */
        tabos_graphics_rect_t clip;
        bool clip_enabled;
} tabos_sprite_draw_options_t;

/* Natural size, no transform, full opacity, and no clipping. */
#define TABOS_SPRITE_DRAW_OPTIONS_DEFAULT ((tabos_sprite_draw_options_t) {.opacity = UINT8_MAX})

/*
 * Draw sprite at natural size with its transformed pivot at world (x, y).
 * Uses full opacity, no transform, and no clip. Returns 0 on success or -1
 * with errno set. The graphics camera affects (x, y).
 */
int tabos_sprite_draw(tabos_graphics_t* graphics, const tabos_sprite_set_t* set, uint32_t sprite, int32_t x, int32_t y);
/*
 * Draw sprite using options. Width/height zero independently select natural
 * post-rotation dimensions. Integer pivot scaling truncates toward zero.
 * Returns 0 on success or -1 with EINVAL/EOVERFLOW or a graphics error.
 */
int tabos_sprite_draw_ex(tabos_graphics_t* graphics, const tabos_sprite_set_t* set, uint32_t sprite, int32_t x,
                         int32_t y, const tabos_sprite_draw_options_t* options);
/*
 * Return the sprite ID selected at elapsed_ms from the clip start. Finite
 * clips hold their final sprite after completion. Returns TABOS_SPRITE_NONE
 * and sets errno to EINVAL for an invalid clip or descriptor.
 */
uint32_t tabos_sprite_animation_sprite(const tabos_sprite_set_t* set, uint32_t animation, uint64_t elapsed_ms);
/*
 * Select and draw an animation sprite at natural size. elapsed_ms is time
 * since this actor's clip started, not a frame delta. Returns 0/-1 with errno.
 */
int tabos_sprite_animation_draw(tabos_graphics_t* graphics, const tabos_sprite_set_t* set, uint32_t animation,
                                int32_t x, int32_t y, uint64_t elapsed_ms);
/* Select and draw an animation sprite using tabos_sprite_draw_ex() options. */
int tabos_sprite_animation_draw_ex(tabos_graphics_t* graphics, const tabos_sprite_set_t* set, uint32_t animation,
                                   int32_t x, int32_t y, uint64_t elapsed_ms,
                                   const tabos_sprite_draw_options_t* options);
/*
 * Report whether a finite clip completed its final frame duration. Infinite
 * clips never finish. Returns 0/-1 with errno and leaves *finished unchanged
 * on failure. Completion stays true on later calls.
 */
int tabos_sprite_animation_finished(const tabos_sprite_set_t* set, uint32_t animation, uint64_t elapsed_ms,
                                    bool* finished);
/*
 * Draw metasprite parts in order around world (x, y). Overall mirrors negate
 * part offsets and XOR with per-part mirrors. Overall opacity multiplies each
 * part's opacity with nearest-integer rounding. Arbitrary overall rotation and
 * scaling are unsupported.
 * Returns 0 on success or -1 with EINVAL/EOVERFLOW or a graphics error; a
 * failure does not undo earlier parts.
 */
int tabos_metasprite_draw(tabos_graphics_t* graphics, const tabos_sprite_set_t* set, uint32_t metasprite, int32_t x,
                          int32_t y, bool mirror_x, bool mirror_y, uint8_t opacity);
/*
 * Return application flags. Zero is valid. Invalid descriptors or sprite IDs
 * return zero with errno=EINVAL; set errno to zero first when disambiguation matters.
 */
uint32_t tabos_sprite_flags(const tabos_sprite_set_t* set, uint32_t sprite);
/*
 * Load and validate a TSP1 version-1 file into process-owned memory. Initialize
 * *set to {0}; unload it before reuse. On failure, *set is unchanged. Returns
 * 0 or -1 with filesystem errno, EIO, EINVAL, or ENOMEM.
 */
int tabos_sprite_set_load(const char* path, tabos_sprite_set_t* set);
/* Free a loaded set and zero it. Safe with NULL and zero-initialized sets. */
void tabos_sprite_set_unload(tabos_sprite_set_t* set);

#endif
