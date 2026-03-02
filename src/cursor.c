#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_scene.h> 
#include <wlr/types/wlr_xdg_shell.h> 

#include "src/output.h"
#include "src/compositor.h"
#include "src/cursor.h"
#include "src/edges.h"



static void process_cursor_move(struct compositor_state *server) {
	/* Move the grabbed toplevel to the new position. */
	struct cursor_toplevel *toplevel = server->grabbed_toplevel;
	wlr_scene_node_set_position(&toplevel->scene_tree->node,
		server->cursor->x - server->grab_x,
		server->cursor->y - server->grab_y);
}

static void process_cursor_resize(struct compositor_state *server) {
	/* Resizing the grabbed toplevel can be a little bit complicated... */
	struct cursor_toplevel *toplevel = server->grabbed_toplevel;
	double border_x = server->cursor->x - server->grab_x;
	double border_y = server->cursor->y - server->grab_y;
	int new_left = server->grab_geobox.x;
	int new_right = server->grab_geobox.x + server->grab_geobox.width;
	int new_top = server->grab_geobox.y;
	int new_bottom = server->grab_geobox.y + server->grab_geobox.height;

	if (server->resize_edges & WLR_EDGE_TOP) {
		new_top = border_y;
		if (new_top >= new_bottom) new_top = new_bottom - 1;
	} else if (server->resize_edges & WLR_EDGE_BOTTOM) {
		new_bottom = border_y;
		if (new_bottom <= new_top) new_bottom = new_top + 1;
	}
	if (server->resize_edges & WLR_EDGE_LEFT) {
		new_left = border_x;
		if (new_left >= new_right) new_left = new_right - 1;
	} else if (server->resize_edges & WLR_EDGE_RIGHT) {
		new_right = border_x;
		if (new_right <= new_left) new_right = new_left + 1;
	}

	struct wlr_box geo_box;
	wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geo_box);
	wlr_scene_node_set_position(&toplevel->scene_tree->node,
		new_left - geo_box.x, new_top - geo_box.y);

	int new_width = new_right - new_left;
	int new_height = new_bottom - new_top;
	wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, new_width, new_height);
}

static void reset_cursor_mode(struct compositor_state *server) {
	/* Reset the cursor mode to passthrough. */
	server->cursor_mode = CURSOR_PASSTHROUGH;
	server->grabbed_toplevel = NULL;
}

static void process_cursor_motion(struct compositor_state *server, uint32_t time) {
	(void)time;  /* не используется пока */
	if (server->cursor_mode == CURSOR_MOVE && server->grabbed_toplevel) {
		process_cursor_move(server);
	} else if (server->cursor_mode == CURSOR_RESIZE && server->grabbed_toplevel) {
		process_cursor_resize(server);
	}
	/* TODO: иначе обновлять surface под курсором */
}

/* Заглушки для недостающих функций (потом реализуете) */
struct cursor_toplevel *desktop_toplevel_at(struct compositor_state *server, 
    double x, double y, struct wlr_surface **surface, double *sx, double *sy) {
	(void)server; (void)x; (void)y; (void)surface; (void)sx; (void)sy;
	return NULL;  /* TODO: найти окно под курсором */
}

void focus_toplevel(struct cursor_toplevel *toplevel) {
	(void)toplevel;
	/* TODO: установить фокус на окно */
}

/* Публичные функции (вызываются из main.c) */

void server_cursor_frame(struct wl_listener *listener, void *data) {
	(void)data;
	struct compositor_state *server = wl_container_of(listener, server, cursor_frame);
	wlr_seat_pointer_notify_frame(server->seat);
}

void server_cursor_axis(struct wl_listener *listener, void *data) {
	struct compositor_state *server = wl_container_of(listener, server, cursor_axis);
	struct wlr_pointer_axis_event *event = data;
	wlr_seat_pointer_notify_axis(server->seat,
		event->time_msec, event->orientation, event->delta,
		event->delta_discrete, event->source, event->relative_direction);
}

void server_cursor_button(struct wl_listener *listener, void *data) {
	struct compositor_state *server = wl_container_of(listener, server, cursor_button);
	struct wlr_pointer_button_event *event = data;
	
	wlr_seat_pointer_notify_button(server->seat,
		event->time_msec, event->button, event->state);
		
	if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
		reset_cursor_mode(server);
	} else {
		double sx, sy;
		struct wlr_surface *surface = NULL;
		struct cursor_toplevel *toplevel = desktop_toplevel_at(server,
			server->cursor->x, server->cursor->y, &surface, &sx, &sy);
		focus_toplevel(toplevel);
	}
}

void server_cursor_motion(struct wl_listener *listener, void *data) {
	struct compositor_state *server = wl_container_of(listener, server, cursor_motion);
	struct wlr_pointer_motion_event *event = data;
	
	wlr_cursor_move(server->cursor, &event->pointer->base,
		event->delta_x, event->delta_y);
	process_cursor_motion(server, event->time_msec);
}

void server_cursor_motion_absolute(struct wl_listener *listener, void *data) {
	struct compositor_state *server = wl_container_of(listener, server, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *event = data;
	
	wlr_cursor_warp_absolute(server->cursor, &event->pointer->base, event->x, event->y);
	process_cursor_motion(server, event->time_msec);
}


void server_new_pointer(struct compositor_state *server, struct wlr_input_device *device) {
	wlr_cursor_attach_input_device(server->cursor, device);
}