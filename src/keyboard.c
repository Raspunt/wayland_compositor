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
#include <sys/wait.h>
#include <spawn.h>
#include <wlr/util/log.h>
#include <stdio.h>  

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
			switch (kb->action) {
			case BINDING_EXEC: {
				if (!kb->arg) break;
				pid_t pid;
				char *argv[] = {kb->arg, NULL};
				posix_spawnp(&pid, kb->arg, NULL, NULL, argv, environ);
				break;
			}
			case BINDING_EXIT:
				wl_display_terminate(server->wl_display);
				break;
			case BINDING_WORKSPACE:
				switch_workspace(server, kb->arg ? atoi(kb->arg) : 1);
				break;
			case BINDING_MOVETOWORKSPACE: {
				struct compositor_toplevel *t = get_focused_toplevel(server);
				if (t) move_toplevel_to_workspace(t, kb->arg ? atoi(kb->arg) : 1);
				break;
			}
			case BINDING_FOCUS_PREV:
				focus_prev(server);
				break;
			case BINDING_FOCUS_NEXT:
				focus_next(server);
				break;
			case BINDING_MOVE_PREV:
				move_toplevel_prev(server);
				break;
			case BINDING_MOVE_NEXT:
				move_toplevel_next(server);
				break;
			case BINDING_KILL: {
				struct compositor_toplevel *t = get_focused_toplevel(server);
				if (t) wlr_xdg_toplevel_send_close(t->xdg_toplevel);
				break;
			}
			case BINDING_RESIZE_LEFT:
				resize_toplevel_left(server);
				break;
			case BINDING_RESIZE_RIGHT:
				resize_toplevel_right(server);
				break;
			case BINDING_RESIZE_UP:
				resize_toplevel_up(server);
				break;
			case BINDING_RESIZE_DOWN:
				resize_toplevel_down(server);
				break;
			default:
				break;
			}
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