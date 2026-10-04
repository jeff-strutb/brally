#version 450
// Glide's combine unit, alpha test and fog: the same equations as the
// software renderer (brr_soft.c) and the Metal shader (brr_metal.m).
#extension GL_GOOGLE_include_directive : require
#include "glide_u.glsl"
layout(set = 1, binding = 0) uniform texture2D t_tex;
layout(set = 2, binding = 0) uniform sampler t_smp;
layout(location = 0) noperspective in vec4 v_col;
layout(location = 1) in vec2 v_st;
layout(location = 2) noperspective in float v_oow;
layout(location = 3) noperspective in float v_z;
layout(location = 0) out vec4 o_col;

float comb(int fn, float f, float l, float la, float ot, int inv) {
    float r;
    switch (fn) {
    case 0x0: r = 0.0; break;
    case 0x1: r = l; break;
    case 0x2: r = la; break;
    case 0x3: r = f * ot; break;
    case 0x4: r = f * ot + l; break;
    case 0x5: r = f * ot + la; break;
    case 0x6: r = f * (ot - l); break;
    case 0x7: r = f * (ot - l) + l; break;
    case 0x8: r = f * (ot - l) + la; break;
    case 0x9: r = -f * l + l; break;
    case 0x10: r = -f * l + la; break;
    default: r = l; break;
    }
    r = clamp(r, 0.0, 1.0);
    return inv != 0 ? 1.0 - r : r;
}

float fac(int k, float l, float la, float oa, float ta) {
    switch (k) {
    case 0: return 0.0;
    case 1: return l;
    case 2: return oa;
    case 3: return la;
    case 4: return ta;
    case 5: return 0.0;                    // LOD fraction: one level
    case 8: return 1.0;
    case 9: return 1.0 - l;
    case 10: return 1.0 - oa;
    case 11: return 1.0 - la;
    case 12: return 1.0 - ta;
    case 13: return 1.0;
    default: return 0.0;
    }
}

bool cmpf(int fn, float a, float b) {
    switch (fn) {
    case 0: return false;
    case 1: return a < b;
    case 2: return a == b;
    case 3: return a <= b;
    case 4: return a > b;
    case 5: return a != b;
    case 6: return a >= b;
    default: return true;
    }
}

float fogv(int i) { return u.fog[i >> 2][i & 3]; }
// table entry i stands for w = 2^(3 + i/4) / (8 - i%4)
float fogw(int i) { return pow(2.0, 3.0 + float(i >> 2)) / float(8 - (i & 3)); }
float fogt(float w) {
    if (w <= fogw(0))
        return fogv(0);
    for (int i = 0; i < 63; i++) {
        float w0 = fogw(i), w1 = fogw(i + 1);
        if (w < w1) {
            float k = (w - w0) / (w1 - w0);
            return fogv(i) * (1.0 - k) + fogv(i + 1) * k;
        }
    }
    return fogv(63);
}

// The Voodoo's 16-bit W-buffer word: 4-bit exponent, 12-bit mantissa of 1/w
// as a .32 fraction. Depth is stored and compared at this precision, which
// is what lets a second pass over the same polygon (the car shadow is drawn
// with grDepthBufferFunction(EQUAL)) hit every pixel the first one wrote;
// the Z-buffer keeps ooz's 16 integer bits.
uint wfloat(float oow) {
    if (oow >= 1.0)
        return 0u;
    if (oow <= 0.0)
        return 0xFFFFu;
    uint t = uint(min(oow * 4294967296.0, 4294967040.0));
    if (t == 0u)
        return 0xFFFFu;
    int e = 31 - findMSB(t);
    uint m = e <= 19 ? (~t >> uint(19 - e)) : (~t << uint(e - 19));
    uint w = (uint(e) << 12) | (m & 0xFFFu);
    return w < 0xFFFFu ? w + 1u : w;
}

void main() {
    vec4 tex = vec4(0.0);
    if (u.has_tex != 0) {
        vec4 tx = texture(sampler2D(t_tex, t_smp), v_st);
        tex.r = comb(u.tc_rgb_fn, fac(u.tc_rgb_factor, tx.r, tx.a, 0.0, tx.a), tx.r, tx.a, 0.0, u.tc_rgb_invert);
        tex.g = comb(u.tc_rgb_fn, fac(u.tc_rgb_factor, tx.g, tx.a, 0.0, tx.a), tx.g, tx.a, 0.0, u.tc_rgb_invert);
        tex.b = comb(u.tc_rgb_fn, fac(u.tc_rgb_factor, tx.b, tx.a, 0.0, tx.a), tx.b, tx.a, 0.0, u.tc_rgb_invert);
        tex.a = comb(u.tc_alpha_fn, fac(u.tc_alpha_factor, tx.a, tx.a, 0.0, tx.a), tx.a, tx.a, 0.0, u.tc_alpha_invert);
    }
    vec4 it = v_col, k = u.konst;
    vec4 cl = u.cc_local == 1 ? k : it;
    vec4 co = u.cc_other == 1 ? tex : (u.cc_other == 2 ? k : it);
    float al = u.ac_local == 1 ? k.a : it.a;
    float ao = u.ac_other == 1 ? tex.a : (u.ac_other == 2 ? k.a : it.a);
    vec4 o;
    o.a = comb(u.ac_fn, fac(u.ac_factor, al, al, ao, tex.a), al, al, ao, u.ac_invert);
    o.r = comb(u.cc_fn, fac(u.cc_factor, cl.r, al, co.a, tex.a), cl.r, al, co.r, u.cc_invert);
    o.g = comb(u.cc_fn, fac(u.cc_factor, cl.g, al, co.a, tex.a), cl.g, al, co.g, u.cc_invert);
    o.b = comb(u.cc_fn, fac(u.cc_factor, cl.b, al, co.a, tex.a), cl.b, al, co.b, u.cc_invert);
    if (u.atest_fn != 7 && !cmpf(u.atest_fn, floor(o.a * 255.0 + 0.5), u.atest_ref))
        discard;
    float f = 0.0;
    if (u.fog_mode == 1)
        f = it.a;
    else if (u.fog_mode == 2)
        f = fogt(v_oow > 0.0 ? 1.0 / v_oow : 65536.0);
    else if (u.fog_mode == 3)
        f = clamp(v_z, 0.0, 1.0);
    if (f > 0.0)
        o.rgb += (u.fog_color.rgb - o.rgb) * f;
    o_col = o;
    gl_FragDepth = (u.depth_mode == 2 || u.depth_mode == 4) ? float(wfloat(v_oow)) / 65536.0
                                     : floor(clamp(v_z * 65535.0, 0.0, 65535.0)) / 65536.0;
}
