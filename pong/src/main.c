#define SOKOL_IMPL
#include "error.h"
#define SOKOL_ASSERT(c) ASSERT(c)
#define SOKOL_UNREACHABLE UNREACHABLE()
#define SOKOL_GLCORE33
#include <sokol_app.h>
#include <sokol_gfx.h>
#include <sokol_glue.h>
#include <sokol_log.h>

sg_pass_action pass_action;

static void init(void) {
    sg_setup(&(sg_desc) {
        .context = sapp_sgcontext(),
        .logger.func = slog_func,
    });
    pass_action = (sg_pass_action) { .colors[0] = {
                                         .load_action = SG_LOADACTION_CLEAR,
                                         .clear_value = { 1.0, 0.0, 0.0, 1.0 },
                                     } };
}

static void frame(void) {
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc) {
        .width = 640,
        .height = 480,
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .logger.func = slog_func,
    };
}
