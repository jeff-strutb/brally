#version 450
/* rcp.vert: an RDP vertex, N64 framebuffer pixels in clip space -> Vulkan's
 * (y down, depth 0..1); the same mapping as rdr_metal.m's vs */
#extension GL_GOOGLE_include_directive : require
#include "rcp_u.glsl"
layout(location = 0) in vec4 a_pos;
layout(location = 1) in vec2 a_st;
layout(location = 2) in vec4 a_rgba;
layout(location = 0) out vec2 v_st;
layout(location = 1) out vec4 v_rgba;
void main()
{
    float w = a_pos.w;
    gl_Position = vec4(a_pos.x / u.fb.x * 2.0 - w, a_pos.y / u.fb.y * 2.0 - w, (a_pos.z + w) * 0.5, w);
    v_st = a_st;
    v_rgba = a_rgba;
}
