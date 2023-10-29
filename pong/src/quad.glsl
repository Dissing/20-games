@vs vs

// Per vertex
in vec2 v_pos;
in vec2 v_uv;

// Per instance

in vec2 i_pos_offset;
in vec2 i_pos_scale;
in vec2 i_uv_offset;
in vec2 i_uv_scale;
in vec4 i_color;

out vec2 f_uv;
out vec4 f_color;

void main() {
    vec2 pos = v_pos * i_pos_scale + i_pos_offset;
    gl_Position = vec4(pos, 0, 1);

    f_uv = v_uv * i_uv_scale + i_uv_offset;

    f_color = i_color;
}

@end

@fs fs

in vec2 f_uv;
in vec4 f_color;

out vec4 out_color;

uniform texture2D tex;
uniform sampler smp;

void main() {
    out_color = texture(sampler2D(tex, smp), f_uv) * f_color;
}

@end

@program quad vs fs
