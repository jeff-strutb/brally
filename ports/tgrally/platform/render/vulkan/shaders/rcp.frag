#version 450
/* rcp.frag: the RDP's texturing, colour combiner, alpha compare and fog, per
 * pixel: rdr_metal.m's fs, line for line. tex[0], tex[1]: the two tiles;
 * tex[2 + k]: level k of a mipmapped texture (selected by a switch, since a
 * per-pixel index into a sampler array needs an extension). */
#extension GL_GOOGLE_include_directive : require
#include "rcp_u.glsl"
layout(set = 1, binding = 0) uniform sampler2D tex[10];
layout(location = 0) in vec2 v_st;
layout(location = 1) in vec4 v_rgba;
layout(location = 0) out vec4 o_col;

float noise8(vec2 p, int sd)     /* the RDP's per-pixel random value, 0..1, new each frame */
{
    uint h = uint(p.x) * 1973u + uint(p.y) * 9277u + uint(sd) * 26699u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return float((h ^ (h >> 16)) & 255u) / 255.0;
}

int wrapi(int i, int n, int clampv, int mirror, int mask, int cw)
{
    if (clampv != 0 && cw > 0)
        i = clamp(i, 0, cw - 1);
    if (mask > 0) {
        int m = ((i % mask) + mask) % mask;
        if (mirror != 0 && (((i < 0 ? -i - 1 : i) / mask) & 1) != 0)
            m = mask - 1 - m;
        return min(m, n - 1);
    }
    return clamp(i, 0, n - 1);
}

vec4 fetch(int k, ivec2 p)
{
    switch (k) {
    case 0: return texelFetch(tex[0], p, 0);
    case 1: return texelFetch(tex[1], p, 0);
    case 2: return texelFetch(tex[2], p, 0);
    case 3: return texelFetch(tex[3], p, 0);
    case 4: return texelFetch(tex[4], p, 0);
    case 5: return texelFetch(tex[5], p, 0);
    case 6: return texelFetch(tex[6], p, 0);
    case 7: return texelFetch(tex[7], p, 0);
    case 8: return texelFetch(tex[8], p, 0);
    default: return texelFetch(tex[9], p, 0);
    }
}

vec4 texel(int k, vec2 st, int filt)
{
    Tile tl = u.tile[k];
    float x = st.x * tl.org.z - tl.org.x, y = st.y * tl.org.w - tl.org.y;
    int w = tl.size.x, h = tl.size.y;
    if (filt == 0) {
        int ix = wrapi(int(floor(x)), w, tl.wrap.x, tl.wrap.z, tl.size.z, tl.cl.x);
        int iy = wrapi(int(floor(y)), h, tl.wrap.y, tl.wrap.w, tl.size.w, tl.cl.y);
        return fetch(k, ivec2(ix, iy));
    }
    float fx = x - floor(x), fy = y - floor(y);
    int x0 = int(floor(x)), y0 = int(floor(y));
    int xa = wrapi(x0, w, tl.wrap.x, tl.wrap.z, tl.size.z, tl.cl.x), xb = wrapi(x0 + 1, w, tl.wrap.x, tl.wrap.z, tl.size.z, tl.cl.x);
    int ya = wrapi(y0, h, tl.wrap.y, tl.wrap.w, tl.size.w, tl.cl.y), yb = wrapi(y0 + 1, h, tl.wrap.y, tl.wrap.w, tl.size.w, tl.cl.y);
    vec4 p00 = fetch(k, ivec2(xa, ya)), p10 = fetch(k, ivec2(xb, ya)), p01 = fetch(k, ivec2(xa, yb)), p11 = fetch(k, ivec2(xb, yb));
    return fx + fy < 1.0 ? p00 + fx * (p10 - p00) + fy * (p01 - p00)
                         : p11 + (1.0 - fx) * (p01 - p11) + (1.0 - fy) * (p10 - p11);
}

vec4 inp(int c, vec4 comb, vec4 t0, vec4 t1, vec4 sh, float lfrac)
{
    switch (c) {
    case 0: return comb;
    case 1: return t0;
    case 2: return t1;
    case 3: return u.prim;
    case 4: return sh;
    case 5: return u.env;
    case 6: return vec4(1.0);
    case 8: return vec4(0.5);
    case 11: return vec4(comb.a);
    case 12: return vec4(t0.a);
    case 13: return vec4(t1.a);
    case 14: return vec4(u.prim.a);
    case 15: return vec4(sh.a);
    case 16: return vec4(u.env.a);
    case 17: return vec4(lfrac);
    case 18: return vec4(u.plod);
    default: return vec4(0.0);
    }
}

void main()
{
    vec4 t0 = vec4(0.0), t1 = vec4(0.0), comb = vec4(0.0);
    float lfrac = 0.0;
    if (u.lodn > 0) {
        vec2 dx = dFdx(v_st), dy = dFdy(v_st);
        float lod = max(max(abs(dx.x), abs(dx.y)), max(abs(dy.x), abs(dy.y))) * u.scale;
        int level = 0;
        if (lod >= 1.0) {
            level = min(int(floor(log2(lod))), 7);
            lfrac = min(lod / exp2(float(level)) - 1.0, 1.0);
        }
        int a = min(level, u.lodn - 1), b = min(level + 1, u.lodn - 1);
        if (level >= u.lodn - 1)
            lfrac = 1.0;
        t0 = texel(2 + a, v_st, u.filt);
        t1 = texel(2 + b, v_st, u.filt);
    } else {
        if ((u.ntex & 1) != 0)
            t0 = texel(0, v_st, u.filt);
        if ((u.ntex & 2) != 0)
            t1 = texel(1, v_st, u.filt);
    }
    for (int c = 0; c < u.cycle; c++) {
        ivec4 r = u.cc[c * 2], al = u.cc[c * 2 + 1];
        vec3 rgb = (inp(r.x, comb, t0, t1, v_rgba, lfrac).rgb - inp(r.y, comb, t0, t1, v_rgba, lfrac).rgb) *
                   inp(r.z, comb, t0, t1, v_rgba, lfrac).rgb + inp(r.w, comb, t0, t1, v_rgba, lfrac).rgb;
        float alpha = (inp(al.x, comb, t0, t1, v_rgba, lfrac).a - inp(al.y, comb, t0, t1, v_rgba, lfrac).a) *
                      inp(al.z, comb, t0, t1, v_rgba, lfrac).a + inp(al.w, comb, t0, t1, v_rgba, lfrac).a;
        comb = clamp(vec4(rgb, alpha), 0.0, 1.0);
    }
    if (u.alpha_cmp == 1 && comb.a < u.blendc.a) discard;
    if (u.alpha_cmp == 2 && comb.a < noise8(floor(gl_FragCoord.xy / u.scale), u.seed)) discard;
    if (u.alpha_cmp == 3 && comb.a < 0.5) discard;
    if (u.alpha_cmp == 4 && comb.a < 1.0 / 255.0) discard;
    if (u.fog_blend != 0)
        comb.rgb = mix(comb.rgb, u.fog.rgb, v_rgba.a);
    if (u.balpha == 1)
        comb.a = u.fog.a;
    else if (u.balpha == 2)
        comb.a = v_rgba.a;
    o_col = comb;
}
