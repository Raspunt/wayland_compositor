#ifndef CURSOR_H
#define CURSOR_H

enum cursor_mode {
	CURSOR_PASSTHROUGH,
	CURSOR_MOVE,
	CURSOR_RESIZE,
};


struct compositor_state;
struct wlr_input_device;

struct cursor_toplevel {
	struct wl_list link;
	struct compositor_state *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener commit;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
};


void server_cursor_motion_absolute(struct wl_listener *listener, void *data);
void server_cursor_motion(struct wl_listener *listener, void *data);
void server_cursor_button(struct wl_listener *listener, void *data);
void server_cursor_axis(struct wl_listener *listener, void *data);
void server_cursor_frame(struct wl_listener *listener, void *data);
void server_new_pointer(struct compositor_state *server,struct wlr_input_device *device);
void seat_request_set_cursor(struct wl_listener *listener, void *data);

#endif