#ifndef TABOS_TILEMAP_H
#define TABOS_TILEMAP_H

#include <tabos/sprite.h>

#include <stdint.h>

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

typedef enum {
    TABOS_TILEMAP_LAYER_TILES   = 0,
    TABOS_TILEMAP_LAYER_OBJECTS = 1,
} tabos_tilemap_layer_type_t;

typedef enum {
    TABOS_TILEMAP_OBJECT_POINT     = 0,
    TABOS_TILEMAP_OBJECT_RECTANGLE = 1,
    TABOS_TILEMAP_OBJECT_TILE      = 2,
} tabos_tilemap_object_shape_t;

typedef struct {
        const char* name;
        int32_t value;
} tabos_tilemap_property_t;

typedef struct {
        uint32_t id;
        const char* name;
        const char* type;
        tabos_tilemap_object_shape_t shape;
        int32_t x;
        int32_t y;
        uint32_t width;
        uint32_t height;
        tabos_tile_t tile;
        const tabos_tilemap_property_t* properties;
        uint32_t property_count;
} tabos_tilemap_object_t;

typedef struct {
        const char* name;
        tabos_tilemap_layer_type_t type;
        tabos_tile_t* cells;
        const tabos_tilemap_object_t* objects;
        uint32_t object_count;
} tabos_tilemap_layer_t;

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
        int32_t camera_x;
        int32_t camera_y;
        tabos_graphics_rect_t viewport;
        uint64_t animation_ms;
} tabos_tilemap_draw_options_t;

int tabos_tilemap_get(const tabos_tilemap_t* map, uint32_t layer, uint32_t column, uint32_t row, tabos_tile_t* tile);
int tabos_tilemap_set(tabos_tilemap_t* map, uint32_t layer, uint32_t column, uint32_t row, tabos_tile_t tile);
int tabos_tilemap_draw_layer(tabos_graphics_t* graphics, const tabos_tilemap_t* map, uint32_t layer,
                             const tabos_sprite_set_t* sprites, const tabos_tilemap_draw_options_t* options);
int tabos_tilemap_object_property(const tabos_tilemap_object_t* object, const char* name, int32_t* value);
int tabos_tilemap_load(const char* path, tabos_tilemap_t* map);
void tabos_tilemap_unload(tabos_tilemap_t* map);

#endif
