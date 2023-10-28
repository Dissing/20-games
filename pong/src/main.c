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

typedef enum {
    INPUT_LEFT_PADDLE_UP,
    INPUT_LEFT_PADDLE_DOWN,
    INPUT_RIGHT_PADDLE_UP,
    INPUT_RIGHT_PADDLE_DOWN,
    INPUT_MAX,
} InputEvent;

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
    AABB ball;
    HMM_Vec2 ball_velocity;

    // Input
    InputEvent events[INPUT_MAX];
} self;

Vertex* allocate_vertices(uint num) {
    ASSERT(self.num_vertices + num < MAX_NUM_VERTICES);
    u8* p = &self.vertices[self.num_vertices * sizeof(Vertex)];
    self.num_vertices += num;

    return (Vertex*) p;
}

const f32 WALL_THICKNESS = 0.1;

static void spawn_ball(f32 direction) {
    const HMM_Vec2 BALL_EXTENT = { 0.02, 0.02 };
    self.ball = (AABB) { { 0, 0 }, BALL_EXTENT };

    const f32 BALL_START_SPEED = 0.01;
    self.ball_velocity = (HMM_Vec2) { direction * BALL_START_SPEED, 0.00 };
}

static void game_init(void) {
    self.walls[0] = (AABB) { { 0, -1 }, { 2, WALL_THICKNESS } };
    self.walls[1] = (AABB) { { 0, 1 }, { 2, WALL_THICKNESS } };

    const f32 PADDLE_X_OFFSET = 0.05;
    const HMM_Vec2 PADDLE_EXTENT = { 0.02, 0.15 };

    self.paddles[0] = (AABB) { { -1 + PADDLE_X_OFFSET, 0 }, PADDLE_EXTENT };
    self.paddles[1] = (AABB) { { 1 - PADDLE_X_OFFSET, 0 }, PADDLE_EXTENT };

    spawn_ball(1.0);
}

static f32 clamp(f32 x, f32 lower, f32 upper) {
    return fmax(fmin(x, upper), lower);
}

static f32 fsign(f32 x) {
    if (x > 0) return 1.0;
    if (x < 0) return -1.0;
    return 0.0;
}

bool aabb_intersect(AABB x, AABB y) {
    HMM_Vec2 distance = HMM_SubV2(x.center, y.center);
    HMM_Vec2 size = HMM_AddV2(x.extent, y.extent);
    return (size.X > HMM_ABS(distance.X)) && (size.Y > HMM_ABS(distance.Y));
}

static void tick(void) {
    const f32 PADDLE_SPEED = 0.02;

    // Handle input event
    if (self.events[INPUT_LEFT_PADDLE_UP]) { self.paddles[0].center.Y += PADDLE_SPEED; }
    if (self.events[INPUT_LEFT_PADDLE_DOWN]) { self.paddles[0].center.Y -= PADDLE_SPEED; }

    if (self.events[INPUT_RIGHT_PADDLE_UP]) { self.paddles[1].center.Y += PADDLE_SPEED; }
    if (self.events[INPUT_RIGHT_PADDLE_DOWN]) { self.paddles[1].center.Y -= PADDLE_SPEED; }

    // Update paddles
    for (uint i = 0; i < NUM_PLAYERS; ++i) {
        f32 min_y = -1 + self.paddles[i].extent.Y + WALL_THICKNESS;
        f32 max_y = 1 - self.paddles[i].extent.Y - WALL_THICKNESS;
        self.paddles[i].center.Y = clamp(self.paddles[i].center.Y, min_y, max_y);
    }

    // Update ball
    self.ball.center = HMM_AddV2(self.ball.center, self.ball_velocity);

    for (uint i = 0; i < NUM_WALLS; ++i)
        if (aabb_intersect(self.ball, self.walls[i])) self.ball_velocity.Y *= -1;

    const f32 BALL_SPEED = 0.025;
    const f32 MAX_BOUNCE_ANGLE = 60 * M_PI / 180;
    for (uint i = 0; i < NUM_PLAYERS; ++i) {
        if (aabb_intersect(self.ball, self.paddles[i])) {
            f32 relative_y = self.paddles[i].center.Y - self.ball.center.Y;
            f32 normalized_y = relative_y / self.paddles[i].extent.Y;
            f32 bounce_angle = normalized_y * MAX_BOUNCE_ANGLE;
            f32 bounce_sign = -fsign(self.ball.center.X);
            HMM_Vec2 direction =
                (HMM_Vec2) { bounce_sign * cosf(bounce_angle), -sinf(bounce_angle) };

            self.ball_velocity = HMM_MulV2F(direction, BALL_SPEED);
        }
    }

    bool left_won = self.ball.center.X > 1.0;
    bool right_won = self.ball.center.X < -1.0;

    if (left_won || right_won) { spawn_ball(left_won ? 1.0 : -1.0); }
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

    draw_aabb(self.ball);
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
    tick();
    extract();
    submit();
}

static void cleanup(void) {
    sg_shutdown();
}

static void handle_key_press(sapp_keycode key_code, bool pressed) {
    switch (key_code) {
        case SAPP_KEYCODE_COMMA: self.events[INPUT_LEFT_PADDLE_UP] = pressed; break;
        case SAPP_KEYCODE_O: self.events[INPUT_LEFT_PADDLE_DOWN] = pressed; break;
        case SAPP_KEYCODE_UP: self.events[INPUT_RIGHT_PADDLE_UP] = pressed; break;
        case SAPP_KEYCODE_DOWN: self.events[INPUT_RIGHT_PADDLE_DOWN] = pressed; break;
        default: break;
    }
}

static void event(const sapp_event* ev) {
    switch (ev->type) {
        case SAPP_EVENTTYPE_KEY_DOWN: handle_key_press(ev->key_code, true); break;
        case SAPP_EVENTTYPE_KEY_UP: handle_key_press(ev->key_code, false); break;
        default: break;
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc) {
        .width = 640,
        .height = 480,
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .logger.func = slog_func,
    };
}
