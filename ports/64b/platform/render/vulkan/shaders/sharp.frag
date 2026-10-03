#version 450
// Sharp bilinear: whole texels at any scale, blended only across the one
// window pixel a texel edge falls in (as the Metal present, brr_metal.m).
layout(set = 1, binding = 0) uniform texture2D t_tex;
layout(set = 2, binding = 0) uniform sampler t_smp;
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_col;
void main() {
    vec2 ts = vec2(textureSize(sampler2D(t_tex, t_smp), 0));
    vec2 px = v_uv * ts - 0.5;
    vec2 i = floor(px), f = px - i, d = max(fwidth(px), vec2(1e-5));
    f = clamp((f - 0.5) / d + 0.5, 0.0, 1.0);
    o_col = vec4(texture(sampler2D(t_tex, t_smp), (i + 0.5 + f) / ts).rgb, 1.0);
}
