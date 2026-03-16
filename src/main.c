#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h> 
#include <wayland-server-core.h>  // Для wl_display_create

#include <wlr/version.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_scene.h>  
#include <wlr/types/wlr_data_device.h>  
#include <wlr/types/wlr_compositor.h>      // Добавлено!
#include <wlr/types/wlr_subcompositor.h>   // Добавлено!

#include "src/output.h"
#include "src/input.h"   
#include "src/compositor.h"
#include "src/renderer.h"

int main(void) {
    printf("Hello wlroots %s\n", WLR_VERSION_STR);

    struct compositor_state cs = {0};  

    cs.wl_display = wl_display_create();
    assert(cs.wl_display);
    
    cs.wl_event_loop = wl_display_get_event_loop(cs.wl_display);
    assert(cs.wl_event_loop);

    // КРИТИЧНО: Создаем сессию перед backend
    cs.wlr_session = wlr_session_create(cs.wl_event_loop);
    assert(cs.wlr_session);
    
    cs.backend = wlr_backend_autocreate(cs.wl_event_loop, &cs.wlr_session);
    assert(cs.backend);

    cs.renderer = wlr_renderer_autocreate(cs.backend);
    assert(cs.renderer);
    
    cs.allocator = wlr_allocator_autocreate(cs.backend, cs.renderer);
    assert(cs.allocator);

    cs.output_layout = wlr_output_layout_create(cs.wl_display);
    cs.seat = wlr_seat_create(cs.wl_display, "seat0");

    cs.request_set_cursor.notify = seat_request_set_cursor;
    wl_signal_add(&cs.seat->events.request_set_cursor, &cs.request_set_cursor);

    // КРИТИЧНО: Создаем сцену и привязываем к output layout
    cs.scene = wlr_scene_create();
    assert(cs.scene);
    cs.scene_layout = wlr_scene_attach_output_layout(cs.scene, cs.output_layout);
    assert(cs.scene_layout);

    wl_list_init(&cs.outputs);
    wl_list_init(&cs.toplevels);

    // КРИТИЧНО: Регистрируем глобальные интерфейсы Wayland ДО старта backend
    wlr_renderer_init_wl_display(cs.renderer, cs.wl_display);
    wlr_compositor_create(cs.wl_display, 6, cs.renderer);
    wlr_subcompositor_create(cs.wl_display);
    wlr_data_device_manager_create(cs.wl_display);

    cs.new_output.notify = new_output_notify;
    wl_signal_add(&cs.backend->events.new_output, &cs.new_output);

    cs.cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(cs.cursor, cs.output_layout);
    
    cs.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
    wlr_xcursor_manager_load(cs.cursor_mgr, 1);
    
    // ИСПРАВЛЕНО: Передаем cursor_mgr вместо NULL
    wlr_cursor_set_xcursor(cs.cursor, cs.cursor_mgr, "default");

    // Предполагается, что CURSOR_PASSTHROUGH = 0 определен в input.h
    // Если нет - замените на: cs.cursor_mode = 0;
    cs.cursor_mode = CURSOR_PASSTHROUGH;
    
    cs.cursor_motion.notify = server_cursor_motion;
    wl_signal_add(&cs.cursor->events.motion, &cs.cursor_motion);
    
    cs.cursor_motion_absolute.notify = server_cursor_motion_absolute;
    wl_signal_add(&cs.cursor->events.motion_absolute, &cs.cursor_motion_absolute);
    
    cs.cursor_button.notify = server_cursor_button;
    wl_signal_add(&cs.cursor->events.button, &cs.cursor_button);
    
    cs.cursor_axis.notify = server_cursor_axis;
    wl_signal_add(&cs.cursor->events.axis, &cs.cursor_axis);
    
    cs.cursor_frame.notify = server_cursor_frame;
    wl_signal_add(&cs.cursor->events.frame, &cs.cursor_frame);

    wl_list_init(&cs.keyboards);

    cs.new_input.notify = server_new_input; 
    wl_signal_add(&cs.backend->events.new_input, &cs.new_input);

    cs.xdg_shell = wlr_xdg_shell_create(cs.wl_display, 3);
    cs.new_xdg_toplevel.notify = server_new_xdg_toplevel;
    wl_signal_add(&cs.xdg_shell->events.new_toplevel, &cs.new_xdg_toplevel);
    cs.new_xdg_popup.notify = server_new_xdg_popup;
    wl_signal_add(&cs.xdg_shell->events.new_popup, &cs.new_xdg_popup);

    if (!wlr_backend_start(cs.backend)) {
        fprintf(stderr, "Failed to start backend\n");
        wl_display_destroy(cs.wl_display);
        return 1;
    }

    const char *socket = wl_display_add_socket_auto(cs.wl_display);
    if (!socket) {
        wlr_backend_destroy(cs.backend);
        return 1;
    }

    setenv("WAYLAND_DISPLAY", socket, true);
    printf("Compositor running on WAYLAND_DISPLAY=%s\n", socket);
    printf("Press Alt+Enter for terminal, Alt+Esc to exit\n");

    wl_display_run(cs.wl_display);
    
    wl_display_destroy_clients(cs.wl_display);

    // Очистка
    wl_list_remove(&cs.request_set_cursor.link);  // Добавлена очистка
    wl_list_remove(&cs.new_xdg_toplevel.link);
    wl_list_remove(&cs.new_xdg_popup.link);
    wl_list_remove(&cs.cursor_motion.link);
    wl_list_remove(&cs.cursor_motion_absolute.link);
    wl_list_remove(&cs.cursor_button.link);
    wl_list_remove(&cs.cursor_axis.link);
    wl_list_remove(&cs.cursor_frame.link);
    wl_list_remove(&cs.new_input.link);
    wl_list_remove(&cs.new_output.link);

    wlr_scene_node_destroy(&cs.scene->tree.node);
    wlr_xcursor_manager_destroy(cs.cursor_mgr);
    wlr_cursor_destroy(cs.cursor);
    wlr_allocator_destroy(cs.allocator);
    wlr_renderer_destroy(cs.renderer);
    wlr_backend_destroy(cs.backend);
    wl_display_destroy(cs.wl_display);
    
    return 0;
}