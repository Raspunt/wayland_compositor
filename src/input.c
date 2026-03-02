#include <wlr/types/wlr_input_device.h>

#include "src/cursor.h"
#include "src/keyboard.h"
#include "compositor.h"


void server_new_input(struct wl_listener *listener, void *data) {
    struct compositor_state *server = wl_container_of(listener, server, new_input);
    struct wlr_input_device *device = data;

    switch (device->type) {
    case WLR_INPUT_DEVICE_POINTER:
        server_new_pointer(server, device);
        break;
    case WLR_INPUT_DEVICE_KEYBOARD:
        server_new_keyboard(server, device);
        break;
    default:
        break;
    }
}