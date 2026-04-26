#define _POSIX_C_SOURCE 200809L
#include "src/compositor.h"

extern char **environ;
#include "src/keyboard.h"
#include "src/cursor.h"
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>
#include <xkbcommon/xkbcommon.h>
#include <stdlib.h>
#include <unistd.h>
#include <spawn.h>
#include <stdio.h>

typedef void (*binding_handler_t)(struct compositor_state *server, struct keybinding *kb);

static void handle_exec(struct compositor_state *server, struct keybinding *kb) {
	(void)server;
	if (!kb->arg) return;
	pid_t pid;
	char *argv[] = {"sh", "-c", kb->arg, NULL};
	posix_spawnp(&pid, "sh", NULL, NULL, argv, environ);
}

static void handle_exit(struct compositor_state *server, struct keybinding *kb) {
	(void)kb;
	wl_display_terminate(server->wl_display);
}

static void handle_workspace(struct compositor_state *server, struct keybinding *kb) {
	switch_workspace(server, kb->arg ? atoi(kb->arg) : 1);
}

static void handle_movetoworkspace(struct compositor_state *server, struct keybinding *kb) {
	struct compositor_toplevel *t = get_focused_toplevel(server);
	if (t) move_toplevel_to_workspace(t, kb->arg ? atoi(kb->arg) : 1);
}

static void handle_focus_prev(struct compositor_state *server, struct keybinding *kb) {
	(void)kb;
	focus_prev(server);
}

static void handle_focus_next(struct compositor_state *server, struct keybinding *kb) {
	(void)kb;
	focus_next(server);
}

static void handle_move_prev(struct compositor_state *server, struct keybinding *kb) {
	(void)kb;
	move_toplevel_prev(server);
}

static void handle_move_next(struct compositor_state *server, struct keybinding *kb) {
	(void)kb;
	move_toplevel_next(server);
}

static void handle_kill(struct compositor_state *server, struct keybinding *kb) {
	(void)kb;
	struct compositor_toplevel *t = get_focused_toplevel(server);
	if (t) wlr_xdg_toplevel_send_close(t->xdg_toplevel);
}

static void handle_resize_left(struct compositor_state *server, struct keybinding *kb) {
	(void)kb;
	resize_toplevel_left(server);
}

static void handle_resize_right(struct compositor_state *server, struct keybinding *kb) {
	(void)kb;
	resize_toplevel_right(server);
}

static void handle_resize_up(struct compositor_state *server, struct keybinding *kb) {
	(void)kb;
	resize_toplevel_up(server);
}

static void handle_resize_down(struct compositor_state *server, struct keybinding *kb) {
	(void)kb;
	resize_toplevel_down(server);
}

static void handle_focus_left(struct compositor_state *server, struct keybinding *kb) {
	(void)kb;
	focus_direction(server, DIR_LEFT);
}

static void handle_focus_right(struct compositor_state *server, struct keybinding *kb) {
	(void)kb;
	focus_direction(server, DIR_RIGHT);
}

static void handle_focus_up(struct compositor_state *server, struct keybinding *kb) {
	(void)kb;
	focus_direction(server, DIR_UP);
}

static void handle_focus_down(struct compositor_state *server, struct keybinding *kb) {
	(void)kb;
	focus_direction(server, DIR_DOWN);
}

static void handle_toggle_floating(struct compositor_state *server, struct keybinding *kb) {
	(void)kb;
	toggle_floating(server);
}

static void handle_reload(struct compositor_state *server, struct keybinding *kb) {
	(void)kb;
	if (!server->cfg) return;
	config_free(server->cfg);
	if (config_load(server->cfg) == 0) {
		printf("FlottyWM: config reloaded\n");
		arrange_workspace(server, server->active_workspace);
	} else {
		fprintf(stderr, "FlottyWM: failed to reload config\n");
	}
}

static const binding_handler_t handlers[] = {
	[BINDING_NONE] = NULL,
	[BINDING_EXEC] = handle_exec,
	[BINDING_EXIT] = handle_exit,
	[BINDING_WORKSPACE] = handle_workspace,
	[BINDING_MOVETOWORKSPACE] = handle_movetoworkspace,
	[BINDING_FOCUS_PREV] = handle_focus_prev,
	[BINDING_FOCUS_NEXT] = handle_focus_next,
	[BINDING_MOVE_PREV] = handle_move_prev,
	[BINDING_MOVE_NEXT] = handle_move_next,
	[BINDING_KILL] = handle_kill,
	[BINDING_RESIZE_LEFT] = handle_resize_left,
	[BINDING_RESIZE_RIGHT] = handle_resize_right,
	[BINDING_RESIZE_UP] = handle_resize_up,
	[BINDING_RESIZE_DOWN] = handle_resize_down,
	[BINDING_FOCUS_LEFT] = handle_focus_left,
	[BINDING_FOCUS_RIGHT] = handle_focus_right,
	[BINDING_FOCUS_UP] = handle_focus_up,
	[BINDING_FOCUS_DOWN] = handle_focus_down,
	[BINDING_TOGGLE_FLOATING] = handle_toggle_floating,
	[BINDING_RELOAD] = handle_reload,
};

static bool handle_keybinding(struct compositor_state *server, uint32_t modifiers, xkb_keysym_t sym) {
	/* Аварийный сброс grab (полезно в nested mode если потерялся button release) */
	if (sym == XKB_KEY_Escape && server->cursor_mode != CURSOR_PASSTHROUGH) {
		reset_cursor_mode(server);
		return true;
	}

	if (!server->cfg) return false;

	for (int i = 0; i < server->cfg->num_bindings; i++) {
		struct keybinding *kb = &server->cfg->bindings[i];
		if (kb->keysym == sym && kb->modifiers == modifiers) {
			binding_handler_t handler = handlers[kb->action];
			if (handler) handler(server, kb);
			return true;
		}
	}

	return false;
}

static void keyboard_handle_modifiers(struct wl_listener *listener, void *data) {
	(void)data;
	struct compositor_keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);

	wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);
	wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
		&keyboard->wlr_keyboard->modifiers);
}

static void keyboard_handle_key(struct wl_listener *listener, void *data) {
	struct compositor_keyboard *keyboard = wl_container_of(listener, keyboard, key);
	struct compositor_state *server = keyboard->server;
	struct wlr_keyboard_key_event *event = data;
	struct wlr_seat *seat = server->seat;

	uint32_t keycode = event->keycode + 8;
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(keyboard->wlr_keyboard->xkb_state, keycode, &syms);

	bool handled = false;

	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);
		for (int i = 0; i < nsyms; i++) {
			handled = handle_keybinding(server, modifiers, syms[i]);
			if (handled) break;
		}
		/* Fallback: ищем базовый keysym без учёта модификаторов раскладки
		   (чтобы Shift+1 находил биндинг на "1", а не только на "!") */
		if (!handled) {
			xkb_layout_index_t layout = xkb_state_key_get_layout(
				keyboard->wlr_keyboard->xkb_state, keycode);
			if (layout == XKB_LAYOUT_INVALID) layout = 0;
			const xkb_keysym_t *base_syms;
			int n_base = xkb_keymap_key_get_syms_by_level(
				keyboard->wlr_keyboard->keymap, keycode, layout, 0, &base_syms);
			for (int i = 0; i < n_base; i++) {
				handled = handle_keybinding(server, modifiers, base_syms[i]);
				if (handled) break;
			}
		}
	}

	if (!handled) {
		wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
		wlr_seat_keyboard_notify_key(seat, event->time_msec,
			event->keycode, event->state);
	}
}

static void keyboard_handle_destroy(struct wl_listener *listener, void *data) {
	(void)data;
	struct compositor_keyboard *keyboard = wl_container_of(listener, keyboard, destroy);

	wl_list_remove(&keyboard->modifiers.link);
	wl_list_remove(&keyboard->key.link);
	wl_list_remove(&keyboard->destroy.link);
	wl_list_remove(&keyboard->link);
	free(keyboard);
}

void server_new_keyboard(struct compositor_state *server,
		struct wlr_input_device *device) {
	struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);

	struct compositor_keyboard *keyboard = calloc(1, sizeof(*keyboard));
	keyboard->server = server;
	keyboard->wlr_keyboard = wlr_keyboard;

	/* Подготавливаем XKB keymap (раскладка US по умолчанию) */
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL,
		XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(wlr_keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

	/* Подключаем обработчики событий */
	keyboard->modifiers.notify = keyboard_handle_modifiers;
	wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);
	keyboard->key.notify = keyboard_handle_key;
	wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);
	keyboard->destroy.notify = keyboard_handle_destroy;
	wl_signal_add(&device->events.destroy, &keyboard->destroy);

	wlr_seat_set_keyboard(server->seat, keyboard->wlr_keyboard);

	/* Добавляем в список клавиатур */
	wl_list_insert(&server->keyboards, &keyboard->link);

	printf("Keyboard added\n");
}
