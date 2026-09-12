#include <desktop/model.h>
#include <assert.h>
#include <stdlib.h>

static int read_color(void* user, tabos_surface_t surface, uint32_t width, uint32_t height, uint16_t* pixels)
{
    (void) user;
    for (size_t index = 0U; index < (size_t) width * height; ++index) {
        pixels[index] = (uint16_t) surface;
    }
    return 0;
}

static void render_test(void)
{
    desktop_model_t model;
    desktop_model_init(&model);
    assert(desktop_model_add(&model, 1, "Bottom") == 0);
    assert(desktop_model_adopt(&model, 0U, 0U, 10, 1280U, 592U));
    assert(desktop_model_add(&model, 2, "Top") == 1);
    assert(desktop_model_configure(&model, 1U, (tabos_gui_rect_t) {100, 80, 800, 480}, false));
    assert(desktop_model_adopt(&model, 1U, 1U, 20, 800U, 432U));
    uint16_t* pixels  = calloc(1280U * 720U + 2U, sizeof(uint16_t));
    uint16_t* scratch = calloc(1280U * 592U, sizeof(uint16_t));
    assert(pixels != NULL && scratch != NULL);
    pixels[0] = pixels[1280U * 720U + 1U] = 0xdeadU;
    tabos_gui_canvas_t canvas             = {.pixels = pixels + 1, .width = 1280U, .height = 720U};
    assert(desktop_render(&model, &canvas, scratch, 1280U * 592U, read_color, NULL));
    assert(canvas.pixels[200U * 1280U + 50U] == 10U && canvas.pixels[200U * 1280U + 200U] == 20U);
    assert(canvas.pixels[680U * 1280U + 100U] == TABOS_GUI_FACE);
    model.damaged            = false;
    model.windows[1].surface = 30;
    desktop_model_damage(&model, (tabos_gui_rect_t) {200, 200, 8, 8});
    assert(desktop_render(&model, &canvas, scratch, 1280U * 592U, read_color, NULL));
    assert(canvas.pixels[200U * 1280U + 200U] == 30U && canvas.pixels[200U * 1280U + 208U] == 20U);
    desktop_model_minimize(&model, 1U);
    assert(desktop_render(&model, &canvas, scratch, 1280U * 592U, read_color, NULL));
    assert(canvas.pixels[200U * 1280U + 200U] == 10U);
    assert(pixels[0] == 0xdeadU && pixels[1280U * 720U + 1U] == 0xdeadU);
    free(scratch);
    free(pixels);
}

int main(void)
{
    render_test();
    desktop_model_t model;
    desktop_model_init(&model);
    const int first  = desktop_model_add(&model, 2, "First");
    const int second = desktop_model_add(&model, 3, "Second");
    assert(first == 0 && second == 1 && model.focus == second);
    assert(model.windows[first].bounds.width == 1280 && model.windows[first].bounds.height == 640);
    assert(desktop_model_hit(&model, 10, 639) == second && desktop_model_hit(&model, 10, 640) == -1);
    assert(desktop_model_adopt(&model, 0U, 0U, 10, 1280U, 592U));
    assert(desktop_model_adopt(&model, 1U, 0U, 11, 1280U, 592U));
    desktop_model_minimize(&model, 1U);
    assert(model.focus == first && desktop_model_hit(&model, 10, 10) == first);
    desktop_model_focus(&model, 1U);
    assert(!model.windows[1].minimized && model.focus == second);
    assert(desktop_model_configure(&model, 1U, (tabos_gui_rect_t) {100, 50, 800, 500}, false));
    assert(model.windows[1].bounds.width == 1280 && model.windows[1].surface == 11);
    assert(!desktop_model_adopt(&model, 1U, 1U, 12, 799U, 452U));
    assert(!desktop_model_adopt(&model, 1U, 2U, 12, 800U, 452U));
    desktop_model_resize_failed(&model, 1U, 1U);
    assert(!model.windows[1].pending && model.windows[1].bounds.width == 1280);
    assert(desktop_model_configure(&model, 1U, (tabos_gui_rect_t) {100, 50, 800, 500}, false));
    assert(desktop_model_adopt(&model, 1U, 2U, 12, 800U, 452U));
    assert(!model.windows[1].maximized && model.windows[1].bounds.x == 100);
    assert(desktop_model_hit(&model, 99, 50) == first && desktop_model_hit(&model, 100, 50) == second);
    desktop_model_drag_begin(&model, 1U, 850, 520, true);
    desktop_model_drag_move(&model, 950, 580);
    assert(model.outline.width == 900 && model.outline.height == 560 && model.windows[1].bounds.width == 800);
    assert(desktop_model_drag_end(&model, false));
    assert(model.windows[1].pending && model.windows[1].bounds.width == 800);
    desktop_model_resize_failed(&model, 1U, model.windows[1].requested_serial);
    assert(model.windows[1].bounds.width == 800 && model.windows[1].surface == 12);
    desktop_model_drag_begin(&model, 1U, 110, 60, false);
    desktop_model_drag_move(&model, -100, -100);
    assert(model.windows[1].bounds.x == 0 && model.windows[1].bounds.y == 0);
    assert(!desktop_model_drag_end(&model, true));
    assert(model.windows[1].bounds.x == 100 && model.windows[1].bounds.y == 50);
    desktop_model_remove(&model, 1U);
    assert(model.focus == first && model.count == 1U);
    assert(desktop_model_add(&model, 4, "Reused") == 1);
    assert(model.windows[1].surface == -1 && model.windows[1].serial == 0U);
    assert(model.damage.x == 0 && model.damage.y == 0 && model.damage.width == 1280 && model.damage.height == 720);
    return 0;
}
