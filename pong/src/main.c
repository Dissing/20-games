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

typedef struct {
    HMM_Vec2 center;
    HMM_Vec2 extent;
} AABB;

#define NUM_WALLS 2
#define NUM_PLAYERS 2

static struct {
    sg_pass_action pass_action;

    // Quad rendering
    u8 vertices[MAX_NUM_VERTICES * sizeof(Vertex)];
    uint num_vertices;
    sg_buffer vbo;
    sg_pipeline pipeline;

    // World
    AABB walls[NUM_WALLS];

    AABB paddles[NUM_PLAYERS];

} self;

Vertex* allocate_vertices(uint num) {
    ASSERT(self.num_vertices + num < MAX_NUM_VERTICES);
    u8* p = &self.vertices[self.num_vertices * sizeof(Vertex)];
    self.num_vertices += num;

    return (Vertex*) p;
}

static void game_init(void) {
    const float WALL_THICKNESS = 0.1;
    self.walls[0] = (AABB) { { 0, -1 }, { 2, WALL_THICKNESS } };
    self.walls[1] = (AABB) { { 0, 1 }, { 2, WALL_THICKNESS } };

    const float PADDLE_X_OFFSET = 0.05;
    const HMM_Vec2 PADDLE_EXTENT = { 0.02, 0.15 };

    self.paddles[0] = (AABB) { { -1 + PADDLE_X_OFFSET, 0 }, PADDLE_EXTENT };
    self.paddles[1] = (AABB) { { 1 - PADDLE_X_OFFSET, 0 }, PADDLE_EXTENT };
}

static void render_init(void) {
    sg_setup(&(sg_desc) {
        .context = sapp_sgcontext(),
        .logger.func = slog_func,
    });
    self.pass_action = (sg_pass_action) { .colors[0] = {
                                              .load_action = SG_LOADACTION_CLEAR,
                                              .clear_value = { 0.0, 0.0, 0.0, 1.0 },
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

static void draw_quad(HMM_Vec2 ps[4]) {
    Vertex* v = allocate_vertices(6);
    v[0].position = ps[0];
    v[1].position = ps[1];
    v[2].position = ps[3];
    v[3].position = ps[1];
    v[4].position = ps[2];
    v[5].position = ps[3];
}

static void draw_aabb(AABB aabb) {
    HMM_Vec2 ps[4] = {
        HMM_SubV2(aabb.center, aabb.extent),
        HMM_AddV2(aabb.center, (HMM_Vec2) { -aabb.extent.X, aabb.extent.Y }),
        HMM_AddV2(aabb.center, aabb.extent),
        HMM_AddV2(aabb.center, (HMM_Vec2) { aabb.extent.X, -aabb.extent.Y }),
    };
    Vertex* v = allocate_vertices(6);
    v[0].position = ps[0];
    v[1].position = ps[1];
    v[2].position = ps[3];
    v[3].position = ps[1];
    v[4].position = ps[2];
    v[5].position = ps[3];
}

static void draw_dashed_line(HMM_Vec2 start, HMM_Vec2 end, uint num_segments, f32 width) {
    HMM_Vec2 d = HMM_SubV2(end, start);
    HMM_Vec2 wd = HMM_MulV2F(HMM_NormV2((HMM_Vec2) { d.Y, -d.X }), width);

    f32 segment_size = 1.0 / (2 * num_segments - 1);

    for (uint i = 0; i < num_segments; ++i) {
        f32 t = 2 * i * segment_size;
        HMM_Vec2 segment_start = HMM_AddV2(start, HMM_MulV2F(d, t));
        HMM_Vec2 segment_end = HMM_AddV2(start, HMM_MulV2F(d, t + segment_size));
        HMM_Vec2 ps[4] = {
            HMM_SubV2(segment_start, wd),
            HMM_AddV2(segment_start, wd),
            HMM_AddV2(segment_end, wd),
            HMM_SubV2(segment_end, wd),
        };
        draw_quad(ps);
    }
}

static void extract(void) {
    self.num_vertices = 0;

    for (uint i = 0; i < NUM_WALLS; ++i) draw_aabb(self.walls[i]);

    draw_dashed_line((HMM_Vec2) { 0.0, 0.88 }, (HMM_Vec2) { 0.0, -0.88 }, 32, 0.01);

    for (uint i = 0; i < NUM_PLAYERS; ++i) draw_aabb(self.paddles[i]);
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

static void init(void) {
    game_init();
    render_init();
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
