#include <tabos/gui.h>
#include <tabos/internal/elf_api.h>
#include <tabos/internal/ipc.h>
#include <tabos/internal/surface.h>
#include <tabos/session.h>

#include <assert.h>
#include <errno.h>
#include <string.h>

const tabos_elf_api_t* tabos_runtime_api;
static uint32_t owner = 1U;
static bool fail_upload;

static int ipc_gate(uint32_t operation, ipc_transport_packet_t* packet)
{
    if (operation == IPC_TRANSPORT_WAIT_SOURCE) {
        return packet->channel;
    }
    return ipc_service_request(owner, 1U, operation, packet);
}

static int surface_gate(uint32_t operation, surface_transport_packet_t* packet, void* pixels)
{
    if (fail_upload && operation == SURFACE_TRANSPORT_UPLOAD) {
        return -ENOMEM;
    }
    return surface_service_request(owner, operation, packet, pixels, (size_t) packet->width * packet->height * 2U);
}

static int control_gate(uint32_t operation, uint32_t token, uint32_t pid)
{
    (void) token;
    (void) pid;
    assert(operation == TABOS_SESSION_CHECKPOINT);
    return 0;
}

static void yield_gate(void)
{
}
static int wait_gate(tabos_elf_wait_item_t* items, uint32_t count, uint32_t timeout)
{
    (void) items;
    (void) count;
    (void) timeout;
    return 0;
}

static void draw(tabos_gui_t* gui)
{
    tabos_gui_fill(&gui->canvas, (tabos_gui_rect_t) {0, 0, (int32_t) gui->canvas.width, (int32_t) gui->canvas.height},
                   0x1234U);
}
static int closing(tabos_gui_t* gui)
{
    (void) gui;
    return 0;
}

int main(void)
{
    assert(ipc_service_init() && surface_service_init());
    const tabos_elf_api_t api          = {.ipc             = ipc_gate,
                                          .surface         = surface_gate,
                                          .session_control = control_gate,
                                          .yield           = yield_gate,
                                          .wait            = wait_gate};
    tabos_runtime_api                  = &api;
    const tabos_ipc_channel_t listener = tabos_ipc_listen();
    assert(listener > 0);
    owner           = 2U;
    tabos_gui_t gui = {.draw = draw, .closing = closing};
    assert(tabos_gui_open(&gui, "Test") == 0 && tabos_gui_step(&gui, 0U) == 1);
    const tabos_surface_t original    = gui.surface;
    owner                             = 1U;
    const tabos_ipc_channel_t channel = tabos_ipc_accept(listener);
    uint32_t kind, sender;
    tabos_gui_packet_t packet;
    assert(tabos_gui_receive(channel, &kind, &packet, &sender) == 0 && kind == TABOS_GUI_HELLO && sender == 2U);
    assert(packet.data.window.width == 1280U && packet.data.window.height == 592U);
    uint16_t pixel = 0;
    assert(tabos_surface_read(original, 0U, 0U, 1U, 1U, &pixel) == 0 && pixel == 0x1234U);
    assert(tabos_gui_send(channel, TABOS_GUI_ADOPT, &packet, true) == 0);
    packet.serial             = 1U;
    packet.data.window.width  = 640U;
    packet.data.window.height = 320U;
    assert(tabos_gui_send(channel, TABOS_GUI_CONFIGURE, &packet, true) == 0);
    owner = 2U;
    assert(tabos_gui_step(&gui, 0U) == 1 && gui.canvas.width == 640U && gui.serial == 1U && gui.retired == original);
    owner = 1U;
    assert(tabos_surface_read(original, 0U, 0U, 1U, 1U, &pixel) == 0);
    assert(tabos_gui_receive(channel, &kind, &packet, &sender) == 0 && kind == TABOS_GUI_FRAME && packet.serial == 1U);
    assert(tabos_gui_send(channel, TABOS_GUI_ADOPT, &packet, true) == 0);
    owner = 2U;
    assert(tabos_gui_step(&gui, 0U) == 1 && gui.retired == -1);
    owner = 1U;
    assert(tabos_surface_read(original, 0U, 0U, 1U, 1U, &pixel) == -1 && errno == EBADF);
    packet.serial             = 2U;
    packet.data.window.width  = 800U;
    packet.data.window.height = 400U;
    assert(tabos_gui_send(channel, TABOS_GUI_CONFIGURE, &packet, true) == 0);
    owner                          = 2U;
    fail_upload                    = true;
    const tabos_surface_t retained = gui.surface;
    assert(tabos_gui_step(&gui, 0U) == 1 && gui.canvas.width == 640U && gui.serial == 1U && gui.surface == retained);
    fail_upload = false;
    owner       = 1U;
    assert(tabos_gui_receive(channel, &kind, &packet, &sender) == 0 && kind == TABOS_GUI_ERROR && packet.serial == 2U);
    assert(tabos_surface_read(retained, 0U, 0U, 1U, 1U, &pixel) == 0 && pixel == 0x1234U);
    assert(tabos_gui_send(channel, TABOS_GUI_CLOSE, &packet, true) == 0);
    owner = 2U;
    assert(tabos_gui_step(&gui, 0U) == 1 && gui.close_pending);
    assert(tabos_gui_send(gui.channel, TABOS_GUI_WAKE, &packet, true) == 0);
    assert(tabos_gui_send(gui.channel, TABOS_GUI_WAKE, &packet, true) == 0);
    tabos_gui_close_reply(&gui, false);
    assert(gui.running && !gui.close_pending && gui.cancel_reply_pending);
    owner = 1U;
    assert(tabos_gui_receive(channel, &kind, &packet, &sender) == 0 && kind == TABOS_GUI_WAKE);
    assert(tabos_gui_receive(channel, &kind, &packet, &sender) == 0 && kind == TABOS_GUI_WAKE);
    owner = 2U;
    assert(tabos_gui_step(&gui, 0U) == 1 && !gui.cancel_reply_pending);
    owner = 1U;
    assert(tabos_gui_receive(channel, &kind, &packet, &sender) == 0 && kind == TABOS_GUI_CLOSE_CANCELLED);
    assert(tabos_gui_send(channel, TABOS_GUI_CLOSE, &packet, true) == 0);
    owner = 2U;
    assert(tabos_gui_step(&gui, 0U) == 1);
    tabos_gui_close_reply(&gui, true);
    assert(tabos_gui_step(&gui, 0U) == 0);
    tabos_gui_shutdown(&gui);
    tabos_surface_stats_t stats;
    assert(tabos_surface_stats(&stats) == 0 && stats.used_bytes == 0U);
    owner = 1U;
    assert(tabos_ipc_close(channel) == 0 && tabos_ipc_close(listener) == 0);
    surface_service_shutdown();
    ipc_service_shutdown();
    return 0;
}
