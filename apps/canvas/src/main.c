#include <tabos/gui.h>
#include <stdio.h>

typedef struct {
        uint16_t x, y, color;
        bool start;
} sketch_point_t;
typedef struct {
        sketch_point_t points[1024];
        size_t count;
        unsigned int ink;
        bool grid;
        bool drawing;
        uint32_t contact;
        tabos_device_id_t device;
} sketch_t;

static const uint16_t inks[] = {0x1082U, 0x2354U, 0xe986U, 0x34e8U};

static void line(tabos_gui_canvas_t* canvas, int x0, int y0, int x1, int y1, uint16_t color)
{
    const int dx = x1 >= x0 ? x1 - x0 : x0 - x1;
    const int dy = y1 >= y0 ? y0 - y1 : y1 - y0;
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    int error    = dx + dy;
    for (;;) {
        tabos_gui_fill(canvas, (tabos_gui_rect_t) {x0 - 2, y0 - 2, 5, 5}, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int twice = 2 * error;
        if (twice >= dy) {
            error += dy;
            x0    += sx;
        }
        if (twice <= dx) {
            error += dx;
            y0    += sy;
        }
    }
}

static void draw(tabos_gui_t* gui)
{
    const sketch_t* sketch = gui->user;
    (void) tabos_gui_ui_add(&gui->ui,
                            (tabos_gui_widget_t) {
                                .id = 1, .kind = TABOS_GUI_BUTTON, .bounds = {12, 12, 96, 48},
                                        .label = "Clear"
    });
    (void) tabos_gui_ui_add(&gui->ui,
                            (tabos_gui_widget_t) {
                                .id = 2, .kind = TABOS_GUI_BUTTON, .bounds = {120, 12, 80, 48},
                                        .label = "Ink"
    });
    (void) tabos_gui_ui_add(
        &gui->ui,
        (tabos_gui_widget_t) {
            .id = 3, .kind = TABOS_GUI_CHECKBOX, .bounds = {212, 12, 108, 48},
                    .label = "Grid", .value = sketch->grid
    });
    if (gui->canvas.width >= 540U) {
        (void) tabos_gui_ui_add(
            &gui->ui, (tabos_gui_widget_t) {
                          .id = 4, .kind = TABOS_GUI_BUTTON, .bounds = {332, 12, 192, 48},
                                  .label = "Run hello"
        });
    }
    const int width  = (int) gui->canvas.width - 32;
    const int height = (int) gui->canvas.height - 88;
    tabos_gui_fill(&gui->canvas, (tabos_gui_rect_t) {16, 72, width, height}, 0xffffU);
    if (sketch->grid) {
        for (int x = 16; x < width + 16; x += 32) {
            tabos_gui_fill(&gui->canvas, (tabos_gui_rect_t) {x, 72, 1, height}, 0xe71cU);
        }
        for (int y = 72; y < height + 72; y += 32) {
            tabos_gui_fill(&gui->canvas, (tabos_gui_rect_t) {16, y, width, 1}, 0xe71cU);
        }
    }
    int previous_x = 0, previous_y = 0;
    for (size_t index = 0U; index < sketch->count; ++index) {
        const sketch_point_t point = sketch->points[index];
        const int x                = 16 + (int) ((uint32_t) point.x * (uint32_t) (width - 1) / 65535U);
        const int y                = 72 + (int) ((uint32_t) point.y * (uint32_t) (height - 1) / 65535U);
        line(&gui->canvas, point.start ? x : previous_x, point.start ? y : previous_y, x, y, point.color);
        previous_x = x;
        previous_y = y;
    }
}

static void action(tabos_gui_t* gui, int id)
{
    sketch_t* sketch = gui->user;
    if (id == 1) {
        sketch->count   = 0U;
        sketch->drawing = false;
    } else if (id == 2) {
        sketch->ink = (sketch->ink + 1U) % 4U;
    } else if (id == 3) {
        sketch->grid = !sketch->grid;
    } else if (id == 4) {
        (void) tabos_gui_launch(gui, "T:/bin/hello");
    }
    tabos_gui_invalidate(gui);
}

static void input(tabos_gui_t* gui, const tabos_gui_packet_t* packet, uint32_t kind)
{
    sketch_t* sketch = gui->user;
    if (kind == TABOS_GUI_CANCEL_INPUT) {
        sketch->drawing = false;
        return;
    }
    if (kind != TABOS_GUI_POINTER) {
        return;
    }
    const tabos_pointer_event_t* event = &packet->data.pointer;
    const tabos_gui_rect_t paper       = {16, 72, (int32_t) gui->canvas.width - 32, (int32_t) gui->canvas.height - 88};
    if (event->type == TABOS_POINTER_DOWN && tabos_gui_contains(paper, event->x, event->y) && !sketch->drawing) {
        sketch->drawing = true;
        sketch->contact = event->contact_id;
        sketch->device  = event->device_id;
    }
    if (!sketch->drawing || sketch->contact != event->contact_id || sketch->device != event->device_id) {
        return;
    }
    if (event->type == TABOS_POINTER_UP || event->type == TABOS_POINTER_CANCEL) {
        sketch->drawing = false;
        return;
    }
    if (!tabos_gui_contains(paper, event->x, event->y) || sketch->count >= 1024U) {
        return;
    }
    sketch->points[sketch->count++] =
        (sketch_point_t) {.x = (uint16_t) ((uint32_t) (event->x - paper.x) * 65535U / (uint32_t) (paper.width - 1)),
                          .y = (uint16_t) ((uint32_t) (event->y - paper.y) * 65535U / (uint32_t) (paper.height - 1)),
                          .color = inks[sketch->ink],
                          .start = event->type == TABOS_POINTER_DOWN};
    tabos_gui_invalidate(gui);
}

static bool pause_sketch(tabos_gui_t* gui)
{
    ((sketch_t*) gui->user)->drawing = false;
    return true;
}

int main(void)
{
    sketch_t sketch = {0};
    tabos_gui_t gui = {.user = &sketch, .draw = draw, .action = action, .input = input, .pause = pause_sketch};
    if (tabos_gui_open(&gui, "Canvas") != 0) {
        fprintf(stderr, "canvas: start from the desktop\n");
        return 1;
    }
    int result;
    do {
        result = tabos_gui_step(&gui, 100U);
    } while (result > 0);
    tabos_gui_shutdown(&gui);
    return result < 0 ? 1 : 0;
}
