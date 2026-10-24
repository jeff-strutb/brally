/* brr_metal_refl.h: specular reflections and wet roads (DESIGN.md 8.4).
 *
 *   BRR_WET_SRC   the G-buffer's material byte (car paint) and wet surfaces (needs LU): weather 2 (rain) and 3 (snow
 *                 melt) wet what faces up (n.z > 0.9), the road; wetness
 *                 darkens a porous surface's albedo (Lagarde 2013, "Water
 *                 drop 3b", the porosity model) and smooths it, and a
 *                 world-space noise mask lays puddles, smoother still.
 *                 radiance() calls wet() first, so every light sees it.
 *   BRR_REFL_SRC  the reflection pass (needs LU, BRR_WET_SRC, BRR_ENV_SRC and the probes' lookup, BRR_GI_SAMPLE_SRC):
 *                 a compute kernel over the lit target, after the deferred
 *                 lighting.  One GGX sample a pixel (the normals visible
 *                 from the eye, Heitz 2018); the ray marched in screen space
 *                 against the G-buffer's positions, one step every
 *                 REFL_STRIDE pixels (a step landing behind the depth found
 *                 there refined by halving, then accepted if close enough) (at most REFL_STEPS, the stride growing
 *                 past that so a ray's steps follow its length on screen)
 *                 then six halvings; its confidence fades at the screen's
 *                 edges and to nothing on a miss.  Traced at half resolution
 *                 in each axis (reflk, one pixel of each 2 x 2 and only below
 *                 REFL_TRACE_MAX), the reflected light into a half-size
 *                 target with a validity in alpha, then brought up by a
 *                 depth and normal aware (bilateral) filter as it is added
 *                 to the lit target (reflc).  Rougher than REFL_TRACE_MAX
 *                 (to REFL_ROUGH_MAX), or with no valid sample on its own
 *                 surface, a pixel keeps the sky's specular the lighting gave it.  What the march does not
 *                 find comes from the local cube: the track drawn around the
 *                 eye each frame, lit, over the sky (brr_metal_rcube.h;
 *                 Forza Horizon's dynamic cube; no rays: 2026-10-14, the
 *                 track's BVH traced here before), or before the track has
 *                 one, the skymap's specular cube (brr_metal_env.h) times
 *                 the baked probes' irradiance about the ray over the open
 *                 sky's, per channel, at most 1.  The split-sum
 *                 (Karis 2013): the reflected light times the lobe's
 *                 directional albedo E(n.v, roughness), the same table the
 *                 diffuse coupling reads (brdf_lut; F0 is 0.04 everywhere,
 *                 so E is the DFG term's F0 A + B).  The deferred pass lit
 *                 every surface with the sky's prefiltered specular already:
 *                 where the trace found something, the composite puts what
 *                 it found in that term's place.
 *
 * LU.wet: x the weather's wetness, y its puddles, z the tier gate (0 off,
 * else on), w unused. */
#ifndef BRR_METAL_REFL_H
#define BRR_METAL_REFL_H

#define REFL_ROUGH_MAX 0.6f      /* above this nothing traced: the lighting takes the probes (brr_metal_lit.h BRR_GISPEC_R0) */
#define REFL_TRACE_MAX 0.45f     /* above this the lobe is too wide to show what a march finds: the sky's specular only */
#define REFL_STRIDE 2.0f         /* the march's step in full-resolution pixels (one half-resolution pixel) */
#define REFL_STEPS 32            /* the march's steps at most; a longer ray strides further */
#define BRR_LAMP_CDM2 10000.0f   /* a lamp texture's luminance at a full texel, cd/m2 (BRR_LAMP_L) */

#define BRR_WET_SRC \
/* alb.a, 8 bits, the pixel's material: below 64 no 3D (the deferred pass \
   and the reflections skip it); 64..80 a lamp (BRR_MAT_LAMP); 88..104 the 3D's car paint (BRR_MAT_CAR, \
   112 and 116 a car's tail and reversing lens, BRR_MAT_LENS; 84 a car's tyre, BRR_MAT_RUBBER; \
   no glow; the capture's car flag, lit 3, br_gfx.c); 128..255 the 3D's \
   other surfaces, 128 + 127 x their glow */ \
"#define BRR_MAT_CAR 96.0\n" \
"static bool carpaint(float a) { a *= 255.0; return a > 87.5 && a < 104.5; }\n" \
/* 72: a lamp (texmat's lamp textures: start lights, their flares), which \
   emits BRR_LAMP_L cd/m2 at a full texel (LED signal heads: 5,000 to 15,000) */ \
"#define BRR_MAT_LAMP 72.0\n" \
/* 112, 116, 120: a car's tail, reversing and headlamp lens as its texture paints it (carlamp.csv) */ \
"#define BRR_MAT_LENS 112.0\n" \
"static int lensmat(float a) { a = a * 255.0 - BRR_MAT_LENS; return a > -1.5 && a < 1.5 ? 1 : a > 2.5 && a < 5.5 ? 2 : a > 6.5 && a < 9.5 ? 3 : 0; }\n" \
"#define BRR_LAMP_L " REFL_STR(BRR_LAMP_CDM2) "\n" \
"static bool lampmat(float a) { a *= 255.0; return a > 63.5 && a < 80.5; }\n" \
/* 84: a car's tyre (texmat's rubber, on a car: BRR_MAT_RUBBER), matte rubber, no clear coat \
   (its roughness BRR_RUBBER_ROUGH: a tyre's sidewall and tread, Lagarde's chart) */ \
"#define BRR_MAT_RUBBER 84.0\n" \
"#define BRR_RUBBER_ROUGH 0.7\n" \
"static bool rubbermat(float a) { a *= 255.0; return a > 82.5 && a < 85.5; }\n" \
/* car paint's two roughnesses: its clear coat's and the paint's under it \
   (brr_metal_lit.h: brdf) */ \
"#define BRR_COAT_ROUGH 0.05\n" \
"#define BRR_CAR_ROUGH 0.35\n" \
"static float wet_h(float2 c) { return fract(sin(dot(c, float2(127.1, 311.7))) * 43758.5453); }\n" \
/* value noise, smooth between the lattice's corners: 0..1 */ \
"static float wet_n(float2 x) { float2 i = floor(x), f = x - i, w = f * f * (3.0 - 2.0 * f);\n" \
"  return mix(mix(wet_h(i), wet_h(i + float2(1, 0)), w.x), mix(wet_h(i + float2(0, 1)), wet_h(i + float2(1, 1)), w.x), w.y); }\n" \
/* 124: a sealed surface (asphalt, concrete, metal: texmat, BRR_MAT_SEALED), where rain lies as a \
   film and pools; other ground is porous and soaks it up */ \
"#define BRR_MAT_SEALED 124.0\n" \
"static bool sealedmat(float a) { a = a * 255.0; return a > 121.5 && a < 126.5; }\n" \
/* puddles: two octaves of noise over metres, the top part of the mask, \
   scaled by the weather's puddles */ \
"static float puddle(constant LU &u, float3 p) {\n" \
"  float m = 0.65 * wet_n(p.xy * 0.25) + 0.35 * wet_n(p.xy * 0.9 + 17.0);\n" \
"  return smoothstep(0.58, 0.68, m) * u.wet.y; }\n" \
/* a surface's wetness 0..1 and its puddle 0..1 (up-facing only; puddles on sealed ground only) */ \
"static float2 wetness(constant LU &u, float3 p, float3 n, float seal) {\n" \
"  if (u.wet.x <= 0.0 || n.z <= 0.9) return 0.0;\n" \
"  float k = saturate((n.z - 0.9) * 20.0), pd = seal > 0.5 ? puddle(u, p) * k : 0.0;\n" \
"  return float2(max(u.wet.x * k, pd), pd); }\n" \
/* the rain on a surface (Lagarde 2013, the wet surfaces of Remember Me): porous ground (soil, \
   sand, grass, rock) soaks it up, darkening (its albedo to WET_POROUS_DARK) with little gloss \
   (roughness to WET_POROUS_ROUGH at most); sealed ground holds a film (darkening little, \
   roughness to WET_FILM_ROUGH: wet asphalt's blurred sheen, Forza Horizon 6's rain, not a \
   mirror) and, in its low spots, puddles (roughness WET_PUDDLE_ROUGH); never rougher */ \
"#define WET_POROUS_DARK 0.55\n#define WET_POROUS_ROUGH 0.6\n#define WET_FILM_DARK 0.85\n#define WET_FILM_ROUGH 0.35\n#define WET_PUDDLE_ROUGH 0.05\n" \
"static void wet(constant LU &u, float3 p, float3 n, thread float3 &alb, thread float &rough, float seal) {\n" \
"  float2 w = wetness(u, p, n, seal); if (w.x <= 0.0) return;\n" \
"  if (seal > 0.5) { alb *= mix(1.0, WET_FILM_DARK, w.x); rough = min(rough, mix(mix(rough, WET_FILM_ROUGH, w.x), WET_PUDDLE_ROUGH, w.y)); }\n" \
"  else { alb *= mix(1.0, WET_POROUS_DARK, w.x); rough = min(rough, mix(rough, WET_POROUS_ROUGH, w.x)); } }\n"

#define REFL_STR_(x) #x
#define REFL_STR(x) REFL_STR_(x)
#define BRR_REFL_SRC \
"#define REFL_ROUGH_MAX " REFL_STR(REFL_ROUGH_MAX) "\n" \
"#define REFL_TRACE_MAX " REFL_STR(REFL_TRACE_MAX) "\n" \
"#define REFL_STRIDE " REFL_STR(REFL_STRIDE) "\n" \
"#define REFL_STEPS " REFL_STR(REFL_STEPS) "\n" \
"static float rf_vd(constant LU &u, float3 q) { return dot(q - u.eye.xyz, u.cfwd.xyz); }\n" \
/* the screen-space march: p (on the surface) along unit r, at most maxd \
   metres (cut at 0.1 m in front of the eye); jit 0..1 the first step's \
   share.  Returns the hit's pixel (xy), its confidence (z: the screen \
   edge's fade; 0 a miss) and the ray's length to it (w) */ \
"static float4 ssr(constant LU &u, texture2d<float> pos, float3 p, float3 r, float maxd, float jit) {\n" \
"  float2 wh = float2(pos.get_width(), pos.get_height());\n" \
"  float dp = rf_vd(u, p), dr = dot(r, u.cfwd.xyz), len = maxd;\n" \
"  if (dp + dr * len < 0.1) len = (0.1 - dp) / dr;\n" \
"  if (len <= 0.0) return 0.0;\n" \
"  float3 e = p + r * len; float4 h0 = u.scr * float4(p, 1.0), h1 = u.scr * float4(e, 1.0);\n" \
"  if (h0.w <= 1e-6 || h1.w <= 1e-6) return 0.0;\n" \
"  float k0 = 1.0 / h0.w, k1 = 1.0 / h1.w; float2 s0 = h0.xy * k0, s1 = h1.xy * k1;\n" \
"  int n = int(clamp(ceil(length(s1 - s0) / REFL_STRIDE), 1.0, float(REFL_STEPS)));\n" \
"  float lp = 0.0, dqp = dp;\n" \
"  for (int i = 0; i < n; i++) {\n" \
"    float l = (float(i) + jit) / float(n); float2 s = mix(s0, s1, l);\n" \
"    if (any(s < 0.0) || any(s >= wh)) return 0.0;\n" \
"    float3 q = mix(p * k0, e * k1, l) / mix(k0, k1, l); float dq = rf_vd(u, q);\n" \
"    float3 P = pos.read(uint2(s)).xyz;\n" \
"    if (!all(P == 0.0)) {\n" \
"      float ds = rf_vd(u, P), th = 0.05 + 0.01 * dq + 2.0 * abs(dq - dqp);\n" \
"      if (dq > ds && length(P - p) > 0.05) {\n" \
"        float lo = lp, hi = l;\n" \
"        for (int b = 0; b < 6; b++) { float m = 0.5 * (lo + hi); float2 sm = mix(s0, s1, m);\n" \
"          float3 qm = mix(p * k0, e * k1, m) / mix(k0, k1, m); float3 Pm = pos.read(uint2(sm)).xyz;\n" \
"          if (!all(Pm == 0.0) && rf_vd(u, qm) > rf_vd(u, Pm)) hi = m; else lo = m; }\n" \
"        float2 sh = mix(s0, s1, hi), ed = min(sh, wh - sh) / (0.08 * wh.y);\n" \
"        float3 qh = mix(p * k0, e * k1, hi) / mix(k0, k1, hi), Ph = pos.read(uint2(sh)).xyz; float dh = rf_vd(u, qh) - rf_vd(u, Ph);\n" \
/* a step that crossed a silhouette lands too far behind to trust: the \
   refined point, within the thickness of the surface found there, is a hit */ \
"        if (dq - ds < th || (!all(Ph == 0.0) && dh >= 0.0 && dh < 0.05 + 0.01 * rf_vd(u, qh)))\n" \
"          return float4(sh, saturate(min(ed.x, ed.y)), length(qh - p));\n" \
"      }\n" \
"    }\n" \
"    lp = l; dqp = dq;\n" \
"  }\n" \
"  return 0.0;\n" \
"}\n" \
/* one GGX normal visible from v (Heitz 2018) for the point (x1, x2) of the \
   unit square, around n */ \
"static float3 rf_vndf(float3 n, float3 v, float a, float2 x) {\n" \
"  float3 t = abs(n.z) < 0.999 ? normalize(cross(float3(0, 0, 1), n)) : float3(1, 0, 0), b = cross(n, t);\n" \
"  float3 vl = float3(dot(v, t), dot(v, b), dot(v, n)), vh = normalize(float3(a * vl.x, a * vl.y, vl.z));\n" \
"  float lsq = vh.x * vh.x + vh.y * vh.y; float3 T1 = lsq > 0.0 ? float3(-vh.y, vh.x, 0) * rsqrt(lsq) : float3(1, 0, 0), T2 = cross(vh, T1);\n" \
"  float r = sqrt(x.x), ph = 6.2831853 * x.y, p1 = r * cos(ph), p2 = r * sin(ph), s = 0.5 * (1.0 + vh.z);\n" \
"  p2 = (1.0 - s) * sqrt(max(1.0 - p1 * p1, 0.0)) + s * p2;\n" \
"  float3 nh = p1 * T1 + p2 * T2 + sqrt(max(1.0 - p1 * p1 - p2 * p2, 0.0)) * vh;\n" \
"  float3 hl = normalize(float3(a * nh.x, a * nh.y, max(nh.z, 0.0)));\n" \
"  return hl.x * t + hl.y * b + hl.z * n; }\n" \
"static float2 rf_svuv(float3 d) { float lat = asin(clamp(d.z, -1.0, 1.0)), lon = atan2(d.y, d.x);\n" \
"  return float2(lon / 3.14159 * 0.5 + 0.5, 0.5 + 0.5 * sign(lat) * sqrt(abs(lat) / 1.5708)); }\n" \
/* the sky's specular along r at roughness rough (the skymap's cube, \
   brr_metal_env.h), white-balanced and pre-exposed */ \
"static float3 rf_env(constant LU &u, texturecube<float> sky, float3 r, float rough) {\n" \
"  return min(env_spec(u, sky, r, rough) * u.wb.rgb * u.misc.x, float3(60000.0)); }\n" \
/* a pixel's surface for the reflections: position, normal toward the eye, \
   roughness after the wet and the car's coat; false where nothing reflects */ \
"static bool rf_surf(constant LU &u, texture2d<float> alb, texture2d<float> pos, texture2d<float> nrm, uint2 px,\n" \
"                    thread float3 &p, thread float3 &n, thread float &rough, thread float &fog) {\n" \
"  float4 a = alb.read(px); if (a.a < 0.25 || u.wet.z < 0.5) return false;\n" \
"  float4 N = nrm.read(px); p = pos.read(px).xyz; n = N.xyz; fog = u.vol.x > 0.5 ? 0.0 : N.w;\n" \
"  if (dot(n, u.eye.xyz - p) < 0.0) n = -n;\n" \
"  float3 al = a.rgb; bool car = carpaint(a.a); rough = car ? BRR_CAR_ROUGH : rubbermat(a.a) ? BRR_RUBBER_ROUGH : u.misc.z; wet(u, p, n, al, rough, car || sealedmat(a.a) ? 1.0 : 0.0);\n" \
"  if (car) rough = BRR_COAT_ROUGH;   /* car paint: the coat's mirror, its own roughness, weighted by its Fresnel (E) */\n" \
"  return rough <= REFL_ROUGH_MAX; }\n" \
/* the frame's reprojection (brr_metal_refl.m): the last frame's scr, the \
   frame's number (the samples' rotation) and whether the last frame's \
   history and lit target are this frame's size (1) or not (0) */ \
"struct RP { float4x4 pscr; uint frame; float valid; float pad0; float pad1; };\n" \
"#define REFL_MIRROR 0.1\n" \
"#define REFL_HIST 16.0\n" \
/* the local cube looked up along a ray from p (brr_metal_rcube.h: the cube \
   about eye e, each texel's distance from e in alpha): 24 steps from 0.2 \
   to 300 m spaced by a constant ratio; the first that lies behind what the \
   cube holds in its direction, after one in front, is refined by the secant \
   of the two (Szirmay-Kalos et al. 2005, distance impostors) and that point's \
   texel taken; none behind, the cube in the ray's direction (the sky's, or \
   what is farther than 300 m) */ \
"static float3 rc_trace(texturecube<float> cube, float3 e, float3 p, float3 r) {\n" \
"  constexpr sampler cs(filter::linear);\n" \
"  float tp = 0.0, gp = -1.0;\n" \
"  for (int i = 0; i < 24; i++) {\n" \
"    float t = 0.2 * pow(1500.0, float(i) / 23.0); float3 q = p + r * t, dq = q - e; float lq = length(dq);\n" \
"    float g = cube.sample(cs, dq / lq).a - lq;\n" \
"    if (g <= 0.0 && gp > 0.0) { float tt = tp + (t - tp) * gp / (gp - g); return cube.sample(cs, normalize(p + r * tt - e)).rgb; }\n" \
"    tp = t; gp = g; }\n" \
"  return cube.sample(cs, r).rgb; }\n" \
/* the half-resolution trace: px a pixel of the half-size target hr, the \
   full-resolution pixel 2 px its sample; rgb the reflected light (before \
   the lobe's albedo), a 0 where not traced (nothing reflects, or too rough \
   to trace), else 1 + the ray's length to what it found (1e4 the sky).  One \
   GGX sample a pixel, rotated each frame (the temporal pass averages them); \
   below REFL_MIRROR the mirror direction alone.  What the march misses: the \
   local cube (texture 10, brr_metal_rcube.h: the track drawn around the eye \
   this frame, rp.pad1 1); without it the \
   sky's cube in the ray's direction times the probes' irradiance about it \
   (gi_irr2, from this pixel's probes, buffers 12, 17, 18 and texture 9) over \
   the open sky's (env_irr), per channel, clamped to 1: a wall or a bank on \
   that side darkens and tints it */ \
"kernel void reflk(uint2 px [[thread_position_in_grid]], constant LU &u [[buffer(3)]], constant RP &rp [[buffer(4)]],\n" \
"                  texture2d<float> alb [[texture(0)]], texture2d<float> pos [[texture(1)]], texture2d<float> nrm [[texture(2)]],\n" \
"                  texture2d<float> src [[texture(3)]], texture2d<float, access::write> hr [[texture(4)]],\n" \
"                  texturecube<float> sky [[texture(5)]],\n" \
"                  constant GiU &gu [[buffer(12)]], device const float4 *gsh [[buffer(18)]], texture2d<float> gdst [[texture(9)]],\n" \
"                  device const float4 *goff [[buffer(17)]], texturecube<float> rcube [[texture(10)]]) {\n" \
"  if (px.x >= hr.get_width() || px.y >= hr.get_height()) return;\n" \
"  float2 wh = float2(src.get_width(), src.get_height());\n" \
"  uint2 fp = min(px * 2, uint2(wh) - 1);\n" \
"  float3 p, n; float rough, fog;\n" \
"  if (!rf_surf(u, alb, pos, nrm, fp, p, n, rough, fog) || rough > REFL_TRACE_MAX) { hr.write(0.0, px); return; }\n" \
"  float3 v = normalize(u.eye.xyz - p);\n" \
"  float j = fract(52.9829189 * fract(dot(float2(px) + 0.5, float2(0.06711056, 0.00583715))) + 0.618034 * float(rp.frame & 1023u));\n" \
"  float ar = max(rough * rough, 0.0447);\n" \
"  float3 r = reflect(-v, n);\n" \
"  if (rough >= REFL_MIRROR) { float3 h = rf_vndf(n, v, ar, float2(fract(j + 0.5), fract(j * 7.31 + 0.25 + 0.7548777 * float(rp.frame & 1023u))));\n" \
"    float3 rh = reflect(-v, h); if (dot(rh, n) > 0.0) r = rh; }\n" \
"  float jm = rough < REFL_MIRROR ? 0.5 : j;\n" \
"  float4 s = ssr(u, pos, p, r, 80.0, jm);\n" \
"  float3 Lss = s.z > 0.0 ? src.read(uint2(s.xy)).rgb : float3(0), Lfb; float hd = s.z > 0.0 ? s.w : 1e4;\n" \
"  constexpr sampler cs(filter::linear);\n" \
"  if (rp.pad1 > 0.5) Lfb = rc_trace(rcube, u.eye.xyz, p + n * 0.02, r);   /* the local cube (brr_metal_rcube.h) */\n" \
"  else { Lfb = rf_env(u, sky, r, 0.0);\n" \
"    if (gu.n.x > 0) { float3 e2; gi_irr2(gu, goff, gsh, gdst, p, n, v, r, e2); Lfb *= clamp(e2 / max(env_irr(u, r), float3(1e-6)), 0.0, 1.0); } }\n" \
"  hr.write(float4(min(mix(Lfb, Lss, s.z), float3(60000.0)), 1.0 + min(hd, 1e4)), px);\n" \
"}\n" \
/* the temporal accumulation, at half resolution: cur reflk's target, hs the \
   last frame's history, ho this frame's.  The history read where this \
   pixel's reflection was a frame ago: for a mirror the reflected point (the \
   ray's length behind the surface along the view), for a rough surface the \
   surface itself, between the two by roughness; clamped to the box of the \
   3 x 3 neighbourhood's new samples (no ghosts), then averaged in with \
   weight 1 / n, n the history's count (a) up to REFL_HIST */ \
"static float3 rf_hist(float3 cur, float3 h, float3 lo, float3 hi, thread float &cnt) {\n" \
"  cnt = min(cnt + 1.0, REFL_HIST); return mix(clamp(h, lo, hi), cur, 1.0 / cnt); }\n" \
"kernel void reflt(uint2 px [[thread_position_in_grid]], constant LU &u [[buffer(3)]], constant RP &rp [[buffer(4)]],\n" \
"                  texture2d<float> alb [[texture(0)]], texture2d<float> pos [[texture(1)]], texture2d<float> nrm [[texture(2)]],\n" \
"                  texture2d<float> cur [[texture(3)]], texture2d<float> hs [[texture(4)]], texture2d<float, access::write> ho [[texture(5)]]) {\n" \
"  int2 hw = int2(cur.get_width(), cur.get_height());\n" \
"  if (int(px.x) >= hw.x || int(px.y) >= hw.y) return;\n" \
"  float4 c = cur.read(px); if (c.a <= 0.0) { ho.write(0.0, px); return; }\n" \
"  float3 lo = c.rgb, hi = c.rgb;\n" \
"  for (int k = 0; k < 9; k++) { int2 q = clamp(int2(px) + int2(k % 3 - 1, k / 3 - 1), int2(0), hw - 1); float4 C = cur.read(uint2(q));\n" \
"    if (C.a > 0.0) { lo = min(lo, C.rgb); hi = max(hi, C.rgb); } }\n" \
"  uint2 fp = min(px * 2, uint2(pos.get_width(), pos.get_height()) - 1); float3 p, n; float rough, fog, cnt = 0.0; float3 L = c.rgb;\n" \
"  if (rp.valid > 0.5 && rf_surf(u, alb, pos, nrm, fp, p, n, rough, fog)) {\n" \
"    float3 dv = normalize(p - u.eye.xyz), q = mix(p + dv * min(c.a - 1.0, 1000.0), p, saturate((rough - REFL_MIRROR) / 0.3));\n" \
"    float4 hc = rp.pscr * float4(q, 1.0);\n" \
"    if (hc.w > 1e-6) { float2 hp = hc.xy / hc.w * 0.5;\n" \
"      if (all(hp >= 0.0) && all(hp < float2(hw))) { float4 H = hs.read(uint2(hp));\n" \
"        if (H.a > 0.0) { cnt = H.a; L = rf_hist(c.rgb, H.rgb, lo, hi, cnt); } } } }\n" \
"  ho.write(float4(L, max(cnt, 1.0)), px);\n" \
"}\n" \
/* the bilateral upsample and spatial filter: the half-resolution samples \
   around full pixel px (its sample pixels 2 h): within rad half-resolution \
   pixels (rad <= 0.5: the four around it, bilinear; else a Gaussian of sigma \
   rad / 1.6, the filter for rough lobes), weights times their validity, their \
   nearness to px's tangent plane (1 cm per metre of depth, plus 2 cm) and \
   their normals' agreement (cos^8); rgb the weighted light, a the weights' \
   sum (0: no sample on this surface) */ \
"static float rf_w(constant LU &u, texture2d<float> pos, texture2d<float> nrm, int2 h, int2 fw, float3 p, float3 n, float tol) {\n" \
"  uint2 rp = uint2(min(h * 2, fw - 1)); float3 P = pos.read(rp).xyz, N = nrm.read(rp).xyz; if (dot(N, u.eye.xyz - P) < 0.0) N = -N;\n" \
"  return saturate(1.0 - abs(dot(P - p, n)) / tol) * pow(saturate(dot(N, n)), 8.0); }\n" \
"static float4 rf_up(constant LU &u, texture2d<float> hr, texture2d<float> pos, texture2d<float> nrm, uint2 px, float3 p, float3 n, float rad) {\n" \
"  int2 hw = int2(hr.get_width(), hr.get_height()), fw = int2(pos.get_width(), pos.get_height());\n" \
"  float2 hc = float2(px) * 0.5, b = floor(hc), f = hc - b; float tol = 0.01 * rf_vd(u, p) + 0.02;\n" \
"  float4 acc = 0.0;\n" \
"  if (rad <= 0.5) {\n" \
"    for (int k = 0; k < 4; k++) {\n" \
"      int2 o = int2(k & 1, k >> 1), h = clamp(int2(b) + o, int2(0), hw - 1);\n" \
"      float4 L = hr.read(uint2(h)); if (L.a <= 0.0) continue;\n" \
"      float wb = (o.x ? f.x : 1.0 - f.x) * (o.y ? f.y : 1.0 - f.y) + 1e-4;\n" \
"      float w = wb * rf_w(u, pos, nrm, h, fw, p, n, tol); acc += float4(L.rgb * w, w);\n" \
"    }\n" \
"  } else {\n" \
"    int R = int(ceil(rad)); float is = 1.28 / (rad * rad);\n" \
"    for (int dy = -R; dy <= R; dy++) for (int dx = -R; dx <= R; dx++) {\n" \
"      int2 h = int2(b) + int2(dx, dy); if (any(h < 0) || any(h >= hw)) continue;\n" \
"      float4 L = hr.read(uint2(h)); if (L.a <= 0.0) continue;\n" \
"      float2 d = float2(h) - hc; float w = exp(-dot(d, d) * is) * rf_w(u, pos, nrm, h, fw, p, n, tol); acc += float4(L.rgb * w, w);\n" \
"    }\n" \
"  }\n" \
"  return acc.a > 1e-5 ? float4(acc.rgb / acc.a, acc.a) : float4(0.0); }\n" \
/* the spatial filter's radius in half-resolution pixels: none below \
   REFL_MIRROR, growing to 4 at REFL_TRACE_MAX */ \
"static float rf_rad(float rough) { return rough < REFL_MIRROR ? 0.0 : 1.0 + 3.0 * saturate((rough - REFL_MIRROR) / (REFL_TRACE_MAX - REFL_MIRROR)); }\n" \
/* the composite: src the lit target as the deferred pass left it \
   (pre-exposed, the sky's prefiltered specular in it), dst the same with \
   the traced reflection in that specular's place where the trace found \
   something; hr reflk's target; sky the specular cube; bE the lobe's \
   albedo table.  The split-sum: the reflected light times E(n.v, roughness) */ \
"kernel void reflc(uint2 px [[thread_position_in_grid]], constant LU &u [[buffer(3)]],\n" \
"                  texture2d<float> alb [[texture(0)]], texture2d<float> pos [[texture(1)]], texture2d<float> nrm [[texture(2)]],\n" \
"                  texture2d<float> src [[texture(3)]], texture2d<float, access::write> dst [[texture(4)]],\n" \
"                  texturecube<float> sky [[texture(5)]], texture2d<float> bE [[texture(6)]], texture2d<float> hr [[texture(7)]],\n" \
"                  texturecube<float> rcube [[texture(10)]]) {\n" \
"  if (px.x >= dst.get_width() || px.y >= dst.get_height()) return;\n" \
"  float4 c = src.read(px); float3 p, n; float rough, fog;\n" \
"  if (int(u.misc.w) == 17) { float3 P = pos.read(px).xyz; constexpr sampler cs(filter::linear);   /* BR_LIGHT_VIEW=17: the local cube toward each pixel's point */\n" \
"    dst.write(float4(all(P == 0.0) ? c.rgb : rcube.sample(cs, normalize(P - u.eye.xyz)).rgb, c.a), px); return; }\n" \
"  if (!rf_surf(u, alb, pos, nrm, px, p, n, rough, fog)) { dst.write(c, px); return; }\n" \
"  float3 v = normalize(u.eye.xyz - p); float nv = clamp(dot(n, v), 1e-3, 1.0);\n" \
"  float4 h = rough <= REFL_TRACE_MAX ? rf_up(u, hr, pos, nrm, px, p, n, rf_rad(rough)) : float4(0.0);\n" \
"  if (int(u.misc.w) == 16) { dst.write(float4(h.a > 0.0 ? h.rgb : float3(0.0), c.a), px); return; }   /* BR_LIGHT_VIEW=16: the reflected light alone */\n" \
"  if (!(h.a > 0.0)) { dst.write(c, px); return; }\n" \
"  float3 L = h.rgb - rf_env(u, sky, reflect(-v, n), rough);\n" \
"  constexpr sampler ls(filter::linear, address::clamp_to_edge);\n" \
"  float2 sz = float2(bE.get_width(), bE.get_height()), tc = (0.5 + float2(sqrt(nv), saturate((rough - 0.0447) / 0.9553)) * (sz - 1.0)) / sz;\n" \
"  float E = bE.sample(ls, tc).r;\n" \
"  dst.write(float4(clamp(c.rgb + L * E * (1.0 - fog), float3(0.0), float3(60000.0)), c.a), px);\n" \
"}\n"


/* BR_LIGHT_SUNCHECK (diagnostic, with BR_LIGHT_VIEW=3): each sun-facing pixel's
 * sun ray traced through the track (the static casters' own triangles) against
 * the sun's visibility the deferred pass wrote (src.r: the shadow map's, or
 * the cascades' with BR_LIGHT_VSM=0); by view depth band (cnt[8 b], bands
 * ending 10, 25, 55, 110, 200, 300 m and beyond): pixels, traced in shadow,
 * shadowed by the trace but lit by the map (missing), lit by the trace but
 * shadowed by the map (extra: the cars' shadows, which the trace does not
 * hold, and acne), and the missing by how far along the ray the trace hit
 * (within 5 cm, 25 cm, 2 m, farther).  The ray leaves the surface by 2 cm and
 * 1e-4 of its depth (less hits its own surface at a distance).  The frame
 * shows the map's visibility in grey, missing red, extra cyan. */
#define BRR_SUNCHECK_SRC \
"kernel void svck(uint2 px [[thread_position_in_grid]], constant LU &u [[buffer(3)]],\n" \
"                 device const RtNode *node [[buffer(0)]], device const float *tri [[buffer(1)]], device const uint *id [[buffer(2)]],\n" \
"                 texture2d<float> pos [[texture(1)]], texture2d<float> nrm [[texture(2)]], texture2d<float> src [[texture(3)]],\n" \
"                 texture2d<float, access::write> dst [[texture(4)]], device atomic_uint *cnt [[buffer(13)]]) {\n" \
"  if (px.x >= dst.get_width() || px.y >= dst.get_height()) return;\n" \
"  float4 c = src.read(px); float3 p = pos.read(px).xyz, n = nrm.read(px).xyz;\n" \
"  if (all(p == 0.0)) { dst.write(c, px); return; }\n" \
"  if (dot(n, u.eye.xyz - p) < 0.0) n = -n;\n" \
"  if (dot(n, u.sun.xyz) <= 0.05) { dst.write(c, px); return; }\n" \
"  float d = dot(p - u.eye.xyz, u.cfwd.xyz); uint b = d < 10.0 ? 0u : d < 25.0 ? 1u : d < 55.0 ? 2u : d < 110.0 ? 3u : d < 200.0 ? 4u : d < 300.0 ? 5u : 6u;\n" \
"  RtHit h = RT_CLOSEST(p + n * (0.02 + 1e-4 * d), u.sun.xyz, 0.0, 4000.0); bool sh = h.tri >= 0; float sv = c.r;\n" \
"  device atomic_uint *q = cnt + 8u * b;\n" \
"  atomic_fetch_add_explicit(&q[0], 1u, memory_order_relaxed);\n" \
"  if (sh) atomic_fetch_add_explicit(&q[1], 1u, memory_order_relaxed);\n" \
"  if (!sh && sv < 0.25) { atomic_fetch_add_explicit(&q[3], 1u, memory_order_relaxed); dst.write(float4(0, 1, 1, 1), px); return; }\n" \
"  if (!(sh && sv > 0.75)) { dst.write(c, px); return; }\n" \
"  atomic_fetch_add_explicit(&q[2], 1u, memory_order_relaxed);\n" \
"  atomic_fetch_add_explicit(&q[h.t < 0.05 ? 4u : h.t < 0.25 ? 5u : h.t < 2.0 ? 6u : 7u], 1u, memory_order_relaxed);\n" \
"  dst.write(float4(1, 0, 0, 1), px); }\n"

#endif
