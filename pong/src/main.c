#include <HandmadeMath.h>

#define SOKOL_IMPL
#include "error.h"
#define SOKOL_ASSERT(c) ASSERT(c)
#define SOKOL_UNREACHABLE UNREACHABLE()
#define SOKOL_GLCORE33
#include <sokol_app.h>
#include <sokol_gfx.h>
#include <sokol_glue.h>
#include <sokol_log.h>

#include "quad.glsl.h"

#define MAX_NUM_VERTICES 1024 * 6
typedef struct {
    HMM_Vec2 position;
} Vertex;

static struct {
    sg_pass_action pass_action;

    // Quad rendering
    u8 vertices[MAX_NUM_VERTICES * sizeof(Vertex)];
    uint num_vertices;
    sg_buffer vbo;
    sg_pipeline pipeline;

} self;

Vertex* allocate_vertices(uint num) {
    ASSERT(self.num_vertices + num < MAX_NUM_VERTICES);
    u8* p = &self.vertices[self.num_vertices * sizeof(Vertex)];
    self.num_vertices += num;

    return (Vertex*) p;
}

static void init(void) {
    sg_setup(&(sg_desc) {
        .context = sapp_sgcontext(),
        .logger.func = slog_func,
    });
    self.pass_action = (sg_pass_action) { .colors[0] = {
                                              .load_action = SG_LOADACTION_CLEAR,
                                              .clear_value = { 1.0, 0.0, 0.0, 1.0 },
                                          } };

    // Setup quad renderer
    self.vbo = sg_make_buffer(&(sg_buffer_desc) {
        .size = MAX_NUM_VERTICES * sizeof(Vertex),
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .usage = SG_USAGE_STREAM,
    });

    sg_shader quad_shader = sg_make_shader(quad_shader_desc(sg_query_backend()));

    self.pipeline = sg_make_pipeline(&(sg_pipeline_desc) {
        .shader = quad_shader,
        .layout = {
            .attrs[0] = {
                .format = SG_VERTEXFORMAT_FLOAT2,
            }
        },
    });
}

void draw_quad(HMM_Vec2 center, HMM_Vec2 extent) {
    HMM_Vec2 ps[4] = {
        HMM_SubV2(center, extent),
        HMM_AddV2(center, (HMM_Vec2) { -extent.X, extent.Y }),
        HMM_AddV2(center, extent),
        HMM_AddV2(center, (HMM_Vec2) { extent.X, -extent.Y }),
    };
    Vertex* v = allocate_vertices(6);
    v[0].position = ps[0];
    v[1].position = ps[1];
    v[2].position = ps[3];
    v[3].position = ps[1];
    v[4].position = ps[2];
    v[5].position = ps[3];
}

static void extract(void) {
    static float t = 0;

    float f1 = sinf(t);
    float f2 = cosf(t);
    float f3 = sinf(0.5 * t);

    self.num_vertices = 0;
    draw_quad((HMM_Vec2) { 0.5 * f1, 0.5 * f2 }, (HMM_Vec2) { 0.1, 0.1 });
    draw_quad((HMM_Vec2) { 0.0, 0.5 * f3 }, (HMM_Vec2) { 0.1, 0.3 });
    draw_quad((HMM_Vec2) { -0.5 * f1, -0.5 }, (HMM_Vec2) { 0.2, 0.1 * f3 });

    t += 0.1;
}

static void submit(void) {
    sg_begin_default_pass(&self.pass_action, sapp_width(), sapp_height());

    // Quad rendering
    sg_update_buffer(self.vbo, &(sg_range) {
                                   .ptr = self.vertices,
                                   .size = self.num_vertices * sizeof(Vertex),
                               });
    sg_apply_pipeline(self.pipeline);
    sg_apply_bindings(&(sg_bindings) {
        .vertex_buffers = { self.vbo },
    });
    sg_draw(0, self.num_vertices, 1);

    sg_end_pass();
    sg_commit();
}

static void frame(void) {
    extract();
    submit();
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
