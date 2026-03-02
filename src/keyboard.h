#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <wayland-server-core.h> 

struct compositor_state;
struct wlr_keyboard;
struct wlr_input_device;

struct compositor_keyboard {
	struct wl_list link;
	struct compositor_state *server;  
	struct wlr_keyboard *wlr_keyboard;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
};

void server_new_keyboard(struct compositor_state *server, struct wlr_input_device *device);

#endif