#include <tabos/sprite.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    TSP_HEADER_SIZE = 60,
    TSP_IMAGE_SIZE  = 20,
    TSP_SPRITE_SIZE = 32,
    TSP_ANIM_SIZE   = 16,
    TSP_FRAME_SIZE  = 8,
    TSP_META_SIZE   = 8,
    TSP_PART_SIZE   = 24,
};

static uint32_t read_u32(const uint8_t* data)
{
    return (uint32_t) data[0] | ((uint32_t) data[1] << 8U) | ((uint32_t) data[2] << 16U) | ((uint32_t) data[3] << 24U);
}

static bool table_valid(uint32_t offset, uint32_t count, uint32_t stride, size_t size)
{
    return offset >= TSP_HEADER_SIZE && (offset & 3U) == 0U && offset <= size && count <= (size - offset) / stride;
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

static uint8_t combined_opacity(uint8_t first, uint8_t second)
{
    return (uint8_t) (((uint32_t) first * second + 127U) / 255U);
}

static void transformed_pivot(const tabos_sprite_t* sprite, const tabos_sprite_draw_options_t* options,
                              uint32_t* natural_width, uint32_t* natural_height, int32_t* pivot_x, int32_t* pivot_y)
{
    int32_t px      = sprite->pivot_x;
    int32_t py      = sprite->pivot_y;
    uint32_t width  = sprite->width;
    uint32_t height = sprite->height;
    if (options->mirror_x) {
        px = (int32_t) width - px;
    }
    if (options->mirror_y) {
        py = (int32_t) height - py;
    }
    if (options->rotation == TABOS_GRAPHICS_ROTATE_90) {
        const int32_t old_x      = px;
        px                       = (int32_t) height - py;
        py                       = old_x;
        const uint32_t old_width = width;
        width                    = height;
        height                   = old_width;
    } else if (options->rotation == TABOS_GRAPHICS_ROTATE_180) {
        px = (int32_t) width - px;
        py = (int32_t) height - py;
    } else if (options->rotation == TABOS_GRAPHICS_ROTATE_270) {
        const int32_t old_x      = px;
        px                       = py;
        py                       = (int32_t) width - old_x;
        const uint32_t old_width = width;
        width                    = height;
        height                   = old_width;
    }
    *natural_width  = width;
    *natural_height = height;
    *pivot_x        = px;
    *pivot_y        = py;
}

int tabos_sprite_draw_ex(tabos_graphics_t* graphics, const tabos_sprite_set_t* set, uint32_t sprite_id, int32_t x,
                         int32_t y, const tabos_sprite_draw_options_t* options)
{
    if (graphics == NULL || set == NULL || options == NULL || sprite_id >= set->sprite_count ||
        options->rotation > TABOS_GRAPHICS_ROTATE_270) {
        errno = EINVAL;
        return -1;
    }
    const tabos_sprite_t* sprite = &set->sprites[sprite_id];
    if (sprite->image >= set->image_count) {
        errno = EINVAL;
        return -1;
    }
    const tabos_sprite_image_t* image = &set->images[sprite->image];
    uint32_t natural_width, natural_height;
    int32_t pivot_x, pivot_y;
    transformed_pivot(sprite, options, &natural_width, &natural_height, &pivot_x, &pivot_y);
    const uint32_t width  = options->width == 0U ? natural_width : options->width;
    const uint32_t height = options->height == 0U ? natural_height : options->height;
    if (natural_width == 0U || natural_height == 0U || width == 0U || height == 0U) {
        errno = EINVAL;
        return -1;
    }
    const int32_t scaled_pivot_x             = (int32_t) ((int64_t) pivot_x * width / natural_width);
    const int32_t scaled_pivot_y             = (int32_t) ((int64_t) pivot_y * height / natural_height);
    const tabos_graphics_blit_options_t blit = {
        .pixels            = image->pixels,
        .bitmap_width      = image->width,
        .bitmap_height     = image->height,
        .source            = {         .x = sprite->x,          .y = sprite->y, .width = sprite->width, .height = sprite->height},
        .destination       = {.x = x - scaled_pivot_x, .y = y - scaled_pivot_y,         .width = width,         .height = height},
        .rotation          = options->rotation,
        .mirror_x          = options->mirror_x,
        .mirror_y          = options->mirror_y,
        .opacity           = options->opacity,
        .color_key_enabled = image->color_key_enabled,
        .color_key_low     = image->color_key,
        .color_key_high    = image->color_key,
        .clip              = options->clip,
        .clip_enabled      = options->clip_enabled,
    };
    return tabos_graphics_blit_ex(graphics, &blit);
}

int tabos_sprite_draw(tabos_graphics_t* graphics, const tabos_sprite_set_t* set, uint32_t sprite, int32_t x, int32_t y)
{
    const tabos_sprite_draw_options_t options = {.opacity = 255U};
    return tabos_sprite_draw_ex(graphics, set, sprite, x, y, &options);
}

static const tabos_sprite_animation_t* animation_get(const tabos_sprite_set_t* set, uint32_t animation_id,
                                                     uint64_t* duration)
{
    if (set == NULL || set->animations == NULL || animation_id >= set->animation_count) {
        errno = EINVAL;
        return NULL;
    }
    const tabos_sprite_animation_t* animation = &set->animations[animation_id];
    if (animation->frames == NULL || animation->frame_count == 0U) {
        errno = EINVAL;
        return NULL;
    }
    *duration = 0U;
    for (uint32_t index = 0U; index < animation->frame_count; ++index) {
        if (animation->frames[index].duration_ms == 0U || animation->frames[index].sprite >= set->sprite_count) {
            errno = EINVAL;
            return NULL;
        }
        *duration += animation->frames[index].duration_ms;
    }
    return animation;
}

int tabos_sprite_animation_finished(const tabos_sprite_set_t* set, uint32_t animation_id, uint64_t elapsed_ms,
                                    bool* finished)
{
    if (finished == NULL) {
        errno = EINVAL;
        return -1;
    }
    uint64_t duration                         = 0U;
    const tabos_sprite_animation_t* animation = animation_get(set, animation_id, &duration);
    if (animation == NULL) {
        return -1;
    }
    /* Division avoids overflow when total duration times repeat count exceeds UINT64_MAX. */
    *finished = animation->repeat_count != 0U && elapsed_ms / duration >= animation->repeat_count;
    return 0;
}

uint32_t tabos_sprite_animation_frame(const tabos_sprite_set_t* set, uint32_t animation_id, uint64_t elapsed_ms)
{
    uint64_t duration                         = 0U;
    const tabos_sprite_animation_t* animation = animation_get(set, animation_id, &duration);
    if (animation == NULL) {
        return TABOS_SPRITE_NONE;
    }
    if (animation->repeat_count != 0U && elapsed_ms / duration >= animation->repeat_count) {
        return animation->frames[animation->frame_count - 1U].sprite;
    }
    uint64_t position = elapsed_ms % duration;
    for (uint32_t index = 0U; index < animation->frame_count; ++index) {
        if (position < animation->frames[index].duration_ms) {
            return animation->frames[index].sprite;
        }
        position -= animation->frames[index].duration_ms;
    }
    return animation->frames[animation->frame_count - 1U].sprite;
}

int tabos_sprite_animation_draw(tabos_graphics_t* graphics, const tabos_sprite_set_t* set, uint32_t animation,
                                int32_t x, int32_t y, uint64_t elapsed_ms)
{
    const tabos_sprite_draw_options_t options = {.opacity = 255U};
    return tabos_sprite_animation_draw_ex(graphics, set, animation, x, y, elapsed_ms, &options);
}

int tabos_sprite_animation_draw_ex(tabos_graphics_t* graphics, const tabos_sprite_set_t* set, uint32_t animation,
                                   int32_t x, int32_t y, uint64_t elapsed_ms,
                                   const tabos_sprite_draw_options_t* options)
{
    const uint32_t frame = tabos_sprite_animation_frame(set, animation, elapsed_ms);
    if (frame == TABOS_SPRITE_NONE) {
        return -1;
    }
    return tabos_sprite_draw_ex(graphics, set, frame, x, y, options);
}

int tabos_metasprite_draw(tabos_graphics_t* graphics, const tabos_sprite_set_t* set, uint32_t metasprite_id, int32_t x,
                          int32_t y, bool mirror_x, bool mirror_y, uint8_t opacity)
{
    if (graphics == NULL || set == NULL || metasprite_id >= set->metasprite_count) {
        errno = EINVAL;
        return -1;
    }
    const tabos_metasprite_t* metasprite = &set->metasprites[metasprite_id];
    for (uint32_t index = 0U; index < metasprite->part_count; ++index) {
        const tabos_metasprite_part_t* part       = &metasprite->parts[index];
        const tabos_sprite_draw_options_t options = {
            .rotation = part->rotation,
            .mirror_x = part->mirror_x != mirror_x,
            .mirror_y = part->mirror_y != mirror_y,
            .opacity  = combined_opacity(part->opacity, opacity),
        };
        const int32_t part_x = x + (mirror_x ? -part->x : part->x);
        const int32_t part_y = y + (mirror_y ? -part->y : part->y);
        if (tabos_sprite_draw_ex(graphics, set, part->sprite, part_x, part_y, &options) != 0) {
            return -1;
        }
    }
    return 0;
}

uint32_t tabos_sprite_flags(const tabos_sprite_set_t* set, uint32_t sprite)
{
    if (set == NULL || sprite >= set->sprite_count) {
        errno = EINVAL;
        return 0U;
    }
    return set->sprites[sprite].flags;
}

void tabos_sprite_set_unload(tabos_sprite_set_t* set)
{
    if (set != NULL) {
        free(set->_storage);
        *set = (tabos_sprite_set_t) {0};
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
    if ((uint64_t) length < TSP_HEADER_SIZE || (uint64_t) length > UINT32_MAX) {
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

int tabos_sprite_set_load(const char* path, tabos_sprite_set_t* set)
{
    if (path == NULL || set == NULL) {
        errno = EINVAL;
        return -1;
    }
    uint8_t* file_data = NULL;
    size_t file_size   = 0U;
    if (load_file(path, &file_data, &file_size) != 0) {
        return -1;
    }
    if (file_size < TSP_HEADER_SIZE || memcmp(file_data, "TSP1", 4U) != 0 || read_u32(file_data + 4U) != 1U ||
        read_u32(file_data + 8U) != file_size) {
        free(file_data);
        errno = EINVAL;
        return -1;
    }
    const uint32_t counts[]  = {read_u32(file_data + 12U), read_u32(file_data + 16U), read_u32(file_data + 20U),
                                read_u32(file_data + 24U), read_u32(file_data + 28U), read_u32(file_data + 32U)};
    const uint32_t offsets[] = {read_u32(file_data + 36U), read_u32(file_data + 40U), read_u32(file_data + 44U),
                                read_u32(file_data + 48U), read_u32(file_data + 52U), read_u32(file_data + 56U)};
    const uint32_t strides[] = {TSP_IMAGE_SIZE, TSP_SPRITE_SIZE, TSP_ANIM_SIZE,
                                TSP_FRAME_SIZE, TSP_META_SIZE,   TSP_PART_SIZE};
    for (uint32_t index = 0U; index < 6U; ++index) {
        if (!table_valid(offsets[index], counts[index], strides[index], file_size)) {
            free(file_data);
            errno = EINVAL;
            return -1;
        }
        for (uint32_t previous = 0U; previous < index; ++previous) {
            if (ranges_overlap(offsets[index], (uint64_t) counts[index] * strides[index], offsets[previous],
                               (uint64_t) counts[previous] * strides[previous])) {
                free(file_data);
                errno = EINVAL;
                return -1;
            }
        }
    }
    size_t allocation_size      = file_size;
    const bool allocation_valid = allocation_add(&allocation_size, counts[0], sizeof(tabos_sprite_image_t)) &&
                                  allocation_add(&allocation_size, counts[1], sizeof(tabos_sprite_t)) &&
                                  allocation_add(&allocation_size, counts[2], sizeof(tabos_sprite_animation_t)) &&
                                  allocation_add(&allocation_size, counts[3], sizeof(tabos_sprite_frame_t)) &&
                                  allocation_add(&allocation_size, counts[4], sizeof(tabos_metasprite_t)) &&
                                  allocation_add(&allocation_size, counts[5], sizeof(tabos_metasprite_part_t)) &&
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
    tabos_sprite_image_t* images          = (tabos_sprite_image_t*) cursor;
    cursor                               += (size_t) counts[0] * sizeof(*images);
    tabos_sprite_t* sprites               = (tabos_sprite_t*) cursor;
    cursor                               += (size_t) counts[1] * sizeof(*sprites);
    tabos_sprite_animation_t* animations  = (tabos_sprite_animation_t*) cursor;
    cursor                               += (size_t) counts[2] * sizeof(*animations);
    tabos_sprite_frame_t* frames          = (tabos_sprite_frame_t*) cursor;
    cursor                               += (size_t) counts[3] * sizeof(*frames);
    tabos_metasprite_t* metasprites       = (tabos_metasprite_t*) cursor;
    cursor                               += (size_t) counts[4] * sizeof(*metasprites);
    tabos_metasprite_part_t* parts        = (tabos_metasprite_part_t*) cursor;

    bool valid = true;
    for (uint32_t index = 0U; index < counts[0] && valid; ++index) {
        const uint8_t* record = storage + offsets[0] + (size_t) index * TSP_IMAGE_SIZE;
        const uint32_t width = read_u32(record), height = read_u32(record + 4U), pixels = read_u32(record + 8U);
        valid = width != 0U && height != 0U && width <= UINT32_MAX / height &&
                table_valid(pixels, width * height, sizeof(tabos_color_t), file_size);
        for (uint32_t table = 0U; table < 6U && valid; ++table) {
            valid = !ranges_overlap(pixels, (uint64_t) width * height * sizeof(tabos_color_t), offsets[table],
                                    (uint64_t) counts[table] * strides[table]);
        }
        images[index] = (tabos_sprite_image_t) {.pixels    = valid ? (const tabos_color_t*) (storage + pixels) : NULL,
                                                .width     = width,
                                                .height    = height,
                                                .color_key = (tabos_color_t) read_u32(record + 16U),
                                                .color_key_enabled = read_u32(record + 12U) != 0U};
    }
    for (uint32_t index = 0U; index < counts[1] && valid; ++index) {
        const uint8_t* record = storage + offsets[1] + (size_t) index * TSP_SPRITE_SIZE;
        sprites[index]        = (tabos_sprite_t) {.image   = read_u32(record),
                                                  .x       = (int32_t) read_u32(record + 4U),
                                                  .y       = (int32_t) read_u32(record + 8U),
                                                  .width   = read_u32(record + 12U),
                                                  .height  = read_u32(record + 16U),
                                                  .pivot_x = (int32_t) read_u32(record + 20U),
                                                  .pivot_y = (int32_t) read_u32(record + 24U),
                                                  .flags   = read_u32(record + 28U)};
        valid                 = sprites[index].image < counts[0] && sprites[index].x >= 0 && sprites[index].y >= 0 &&
                sprites[index].width != 0U && sprites[index].height != 0U &&
                (uint64_t) (uint32_t) sprites[index].x + sprites[index].width <= images[sprites[index].image].width &&
                (uint64_t) (uint32_t) sprites[index].y + sprites[index].height <= images[sprites[index].image].height;
    }
    for (uint32_t index = 0U; index < counts[3] && valid; ++index) {
        const uint8_t* record = storage + offsets[3] + (size_t) index * TSP_FRAME_SIZE;
        frames[index] = (tabos_sprite_frame_t) {.sprite = read_u32(record), .duration_ms = read_u32(record + 4U)};
        valid         = frames[index].sprite < counts[1] && frames[index].duration_ms != 0U;
    }
    for (uint32_t index = 0U; index < counts[2] && valid; ++index) {
        const uint8_t* record = storage + offsets[2] + (size_t) index * TSP_ANIM_SIZE;
        const uint32_t first = read_u32(record), count = read_u32(record + 4U);
        const uint32_t trigger = read_u32(record + 12U);
        valid                  = count != 0U && first <= counts[3] && count <= counts[3] - first && trigger < counts[1];
        animations[index]      = (tabos_sprite_animation_t) {
                 .frames         = valid ? frames + first : NULL,
                 .frame_count    = count,
                 .repeat_count   = read_u32(record + 8U),
                 .trigger_sprite = trigger,
        };
    }
    for (uint32_t index = 0U; index < counts[5] && valid; ++index) {
        const uint8_t* record = storage + offsets[5] + (size_t) index * TSP_PART_SIZE;
        parts[index]          = (tabos_metasprite_part_t) {.sprite   = read_u32(record),
                                                           .x        = (int32_t) read_u32(record + 4U),
                                                           .y        = (int32_t) read_u32(record + 8U),
                                                           .rotation = (tabos_graphics_rotation_t) read_u32(record + 12U),
                                                           .mirror_x = (record[16U] & 1U) != 0U,
                                                           .mirror_y = (record[16U] & 2U) != 0U,
                                                           .opacity  = record[17U]};
        valid                 = parts[index].sprite < counts[1] && read_u32(record + 12U) <= TABOS_GRAPHICS_ROTATE_270;
    }
    for (uint32_t index = 0U; index < counts[4] && valid; ++index) {
        const uint8_t* record = storage + offsets[4] + (size_t) index * TSP_META_SIZE;
        const uint32_t first = read_u32(record), count = read_u32(record + 4U);
        valid              = first <= counts[5] && count <= counts[5] - first;
        metasprites[index] = (tabos_metasprite_t) {.parts = valid ? parts + first : NULL, .part_count = count};
    }
    if (!valid) {
        free(storage);
        errno = EINVAL;
        return -1;
    }
    *set = (tabos_sprite_set_t) {.images           = images,
                                 .image_count      = counts[0],
                                 .sprites          = sprites,
                                 .sprite_count     = counts[1],
                                 .animations       = animations,
                                 .animation_count  = counts[2],
                                 .metasprites      = metasprites,
                                 .metasprite_count = counts[4],
                                 ._storage         = storage};
    return 0;
}
