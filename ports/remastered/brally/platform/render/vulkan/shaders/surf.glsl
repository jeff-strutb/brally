// surf.glsl: the Remastered textures' shading on Vulkan (BR_FLAG_TEXTURES;
// docs/brally/remaster/textures/DESIGN.md), line for line the reference
// light_surf_shade.c and the Metal shader (render/metal/brr_metal_surf.h): the
// texel the combiners read, from the surfaces, where a draw's texture has a
// surface map. Included by glide.frag.

layout(set = 4, binding = 0) uniform texture2D s_map;        // the regions (RGBA8)
layout(set = 4, binding = 1) uniform texture2D s_var;        // their own light and dark (RG8)
layout(set = 4, binding = 2) uniform texture2DArray s_seta;  // the sets' albedo detail and height
layout(set = 4, binding = 3) uniform texture2DArray s_setb;  // their normals' x and y
layout(set = 4, binding = 4) uniform sampler s_surfp;        // nearest, repeat
layout(set = 4, binding = 5) uniform sampler s_surfl;        // linear, repeat
layout(set = 4, binding = 6) uniform sampler s_surfa;        // trilinear, 16x anisotropic, repeat

uint surfhash(int x, int y) {
    uint h = uint(x) * 0x8da6b343u ^ uint(y) * 0xd8163841u;
    h ^= h >> 13; h *= 0x2c1b3c6du; h ^= h >> 12; h *= 0x297a2d39u; h ^= h >> 15;
    return h;
}
vec2 surfh2(ivec2 c) { uint h = surfhash(c.x, c.y); return vec2(float(h & 0xFFFFu), float(h >> 16)) / 65536.0; }
float vhash(ivec2 c) { return float(surfhash(c.x, c.y) & 0xFFFFFFu) / 16777216.0; }
float vnoise(vec2 q) {
    vec2 fl = floor(q), f = q - fl; ivec2 i = ivec2(fl); f = f * f * (3.0 - 2.0 * f);
    return mix(mix(vhash(i), vhash(i + ivec2(1, 0)), f.x), mix(vhash(i + ivec2(0, 1)), vhash(i + ivec2(1, 1)), f.x), f.y);
}
float vfbm(vec2 q) { return 0.5 * vnoise(q) + 0.3 * vnoise(q * 2.03 + 5.1) + 0.2 * vnoise(q * 4.11 + 9.7); }
void trigrid(vec2 uv, out vec3 w, out ivec2 v1, out ivec2 v2, out ivec2 v3) {
    vec2 sk = vec2(uv.x - 0.57735027 * uv.y, 1.15470054 * uv.y) * 2.0;
    ivec2 b = ivec2(floor(sk)); vec2 f = sk - floor(sk); float z = 1.0 - f.x - f.y;
    if (z > 0.0) { w = vec3(z, f.y, f.x); v1 = b; v2 = b + ivec2(0, 1); v3 = b + ivec2(1, 0); }
    else { w = vec3(-z, 1.0 - f.y, 1.0 - f.x); v1 = b + ivec2(1, 1); v2 = b + ivec2(1, 0); v3 = b + ivec2(0, 1); }
}
struct SurfS { vec3 alb; float h; vec2 n; };
SurfS hexsample(int slice, vec2 uv, vec2 dx, vec2 dy, float rot) {
    vec3 w; ivec2 v[3]; trigrid(uv, w, v[0], v[1], v[2]);
    w = pow(w, vec3(8.0)); w /= (w.x + w.y + w.z);
    SurfS o; o.alb = vec3(0.0); o.h = 0.0; o.n = vec2(0.0);
    for (int i = 0; i < 3; i++) {
        if (w[i] < 0.02) continue;
        vec2 r = surfh2(v[i]); float a = rot * r.x * 6.2831853; float c = cos(a), s = sin(a);
        mat2 R = mat2(vec2(c, s), vec2(-s, c));
        vec2 q = R * uv + r * 7.31;
        vec4 A = textureGrad(sampler2DArray(s_seta, s_surfa), vec3(q, float(slice)), R * dx, R * dy);
        vec2 B = textureGrad(sampler2DArray(s_setb, s_surfa), vec3(q, float(slice)), R * dx, R * dy).xy;
        vec2 nt = vec2(B.x * 2.0 - 1.0, 1.0 - B.y * 2.0); nt = transpose(R) * nt;
        o.alb += w[i] * A.rgb * 2.0; o.h += w[i] * A.a; o.n += w[i] * nt;
    }
    return o;
}
struct SurfR { vec3 alb; float h; vec3 dn; };
SurfR surfone(int slice, float k, vec4 sa, vec4 sc, vec3 p, vec3 n, vec3 dp0, vec3 dp1) {
    SurfR o;
    vec3 an = abs(n); vec3 bw = an * an * an * an; bw /= (bw.x + bw.y + bw.z);
    vec3 alb = vec3(0.0); float h = 0.0; vec3 dn = vec3(0.0); float tw = 0.0;
    vec3 dpx = dp0 * k, dpy = dp1 * k;
    if (bw.z > 0.02) {
        float sg = n.z >= 0.0 ? 1.0 : -1.0;
        SurfS s = hexsample(slice, vec2(p.x * k, p.y * sg * k), vec2(dpx.x, dpx.y * sg), vec2(dpy.x, dpy.y * sg), sc.x);
        alb += bw.z * s.alb; h += bw.z * s.h; dn += bw.z * vec3(s.n.x, sg * s.n.y, 0.0); tw += bw.z;
    }
    if (bw.x > 0.02) {
        float sg = n.x >= 0.0 ? 1.0 : -1.0;
        SurfS s = hexsample(slice, vec2(p.y * sg * k, p.z * k), vec2(dpx.y * sg, dpx.z), vec2(dpy.y * sg, dpy.z), sc.x);
        alb += bw.x * s.alb; h += bw.x * s.h; dn += bw.x * vec3(0.0, sg * s.n.x, s.n.y); tw += bw.x;
    }
    if (bw.y > 0.02) {
        float sg = n.y >= 0.0 ? 1.0 : -1.0;
        SurfS s = hexsample(slice, vec2(-p.x * sg * k, p.z * k), vec2(-dpx.x * sg, dpx.z), vec2(-dpy.x * sg, dpy.z), sc.x);
        alb += bw.y * s.alb; h += bw.y * s.h; dn += bw.y * vec3(-sg * s.n.x, 0.0, s.n.y); tw += bw.y;
    }
    o.alb = 1.0 + (alb / tw - 1.0) * sa.z; o.h = h / tw; o.dn = dn / tw * sa.w;
    return o;
}
SurfR surfset(vec4 sa, vec4 sv, vec4 sk, vec4 sc, vec3 p, vec3 n, vec3 dp0, vec3 dp1) {
    SurfR o; o.alb = vec3(1.0); o.h = 0.5; o.dn = vec3(0.0);
    if (sa.x < 0.0) return o;
    int nv = int(sv.z + 0.5);
    if (nv < 2) return surfone(int(sa.x + 0.5), sa.y, sa, sc, p, n, dp0, dp1);
    int sl[3] = int[3](int(sa.x + 0.5), int(max(sv.x, 0.0) + 0.5), int(max(sv.y, 0.0) + 0.5));
    float kk[3] = float[3](sa.y, sk.x, sk.y), br[3] = float[3](1.0, sk.z, sk.w);
    float u = vfbm(p.xy * sv.w + sc.w * 37.0);
    float f = clamp((u - 0.5) * 2.6 + 0.5, 0.0, 1.0) * float(nv - 1);
    int i = min(int(f), nv - 2); float t = f - float(i), w = 0.1, hk = 0.35;
    if (t < 0.5 - w - hk || t > 0.5 + w + hk) { int j = t < 0.5 ? i : i + 1; o = surfone(sl[j], kk[j], sa, sc, p, n, dp0, dp1); o.alb *= br[j]; return o; }
    SurfR A = surfone(sl[i], kk[i], sa, sc, p, n, dp0, dp1), B = surfone(sl[i + 1], kk[i + 1], sa, sc, p, n, dp0, dp1);
    float wb = smoothstep(0.5 - w, 0.5 + w, t + (B.h - A.h) * hk);
    o.alb = mix(A.alb * br[i], B.alb * br[i + 1], wb); o.h = mix(A.h, B.h, wb); o.dn = mix(A.dn, B.dn, wb);
    return o;
}
float surflin(float c) { return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }
float surfsrgb(float c) { c = clamp(c, 0.0, 1.0); return c <= 0.0031308 ? 12.92 * c : 1.055 * pow(c, 1.0 / 2.4) - 0.055; }
// the texel (sRGB) where the draw's texture has a surface map (u.sflags.x; z is 1: the game's own lighting)
vec3 surfshade(vec2 st, vec3 p, vec3 n, vec3 ref, vec3 vc) {
    vec4 P = texture(sampler2D(s_map, s_surfp), st), L = texture(sampler2D(s_map, s_surfl), st);
    vec2 V = texture(sampler2D(s_var, s_surfl), st).rg + 0.5;
    int lo = int(P.r * 255.0 + 0.5), hi = int(P.g * 255.0 + 0.5); float t = L.b;
    vec3 dp0 = dFdx(p), dp1 = dFdy(p); float ft = fwidth(t);
    vec3 orig = vec3(surflin(ref.r), surflin(ref.g), surflin(ref.b));
    SurfR a = surfset(u.sa[lo], u.sv[lo], u.sk[lo], u.sc[lo], p, n, dp0, dp1);
    vec3 ca = u.sa[lo].x == -1.0 ? orig : u.sb[lo].rgb * (1.0 + (V.x - 1.0) * u.sb[lo].w) * a.alb;
    vec3 alb = ca, dn = a.dn;
    if (hi != lo) {
        float soft = max(u.sc[lo].y, u.sc[hi].y), hk = max(u.sc[lo].z, u.sc[hi].z);
        float fw = max(ft * 0.75, max(soft, 1e-3));
        if (t > 0.5 - fw - hk) {
            SurfR b = surfset(u.sa[hi], u.sv[hi], u.sk[hi], u.sc[hi], p, n, dp0, dp1);
            float wb = smoothstep(0.5 - fw, 0.5 + fw, t + (b.h - a.h) * hk);
            vec3 cb = u.sa[hi].x == -1.0 ? orig : u.sb[hi].rgb * (1.0 + (V.y - 1.0) * u.sb[hi].w) * b.alb;
            alb = mix(ca, cb, wb); dn = mix(a.dn, b.dn, wb);
        }
    }
    if (u.sflags.y < 0.5) alb *= 0.92 + 0.16 * vfbm(p.xy * (1.0 / 120.0) + 3.1);
    if (u.sflags.w == 1.0) alb = 0.18 * a.alb;
    if (u.sflags.w == 2.0) { float id = u.sc[(hi != lo && t > 0.5) ? hi : lo].w; float hh = fract(id * 0.618034);
        alb = 0.5 * clamp(abs(fract(hh + vec3(0.0, 0.6667, 0.3333)) * 6.0 - 3.0) - 1.0, 0.0, 1.0); }
    float sh = clamp(1.0 + 0.8 * dot(dn, vec3(0.42, 0.33, 0.84)), 0.6, 1.4);
    vec3 c = u.sflags.w > 0.5 ? alb : alb * sh;
    return vec3(surfsrgb(c.r), surfsrgb(c.g), surfsrgb(c.b));
}
