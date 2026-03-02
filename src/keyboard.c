#include "src/compositor.h"
#include "src/keyboard.h"
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>
#include <xkbcommon/xkbcommon.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <wlr/util/log.h>

/* Заглушка для обработки хоткеев (Alt+F4 и т.д.) */
static bool handle_keybinding(struct compositor_state *server, uint32_t modifiers, xkb_keysym_t sym) {
	/* Win/Super + Enter - запуск alacritty */
	if ((modifiers & WLR_MODIFIER_LOGO) && sym == XKB_KEY_Return) {
		pid_t pid = fork();
		if (pid == 0) {
			/* Дочерний процесс */
			execlp("alacritty", "alacritty", NULL);
			_exit(1);  /* Если execlp не сработал */
		} else if (pid < 0) {
			wlr_log(WLR_ERROR, "Failed to fork for alacritty");
		}
		return true;
	}
	
	/* Ctrl + Escape - выход из compositor (смерть) */
	if ((modifiers & WLR_MODIFIER_CTRL) && sym == XKB_KEY_Escape) {
		wlr_log(WLR_INFO, "Ctrl+Escape pressed, terminating compositor...");
		exit(0);
		// wl_display_terminate(server->wl_display);
		return true;
	}
	
	return false;
}
/* Обработчики событий клавиатуры (static - только для этого файла) */

static void keyboard_handle_modifiers(struct wl_listener *listener, void *data) {
	(void)data;
	/* Этот код вызывается при нажатии модификаторов (Shift, Ctrl, Alt) */
	struct compositor_keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);
	
	/* Устанавливаем текущую клавиатуру для seat */
	wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);
	/* Отправляем модификаторы клиенту с фокусом */
	wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
		&keyboard->wlr_keyboard->modifiers);
}

static void keyboard_handle_key(struct wl_listener *listener, void *data) {
	/* Этот код вызывается при нажатии/отпускании клавиши */
	struct compositor_keyboard *keyboard = wl_container_of(listener, keyboard, key);
	struct compositor_state *server = keyboard->server;  /* Исправлено с tinywl_server */
	struct wlr_keyboard_key_event *event = data;
	struct wlr_seat *seat = server->seat;

	/* Переводим keycode libinput -> xkbcommon */
	uint32_t keycode = event->keycode + 8;
	/* Получаем список keysym по keymap этой клавиатуры */
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(
			keyboard->wlr_keyboard->xkb_state, keycode, &syms);

	bool handled = false;
	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);
	if ((modifiers & WLR_MODIFIER_ALT) &&
			event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		/* Если Alt зажат и кнопка нажата - проверяем хоткеи compositor'а */
		for (int i = 0; i < nsyms; i++) {
			handled = handle_keybinding(server, modifiers, syms[i]);
		}
	}

	if (!handled) {
		/* Иначе передаем клиенту */
		wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
		wlr_seat_keyboard_notify_key(seat, event->time_msec,
			event->keycode, event->state);
	}
}

static void keyboard_handle_destroy(struct wl_listener *listener, void *data) {
	(void)data;
	/* Вызывается при отключении клавиатуры (USB unplug) */
	struct compositor_keyboard *keyboard = wl_container_of(listener, keyboard, destroy);
	
	wl_list_remove(&keyboard->modifiers.link);
	wl_list_remove(&keyboard->key.link);
	wl_list_remove(&keyboard->destroy.link);
	wl_list_remove(&keyboard->link);
	free(keyboard);
}

/* Публичная функция (без static - вызывается из input.c или main.c) */

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
}