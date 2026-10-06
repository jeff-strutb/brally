#version 450
/* blit.vert: the finished frame as a quad in the window, letterboxed (r: x, y,
 * width, height in Vulkan's normalised coordinates, y down) */
layout(push_constant) uniform P { vec4 r; int gamma; } p;
layout(location = 0) out vec2 v_uv;
void main()
{
    vec2 q = vec2((gl_VertexIndex & 1) != 0 ? 1.0 : 0.0, (gl_VertexIndex & 2) != 0 ? 1.0 : 0.0);
    gl_Position = vec4(p.r.x + q.x * p.r.z, p.r.y + q.y * p.r.w, 0.0, 1.0);
    v_uv = q;
}
