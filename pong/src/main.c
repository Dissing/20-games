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

typedef struct {
    HMM_Vec2 position_offset;
    HMM_Vec2 position_scale;
    HMM_Vec2 uv_offset;
    HMM_Vec2 uv_scale;
    HMM_Vec4 color;
} BatchInstance;

typedef struct {
    uint instance_offset;
    uint num_instance;
    sg_image image;
} SpriteBatch;

typedef struct {
    sg_image image;
    uint width, height;
    uint num_cols, num_rows;
} SpriteAtlas;

typedef struct {
    BatchInstance instance;
    sg_image image;
} SpriteEntry;

typedef struct {
    HMM_Vec2 center;
    HMM_Vec2 extent;
} AABB;

typedef enum {
    INPUT_LEFT_PADDLE_UP,
    INPUT_LEFT_PADDLE_DOWN,
    INPUT_RIGHT_PADDLE_UP,
    INPUT_RIGHT_PADDLE_DOWN,
    INPUT_MAX,
} InputEvent;

#define MAX_NUM_SPRITES 1024
#define NUM_WALLS 2
#define NUM_PLAYERS 2

static struct {
    sg_pass_action pass_action;

    // Sprite rendering
    u8 instances[MAX_NUM_SPRITES * sizeof(BatchInstance)];
    sg_buffer vertex_vbo;
    sg_buffer instance_vbo;
    sg_pipeline pipeline;

    SpriteEntry sprite_entries[MAX_NUM_SPRITES];
    uint num_sprite_entries;

    sg_image white_image;
    sg_sampler sampler;

    // World
    AABB walls[NUM_WALLS];

    AABB paddles[NUM_PLAYERS];
    AABB ball;
    HMM_Vec2 ball_velocity;

    // Input
    InputEvent events[INPUT_MAX];
} self;

const f32 WALL_THICKNESS = 0.1;

static void spawn_ball(f32 direction) {
    const HMM_Vec2 BALL_EXTENT = { 0.02, 0.02 };
    self.ball = (AABB) { { 0, 0 }, BALL_EXTENT };

    const f32 BALL_START_SPEED = 0.01;
    self.ball_velocity = (HMM_Vec2) { direction * BALL_START_SPEED, 0.00 };
}

static void game_init(void) {
    self.walls[0] = (AABB) { { 0, -1 }, { 1, WALL_THICKNESS } };
    self.walls[1] = (AABB) { { 0, 1 }, { 1, WALL_THICKNESS } };

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

    // clang-format off
    f32 vertex_data[] = {
        //pos     uv
        -1.0, -1.0, 0.0, 0.0,
         1.0, -1.0, 1.0, 0.0,
        -1.0,  1.0, 0.0, 1.0,
         1.0, -1.0, 1.0, 0.0,
         1.0,  1.0, 1.0, 1.0,
        -1.0,  1.0, 0.0, 1.0,
    };
    // clang-format on

    // Setup quad renderer
    self.vertex_vbo = sg_make_buffer(&(sg_buffer_desc) {
        .size = sizeof(vertex_data),
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .usage = SG_USAGE_IMMUTABLE,
        .data = vertex_data,
    });

    self.instance_vbo = sg_make_buffer(&(sg_buffer_desc) {
        .size = sizeof(BatchInstance) * MAX_NUM_SPRITES,
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .usage = SG_USAGE_STREAM,
    });

    sg_shader quad_shader = sg_make_shader(quad_shader_desc(sg_query_backend()));

    self.pipeline = sg_make_pipeline(&(sg_pipeline_desc) {
        .shader = quad_shader,
        .layout = {
            .buffers[1].step_func = SG_VERTEXSTEP_PER_INSTANCE,
            .buffers[1].stride = sizeof(BatchInstance),
            .attrs = {
                [ATTR_vs_v_pos] = {
                    .format = SG_VERTEXFORMAT_FLOAT2,
                    .buffer_index = 0
                },
                [ATTR_vs_v_uv] = {
                    .format = SG_VERTEXFORMAT_FLOAT2,
                    .buffer_index = 0
                },
                [ATTR_vs_i_pos_offset] = {
                    .format = SG_VERTEXFORMAT_FLOAT2,
                    .buffer_index = 1,
                    .offset = offsetof(BatchInstance, position_offset),
                },
                [ATTR_vs_i_pos_scale] = {
                    .format = SG_VERTEXFORMAT_FLOAT2,
                    .buffer_index = 1,
                    .offset = offsetof(BatchInstance, position_scale),
                },
                [ATTR_vs_i_uv_offset] = {
                    .format = SG_VERTEXFORMAT_FLOAT2,
                    .buffer_index = 1,
                    .offset = offsetof(BatchInstance, uv_offset),
                },
                [ATTR_vs_i_uv_scale] = {
                    .format = SG_VERTEXFORMAT_FLOAT2,
                    .buffer_index = 1,
                    .offset = offsetof(BatchInstance, uv_scale),
                },
                [ATTR_vs_i_color] = {
                    .format = SG_VERTEXFORMAT_FLOAT4,
                    .buffer_index = 1,
                    .offset = offsetof(BatchInstance, color),
                },
            },
        },
    });

    u8 white_pixel[] = { 0xFF, 0xFF, 0xFF, 0xFF };

    self.white_image = sg_make_image(&(sg_image_desc) {
        .width = 1,
        .height = 1,
        .data.subimage[0][0] = SG_RANGE(white_pixel),
    });

    self.sampler = sg_make_sampler(&(sg_sampler_desc) {});
}

static void draw_sprite(SpriteAtlas* atlas, uint index, AABB aabb, HMM_Vec4 color) {
    ASSERT(self.num_sprite_entries < MAX_NUM_SPRITES);
    uint idx = self.num_sprite_entries++;

    uint uv_col = index % atlas->num_cols;
    uint uv_row = index / atlas->num_cols;
    ASSERT(uv_row < atlas->num_rows);

    HMM_Vec2 uv_offset = (HMM_Vec2) {
        uv_col / (f32) atlas->width,
        uv_row / (f32) atlas->height,
    };

    HMM_Vec2 uv_scale = (HMM_Vec2) {
        atlas->num_cols / (f32) atlas->width,
        atlas->num_rows / (f32) atlas->height,
    };

    self.sprite_entries[idx] = (SpriteEntry) {
        .instance = {
            .position_offset = aabb.center,
            .position_scale = aabb.extent,
            .uv_offset = uv_offset,
            .uv_scale = uv_scale,
            .color = color,
        },
        .image = atlas->image,
    };
}

static void draw_aabb(AABB aabb, HMM_Vec4 color) {
    ASSERT(self.num_sprite_entries < MAX_NUM_SPRITES);
    uint idx = self.num_sprite_entries++;
    self.sprite_entries[idx] = (SpriteEntry) {
        .instance = {
            .position_offset = aabb.center,
            .position_scale = aabb.extent,
            .uv_offset = {},
            .uv_scale = {1.0, 1.0},
            .color = color,
        },
        .image = self.white_image,
    };
}

int compare_sprite_entries(const void* a, const void* b) {
    sg_image img_a = ((const SpriteEntry*) a)->image;
    sg_image img_b = ((const SpriteEntry*) b)->image;

    if (img_a.id < img_b.id) return -1;
    if (img_a.id > img_b.id) return 1;
    return 0;
}

static void draw_sprites() {
    if (self.num_sprite_entries == 0) return;

    // Sort entries according to texture
    qsort(self.sprite_entries, self.num_sprite_entries, sizeof(SpriteEntry),
          compare_sprite_entries);

    // Prepare instance data and build batches
    const uint MAX_NUM_BATCHES = 32;
    SpriteBatch batches[MAX_NUM_BATCHES];
    uint num_batches;

    uint prev_image_id = self.sprite_entries[0].image.id;
    uint prev_end = 0;
    uint num_batch_instances = 0;

    for (uint i = 0; i < self.num_sprite_entries; ++i) {
        num_batch_instances++;
        uint offset = i * sizeof(BatchInstance);

        SpriteEntry* entry = &self.sprite_entries[i];
        memcpy(self.instances + offset, &entry->instance, sizeof(BatchInstance));

        if (entry->image.id != prev_image_id || i == self.num_sprite_entries - 1) {
            ASSERT(num_batches < MAX_NUM_BATCHES);
            SpriteBatch* batch = &batches[num_batches++];
            batch->instance_offset = prev_end;
            batch->num_instance = num_batch_instances;
            batch->image = entry->image;

            prev_image_id = entry->image.id;
            prev_end = offset + sizeof(BatchInstance);
            num_batch_instances = 0;
        }
    }

    // Upload instances to GPU
    sg_update_buffer(self.instance_vbo, &(sg_range) {
                                            .ptr = self.instances,
                                            .size = self.num_sprite_entries * sizeof(BatchInstance),
                                        });

    sg_apply_pipeline(self.pipeline);

    // Draw each batch
    for (uint i = 0; i < num_batches; ++i) {
        sg_apply_bindings(&(sg_bindings) {
            .fs.images = { batches[i].image },
            .fs.samplers = { self.sampler },
            .vertex_buffers = { self.vertex_vbo, self.instance_vbo },
            .vertex_buffer_offsets[1] = batches[i].instance_offset,
        });
        sg_draw(0, 6, batches[i].num_instance);
    }

    // Clear state for next frame
    self.num_sprite_entries = 0;
}

const HMM_Vec4 WHITE = (HMM_Vec4) { 1.0, 1.0, 1.0, 1.0 };
const HMM_Vec4 RED = (HMM_Vec4) { 1.0, 0.0, 0.0, 1.0 };

static void draw_vertical_dashed_line(HMM_Vec2 start, HMM_Vec2 end, uint num_segments, f32 width) {
    HMM_Vec2 d = HMM_SubV2(end, start);
    HMM_Vec2 wd = HMM_MulV2F(HMM_NormV2((HMM_Vec2) { d.Y, -d.X }), width);

    f32 segment_length = 1.0 / (2 * num_segments - 1);

    for (uint i = 0; i < num_segments; ++i) {
        f32 t = 2 * i * segment_length;
        HMM_Vec2 center = HMM_AddV2(start, HMM_MulV2F(d, t));
        HMM_Vec2 extent = (HMM_Vec2) { width / 2.0, segment_length };

        draw_aabb((AABB) { center, extent }, WHITE);
    }
}

static void extract(void) {
    for (uint i = 0; i < NUM_WALLS; ++i) draw_aabb(self.walls[i], WHITE);

    draw_vertical_dashed_line((HMM_Vec2) { 0.0, 0.88 }, (HMM_Vec2) { 0.0, -0.88 }, 32, 0.01);

    for (uint i = 0; i < NUM_PLAYERS; ++i) draw_aabb(self.paddles[i], WHITE);

    draw_aabb(self.ball, WHITE);
}

static void submit(void) {
    sg_begin_default_pass(&self.pass_action, sapp_width(), sapp_height());

    draw_sprites();

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
