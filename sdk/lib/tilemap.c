#include <tabos/tilemap.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    TMP_HEADER_SIZE   = 64,
    TMP_LAYER_SIZE    = 20,
    TMP_OBJECT_SIZE   = 48,
    TMP_PROPERTY_SIZE = 12,
};

static uint32_t read_u32(const uint8_t* data)
{
    return (uint32_t) data[0] | ((uint32_t) data[1] << 8U) | ((uint32_t) data[2] << 16U) | ((uint32_t) data[3] << 24U);
}

static bool table_valid(uint32_t offset, uint32_t count, uint32_t stride, size_t size)
{
    return offset >= TMP_HEADER_SIZE && (offset & 3U) == 0U && offset <= size && count <= (size - offset) / stride;
}

static bool ranges_overlap(uint32_t first_offset, uint64_t first_size, uint32_t second_offset, uint64_t second_size)
{
    return first_size != 0U && second_size != 0U && first_offset < (uint64_t) second_offset + second_size &&
           second_offset < (uint64_t) first_offset + first_size;
}

static bool allocation_add(size_t* size, uint32_t count, size_t item_size)
{
    if (count > (SIZE_MAX - *size) / item_size) {
        return false;
    }
    *size += (size_t) count * item_size;
    return true;
}

static bool valid_tile_layer(const tabos_tilemap_t* map, uint32_t layer)
{
    return map != NULL && layer < map->layer_count && map->layers[layer].type == TABOS_TILEMAP_LAYER_TILES &&
           map->layers[layer].cells != NULL;
}

int tabos_tilemap_get(const tabos_tilemap_t* map, uint32_t layer, uint32_t column, uint32_t row, tabos_tile_t* tile)
{
    if (tile == NULL || !valid_tile_layer(map, layer)) {
        errno = EINVAL;
        return -1;
    }
    if (column >= map->width || row >= map->height) {
        errno = ERANGE;
        return -1;
    }
    *tile = map->layers[layer].cells[(size_t) row * map->width + column];
    return 0;
}

int tabos_tilemap_set(tabos_tilemap_t* map, uint32_t layer, uint32_t column, uint32_t row, tabos_tile_t tile)
{
    if (!valid_tile_layer(map, layer)) {
        errno = EINVAL;
        return -1;
    }
    if (column >= map->width || row >= map->height) {
        errno = ERANGE;
        return -1;
    }
    if ((tile & TABOS_TILE_RESERVED) != 0U) {
        errno = EINVAL;
        return -1;
    }
    map->layers[layer].cells[(size_t) row * map->width + column] = tile;
    return 0;
}

static void tile_transform(tabos_tile_t tile, tabos_sprite_draw_options_t* draw)
{
    const bool horizontal = (tile & TABOS_TILE_FLIP_HORIZONTAL) != 0U;
    const bool vertical   = (tile & TABOS_TILE_FLIP_VERTICAL) != 0U;
    const bool diagonal   = (tile & TABOS_TILE_FLIP_DIAGONAL) != 0U;
    if (!diagonal) {
        draw->mirror_x = horizontal;
        draw->mirror_y = vertical;
        return;
    }
    draw->rotation = TABOS_GRAPHICS_ROTATE_90;
    if (!horizontal && !vertical) {
        draw->mirror_x = true;
    } else if (horizontal && !vertical) {
        draw->rotation = TABOS_GRAPHICS_ROTATE_270;
    } else if (horizontal && vertical) {
        draw->mirror_y = true;
    }
}

static uint32_t animated_sprite(const tabos_sprite_set_t* sprites, uint32_t sprite, uint64_t elapsed_ms)
{
    for (uint32_t index = 0U; index < sprites->animation_count; ++index) {
        const tabos_sprite_animation_t* animation = &sprites->animations[index];
        if (animation->frame_count != 0U && animation->trigger_sprite == sprite) {
            return tabos_sprite_animation_frame(sprites, index, elapsed_ms);
        }
    }
    return sprite;
}

int tabos_tilemap_draw_layer(tabos_graphics_t* graphics, const tabos_tilemap_t* map, uint32_t layer,
                             const tabos_sprite_set_t* sprites, const tabos_tilemap_draw_options_t* options)
{
    if (graphics == NULL || !graphics->open || sprites == NULL || options == NULL || !valid_tile_layer(map, layer) ||
        map->tile_width == 0U || map->tile_height == 0U || map->tile_width > INT32_MAX ||
        map->tile_height > INT32_MAX) {
        errno = EINVAL;
        return -1;
    }
    /* Intersect the screen viewport before converting it to world-space cell bounds. */
    const int64_t left = options->viewport.x > 0 ? options->viewport.x : 0;
    const int64_t top  = options->viewport.y > 0 ? options->viewport.y : 0;
    int64_t right      = (int64_t) options->viewport.x + options->viewport.width;
    int64_t bottom     = (int64_t) options->viewport.y + options->viewport.height;
    if (right > graphics->width) {
        right = graphics->width;
    }
    if (bottom > graphics->height) {
        bottom = graphics->height;
    }
    if (left >= right || top >= bottom) {
        return 0;
    }
    int64_t world_left         = left + graphics->camera_x;
    int64_t world_top          = top + graphics->camera_y;
    const int64_t world_right  = right + graphics->camera_x;
    const int64_t world_bottom = bottom + graphics->camera_y;
    if (world_right <= 0 || world_bottom <= 0) {
        return 0;
    }
    if (world_left < 0) {
        world_left = 0;
    }
    if (world_top < 0) {
        world_top = 0;
    }
    const int64_t first_column = world_left / map->tile_width;
    const int64_t first_row    = world_top / map->tile_height;
    int64_t last_column        = (world_right + map->tile_width - 1U) / map->tile_width;
    int64_t last_row           = (world_bottom + map->tile_height - 1U) / map->tile_height;
    if (last_column > map->width) {
        last_column = map->width;
    }
    if (last_row > map->height) {
        last_row = map->height;
    }
    /* Tile positions are already projected for culling; do not apply the camera twice. */
    tabos_graphics_t screen = *graphics;
    screen.camera_x         = 0;
    screen.camera_y         = 0;
    for (int64_t row = first_row; row < last_row; ++row) {
        for (int64_t column = first_column; column < last_column; ++column) {
            const tabos_tile_t tile   = map->layers[layer].cells[(size_t) row * map->width + (uint32_t) column];
            const uint32_t encoded_id = tile & TABOS_TILE_ID_MASK;
            if (encoded_id == 0U) {
                continue;
            }
            uint32_t sprite = encoded_id - 1U;
            if (sprite >= sprites->sprite_count) {
                errno = EINVAL;
                return -1;
            }
            sprite = animated_sprite(sprites, sprite, options->animation_ms);
            if (sprite == TABOS_SPRITE_NONE) {
                return -1;
            }
            tabos_sprite_draw_options_t draw = {
                .width        = map->tile_width,
                .height       = map->tile_height,
                .opacity      = 255U,
                .clip         = options->viewport,
                .clip_enabled = true,
            };
            tile_transform(tile, &draw);
            const int32_t destination_x      = (int32_t) (column * map->tile_width - graphics->camera_x);
            const int32_t destination_y      = (int32_t) (row * map->tile_height - graphics->camera_y);
            const tabos_sprite_t* descriptor = &sprites->sprites[sprite];
            if (descriptor->image >= sprites->image_count) {
                errno = EINVAL;
                return -1;
            }
            const tabos_sprite_image_t* image        = &sprites->images[descriptor->image];
            const tabos_graphics_blit_options_t blit = {
                .pixels            = image->pixels,
                .bitmap_width      = image->width,
                .bitmap_height     = image->height,
                .source            = {.x      = descriptor->x,
                                      .y      = descriptor->y,
                                      .width  = descriptor->width,
                                      .height = descriptor->height},
                .destination       = {.x      = destination_x,
                                      .y      = destination_y,
                                      .width  = map->tile_width,
                                      .height = map->tile_height  },
                .rotation          = draw.rotation,
                .mirror_x          = draw.mirror_x,
                .mirror_y          = draw.mirror_y,
                .opacity           = 255U,
                .color_key_enabled = image->color_key_enabled,
                .color_key_low     = image->color_key,
                .color_key_high    = image->color_key,
                .clip              = options->viewport,
                .clip_enabled      = true,
            };
            if (tabos_graphics_blit_ex(&screen, &blit) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

int tabos_tilemap_object_property(const tabos_tilemap_object_t* object, const char* name, int32_t* value)
{
    if (object == NULL || name == NULL || value == NULL) {
        errno = EINVAL;
        return -1;
    }
    for (uint32_t index = 0U; index < object->property_count; ++index) {
        if (strcmp(object->properties[index].name, name) == 0) {
            *value = object->properties[index].value;
            return 0;
        }
    }
    errno = ENOENT;
    return -1;
}

void tabos_tilemap_unload(tabos_tilemap_t* map)
{
    if (map != NULL) {
        free(map->_storage);
        *map = (tabos_tilemap_t) {0};
    }
}

static int load_file(const char* path, uint8_t** output, size_t* output_size)
{
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }
    const long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        errno = EIO;
        return -1;
    }
    if ((uint64_t) length < TMP_HEADER_SIZE || (uint64_t) length > UINT32_MAX) {
        fclose(file);
        errno = EINVAL;
        return -1;
    }
    uint8_t* data = malloc((size_t) length);
    if (data == NULL) {
        fclose(file);
        errno = ENOMEM;
        return -1;
    }
    const bool read_ok  = fread(data, 1U, (size_t) length, file) == (size_t) length;
    const bool close_ok = fclose(file) == 0;
    if (!read_ok || !close_ok) {
        free(data);
        errno = EIO;
        return -1;
    }
    *output      = data;
    *output_size = (size_t) length;
    return 0;
}

static const char* string_at(const uint8_t* storage, uint32_t strings_offset, uint32_t strings_size, uint32_t offset)
{
    if (offset >= strings_size) {
        return NULL;
    }
    const char* string = (const char*) storage + strings_offset + offset;
    return memchr(string, '\0', strings_size - offset) == NULL ? NULL : string;
}

int tabos_tilemap_load(const char* path, tabos_tilemap_t* map)
{
    if (path == NULL || map == NULL) {
        errno = EINVAL;
        return -1;
    }
    uint8_t* file_data = NULL;
    size_t file_size   = 0U;
    if (load_file(path, &file_data, &file_size) != 0) {
        return -1;
    }
    if (file_size < TMP_HEADER_SIZE || memcmp(file_data, "TMP1", 4U) != 0 || read_u32(file_data + 4U) != 1U ||
        read_u32(file_data + 8U) != file_size) {
        free(file_data);
        errno = EINVAL;
        return -1;
    }
    const uint32_t width = read_u32(file_data + 12U), height = read_u32(file_data + 16U);
    const uint32_t tile_width = read_u32(file_data + 20U), tile_height = read_u32(file_data + 24U);
    const uint32_t layer_count = read_u32(file_data + 28U), object_count = read_u32(file_data + 32U);
    const uint32_t property_count = read_u32(file_data + 36U), strings_size = read_u32(file_data + 40U);
    const uint32_t layers_offset = read_u32(file_data + 44U), cells_offset = read_u32(file_data + 48U);
    const uint32_t objects_offset = read_u32(file_data + 52U), properties_offset = read_u32(file_data + 56U);
    const uint32_t strings_offset = read_u32(file_data + 60U);
    if (width == 0U || height == 0U || width > INT32_MAX || height > INT32_MAX || width > UINT32_MAX / height ||
        tile_width == 0U || tile_height == 0U || tile_width > INT32_MAX || tile_height > INT32_MAX ||
        !table_valid(layers_offset, layer_count, TMP_LAYER_SIZE, file_size) ||
        !table_valid(objects_offset, object_count, TMP_OBJECT_SIZE, file_size) ||
        !table_valid(properties_offset, property_count, TMP_PROPERTY_SIZE, file_size) ||
        !table_valid(strings_offset, strings_size, 1U, file_size) || !table_valid(cells_offset, 0U, 4U, file_size)) {
        free(file_data);
        errno = EINVAL;
        return -1;
    }
    const uint32_t offsets[] = {layers_offset, objects_offset, properties_offset, strings_offset};
    const uint64_t sizes[]   = {(uint64_t) layer_count * TMP_LAYER_SIZE, (uint64_t) object_count * TMP_OBJECT_SIZE,
                                (uint64_t) property_count * TMP_PROPERTY_SIZE, strings_size};
    for (uint32_t index = 0U; index < 4U; ++index) {
        for (uint32_t previous = 0U; previous < index; ++previous) {
            if (ranges_overlap(offsets[index], sizes[index], offsets[previous], sizes[previous])) {
                free(file_data);
                errno = EINVAL;
                return -1;
            }
        }
    }
    const uint32_t cell_count   = width * height;
    size_t allocation_size      = file_size;
    const bool allocation_valid = allocation_add(&allocation_size, layer_count, sizeof(tabos_tilemap_layer_t)) &&
                                  allocation_add(&allocation_size, object_count, sizeof(tabos_tilemap_object_t)) &&
                                  allocation_add(&allocation_size, property_count, sizeof(tabos_tilemap_property_t)) &&
                                  allocation_add(&allocation_size, 64U, 1U);
    if (!allocation_valid) {
        free(file_data);
        errno = ENOMEM;
        return -1;
    }
    uint8_t* storage = calloc(1U, allocation_size);
    if (storage == NULL) {
        free(file_data);
        errno = ENOMEM;
        return -1;
    }
    memcpy(storage, file_data, file_size);
    free(file_data);
    uintptr_t cursor = ((uintptr_t) storage + file_size + sizeof(void*) - 1U) & ~(uintptr_t) (sizeof(void*) - 1U);
    tabos_tilemap_layer_t* layers         = (tabos_tilemap_layer_t*) cursor;
    cursor                               += (size_t) layer_count * sizeof(*layers);
    tabos_tilemap_object_t* objects       = (tabos_tilemap_object_t*) cursor;
    cursor                               += (size_t) object_count * sizeof(*objects);
    tabos_tilemap_property_t* properties  = (tabos_tilemap_property_t*) cursor;
    bool valid                            = true;
    for (uint32_t index = 0U; index < property_count && valid; ++index) {
        const uint8_t* record = storage + properties_offset + (size_t) index * TMP_PROPERTY_SIZE;
        properties[index] =
            (tabos_tilemap_property_t) {.name  = string_at(storage, strings_offset, strings_size, read_u32(record)),
                                        .value = (int32_t) read_u32(record + 4U)};
        valid = properties[index].name != NULL;
    }
    for (uint32_t index = 0U; index < object_count && valid; ++index) {
        const uint8_t* record         = storage + objects_offset + (size_t) index * TMP_OBJECT_SIZE;
        const uint32_t first_property = read_u32(record + 40U), count = read_u32(record + 44U);
        const bool properties_valid = first_property <= property_count && count <= property_count - first_property;
        objects[index] =
            (tabos_tilemap_object_t) {.id     = read_u32(record),
                                      .name   = string_at(storage, strings_offset, strings_size, read_u32(record + 4U)),
                                      .type   = string_at(storage, strings_offset, strings_size, read_u32(record + 8U)),
                                      .shape  = (tabos_tilemap_object_shape_t) read_u32(record + 12U),
                                      .x      = (int32_t) read_u32(record + 16U),
                                      .y      = (int32_t) read_u32(record + 20U),
                                      .width  = read_u32(record + 24U),
                                      .height = read_u32(record + 28U),
                                      .tile   = read_u32(record + 32U),
                                      .properties     = properties_valid ? properties + first_property : NULL,
                                      .property_count = count};
        valid = objects[index].name != NULL && objects[index].type != NULL &&
                read_u32(record + 12U) <= TABOS_TILEMAP_OBJECT_TILE && properties_valid &&
                (objects[index].tile & TABOS_TILE_RESERVED) == 0U;
    }
    for (uint32_t index = 0U; index < layer_count && valid; ++index) {
        const uint8_t* record = storage + layers_offset + (size_t) index * TMP_LAYER_SIZE;
        const uint32_t type = read_u32(record + 4U), first = read_u32(record + 8U), count = read_u32(record + 12U);
        layers[index].name = string_at(storage, strings_offset, strings_size, read_u32(record));
        layers[index].type = (tabos_tilemap_layer_type_t) type;
        valid              = layers[index].name != NULL && type <= TABOS_TILEMAP_LAYER_OBJECTS;
        if (type == TABOS_TILEMAP_LAYER_TILES) {
            const uint64_t cell_offset = (uint64_t) cells_offset + (uint64_t) first * 4U;
            valid                      = valid && count == cell_count && cell_offset <= UINT32_MAX &&
                    table_valid((uint32_t) cell_offset, count, 4U, file_size);
            /* Writable cells must not alias validated names or descriptor tables. */
            for (uint32_t table = 0U; table < 4U && valid; ++table) {
                valid = !ranges_overlap((uint32_t) cell_offset, (uint64_t) count * 4U, offsets[table], sizes[table]);
            }
            layers[index].cells = valid ? (tabos_tile_t*) (storage + cell_offset) : NULL;
            for (uint32_t cell = 0U; cell < count && valid; ++cell) {
                valid = (layers[index].cells[cell] & TABOS_TILE_RESERVED) == 0U;
            }
        } else {
            valid                      = valid && first <= object_count && count <= object_count - first;
            layers[index].objects      = valid ? objects + first : NULL;
            layers[index].object_count = count;
        }
    }
    if (!valid) {
        free(storage);
        errno = EINVAL;
        return -1;
    }
    *map = (tabos_tilemap_t) {.width       = width,
                              .height      = height,
                              .tile_width  = tile_width,
                              .tile_height = tile_height,
                              .layers      = layers,
                              .layer_count = layer_count,
                              ._storage    = storage};
    return 0;
}
