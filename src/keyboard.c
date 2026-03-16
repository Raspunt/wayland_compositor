#include "src/compositor.h"
#include "src/keyboard.h"
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>
#include <xkbcommon/xkbcommon.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <wlr/util/log.h>
#include <stdio.h>  

static bool handle_keybinding(struct compositor_state *server, uint32_t modifiers, xkb_keysym_t sym) {
	printf("key has been pressed: modifiers=%u, sym=%u\n", modifiers, sym);
	fflush(stdout);

	if ((modifiers & WLR_MODIFIER_ALT) && sym == XKB_KEY_Return) {
		printf("HOTKEY: Alt+Enter detected, launching terminal...\n");
		fflush(stdout);
		
		pid_t pid = fork();
		if (pid == 0) {
			setsid();
			
			execlp("alacritty", "alacritty", NULL);
			
			perror("Failed to launch terminal");
			_exit(1);
		} else if (pid < 0) {
			wlr_log(WLR_ERROR, "Failed to fork for terminal");
			printf("ERROR: fork failed\n");
		} else {
			printf("Terminal launched with PID %d\n", pid);
		}
		return true;
	}
	
	if ((modifiers & WLR_MODIFIER_ALT) && sym == XKB_KEY_Escape) {
		printf("HOTKEY: Alt+Escape pressed, terminating compositor...\n");
		wl_display_terminate(server->wl_display);
		return true;
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

    /* Получаем keycode и keysyms */
    uint32_t keycode = event->keycode + 8;
    const xkb_keysym_t *syms;
    int nsyms = xkb_state_key_get_syms(keyboard->wlr_keyboard->xkb_state, keycode, &syms);

    bool handled = false;
    
    /* Обрабатываем keybindings ТОЛЬКО при нажатии (PRESS) и с модификатором Alt */
    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);
        
        /* Отладка */
        printf("Key PRESS: code=%d, mods=%d (ALT=%d), syms=%d\n", 
               keycode, modifiers, 
               (modifiers & WLR_MODIFIER_ALT) ? 1 : 0,
               nsyms);
        
        if (modifiers & WLR_MODIFIER_ALT) {
            for (int i = 0; i < nsyms; i++) {
                printf("  Checking sym[%d]=%d\n", i, syms[i]);
                handled = handle_keybinding(server, modifiers, syms[i]);
                if (handled) {
                    printf("  -> handled by compositor (keybinding)\n");
                    break;
                }
            }
        }
    }

    /* Если не обработано как keybinding, передаем клиенту */
    if (!handled) {
        /* Устанавливаем эту клавиатуру как активную для seat */
        wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
        
        /* Отладка: проверяем, есть ли фокус */
        struct wlr_surface *focused = seat->keyboard_state.focused_surface;
        printf("  -> sending to client, focused_surface=%p\n", (void*)focused);
        
        if (focused == NULL) {
            printf("  WARNING: No focused surface! Click on window to focus.\n");
        }
        
        /* Отправляем событие клиенту (и PRESS, и RELEASE) */
        wlr_seat_keyboard_notify_key(seat, event->time_msec,
            event->keycode, event->state);
    } else {
        /* Обработано как keybinding - не отправляем клиенту ни PRESS, ни RELEASE */
        printf("  -> keybinding consumed event, not sending to client\n");
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