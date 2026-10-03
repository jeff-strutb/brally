#version 450
// Glide's screen-space vertices: pixels, depth 0..1, 1/w, colour 0..255,
// s/t already divided by w. w = 1/oow makes the hardware's interpolation
// perspective-correct for texture coordinates; colour, 1/w and depth are
// screen-linear, as the Voodoo interpolates them.
#extension GL_GOOGLE_include_directive : require
#include "glide_u.glsl"
layout(location = 0) in vec2 a_pos;
layout(location = 1) in float a_z;
layout(location = 2) in float a_oow;
layout(location = 3) in vec4 a_col;
layout(location = 4) in vec2 a_st;
layout(location = 0) noperspective out vec4 v_col;
layout(location = 1) out vec2 v_st;
layout(location = 2) noperspective out float v_oow;
layout(location = 3) noperspective out float v_z;
// the screen map (glide.c): the game's pixels to the target, this frame's
// table; its y is up, Vulkan's down
layout(std430, set = 3, binding = 0) readonly buffer XT { vec4 xt[]; };
void main() {
    float w = a_oow > 0.0 ? 1.0 / a_oow : 1.0;
    vec4 T = xt[int(u.xf)];
    vec2 ndc = vec2(a_pos.x * T.x + T.y, -(a_pos.y * T.z + T.w));
    gl_Position = vec4(ndc * w, clamp(a_z, 0.0, 1.0) * w, w);
    v_col = clamp(a_col / 255.0, 0.0, 1.0);
    v_st = a_st;
    v_oow = a_oow;
    v_z = a_z;
}
