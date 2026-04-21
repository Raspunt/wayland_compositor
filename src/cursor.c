#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_scene.h> 
#include <wlr/types/wlr_xdg_shell.h> 
#include <wlr/util/log.h>
#include <wlr/util/edges.h>

#include "src/output.h"
#include "src/compositor.h"
#include "src/cursor.h"

void focus_toplevel(struct compositor_toplevel *toplevel);

static void process_cursor_move(struct compositor_state *server) {
	struct compositor_toplevel *toplevel = server->grabbed_toplevel;
	if (!toplevel) return;
	
	wlr_scene_node_set_position(&toplevel->scene_tree->node,
		server->cursor->x - server->grab_x,
		server->cursor->y - server->grab_y);
}

static void process_cursor_resize(struct compositor_state *server) {
	struct compositor_toplevel *toplevel = server->grabbed_toplevel;
	if (!toplevel) return;
	
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

void reset_cursor_mode(struct compositor_state *server) {
	server->cursor_mode = CURSOR_PASSTHROUGH;
	server->grabbed_toplevel = NULL;
}

void begin_interactive(struct compositor_toplevel *toplevel, 
                       enum cursor_mode mode, uint32_t edges) {
	struct compositor_state *server = toplevel->server;
	server->grabbed_toplevel = toplevel;
	server->cursor_mode = mode;
	server->resize_edges = edges;
	
	if (mode == CURSOR_MOVE) {
		server->grab_x = server->cursor->x - toplevel->scene_tree->node.x;
		server->grab_y = server->cursor->y - toplevel->scene_tree->node.y;
	} else {
		struct wlr_box geo_box;
		wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geo_box);
		
		double border_x = (toplevel->scene_tree->node.x + geo_box.x) +
			((edges & WLR_EDGE_RIGHT) ? geo_box.width : 0);
		double border_y = (toplevel->scene_tree->node.y + geo_box.y) +
			((edges & WLR_EDGE_BOTTOM) ? geo_box.height : 0);
			
		server->grab_x = server->cursor->x - border_x;
		server->grab_y = server->cursor->y - border_y;
		server->grab_geobox = geo_box;
		server->grab_geobox.x += toplevel->scene_tree->node.x;
		server->grab_geobox.y += toplevel->scene_tree->node.y;
	}
}

/* ИСПРАВЛЕНО: Теперь реально ищет окно под курсором */
struct compositor_toplevel *desktop_toplevel_at(struct compositor_state *server, 
    double lx, double ly, struct wlr_surface **surface, double *sx, double *sy) {
	
	/* Ищем узел сцены под курсором */
	struct wlr_scene_node *node = wlr_scene_node_at(
		&server->scene->tree.node, lx, ly, sx, sy);
	
	if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER) {
		return NULL;
	}
	
	struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
	struct wlr_scene_surface *scene_surface = 
		wlr_scene_surface_try_from_buffer(scene_buffer);
	
	if (!scene_surface) {
		return NULL;
	}
	
	*surface = scene_surface->surface;
	
	/* Ищем родительский scene_tree, у которого есть data = toplevel */
	struct wlr_scene_tree *tree = node->parent;
	while (tree != NULL && tree->node.data == NULL) {
		tree = tree->node.parent;
	}
	
	return tree ? tree->node.data : NULL;
}

static void process_cursor_motion(struct compositor_state *server, uint32_t time) {
	/* Обработка drag/resize */
	if (server->cursor_mode == CURSOR_MOVE && server->grabbed_toplevel) {
		process_cursor_move(server);
		return;
	} else if (server->cursor_mode == CURSOR_RESIZE && server->grabbed_toplevel) {
		process_cursor_resize(server);
		return;
	}
	
	/* Passthrough: отправляем движение клиенту под курсором */
	double sx, sy;
	struct wlr_surface *surface = NULL;
	desktop_toplevel_at(server,
		server->cursor->x, server->cursor->y, &surface, &sx, &sy);
		
	if (surface) {
		wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
		wlr_seat_pointer_notify_motion(server->seat, time, sx, sy);
	} else {
		wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");
		wlr_seat_pointer_clear_focus(server->seat);
	}
}

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
		/* При нажатии ищем окно и фокусируем его */
		double sx, sy;
		struct wlr_surface *surface = NULL;
		struct compositor_toplevel *toplevel = desktop_toplevel_at(server,
			server->cursor->x, server->cursor->y, &surface, &sx, &sy);
			
		if (toplevel) {
			focus_toplevel(toplevel);
		}
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

void seat_request_set_cursor(struct wl_listener *listener, void *data) {
	struct compositor_state *server =
		wl_container_of(listener, server, request_set_cursor);
	struct wlr_seat_pointer_request_set_cursor_event *event = data;

	struct wlr_seat_client *focused = server->seat->pointer_state.focused_client;
	if (focused != event->seat_client) {
		return;
	}

	wlr_cursor_set_surface(server->cursor, event->surface,
		event->hotspot_x, event->hotspot_y);
}