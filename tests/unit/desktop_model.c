#include <desktop/model.h>
#include <assert.h>

int main(void)
{
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
