#ifndef CURSOR_H
#define CURSOR_H

enum cursor_mode {
	CURSOR_PASSTHROUGH,
	CURSOR_MOVE,
	CURSOR_RESIZE,
};


struct compositor_state;
struct wlr_input_device;

void server_cursor_motion_absolute(struct wl_listener *listener, void *data);
void server_cursor_motion(struct wl_listener *listener, void *data);
void server_cursor_button(struct wl_listener *listener, void *data);
void server_cursor_axis(struct wl_listener *listener, void *data);
void server_cursor_frame(struct wl_listener *listener, void *data);
void server_new_pointer(struct compositor_state *server,struct wlr_input_device *device);
void seat_request_set_cursor(struct wl_listener *listener, void *data);

#endif