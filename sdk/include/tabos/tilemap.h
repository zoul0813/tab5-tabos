#ifndef TABOS_TILEMAP_H
#define TABOS_TILEMAP_H

#include <tabos/sprite.h>

#include <stdint.h>

/*
 * One map cell. Zero is empty. Bits 0-27 store sprite ID + 1, and the top
 * three bits preserve Tiled transforms. Bit 28 is reserved and invalid.
 */
typedef uint32_t tabos_tile_t;

#define TABOS_TILE_FLIP_HORIZONTAL UINT32_C(0x80000000)
#define TABOS_TILE_FLIP_VERTICAL   UINT32_C(0x40000000)
#define TABOS_TILE_FLIP_DIAGONAL   UINT32_C(0x20000000)
#define TABOS_TILE_RESERVED        UINT32_C(0x10000000)
#define TABOS_TILE_ID_MASK         UINT32_C(0x0fffffff)

#define TABOS_TILE(sprite_id)       ((tabos_tile_t) ((uint32_t) (sprite_id) + 1U))
#define TABOS_TILE_ID(tile)         (((uint32_t) (tile) & TABOS_TILE_ID_MASK) - 1U)
#define TABOS_TILE_TRANSFORMS(tile) ((uint32_t) (tile) & UINT32_C(0xe0000000))
#define TABOS_TILE_EMPTY            UINT32_C(0)

/* TABOS_TILE_ID(TABOS_TILE_EMPTY) evaluates to TABOS_SPRITE_NONE. */

/* A map retains tile and object layers in authored draw order. */
typedef enum {
    TABOS_TILEMAP_LAYER_TILES   = 0,
    TABOS_TILEMAP_LAYER_OBJECTS = 1,
} tabos_tilemap_layer_type_t;

typedef enum {
    TABOS_TILEMAP_OBJECT_POINT     = 0,
    TABOS_TILEMAP_OBJECT_RECTANGLE = 1,
    TABOS_TILEMAP_OBJECT_TILE      = 2,
} tabos_tilemap_object_shape_t;

/* Named signed integer property copied from a Tiled object. */
typedef struct {
        const char* name;
        int32_t value;
} tabos_tilemap_property_t;

/* Marker from an object layer. Object layers are never drawn automatically. */
typedef struct {
        /* Stable Tiled object ID; generated OBJECT constants use this value. */
        uint32_t id;
        const char* name;
        /* Tiled class, or legacy type when class is absent. */
        const char* type;
        tabos_tilemap_object_shape_t shape;
        /* Top-left integer world geometry; conversion normalizes Tiled tile-object alignment. */
        int32_t x;
        int32_t y;
        uint32_t width;
        uint32_t height;
        /* Encoded tile for TILE objects; TABOS_TILE_EMPTY for other shapes. */
        tabos_tile_t tile;
        const tabos_tilemap_property_t* properties;
        uint32_t property_count;
} tabos_tilemap_object_t;

/* Exactly one of cells or objects is populated according to type. */
typedef struct {
        const char* name;
        tabos_tilemap_layer_type_t type;
        /* Writable row-major width * height cells for tile layers. */
        tabos_tile_t* cells;
        /* Read-only markers for object layers. */
        const tabos_tilemap_object_t* objects;
        uint32_t object_count;
} tabos_tilemap_layer_t;

/*
 * Finite orthogonal map. width/height are cell counts; tile_width/tile_height
 * are world pixels. A loaded map owns _storage. Applications may edit cells,
 * but must not access or modify _storage.
 */
typedef struct {
        uint32_t width;
        uint32_t height;
        uint32_t tile_width;
        uint32_t tile_height;
        tabos_tilemap_layer_t* layers;
        uint32_t layer_count;
        void* _storage;
} tabos_tilemap_t;

typedef struct {
        /* Screen-space clip; zero width or height draws nothing. */
        tabos_graphics_rect_t viewport;
        /* Elapsed time used for every animated tile in this layer draw. */
        uint64_t animation_ms;
} tabos_tilemap_draw_options_t;

/* Full graphics canvas at animation time zero. Override only fields needed. */
#define TABOS_TILEMAP_DRAW_OPTIONS_DEFAULT \
    ((tabos_tilemap_draw_options_t) {      \
        .viewport = {.width = UINT32_MAX, .height = UINT32_MAX} \
    })

/*
 * Read a cell by tile-layer index, column, and row. Returns 0 on success or -1
 * with errno=EINVAL for an invalid map/layer/output and errno=ERANGE outside
 * map bounds. Failure leaves *tile unchanged.
 */
int tabos_tilemap_get(const tabos_tilemap_t* map, uint32_t layer, uint32_t column, uint32_t row, tabos_tile_t* tile);
/*
 * Replace a cell in memory. Use TABOS_TILE_EMPTY or TABOS_TILE(sprite_id),
 * optionally ORed with transform bits. Returns 0 on success or -1 with
 * errno=EINVAL for invalid layers/reserved bit 28 and errno=ERANGE outside map
 * bounds. Changes are not saved.
 */
int tabos_tilemap_set(tabos_tilemap_t* map, uint32_t layer, uint32_t column, uint32_t row, tabos_tile_t tile);
/*
 * Draw one tile layer at map world origin (0, 0), using the graphics camera.
 * The viewport clips in screen space. Tiles fill map cells and ignore sprite
 * pivots; Tiled transforms and trigger-sprite animations apply automatically.
 * Drawing layers separately lets games place actors between them. Requires an
 * open graphics context. Returns 0 on success or -1 with errno.
 */
int tabos_tilemap_draw_layer(tabos_graphics_t* graphics, const tabos_tilemap_t* map, uint32_t layer,
                             const tabos_sprite_set_t* sprites, const tabos_tilemap_draw_options_t* options);
/*
 * Find an object by its stable Tiled ID in one object layer. Generated OBJECT
 * constants supply this ID. Returns NULL with errno=EINVAL for an invalid map
 * or layer and errno=ENOENT when the ID is absent.
 */
const tabos_tilemap_object_t* tabos_tilemap_object(const tabos_tilemap_t* map, uint32_t layer, uint32_t object_id);
/*
 * Find a named signed integer object property. Returns 0 on success or -1 with
 * errno=EINVAL for invalid arguments and errno=ENOENT when absent. Failure
 * leaves *value unchanged.
 */
int tabos_tilemap_object_property(const tabos_tilemap_object_t* object, const char* name, int32_t* value);
/*
 * Load and validate a TMP1 version-1 file into process-owned memory. Initialize
 * *map to {0}; unload it before reuse. On failure, *map is unchanged. Returns
 * 0 or -1 with filesystem errno, EIO, EINVAL, or ENOMEM.
 */
int tabos_tilemap_load(const char* path, tabos_tilemap_t* map);
/* Free a loaded map and zero it. Safe with NULL and zero-initialized maps. */
void tabos_tilemap_unload(tabos_tilemap_t* map);

#endif
