#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <wlr/version.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_seat.h>

#include "src/output.h"
#include "src/input.h"   
#include "src/compositor.h"


int main(void) {
    printf("Hello wlroots %s\n", WLR_VERSION_STR);

    struct compositor_state cs = {0};  

    cs.wl_display = wl_display_create();
    assert(cs.wl_display);
    cs.wl_event_loop = wl_display_get_event_loop(cs.wl_display);
    assert(cs.wl_event_loop);

    cs.wlr_session = wlr_session_create(cs.wl_event_loop);
    cs.backend = wlr_backend_autocreate(cs.wl_event_loop, &cs.wlr_session);
    assert(cs.backend);

    cs.renderer = wlr_renderer_autocreate(cs.backend);
    assert(cs.renderer);
    cs.allocator = wlr_allocator_autocreate(cs.backend, cs.renderer);
    assert(cs.allocator);

    cs.output_layout = wlr_output_layout_create(cs.wl_display);

    cs.seat = wlr_seat_create(cs.wl_display, "seat0");

    wl_list_init(&cs.outputs);

    cs.new_output.notify = new_output_notify;
    wl_signal_add(&cs.backend->events.new_output, &cs.new_output);

    cs.cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(cs.cursor, cs.output_layout);
    
    cs.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
    wlr_xcursor_manager_load(cs.cursor_mgr, 1);

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

    if (!wlr_backend_start(cs.backend)) {
        fprintf(stderr, "Failed to start backend\n");
        wl_display_destroy(cs.wl_display);
        return 1;
    }

    wl_display_run(cs.wl_display);
    
    wl_display_destroy(cs.wl_display);
    return 0;
}