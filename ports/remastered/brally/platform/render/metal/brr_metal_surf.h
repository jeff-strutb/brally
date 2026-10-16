/* brr_metal_surf.h: the Remastered textures' shading on Metal (BR_FLAG_TEXTURES;
 * docs/brally/remaster/textures/DESIGN.md), line for line the reference
 * light_surf_shade.c (render/vulkan/shaders/surf.glsl is the same again).
 * Part of the library before the game's own fragment stage: under the
 * Remastered lighting the G-buffer pass takes its albedo and relief (fsgb),
 * under the game's own the combiners read it as their texel (fs).
 *
 * A draw whose texture has a surface map (light_surf.h) reads it at the
 * texel: the two regions there and where the texel sits between them.  Each
 * region is drawn in its photographed sets laid over the world at their real
 * size (1 unit is about a metre): from above on ground, from the side on
 * walls, both blended on slopes (weights |n|^4, a pass skipped under 2%).
 * Every projection is hex-tiled (Mikkelsen 2022, "Practical Real-Time
 * Hex-Tiling", JCGT 11(3)): three samples on a triangle grid of about one
 * tile, each moved (and on natural ground turned) at random, their weights
 * sharpened to the 8th power, so the 2 m sets never show their repeat.
 * Gradients go with each sample (turned with it) so mipmapping and 16x
 * anisotropic filtering work through the turn.
 *
 * Variety: a surface's two or three variants share the ground in patches
 * tens of metres across (surfset), and all natural ground is lighter and
 * darker by 8% over some 120 m, so no road or field is one material end to
 * end.  The noise is an integer hash: the same patches on every GPU.
 *
 * Albedo: the region's colour times the set's detail times the region's own
 * light and dark from the game texture (var).  The border: crisp under any
 * magnification (the map's t by its screen derivative), on natural ground
 * moved by the two sets' heights.  The relief: each projection's normal,
 * turned with its sample, added to the shaded normal (UDN), shortened by mip
 * averaging with distance; under the game's own lighting, a light from above
 * on it, as the 1999 art paints its light. */
#ifndef BRR_METAL_SURF_H
#define BRR_METAL_SURF_H

#define BRR_SURF_SRC \
"struct SurfU { float4 sa[8]; float4 sb[8]; float4 sc[8]; float4 sv[8]; float4 sk[8]; float4 flags; float4 misc; };\n" \
"constexpr sampler s_surfp(filter::nearest, address::repeat);\n" \
"constexpr sampler s_surfl(filter::linear, address::repeat);\n" \
"constexpr sampler s_surfa(filter::linear, mip_filter::linear, address::repeat, max_anisotropy(16));\n" \
"static uint surfhash(int x, int y) { uint h = uint(x) * 0x8da6b343u ^ uint(y) * 0xd8163841u; h ^= h >> 13; h *= 0x2c1b3c6du; h ^= h >> 12;\n" \
"  h *= 0x297a2d39u; h ^= h >> 15; return h; }\n" \
"static float2 surfh2(int2 c) { uint h = surfhash(c.x, c.y); return float2(float(h & 0xFFFFu), float(h >> 16)) / 65536.0; }\n" \
"static float vhash(int2 c) { return float(surfhash(c.x, c.y) & 0xFFFFFFu) / 16777216.0; }\n" \
"static float vnoise(float2 q) { float2 fl = floor(q), f = q - fl; int2 i = int2(fl); f = f * f * (3.0 - 2.0 * f);\n" \
"  return mix(mix(vhash(i), vhash(i + int2(1, 0)), f.x), mix(vhash(i + int2(0, 1)), vhash(i + int2(1, 1)), f.x), f.y); }\n" \
"static float vfbm(float2 q) { return 0.5 * vnoise(q) + 0.3 * vnoise(q * 2.03 + 5.1) + 0.2 * vnoise(q * 4.11 + 9.7); }\n" \
"static void trigrid(float2 uv, thread float3 &w, thread int2 &v1, thread int2 &v2, thread int2 &v3) {\n" \
"  float2 sk = float2(uv.x - 0.57735027 * uv.y, 1.15470054 * uv.y) * 2.0;\n" \
"  int2 b = int2(floor(sk)); float2 f = sk - floor(sk); float z = 1.0 - f.x - f.y;\n" \
"  if (z > 0.0) { w = float3(z, f.y, f.x); v1 = b; v2 = b + int2(0, 1); v3 = b + int2(1, 0); }\n" \
"  else { w = float3(-z, 1.0 - f.y, 1.0 - f.x); v1 = b + int2(1, 1); v2 = b + int2(1, 0); v3 = b + int2(0, 1); }\n" \
"}\n" \
"struct SurfS { float3 alb; float h; float2 n; };\n" \
"static SurfS hexsample(texture2d_array<float> ta, texture2d_array<float> tb, uint slice, float2 uv, float2 dx, float2 dy, float rot) {\n" \
"  float3 w; int2 v[3]; trigrid(uv, w, v[0], v[1], v[2]);\n" \
"  w = pow(w, float3(8.0)); w /= (w.x + w.y + w.z);\n" \
"  SurfS o; o.alb = float3(0); o.h = 0; o.n = float2(0);\n" \
"  for (int i = 0; i < 3; i++) {\n" \
"    if (w[i] < 0.02) continue;\n" \
"    float2 r = surfh2(v[i]); float a = rot * r.x * 6.2831853; float c = cos(a), s = sin(a);\n" \
"    float2x2 R = float2x2(float2(c, s), float2(-s, c));\n" \
"    float2 q = R * uv + r * 7.31;\n" \
"    gradient2d g(R * dx, R * dy);\n" \
"    float4 A = ta.sample(s_surfa, q, slice, g); float2 B = tb.sample(s_surfa, q, slice, g).xy;\n" \
"    float2 nt = float2(B.x * 2.0 - 1.0, 1.0 - B.y * 2.0); nt = transpose(R) * nt;   /* OpenGL's green points up the image; back to the projection's axes */\n" \
"    o.alb += w[i] * A.rgb * 2.0; o.h += w[i] * A.a; o.n += w[i] * nt;\n" \
"  }\n" \
"  return o;\n" \
"}\n" \
"struct SurfR { float3 alb; float h; float3 dn; };\n" \
"static SurfR surfone(texture2d_array<float> ta, texture2d_array<float> tb, uint slice, float k, float4 sa, float4 sc, float3 p, float3 n, float3 dp0, float3 dp1) {\n" \
"  SurfR o;\n" \
"  float3 an = abs(n); float3 bw = an * an * an * an; bw /= (bw.x + bw.y + bw.z);\n" \
"  float3 alb = float3(0); float h = 0; float3 dn = float3(0); float tw = 0;\n" \
"  float3 dpx = dp0 * k, dpy = dp1 * k;\n" \
"  if (bw.z > 0.02) {   /* from above (Z up): ground */\n" \
"    float sg = n.z >= 0.0 ? 1.0 : -1.0;\n" \
"    SurfS s = hexsample(ta, tb, slice, float2(p.x * k, p.y * sg * k), float2(dpx.x, dpx.y * sg), float2(dpy.x, dpy.y * sg), sc.x);\n" \
"    alb += bw.z * s.alb; h += bw.z * s.h; dn += bw.z * float3(s.n.x, sg * s.n.y, 0); tw += bw.z;\n" \
"  }\n" \
"  if (bw.x > 0.02) {   /* from the side, along x */\n" \
"    float sg = n.x >= 0.0 ? 1.0 : -1.0;\n" \
"    SurfS s = hexsample(ta, tb, slice, float2(p.y * sg * k, p.z * k), float2(dpx.y * sg, dpx.z), float2(dpy.y * sg, dpy.z), sc.x);\n" \
"    alb += bw.x * s.alb; h += bw.x * s.h; dn += bw.x * float3(0, sg * s.n.x, s.n.y); tw += bw.x;\n" \
"  }\n" \
"  if (bw.y > 0.02) {   /* from the side, along y */\n" \
"    float sg = n.y >= 0.0 ? 1.0 : -1.0;\n" \
"    SurfS s = hexsample(ta, tb, slice, float2(-p.x * sg * k, p.z * k), float2(-dpx.x * sg, dpx.z), float2(-dpy.x * sg, dpy.z), sc.x);\n" \
"    alb += bw.y * s.alb; h += bw.y * s.h; dn += bw.y * float3(-sg * s.n.x, 0, s.n.y); tw += bw.y;\n" \
"  }\n" \
"  o.alb = 1.0 + (alb / tw - 1.0) * sa.z; o.h = h / tw; o.dn = dn / tw * sa.w;\n" \
"  return o;\n" \
"}\n" \
/* a region's surface: its variants share the ground in patches of 1 / sv.w metres from a noise \
   field of their own (the surface's seed); where two meet, the one whose height stands higher \
   takes it (a band of 0.1 of the field, moved by the heights' difference times 0.35) */ \
/* water's moving ripples: four deep-water waves (wavelengths 0.9 to 4.3 m, each at the speed its \
   length gives, w = sqrt(g k)), their slope at p and time tm */ \
"static float2 surfripple(float2 p, float tm) {\n" \
"  float2 sl = float2(0); float a[4] = { 0.3, 1.9, 3.4, 5.0 }, lw[4] = { 0.9, 1.6, 2.7, 4.3 }, am[4] = { 0.012, 0.02, 0.03, 0.045 };\n" \
"  for (int i = 0; i < 4; i++) { float2 d = float2(cos(a[i]), sin(a[i])); float k = 6.2831853 / lw[i], w = sqrt(9.81 * k);\n" \
"    sl += am[i] * k * d * cos(k * dot(d, p) - w * tm); }\n" \
"  return sl;\n" \
"}\n" \
"static SurfR surfset(texture2d_array<float> ta, texture2d_array<float> tb, float4 sa, float4 sv, float4 sk, float4 sc,\n" \
"                     float3 p, float3 n, float3 dp0, float3 dp1, float4 misc, float2 st, float2 ds0, float2 ds1) {\n" \
"  SurfR o; o.alb = float3(1); o.h = 0.5; o.dn = float3(0);\n" \
"  if (sa.x == -3.0) { if (abs(n.z) > 0.6) o.dn = float3(-surfripple(p.xy, misc.y), 0.0) * sa.w; return o; }   /* water, flat: its ripples */\n" \
"  if (sa.x < 0.0) return o;\n" \
"  if (misc.z > 0.0) {   /* in the texture's own space (a foliage card) */\n" \
"    SurfS s = hexsample(ta, tb, uint(sa.x + 0.5), st * misc.z, ds0 * misc.z, ds1 * misc.z, sc.x);\n" \
"    o.alb = 1.0 + (s.alb - 1.0) * sa.z; o.h = s.h; return o; }\n" \
"  int nv = int(sv.z + 0.5);\n" \
"  if (nv < 2) return surfone(ta, tb, uint(sa.x + 0.5), sa.y, sa, sc, p, n, dp0, dp1);\n" \
"  uint sl[3] = { uint(sa.x + 0.5), uint(max(sv.x, 0.0) + 0.5), uint(max(sv.y, 0.0) + 0.5) };\n" \
"  float kk[3] = { sa.y, sk.x, sk.y }, br[3] = { 1.0, sk.z, sk.w };\n" \
"  float u = vfbm(p.xy * sv.w + sc.w * 37.0);\n" \
"  float f = saturate((u - 0.5) * 2.6 + 0.5) * float(nv - 1);\n" \
"  int i = min(int(f), nv - 2); float t = f - float(i), w = 0.1, hk = 0.35;\n" \
"  if (t < 0.5 - w - hk || t > 0.5 + w + hk) { int j = t < 0.5 ? i : i + 1; o = surfone(ta, tb, sl[j], kk[j], sa, sc, p, n, dp0, dp1); o.alb *= br[j]; return o; }\n" \
"  SurfR A = surfone(ta, tb, sl[i], kk[i], sa, sc, p, n, dp0, dp1), B = surfone(ta, tb, sl[i + 1], kk[i + 1], sa, sc, p, n, dp0, dp1);\n" \
"  float wb = smoothstep(0.5 - w, 0.5 + w, t + (B.h - A.h) * hk);\n" \
"  o.alb = mix(A.alb * br[i], B.alb * br[i + 1], wb); o.h = mix(A.h, B.h, wb); o.dn = mix(A.dn, B.dn, wb);\n" \
"  return o;\n" \
"}\n" \
"static float surflin(float c) { return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }\n" \
"static float surfsrgb(float c) { c = saturate(c); return c <= 0.0031308 ? 12.92 * c : 1.055 * pow(c, 1.0 / 2.4) - 0.055; }\n" \
/* the draw's colour and relief where its texture has a surface map: ref the game's texel (for a \
   region of none: the lighting's albedo of it, or the texel itself), vc the vertex colour; dn the \
   relief (world).  flags.z 0: albedo, linear; 1: the texel, sRGB */ \
"struct SurfO { float3 alb; float3 dn; float a; };   /* a: a cutout's alpha (foliage), else -1 */\n" \
"static SurfO surfshade(constant SurfU &su, texture2d<float> mp, texture2d<float> vr, texture2d_array<float> ta, texture2d_array<float> tb,\n" \
"                       float2 st, float3 p, float3 n, float3 ref, float3 vc) {\n" \
"  float4 P = mp.sample(s_surfp, st), L = mp.sample(s_surfl, st); float2 V = vr.sample(s_surfl, st).rg + 0.5;\n" \
"  int lo = int(P.r * 255.0 + 0.5), hi = int(P.g * 255.0 + 0.5); float t = L.b;\n" \
"  float3 dp0 = dfdx(p), dp1 = dfdy(p); float ft = fwidth(t), fa = fwidth(L.a);   /* where every pixel of the quad runs */\n" \
"  float2 ds0 = dfdx(st), ds1 = dfdy(st);\n" \
"  bool texel = su.flags.z > 0.5;\n" \
"  float3 orig = texel ? float3(surflin(ref.r), surflin(ref.g), surflin(ref.b)) : ref, hue = float3(1);\n" \
"  if (su.flags.y > 0.5 && !texel) { float lv = dot(vc, float3(0.2126, 0.7152, 0.0722)); float3 hh = lv > 0.02 ? clamp(vc / lv, 0.0, 2.0) : float3(1);\n" \
"    hh = saturate(hh * 0.5); hue = float3(surflin(hh.r), surflin(hh.g), surflin(hh.b)) * 4.672; }\n" \
"  SurfR a = surfset(ta, tb, su.sa[lo], su.sv[lo], su.sk[lo], su.sc[lo], p, n, dp0, dp1, su.misc, st, ds0, ds1);\n" \
"  bool lay = su.flags.y > 0.5;   /* a layout (a wall): its own texels' colour, every window and door as painted; the material's grain over it */\n" \
"  float3 ca = su.sa[lo].x == -1.0 ? orig : lay ? orig * a.alb : su.sb[lo].rgb * (1.0 + (V.x - 1.0) * su.sb[lo].w) * a.alb * hue;\n" \
"  SurfO o; o.alb = ca; o.dn = a.dn;\n" \
"  o.a = su.misc.x > 0.5 ? smoothstep(0.5 - max(fa * 0.75, 0.02), 0.5 + max(fa * 0.75, 0.02), L.a) : -1.0;   /* a crisp silhouette */\n" \
"  if (hi != lo) {\n" \
"    float soft = max(su.sc[lo].y, su.sc[hi].y), hk = max(su.sc[lo].z, su.sc[hi].z);\n" \
"    float fw = max(ft * 0.75, max(soft, 1e-3));\n" \
"    if (t > 0.5 - fw - hk) {   /* near a border (a height can move it by hk) */\n" \
"      SurfR b = surfset(ta, tb, su.sa[hi], su.sv[hi], su.sk[hi], su.sc[hi], p, n, dp0, dp1, su.misc, st, ds0, ds1);\n" \
"      float wb = smoothstep(0.5 - fw, 0.5 + fw, t + (b.h - a.h) * hk);\n" \
"      float3 cb = su.sa[hi].x == -1.0 ? orig : lay ? orig * b.alb : su.sb[hi].rgb * (1.0 + (V.y - 1.0) * su.sb[hi].w) * b.alb * hue;\n" \
"      o.alb = mix(ca, cb, wb); o.dn = mix(a.dn, b.dn, wb);\n" \
"    }\n" \
"  }\n" \
"  if (su.flags.y < 0.5) o.alb *= 0.92 + 0.16 * vfbm(p.xy * (1.0 / 120.0) + 3.1);   /* the ground's broad light and dark */\n" \
"  if (su.flags.w == 1.0) o.alb = 0.18 * a.alb;   /* BR_SURF_VIEW=1: the detail alone */\n" \
"  if (su.flags.w == 2.0) { float id = su.sc[(hi != lo && t > 0.5) ? hi : lo].w; float hh = fract(id * 0.618034);   /* 2: the surface's own colour */\n" \
"    o.alb = 0.5 * saturate(abs(fract(hh + float3(0.0, 0.6667, 0.3333)) * 6.0 - 3.0) - 1.0); }\n" \
"  if (texel) {   /* the game's own lighting: the relief lit from above and a little to one side, then sRGB */\n" \
"    float sh = clamp(1.0 + 0.8 * dot(o.dn, float3(0.42, 0.33, 0.84)), 0.6, 1.4);\n" \
"    float3 c = su.flags.w > 0.5 ? o.alb : o.alb * sh; o.alb = float3(surfsrgb(c.r), surfsrgb(c.g), surfsrgb(c.b));\n" \
"  } else o.alb = min(o.alb, float3(0.9));\n" \
"  return o;\n" \
"}\n"

#endif
