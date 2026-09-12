#include <tabos/gui_draw.h>
#include <tabos/gui_protocol.h>

#include <assert.h>
#include <limits.h>
#include <string.h>

int main(void)
{
    uint16_t guarded[64U * 64U + 2U] = {0};
    guarded[0] = guarded[64U * 64U + 1U] = 0x1234U;
    tabos_gui_canvas_t canvas            = {.pixels = guarded + 1U, .width = 64U, .height = 64U};
    tabos_gui_fill(&canvas, (tabos_gui_rect_t) {-20, -20, 30, 30}, 0xabcdU);
    assert(canvas.pixels[0] == 0xabcdU && canvas.pixels[9U * 64U + 9U] == 0xabcdU);
    assert(canvas.pixels[10] == 0U && guarded[0] == 0x1234U && guarded[64U * 64U + 1U] == 0x1234U);
    tabos_gui_rect_t clipped;
    assert(!tabos_gui_intersect((tabos_gui_rect_t) {INT_MAX, 0, INT_MAX, 10}, (tabos_gui_rect_t) {0, 0, 64, 64},
                                &clipped));
    tabos_gui_fill(&canvas, (tabos_gui_rect_t) {0, 0, 64, 64}, 0U);
    tabos_gui_text(&canvas, -4, -4, "\xdb", 0xffffU, 2U);
    bool drawn = false;
    for (size_t index = 0U; index < 64U * 64U; ++index) {
        drawn = drawn || canvas.pixels[index] != 0U;
    }
    assert(drawn && guarded[0] == 0x1234U && guarded[64U * 64U + 1U] == 0x1234U);

    tabos_gui_ui_t ui               = {0};
    char text[8]                    = "";
    const tabos_gui_widget_t button = {
        .id = 1, .kind = TABOS_GUI_BUTTON, .bounds = {0, 0, 100, 44},
                .label = "Button"
    };
    const tabos_gui_widget_t check = {
        .id = 2, .kind = TABOS_GUI_CHECKBOX, .bounds = {110, 0, 100, 44},
                .label = "Check"
    };
    const tabos_gui_widget_t field = {
        .id = 3, .kind = TABOS_GUI_TEXT_FIELD, .bounds = {0, 50, 200, 44},
                .text = text, .capacity = sizeof(text)
    };
    assert(tabos_gui_ui_add(&ui, button) && tabos_gui_ui_add(&ui, check) && tabos_gui_ui_add(&ui, field));
    assert(!tabos_gui_ui_add(&ui, button));
    tabos_pointer_event_t pointer = {.type = TABOS_POINTER_DOWN, .x = 10, .y = 10, .contact_id = 1U, .device_id = 2U};
    assert(tabos_gui_ui_pointer(&ui, &pointer) == 0 && ui.focus == 1 && ui.pressed == 1);
    pointer.contact_id = 2U;
    pointer.type       = TABOS_POINTER_UP;
    assert(tabos_gui_ui_pointer(&ui, &pointer) == 0 && ui.pressed == 1);
    pointer.contact_id = 1U;
    pointer.x          = 500;
    assert(tabos_gui_ui_pointer(&ui, &pointer) == 0 && ui.pressed == 0);
    pointer.type = TABOS_POINTER_DOWN;
    pointer.x    = 10;
    (void) tabos_gui_ui_pointer(&ui, &pointer);
    pointer.type = TABOS_POINTER_CANCEL;
    assert(tabos_gui_ui_pointer(&ui, &pointer) == 0 && ui.pressed == 0);
    pointer.type = TABOS_POINTER_UP;
    assert(tabos_gui_ui_pointer(&ui, &pointer) == 0);
    ui.focus                = 0;
    tabos_input_event_t key = {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_TAB};
    assert(tabos_gui_ui_keyboard(&ui, &key) == 0 && ui.focus == 1);
    (void) tabos_gui_ui_keyboard(&ui, &key);
    assert(ui.focus == 2);
    key.key = TABOS_KEY_SPACE;
    assert(tabos_gui_ui_keyboard(&ui, &key) == 2 && ui.widgets[1].value == 1);
    key.repeat = true;
    assert(tabos_gui_ui_keyboard(&ui, &key) == 0 && ui.widgets[1].value == 1);
    key = (tabos_input_event_t) {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_TAB};
    (void) tabos_gui_ui_keyboard(&ui, &key);
    assert(ui.focus == 3);
    key.key = TABOS_KEY_A;
    assert(tabos_gui_ui_keyboard(&ui, &key) == 0 && text[0] == '\0');
    key = (tabos_input_event_t) {.type = TABOS_INPUT_TEXT, .text = "a"};
    assert(tabos_gui_ui_keyboard(&ui, &key) == 3 && strcmp(text, "a") == 0);
    key.type = TABOS_INPUT_KEY_UP;
    assert(tabos_gui_ui_keyboard(&ui, &key) == 0 && !ui.changed);
    tabos_gui_ui_begin(&ui);
    assert(tabos_gui_ui_add(&ui, field) && ui.widgets[0].cursor == 1U);
    key = (tabos_input_event_t) {.type = TABOS_INPUT_TEXT, .text = "bcdefg"};
    assert(tabos_gui_ui_keyboard(&ui, &key) == 3 && strcmp(text, "abcdefg") == 0);
    key.text[0] = 'h';
    key.text[1] = '\0';
    assert(tabos_gui_ui_keyboard(&ui, &key) == 0 && strcmp(text, "abcdefg") == 0);
    key = (tabos_input_event_t) {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_BACKSPACE};
    assert(tabos_gui_ui_keyboard(&ui, &key) == 3 && strcmp(text, "abcdef") == 0);
    key.key = TABOS_KEY_LEFT;
    assert(tabos_gui_ui_keyboard(&ui, &key) == 0 && ui.changed);
    key = (tabos_input_event_t) {.type = TABOS_INPUT_TEXT, .text = "\r\n\t"};
    assert(tabos_gui_ui_keyboard(&ui, &key) == 0 && strcmp(text, "abcdef") == 0);
    tabos_gui_ui_draw(&ui, &canvas);
    assert(guarded[0] == 0x1234U && guarded[64U * 64U + 1U] == 0x1234U);
    return 0;
}
