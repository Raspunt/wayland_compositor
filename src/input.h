#ifndef INPUT_H
#define INPUT_H

#include <wayland-server-core.h>


void server_new_input(struct wl_listener *listener, void *data);

#endif