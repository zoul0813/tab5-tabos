#include <tabos/tilemap.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Small, independently authored wire fixtures exercise every descriptor table. */
enum {
    ASSET_SIZE      = 176,
    TSP_IMAGES      = 60,
    TSP_SPRITES     = 80,
    TSP_ANIMATIONS  = 112,
    TSP_FRAMES      = 128,
    TSP_METASPRITES = 136,
    TSP_PARTS       = 144,
    TSP_PIXELS      = 168,
    TMP_LAYERS      = 64,
    TMP_CELLS       = 104,
    TMP_OBJECTS     = 108,
    TMP_PROPERTIES  = 156,
    TMP_STRINGS     = 168,
};

static void* allocations[16];
static size_t allocation_calls;
static size_t fail_allocation;
static unsigned int failures;
static unsigned int cases;

void* test_asset_malloc(size_t size);
void* test_asset_calloc(size_t count, size_t size);
void test_asset_free(void* pointer);

static void* track(void* pointer)
{
    if (pointer != NULL) {
        for (size_t index = 0U; index < 16U; ++index) {
            if (allocations[index] == NULL) {
                allocations[index] = pointer;
                return pointer;
            }
        }
        abort();
    }
    return pointer;
}

void* test_asset_malloc(size_t size)
{
    ++allocation_calls;
    if (allocation_calls == fail_allocation) {
        errno = ENOMEM;
        return NULL;
    }
    return track(malloc(size));
}

void* test_asset_calloc(size_t count, size_t size)
{
    ++allocation_calls;
    if (allocation_calls == fail_allocation) {
        errno = ENOMEM;
        return NULL;
    }
    return track(calloc(count, size));
}

void test_asset_free(void* pointer)
{
    if (pointer == NULL) {
        return;
    }
    for (size_t index = 0U; index < 16U; ++index) {
        if (allocations[index] == pointer) {
            allocations[index] = NULL;
            free(pointer);
            return;
        }
    }
    fprintf(stderr, "untracked or duplicate free\n");
    abort();
}

static size_t live_allocations(void)
{
    size_t live = 0U;
    for (size_t index = 0U; index < 16U; ++index) {
        if (allocations[index] != NULL) {
            ++live;
        }
    }
    return live;
}

/* Drawing is outside this loader test; fail loudly if a loader starts drawing. */
int tabos_graphics_blit_ex(tabos_graphics_t* graphics, const tabos_graphics_blit_options_t* options)
{
    (void) graphics;
    (void) options;
    abort();
}

static void expect(bool condition, const char* name)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", name);
        ++failures;
    }
}

static void put_u32(uint8_t* data, size_t offset, uint32_t value)
{
    for (size_t index = 0U; index < 4U; ++index) {
        data[offset + index] = (uint8_t) (value >> (index * 8U));
    }
}

static void fixture(uint8_t* data, bool map)
{
    memset(data, 0, ASSET_SIZE);
    memcpy(data, map ? "TMP1" : "TSP1", 4U);
    put_u32(data, 4U, 1U);
    put_u32(data, 8U, ASSET_SIZE);
    if (!map) {
        const uint32_t offsets[] = {TSP_IMAGES, TSP_SPRITES, TSP_ANIMATIONS, TSP_FRAMES, TSP_METASPRITES, TSP_PARTS};
        for (size_t index = 0U; index < 6U; ++index) {
            put_u32(data, 12U + index * 4U, 1U);
            put_u32(data, 36U + index * 4U, offsets[index]);
        }
        put_u32(data, TSP_IMAGES, 2U);
        put_u32(data, TSP_IMAGES + 4U, 2U);
        put_u32(data, TSP_IMAGES + 8U, TSP_PIXELS);
        put_u32(data, TSP_SPRITES + 12U, 2U);
        put_u32(data, TSP_SPRITES + 16U, 2U);
        put_u32(data, TSP_ANIMATIONS + 4U, 1U);
        put_u32(data, TSP_FRAMES + 4U, 10U);
        put_u32(data, TSP_METASPRITES + 4U, 1U);
        data[TSP_PARTS + 17U] = 255U;
        put_u32(data, TSP_PIXELS, UINT32_C(0x07e0f800));
        put_u32(data, TSP_PIXELS + 4U, UINT32_C(0xffff001f));
        return;
    }
    for (size_t offset = 12U; offset <= 24U; offset += 4U) {
        put_u32(data, offset, 1U);
    }
    put_u32(data, 28U, 2U);
    put_u32(data, 32U, 1U);
    put_u32(data, 36U, 1U);
    put_u32(data, 40U, 8U);
    put_u32(data, 44U, TMP_LAYERS);
    put_u32(data, 48U, TMP_CELLS);
    put_u32(data, 52U, TMP_OBJECTS);
    put_u32(data, 56U, TMP_PROPERTIES);
    put_u32(data, 60U, TMP_STRINGS);
    put_u32(data, TMP_LAYERS + 12U, 1U);
    put_u32(data, TMP_LAYERS + 24U, TABOS_TILEMAP_LAYER_OBJECTS);
    put_u32(data, TMP_LAYERS + 32U, 1U);
    put_u32(data, TMP_CELLS, TABOS_TILE(0U));
    put_u32(data, TMP_OBJECTS, 42U);
    put_u32(data, TMP_OBJECTS + 4U, 2U);
    put_u32(data, TMP_OBJECTS + 8U, 4U);
    put_u32(data, TMP_OBJECTS + 12U, TABOS_TILEMAP_OBJECT_TILE);
    put_u32(data, TMP_OBJECTS + 24U, 1U);
    put_u32(data, TMP_OBJECTS + 28U, 1U);
    put_u32(data, TMP_OBJECTS + 32U, TABOS_TILE(0U));
    put_u32(data, TMP_OBJECTS + 44U, 1U);
    put_u32(data, TMP_PROPERTIES, 6U);
    put_u32(data, TMP_PROPERTIES + 4U, 7U);
    memcpy(data + TMP_STRINGS, "a\0b\0c\0d", 8U);
}

static void write_fixture(const char* path, const uint8_t* data, size_t size)
{
    FILE* file = fopen(path, "wb");
    if (file == NULL || fwrite(data, 1U, size, file) != size || fclose(file) != 0) {
        fprintf(stderr, "cannot write fixture\n");
        exit(2);
    }
}

static void check_load(const char* path, bool map, int expected_errno, const char* name)
{
    ++cases;
    tabos_tilemap_t loaded_map    = {0};
    tabos_sprite_set_t loaded_set = {0};
    errno                         = 0;
    const int result = map ? tabos_tilemap_load(path, &loaded_map) : tabos_sprite_set_load(path, &loaded_set);
    const int error  = errno;
    expect(result == (expected_errno == 0 ? 0 : -1), name);
    if (expected_errno != 0) {
        expect(error == expected_errno, name);
        expect(live_allocations() == 0U, "failed load frees all allocations before returning");
        expect(loaded_map._storage == NULL && loaded_map.layers == NULL && loaded_map.layer_count == 0U &&
                   loaded_map.width == 0U && loaded_map.height == 0U && loaded_map.tile_width == 0U &&
                   loaded_map.tile_height == 0U,
               "failed map load leaves output empty");
        expect(loaded_set._storage == NULL && loaded_set.images == NULL && loaded_set.sprites == NULL &&
                   loaded_set.animations == NULL && loaded_set.metasprites == NULL && loaded_set.image_count == 0U &&
                   loaded_set.sprite_count == 0U && loaded_set.animation_count == 0U &&
                   loaded_set.metasprite_count == 0U,
               "failed sprite load leaves output empty");
    } else if (result == 0) {
        expect(live_allocations() == 1U, "successful load retains only owned storage");
        if (map) {
            int32_t value = 0;
            expect(loaded_map.layer_count == 2U && loaded_map.layers[0].cells[0] == TABOS_TILE(0U) &&
                       loaded_map.layers[1].objects[0].id == 42U &&
                       tabos_tilemap_object_property(&loaded_map.layers[1].objects[0], "d", &value) == 0 && value == 7,
                   "map fixture descriptors decode");
            expect(tabos_tilemap_set(&loaded_map, 0U, 0U, 0U, 0U) == 0 && strcmp(loaded_map.layers[0].name, "a") == 0,
                   "editing cells preserves metadata");
        } else {
            expect(loaded_set.image_count == 1U && loaded_set.sprite_count == 1U &&
                       loaded_set.images[0].pixels[0] == UINT16_C(0xf800) &&
                       loaded_set.animations[0].frames[0].duration_ms == 10U &&
                       loaded_set.metasprites[0].parts[0].opacity == 255U,
                   "sprite fixture descriptors decode");
        }
    }
    tabos_tilemap_unload(&loaded_map);
    tabos_sprite_set_unload(&loaded_set);
    expect(loaded_map.layers == NULL && loaded_map._storage == NULL && loaded_map.layer_count == 0U &&
               loaded_set.images == NULL && loaded_set._storage == NULL && loaded_set.image_count == 0U,
           "unload clears descriptors");
    tabos_tilemap_unload(&loaded_map);
    tabos_sprite_set_unload(&loaded_set);
    expect(live_allocations() == 0U, "unload is idempotent and frees all storage");
}

typedef struct {
        const char* name;
        size_t offset;
        uint32_t value;
} mutation_t;

static void check_mutations(const char* path, bool map, const mutation_t* mutations, size_t count)
{
    for (size_t index = 0U; index < count; ++index) {
        uint8_t data[ASSET_SIZE];
        fixture(data, map);
        put_u32(data, mutations[index].offset, mutations[index].value);
        write_fixture(path, data, sizeof(data));
        check_load(path, map, EINVAL, mutations[index].name);
    }
}

static void check_common(const char* path, bool map)
{
    uint8_t data[ASSET_SIZE];
    fixture(data, map);
    write_fixture(path, data, sizeof(data));
    allocation_calls = 0U;
    check_load(path, map, 0, "valid fixture");
    const size_t successful_calls = allocation_calls;
    expect(successful_calls > 0U, "fixture exercises allocations");
    for (size_t index = 1U; index <= successful_calls; ++index) {
        allocation_calls = 0U;
        fail_allocation  = index;
        check_load(path, map, ENOMEM, "injected allocation failure");
        expect(allocation_calls >= index, "failure injection reached");
        fail_allocation = 0U;
        check_load(path, map, 0, "reload after allocation failure");
    }
    for (size_t size = 0U; size < sizeof(data); ++size) {
        write_fixture(path, data, size);
        check_load(path, map, EINVAL, "truncated asset with intact magic");
        if (size >= (map ? 64U : 60U)) {
            put_u32(data, 8U, (uint32_t) size);
            write_fixture(path, data, size);
            check_load(path, map, EINVAL, "truncated tables with matching declared file size");
            put_u32(data, 8U, ASSET_SIZE);
        }
    }
    const mutation_t common[] = {
        {              "bad magic", 0U,              0U},
        {    "unsupported version", 4U,              2U},
        {"declared size too small", 8U, ASSET_SIZE - 1U},
        {"declared size too large", 8U, ASSET_SIZE + 1U},
    };
    check_mutations(path, map, common, sizeof(common) / sizeof(common[0]));
    const size_t first_offset = map ? 44U : 36U;
    const size_t table_count  = map ? 5U : 6U;
    for (size_t index = 0U; index < table_count; ++index) {
        const mutation_t offsets[] = {
            {"unaligned table offset", first_offset + index * 4U,             65U},
            {        "table past EOF", first_offset + index * 4U, ASSET_SIZE + 4U},
            {  "table aliases header", first_offset + index * 4U,              4U},
        };
        check_mutations(path, map, offsets, sizeof(offsets) / sizeof(offsets[0]));
    }
    for (size_t index = 0U; index < (map ? 4U : 6U); ++index) {
        const mutation_t count = {"overflowing table count", (map ? 28U : 12U) + index * 4U, UINT32_MAX};
        check_mutations(path, map, &count, 1U);
    }
    fixture(data, map);
    write_fixture(path, data, sizeof(data));
    for (size_t index = 0U; index < 32U; ++index) {
        check_load(path, map, 0, "repeated load/unload");
    }
}

static void check_preserved_output(const char* path, bool map)
{
    uint8_t data[ASSET_SIZE];
    fixture(data, map);
    write_fixture(path, data, sizeof(data));
    tabos_tilemap_t loaded_map    = {0};
    tabos_sprite_set_t loaded_set = {0};
    const int loaded = map ? tabos_tilemap_load(path, &loaded_map) : tabos_sprite_set_load(path, &loaded_set);
    expect(loaded == 0, "load asset before failed replacement");
    if (loaded != 0) {
        return;
    }
    uint8_t saved_map[sizeof(loaded_map)];
    uint8_t saved_set[sizeof(loaded_set)];
    memcpy(saved_map, &loaded_map, sizeof(saved_map));
    memcpy(saved_set, &loaded_set, sizeof(saved_set));
    for (size_t failure = 0U; failure < 3U; ++failure) {
        fixture(data, map);
        if (failure == 0U) {
            /* Fail after allocation and partial descriptor decoding. */
            put_u32(data, map ? TMP_OBJECTS + 40U : TSP_METASPRITES, UINT32_MAX);
        }
        write_fixture(path, data, sizeof(data));
        allocation_calls = 0U;
        fail_allocation  = failure;
        errno            = 0;
        const int result = map ? tabos_tilemap_load(path, &loaded_map) : tabos_sprite_set_load(path, &loaded_set);
        const int error  = errno;
        fail_allocation  = 0U;
        ++cases;
        expect(result == -1 && error == (failure == 0U ? EINVAL : ENOMEM), "failed replacement reports error");
        expect(memcmp(saved_map, &loaded_map, sizeof(saved_map)) == 0 &&
                   memcmp(saved_set, &loaded_set, sizeof(saved_set)) == 0,
               "failure preserves existing output");
        expect(live_allocations() == 1U, "failed replacement retains only original storage");
    }
    tabos_tilemap_unload(&loaded_map);
    tabos_sprite_set_unload(&loaded_set);
    expect(live_allocations() == 0U, "original asset remains unloadable after failures");
}

int main(void)
{
    char path[]          = "/tmp/tabos-asset-loaders-XXXXXX";
    const int descriptor = mkstemp(path);
    if (descriptor < 0 || close(descriptor) != 0) {
        return 2;
    }
    check_common(path, false);
    check_common(path, true);
    check_preserved_output(path, false);
    check_preserved_output(path, true);
    const mutation_t sprite_cases[] = {
        {                 "zero image width",           TSP_IMAGES,              0U},
        {           "overflowing image area",           TSP_IMAGES,      UINT32_MAX},
        {                 "unaligned pixels",      TSP_IMAGES + 8U, TSP_PIXELS + 1U},
        {                  "pixels past EOF",      TSP_IMAGES + 8U,      ASSET_SIZE},
        {              "pixels alias header",      TSP_IMAGES + 8U,              4U},
        {         "pixels alias descriptors",      TSP_IMAGES + 8U,     TSP_SPRITES},
        {    "overlapping descriptor tables",                  52U,  TSP_ANIMATIONS},
        {          "invalid image reference",          TSP_SPRITES,              1U},
        {                "negative source x",     TSP_SPRITES + 4U,      UINT32_MAX},
        {              "source beyond image",     TSP_SPRITES + 8U,              2U},
        {                "zero sprite width",    TSP_SPRITES + 12U,              0U},
        {               "zero sprite height",    TSP_SPRITES + 16U,              0U},
        {         "overflowing sprite width",    TSP_SPRITES + 12U,      UINT32_MAX},
        {             "invalid frame sprite",           TSP_FRAMES,              1U},
        {              "zero frame duration",      TSP_FRAMES + 4U,              0U},
        {    "invalid animation first frame",       TSP_ANIMATIONS,      UINT32_MAX},
        {"overflowing animation frame count",  TSP_ANIMATIONS + 4U,      UINT32_MAX},
        {                  "empty animation",  TSP_ANIMATIONS + 4U,              0U},
        {        "invalid animation trigger", TSP_ANIMATIONS + 12U,              1U},
        {   "invalid metasprite part sprite",            TSP_PARTS,              1U},
        {            "invalid part rotation",      TSP_PARTS + 12U,              4U},
        {           "negative part rotation",      TSP_PARTS + 12U,      UINT32_MAX},
        {    "invalid metasprite first part",      TSP_METASPRITES,      UINT32_MAX},
        {     "overflowing metasprite count", TSP_METASPRITES + 4U,      UINT32_MAX},
    };
    check_mutations(path, false, sprite_cases, sizeof(sprite_cases) / sizeof(sprite_cases[0]));
    const mutation_t map_cases[] = {
        {                       "zero map width",               12U,                       0U},
        {                      "zero map height",               16U,                       0U},
        {                  "oversized map width",               12U,               UINT32_MAX},
        {         "overflowing map cell product",               12U,     UINT32_C(0x7fffffff)},
        {                      "zero tile width",               20U,                       0U},
        {                     "zero tile height",               24U,                       0U},
        {                 "oversized tile width",               20U,               UINT32_MAX},
        {                "oversized tile height",               24U,               UINT32_MAX},
        {                  "cells alias strings",               48U,              TMP_STRINGS},
        {"overlapping property and layer tables",               56U,               TMP_LAYERS},
        {                   "invalid layer name",        TMP_LAYERS,                       8U},
        {                   "invalid layer type",   TMP_LAYERS + 4U,               UINT32_MAX},
        {               "overflowing first cell",   TMP_LAYERS + 8U,               UINT32_MAX},
        {                  "cell count mismatch",  TMP_LAYERS + 12U,                       2U},
        {             "overflowing first object",  TMP_LAYERS + 28U,               UINT32_MAX},
        {       "overflowing layer object count",  TMP_LAYERS + 32U,               UINT32_MAX},
        {                "reserved cell GID bit",         TMP_CELLS, TABOS_TILE_RESERVED | 1U},
        {                  "invalid object name",  TMP_OBJECTS + 4U,                       8U},
        {           "invalid object type string",  TMP_OBJECTS + 8U,               UINT32_MAX},
        {                 "invalid object shape", TMP_OBJECTS + 12U,                       3U},
        {                "negative object shape", TMP_OBJECTS + 12U,               UINT32_MAX},
        {              "reserved object GID bit", TMP_OBJECTS + 32U, TABOS_TILE_RESERVED | 1U},
        {               "invalid first property", TMP_OBJECTS + 40U,               UINT32_MAX},
        {    "overflowing object property count", TMP_OBJECTS + 44U,               UINT32_MAX},
        {                "invalid property name",    TMP_PROPERTIES,                       8U},
        {           "unterminated property name",  TMP_STRINGS + 4U,     UINT32_C(0x61616161)},
    };
    check_mutations(path, true, map_cases, sizeof(map_cases) / sizeof(map_cases[0]));
    uint8_t data[ASSET_SIZE];
    fixture(data, true);
    put_u32(data, 12U, UINT32_C(0x40000000));
    put_u32(data, 16U, 4U);
    write_fixture(path, data, sizeof(data));
    check_load(path, true, EINVAL, "map area overflows uint32");
    tabos_tilemap_t map    = {0};
    tabos_sprite_set_t set = {0};
    expect(tabos_tilemap_load(NULL, &map) == -1 && errno == EINVAL, "null map path");
    expect(tabos_sprite_set_load(NULL, &set) == -1 && errno == EINVAL, "null sprite path");
    expect(tabos_tilemap_load(path, NULL) == -1 && errno == EINVAL, "null map output");
    expect(tabos_sprite_set_load(path, NULL) == -1 && errno == EINVAL, "null sprite output");
    tabos_tilemap_unload(NULL);
    tabos_sprite_set_unload(NULL);
    expect(remove(path) == 0, "temporary fixture removed");
    check_load(path, false, ENOENT, "missing sprite file");
    check_load(path, true, ENOENT, "missing map file");
    printf("asset loaders: %u cases, %u failures\n", cases, failures);
    return failures == 0U ? 0 : 1;
}
