/* rcp_u.glsl: one draw's RDP state (rdr_vulkan.c U), std140 with the C
 * struct's offsets spelled out */
struct Tile { vec4 org; ivec4 size; ivec4 wrap; ivec4 cl; };
layout(std140, set = 0, binding = 0) uniform U {
    layout(offset = 0)   ivec4 cc[4];
    layout(offset = 64)  vec4 prim;
    layout(offset = 80)  vec4 env;
    layout(offset = 96)  vec4 fog;
    layout(offset = 112) vec4 blendc;
    layout(offset = 128) float plod;
    layout(offset = 132) float scale;
    layout(offset = 136) int cycle;
    layout(offset = 140) int filt;
    layout(offset = 144) int fog_blend;
    layout(offset = 148) int alpha_cmp;
    layout(offset = 152) int ntex;
    layout(offset = 156) int balpha;
    layout(offset = 160) int lodn;
    layout(offset = 164) int seed;
    layout(offset = 168) vec2 fb;
    layout(offset = 176) Tile tile[10];
} u;
