/* host_fx.m -- experimental modern lighting over the native renderer (port code).
 *
 * The game still draws every triangle exactly as before into the colour
 * target.  Alongside it, host_glide.m writes a G-buffer from the native 3D
 * leaves: the world-space position of every pixel (native/render.m rebuilds it
 * from the clip-space corners through the inverse of the game's view x
 * projection matrix) and the face normal.  At the swap this module relights the
 * finished picture:
 *
 *   sun shadows    the frame's opaque 3D triangles re-drawn from the sun into a
 *                  2048^2 shadow map (alpha-tested foliage included), 16-tap PCF
 *   AO + GI        world-space hemisphere samples tested against the G-buffer
 *                  (occlusion, and one bounce of the occluders' colour),
 *                  half resolution, bilateral blur
 *   reflections    screen-space ray march, Fresnel weighted, wet roads in rain
 *   sun specular   GGX, sharper when wet
 *   light shafts   radial scattering from the sky around the sun
 *   bloom          six-level down/up chain
 *   tonemap        ACES, exposure and grade per weather
 *
 * The weather the game is running (g_brCarPhysWeather 0x104B15E8: 0 sunny,
 * 1 fog, 2 storm, 3 snow, 4 rain) picks the light rig, and the storm's
 * lightning state (0x100A79CC) flashes the scene.  2D (menus, HUD) is left as
 * the game drew it.
 *
 * ~ (the key left of 1) switches live between Remastered (all of this) and
 * Original (the frame exactly as the game drew it), like Tab's PC / N64
 * switch; the game never sees the key, and the choice is remembered.
 * BR_FX=1 forces it on, BR_FX=0 off; default on in a window, off headless.
 */
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#include "host.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

CAMetalLayer *happ_metal_layer(void);
id<MTLTexture> hsky_tex(id<MTLDevice> dev, int weather);   /* host_sky.m */
static id<MTLTexture> g_skyt;   /* this frame's Remastered sky, or nil */
static id<MTLTexture> g_tsv, g_tgi;   /* this frame's landscape light maps (host_terrain.m), or nil */
void hglide_map(int view, float T[4]);                     /* host_glide.m */
float hglide_bake_ref(void);
double hframe_game_ms(void);                               /* native/frame.m */
void henv_shadow(id<MTLRenderCommandEncoder> e, const float *svp, const float *eye, float range);   /* host_env.m */
void hter_shadow(id<MTLRenderCommandEncoder> e, const float *svp, const float *eye, float range);  /* host_terrain.m */
int hter_ready(void);                                                                                /* host_terrain.m */
void hter_resolve(id<MTLCommandBuffer> cb, id<MTLTexture> gn, id<MTLTexture> ga, id<MTLTexture> gp, id<MTLTexture> gm); /* host_terrain.m */
int hter_light(float p[3][4], id<MTLTexture> *sv, id<MTLTexture> *gi, id<MTLTexture> *fh);           /* host_terrain.m */

#define STR(...) #__VA_ARGS__
static const char *FXSRC = "#include <metal_stdlib>\n" STR(
using namespace metal;
struct CV { float x, y, z, w, r, g, b, a, s, t, wx, wy, wz, fl, xf, nx, ny, nz; };
struct FXU {
  float4x4 vp, ivp, svp;
  float4 eye, sun, sunc, skyc, grnd, fogc, vpt, scr, p0, p1, p2, p3, p4, flash, wb, nsky, sk2, tm, wx;
  float4x4 pvp; float4 jit; float4x4 svp2; float4 csm; float4 mat; float4 matmean[8]; float4 rmap;
  float4 m3;   /* host_glide.m's screen map of the view: NDC = (x, y down) * m3.xz + m3.yw */
  float4 cmap; /* the forest's canopy grid (host_env.m): x0, y0, 1/width, 1/height (metres); z 0 none */
  float4 mk;   /* the tyre marks map (host_marks.m): x0, y0, texels a metre, texels a tile; z 0 none */
  float4 bnd;  /* the horizon band (host_sky.m): bottom row's elevation, its height (radians), copies round, haze; z 0 none */
  float4 ter[3]; /* the landscape's light maps (host_terrain.m): map x0 y0 texel side; far x0 y0 cell on (w 0 none); far w h */
  float4 tmo;    /* the tone curve: x 0 PBR Neutral, 1 filmic (Uchimura); y its contrast, z its black toe; w asphalt albedo */
  float4 air2;   /* x: the landscape's haze, per metre of air (scaled by fogc.w) */
};
/* headlights: up to 32 spot lights (two per car); misc = count, beam
   strength, light intensity, air density */
struct HL { float4 p[64]; float4 d[64]; float4 col; float4 misc;   /* p.w: 0 head, 1 tail; d.w: its level */
  float4 g[64]; float4 gd[64]; float4 gmisc; };   /* g: lamp position, w 0 head / 1 tail; gd: facing */
/* how much of headlight i reaches point x, and the direction to it */
/* a low-beam pattern, not a round spot: the hot spot just below the
   horizon, a sharp cutoff above it, little light straight down in front of
   the car, a wide spread to the sides; intensity falls with the square of
   the distance.  hl.d is the lamp's level forward direction. */
float spotw(constant HL &hl, int i, float3 x, thread float3 &Ld) {
  float3 Lv = hl.p[i].xyz - x; float d = length(Lv); Ld = Lv / max(d, 1e-3);
  float3 dir = -Ld;
  float below = -dir.z;                                  /* sine of the angle under the horizon */
  float2 fh = normalize(hl.d[i].xy + float2(1e-5, 0)), dh = normalize(dir.xy + float2(1e-5, 0));
  float ch = dot(fh, dh) * step(0.0, dot(dir, hl.d[i].xyz));
  /* the road just ahead of the bumper is lit too (the reflector's spill,
     at a steep angle and a metre or two away, so it is the brightest patch
     of all), and the pool fans out wide to both verges */
  float iv = smoothstep(-0.03, 0.0, below)                 /* the cutoff */
           * mix(0.08, 1.0, exp(-pow(max(below - 0.015, 0.0) / 0.05, 2.0)))
           /* above it a lamp still spills some light, falling off with the
              angle: the trees over the road catch it */
           + 0.12 * (1.0 - smoothstep(-0.03, 0.0, below)) * exp(-max(-below, 0.0) / 0.3);
  float ih = smoothstep(0.82, 0.97, ch) + 0.45 * smoothstep(0.2, 0.8, ch);
  /* a tail or brake lamp: a faint red wash behind the car, a few metres.
     Its lenses are a few candela against a headlamp's thousands, so on dry
     tarmac it barely shows; it eases in from the bumper rather than starting
     at a line, which drew a hard red rectangle on the road */
  if (hl.p[i].w > 0.5)
    return 0.06 * hl.d[i].w * smoothstep(-0.1, 0.8, dot(dir, hl.d[i].xyz)) / (d * d + 0.3) * smoothstep(6.0, 2.0, d);
  return 150.0 * hl.d[i].w * iv * ih / (d * d + 0.5) * smoothstep(200.0, 110.0, d); }
float3 spotc(constant HL &hl, int i) { return hl.p[i].w > 0.5 ? float3(1.0, 0.04, 0.015) : hl.col.rgb; }
struct MVC { float4 cur[16][4]; float4 prev[16][4]; float4 misc; };   /* rows 0-2 + position; box in misc */
struct SHU { int at_fn, at_ref, use_tex, pad; float su, sv, pad2, pad3; float4x4 svp; };
struct QO { float4 pos [[position]]; float2 uv; };
vertex QO fsq(uint vid [[vertex_id]]) {
  float2 p = float2((vid << 1) & 2, vid & 2); QO o;
  o.pos = float4(p * 2.0 - 1.0, 0, 1); o.uv = float2(p.x, 1.0 - p.y); return o; }
constexpr sampler ns(filter::nearest, address::clamp_to_edge);
constexpr sampler ls(filter::linear, address::clamp_to_edge);
float hash2(float2 p) { return fract(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
float vnoise(float2 p) { float2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
  return mix(mix(hash2(i), hash2(i + float2(1, 0)), f.x), mix(hash2(i + float2(0, 1)), hash2(i + float2(1, 1)), f.x), f.y); }
float fbm(float2 p) { return vnoise(p) * 0.55 + vnoise(p * 2.03 + 17.1) * 0.3 + vnoise(p * 4.1 + 5.3) * 0.15; }
/* the tyre marks map (host_marks.m) at a ground point: R its height (0.5
   untouched), G the mark's darkness.  Bilinear by hand: each of the four
   texels finds its own tile through the page grid, so tile edges are seamless;
   a page whose ground is at another height (a road over this one) is none */
float2 mark_at(texture2d<float> mkp, texture2d<float> mka, float4 mk, float2 p, float z) {
  float2 q = (p - mk.xy) * mk.z - 0.5, f = fract(q);
  int2 i0 = int2(floor(q)); int T = int(mk.w);
  float2 v[4];
  for (int k = 0; k < 4; k++) {
    int2 t = i0 + int2(k & 1, k >> 1);
    v[k] = float2(0.5, 0.0);
    if (t.x < 0 || t.y < 0) continue;
    uint2 pc = uint2(t / T);
    if (pc.x >= mkp.get_width() || pc.y >= mkp.get_height()) continue;
    float4 P = mkp.read(pc);
    if (P.w < 0.5 || abs(P.z - z) > 4.0) continue;
    v[k] = mka.read(uint2(uint(P.x) * uint(T) + uint(t.x % T), uint(P.y) * uint(T) + uint(t.y % T))).rg; }
  return mix(mix(v[0], v[1], f.x), mix(v[2], v[3], f.x), f.y); }
float ign(float2 p) { return fract(52.9829189 * fract(dot(p, float2(0.06711056, 0.00583715)))); }
/* the game's screen (640x480, its own y) to the target's uv, and back */
float2 scr_uv(constant FXU &u, float sx, float sy) {
  float yd = u.p3.x > 0.5 ? 480.0 - sy : sy;
  return float2(u.m3.x * sx + u.m3.y, u.m3.z * yd + u.m3.w) * float2(0.5, -0.5) + 0.5; }
float2 uv_scr(constant FXU &u, float2 uv) {
  float2 n = (uv - 0.5) * float2(2.0, -2.0);
  float sx = (n.x - u.m3.y) / u.m3.x, yd = (n.y - u.m3.w) / u.m3.z;
  return float2(sx, u.p3.x > 0.5 ? 480.0 - yd : yd); }
float2 to_uv(constant FXU &u, float3 wp, thread float &cw) {
  float4 c = u.vp * float4(wp, 1); cw = c.w;
  return scr_uv(u, u.vpt.x * c.x / c.w + u.vpt.y, u.vpt.z * c.y / c.w + u.vpt.w); }
float3 view_dir(constant FXU &u, float2 uv) {
  float2 sp = uv_scr(u, uv); float sx = sp.x, sy = sp.y;
  float4 h = u.ivp * float4((sx - u.vpt.y) / u.vpt.x, (sy - u.vpt.w) / u.vpt.z, 0.5, 1.0);
  float3 d = (h.xyz - u.eye.xyz * h.w) * (h.w < 0 ? -1.0 : 1.0);
  return length(d) > 1e-12 ? normalize(d) : float3(0, 0, 1); }
bool is_geo(float w) { return w > 0.5 && w < 2.5; }
/* the material ids of host_glide.m's texmat.csv */
constant int MAT_ASPHALT = 1, MAT_MARKING = 2, MAT_DIRT = 3, MAT_SAND = 4, MAT_GRASS = 5, MAT_ROCK = 6,
             MAT_SNOW = 7, MAT_ICE = 8, MAT_WATER = 9,
             MAT_TERRAIN = 10;   /* host_terrain.m's field: its own photographed ground, finished */
/* anything solid: lit geometry or the pre-lit Remastered car (class 4) */
bool is_solid(float w) { return (w > 0.5 && w < 2.5) || (w > 3.5 && w < 4.5); }
bool cmpf(int f, float a, float b) {
  switch (f) { case 0: return false; case 1: return a < b; case 2: return a == b;
  case 3: return a <= b; case 4: return a > b; case 5: return a != b; case 6: return a >= b;
  default: return true; } }
float3 onormal(float3 n, float3 wp, float3 eye) {
  float l = length(n); n = l > 1e-6 ? n / l : float3(0, 0, 1); if (dot(n, eye - wp) < 0) n = -n; return n; }

/* the air's colour looking along `vd`: the sky (or, at night and in fog, the
   game's own fog colour), brightened toward the sun by forward scattering */
float3 air(constant FXU &u, float3 vd) {
  float mu = saturate(dot(vd, u.sun.xyz));
  float3 base = mix(u.skyc.rgb * u.skyc.w, u.fogc.rgb, u.grnd.w);
  return base + u.sunc.rgb * (pow(mu, 8.0) * 0.55 + pow(mu, 64.0) * 0.6) * u.sun.w; }

/* a clear daytime sky: deep blue overhead, pale toward the horizon, the
   sun's glow and its disc */
float3 atmos(constant FXU &u, float3 vd) {
  float t = saturate(vd.z);
  float3 c = mix(float3(0.62, 0.74, 0.94), float3(0.10, 0.26, 0.68), pow(t, 0.55));
  if (vd.z < 0.0) c = float3(0.62, 0.74, 0.94) * 0.95;
  float mu = saturate(dot(vd, u.sun.xyz));
  c += u.sunc.rgb * (pow(mu, 8.0) * 0.22 + pow(mu, 64.0) * 0.55);
  c += u.sunc.rgb * 22.0 * smoothstep(0.99962, 0.99990, mu);
  return c; }
/* the sea's surface normal at world xy: two layers of moving waves, their
   height flattened with distance so far water does not shimmer */
float3 waveN(float2 xy, float t, float amp) {
  float e = 0.12;
  float2 w1 = float2(t * 0.06, t * 0.04), w2 = float2(-t * 0.1, t * 0.07);
  float h0 = fbm(xy * 0.35 + w1) + 0.45 * fbm(xy * 1.3 + w2);
  float hx = fbm((xy + float2(e, 0)) * 0.35 + w1) + 0.45 * fbm((xy + float2(e, 0)) * 1.3 + w2);
  float hy = fbm((xy + float2(0, e)) * 0.35 + w1) + 0.45 * fbm((xy + float2(0, e)) * 1.3 + w2);
  return normalize(float3(-(hx - h0) / e * amp, -(hy - h0) / e * amp, 1.0)); }
/* blue-green, which only the sea is in these tracks */
float seamask(float3 sb) {
  /* green over red with blue at least level with red, and not bright: the
     open sea and the greyer shallows alike, but not grass, walls or snow */
  float mx = max(sb.r, max(sb.g, sb.b));
  return smoothstep(1.04, 1.09, sb.g / max(sb.r, 0.02)) * smoothstep(0.96, 1.0, sb.b / max(sb.r, 0.02))
       * (1.0 - smoothstep(0.66, 0.76, mx)); }
/* ---- volumetric clouds: a cumulus layer 1.2-2.6 km up, ray-marched, lit by
   the sun through the cloud (Beer-Lambert with a powder term and forward
   scattering) and by the sky from above */
float hash3(float3 p) { return fract(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453); }
float vnoise3(float3 p) {
  float3 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
  float a = mix(mix(hash3(i), hash3(i + float3(1, 0, 0)), f.x), mix(hash3(i + float3(0, 1, 0)), hash3(i + float3(1, 1, 0)), f.x), f.y);
  float b = mix(mix(hash3(i + float3(0, 0, 1)), hash3(i + float3(1, 0, 1)), f.x), mix(hash3(i + float3(0, 1, 1)), hash3(i + float3(1, 1, 1)), f.x), f.y);
  return mix(a, b, f.z); }
float cloud_d(float3 p, float t) {
  float h = (p.z - 1200.0) / 1400.0;
  if (h <= 0.0 || h >= 1.0) return 0.0;
  float3 q = p * 0.00055 + float3(t * 0.006, t * 0.002, 0.0);
  float n = vnoise3(q) * 0.55 + vnoise3(q * 2.3 + 7.1) * 0.3 + vnoise3(q * 5.1 + 3.7) * 0.15;
  float prof = smoothstep(0.0, 0.18, h) * smoothstep(1.0, 0.45, h);
  return saturate((n - 0.5) * 3.2) * prof; }
float4 clouds(constant FXU &u, float3 vd, float2 pos) {
  if (vd.z < 0.015) return float4(0, 0, 0, 1);
  float3 ro = u.eye.xyz;
  float t0 = (1200.0 - ro.z) / vd.z, t1 = (2600.0 - ro.z) / vd.z;
  if (t0 > 30000.0) return float4(0, 0, 0, 1);
  t1 = min(t1, t0 + 6000.0);
  const int NS = 18;
  float dt = (t1 - t0) / float(NS), t = t0 + dt * ign(pos), T = 1.0;
  float3 L = 0, sd = u.sun.xyz;
  float mu = dot(vd, sd), g = 0.6;
  float phase = (1.0 - g * g) / (4.0 * 3.14159 * pow(1.0 + g * g - 2.0 * g * mu, 1.5)) * 6.0 + 0.35;
  for (int i = 0; i < NS && T > 0.03; i++, t += dt) {
    float3 p = ro + vd * t;
    float d = cloud_d(p, u.tm.x);
    if (d <= 0.001) continue;
    float od = 0.0;
    for (int j = 1; j <= 3; j++) od += cloud_d(p + sd * (140.0 * float(j)), u.tm.x) * 140.0;
    float sunT = exp(-od * 0.012), powder = 1.0 - exp(-d * 4.0);
    float3 lum = u.sunc.rgb * sunT * phase * powder * 1.6 + mix(float3(0.55, 0.62, 0.75), float3(0.95, 0.97, 1.0), saturate((p.z - 1200.0) / 1400.0)) * 0.6;
    float a = exp(-d * dt * 0.012);
    L += T * (1.0 - a) * lum;
    T *= a; }
  /* fade into the haze toward the horizon */
  float f = smoothstep(0.015, 0.09, vd.z);
  return float4(L * f, mix(1.0, T, f)); }
/* ---- rain on standing water: rings spreading from where drops land; the
   gradient they add to a flat surface's normal */
float2 ripples(float2 xy, float t, float amt) {
  float2 g = 0;
  for (int l = 0; l < 2; l++) {
    float2 p = xy * (l == 0 ? 3.0 : 4.7) + float(l) * 17.0;
    float2 c = floor(p), f = fract(p) - 0.5;
    float h = hash2(c), ph = fract(t * (1.1 + 0.4 * float(l)) + h * 5.0);
    float2 o = float2(hash2(c + 3.1), hash2(c + 7.7)) * 0.5 - 0.25;
    float2 dv = f - o; float r = length(dv), R = ph * 0.45;
    float ring = exp(-pow((r - R) / 0.035, 2.0)) * (1.0 - ph) * step(h, amt);
    g += dv / max(r, 1e-3) * ring * cos((r - R) * 70.0); }
  return g; }

/* the colour a reflected ray sees when it leaves the screen */

/* ---- shadow map: the frame's opaque triangles from the sun */
struct SO { float4 pos [[position]]; float2 uv; };
vertex SO shvs(uint vid [[vertex_id]], const device CV *v [[buffer(0)]], constant SHU &s [[buffer(1)]]) {
  CV g = v[vid]; SO o;
  o.pos = g.fl == 1.0 ? s.svp * float4(g.wx, g.wy, g.wz, 1) : float4(0, 0, 2, 1);
  o.uv = float2(g.s * s.su, g.t * s.sv); return o; }
fragment void shfs(SO in [[stage_in]], constant SHU &s [[buffer(0)]],
                   texture2d<float> t [[texture(0)]], sampler smp [[sampler(0)]]) {
  if (s.use_tex && s.at_fn != 7) {
    float a = floor(t.sample(smp, in.uv).a * 255.0);
    if (!cmpf(s.at_fn, a, float(s.at_ref))) discard_fragment(); } }

/* ---- ambient occlusion + one bounce (half res): rgb bounce, a visibility */
fragment float4 aofs(QO in [[stage_in]], constant FXU &u [[buffer(0)]],
                     texture2d<float> gn [[texture(0)]], texture2d<float> gp [[texture(1)]],
                     texture2d<float> col [[texture(2)]]) {
  float4 P = gp.sample(ns, in.uv);
  if (!is_geo(P.w)) return float4(0, 0, 0, 1);
  float3 wp = P.xyz, N = onormal(gn.sample(ns, in.uv).xyz, wp, u.eye.xyz);
  float R = u.p2.z * (1.0 + distance(u.eye.xyz, wp) * 0.004);
  float3 T = normalize(abs(N.z) < 0.9 ? cross(N, float3(0, 0, 1)) : cross(N, float3(1, 0, 0)));
  float3 B = cross(N, T);
  float rot = (ign(in.pos.xy) + u.p2.w * 0.618) * 6.2831853;
  float occ = 0; float3 gi = 0; const int NS = 10;
  float de = distance(u.eye.xyz, wp);
  for (int i = 0; i < NS; i++) {
    float fi = (float(i) + 0.5) / float(NS);
    float phi = rot + float(i) * 2.3999632, r = sqrt(fi);
    float3 dir = T * (cos(phi) * r) + B * (sin(phi) * r) + N * sqrt(max(0.0, 1.0 - fi));
    float len = R * (0.15 + 0.85 * fract(fi * 5.13 + rot * 0.159));
    float3 s = wp + N * (0.03 * R) + dir * len;
    float cw; float2 suv = to_uv(u, s, cw);
    if (cw <= 0 || any(suv < 0.0) || any(suv > 1.0)) continue;
    float4 Q = gp.sample(ns, suv);
    if (!is_geo(Q.w)) continue;
    float ds = distance(u.eye.xyz, s), dq = distance(u.eye.xyz, Q.xyz);
    if (dq < ds - 0.02 * R) {
      float3 dd = Q.xyz - wp; float L = length(dd);
      float fall = saturate(1.0 - L / (R * 2.5)) * saturate((de - dq) / R + 1.0);
      occ += fall;
      float3 c = pow(col.sample(ns, suv).rgb, 2.2);
      gi += c * saturate(dot(N, dd / max(L, 1e-4))) * fall; } }
  return float4(gi * (2.0 / float(NS)), 1.0 - occ / float(NS)); }
fragment float4 blurfs(QO in [[stage_in]], constant FXU &u [[buffer(0)]], constant float2 &dir [[buffer(1)]],
                       texture2d<float> src [[texture(0)]], texture2d<float> gp [[texture(1)]],
                       texture2d<float> gn [[texture(2)]]) {
  float4 P = gp.sample(ns, in.uv); float3 N = onormal(gn.sample(ns, in.uv).xyz, P.xyz, u.eye.xyz);
  if (!is_geo(P.w)) return src.sample(ns, in.uv);
  float2 px = dir / float2(src.get_width(), src.get_height());
  float de = distance(u.eye.xyz, P.xyz);
  float4 acc = 0; float wsum = 0;
  for (int i = -4; i <= 4; i++) {
    float2 q = in.uv + px * float(i) * 1.5;
    float4 Q = gp.sample(ns, q);
    if (!is_geo(Q.w)) continue;
    float w = exp(-float(i * i) / 12.0);
    w *= saturate(1.0 - abs(distance(u.eye.xyz, Q.xyz) - de) / (0.03 * de + 0.5));
    w *= pow(saturate(dot(N, onormal(gn.sample(ns, q).xyz, Q.xyz, u.eye.xyz)) * 0.5 + 0.5), 8.0);
    acc += src.sample(ns, q) * w; wsum += w; }
  return wsum > 0 ? acc / wsum : src.sample(ns, in.uv); }

/* ---- screen-space reflections (half res): rgb colour, a confidence */
fragment float4 ssrfs(QO in [[stage_in]], constant FXU &u [[buffer(0)]],
                      texture2d<float> gn [[texture(0)]], texture2d<float> gp [[texture(1)]],
                      texture2d<float> col [[texture(2)]]) {
  float4 P = gp.sample(ns, in.uv);
  if (!is_geo(P.w) || u.p0.z <= 0) return 0;
  float3 wp = P.xyz, N = onormal(gn.sample(ns, in.uv).xyz, wp, u.eye.xyz);
  float3 Vd = normalize(wp - u.eye.xyz), R = reflect(Vd, N);
  float de = distance(u.eye.xyz, wp);
  float step = max(0.6, de * 0.015) * (0.75 + 0.5 * ign(in.pos.xy + u.p2.w));
  float3 pos = wp + N * (0.01 * de);
  for (int i = 0; i < 48; i++) {
    pos += R * step; step *= 1.07;
    float cw; float2 q = to_uv(u, pos, cw);
    if (cw <= 0 || any(q < 0.0) || any(q > 1.0)) break;
    float4 Q = gp.sample(ns, q);
    float dr = distance(u.eye.xyz, pos);
    if (Q.w > 2.5 && Q.w < 3.5) continue;
    if (!is_geo(Q.w)) break;
    float dq = distance(u.eye.xyz, Q.xyz);
    if (dr > dq && dr - dq < step * 3.0) {
      float3 lo = pos - R * step, hi = pos;
      for (int k = 0; k < 5; k++) {
        float3 m = (lo + hi) * 0.5; float c2; float2 mq = to_uv(u, m, c2);
        float4 M = gp.sample(ns, mq);
        if (distance(u.eye.xyz, m) > distance(u.eye.xyz, M.xyz)) hi = m; else lo = m; }
      q = to_uv(u, hi, cw);
      float2 e = smoothstep(0.0, 0.08, q) * smoothstep(0.0, 0.08, 1.0 - q);
      return float4(pow(col.sample(ls, q).rgb, 2.2), e.x * e.y * saturate(1.0 - float(i) / 48.0)); } }
  /* missed: the sky where the ray leaves, when that pixel is sky */
  float cw; float2 q = to_uv(u, u.eye.xyz + R * 4000.0, cw);
  if (R.z > 0 && cw > 0 && all(q >= 0.0) && all(q <= 1.0) && abs(gp.sample(ns, q).w - 3.0) < 0.5)
    return float4(pow(col.sample(ls, q).rgb, 2.2), 0.8 * u.p0.w);
  /* off screen: the game's own fog colour stands for the sky (black at night) */
  return float4(u.fogc.rgb, R.z > 0 ? 0.5 * u.p0.w : 0.0); }

/* ---- lighting composite (full res, HDR): a = how much of the pixel is scene */
float shadow1(depth2d<float> sm, float4x4 m, float3 wp, float bias, float soft, float2 px, thread float &edge) {
  constexpr sampler cs(filter::linear, address::clamp_to_border, border_color::opaque_white, compare_func::less_equal);
  float4 c = m * float4(wp, 1);
  float2 st = float2(c.x * 0.5 + 0.5, 0.5 - c.y * 0.5);
  edge = max(abs(c.x), abs(c.y));
  if (any(st < 0.0) || any(st > 1.0) || c.z > 1.0) { edge = 2.0; return 1.0; }
  float rot = ign(px) * 6.2831853, s = 0;
  for (int i = 0; i < 12; i++) {
    float r = sqrt((float(i) + 0.5) / 12.0), a = rot + float(i) * 2.3999632;
    s += sm.sample_compare(cs, st + float2(cos(a), sin(a)) * r * soft, c.z - bias); }
  return s / 12.0; }
/* two cascades: a sharp one around the car, a wide one to the distance */
float shadow_at(constant FXU &u, depth2d<float> sm, float3 wp, float3 N, float2 px, depth2d<float> sm2,
                depth2d<float> smd, bool dyn) {
  float e1, e2;
  float s1 = shadow1(sm, u.svp, wp + N * u.p3.w * 1.2, 0.00012, u.p2.y / 2048.0, px, e1);
  /* the cars' own map (drawn every frame; the cascade is the cache) */
  if (dyn) { float ed; s1 = min(s1, shadow1(smd, u.svp, wp + N * u.p3.w * 1.2, 0.00012, u.p2.y / 2048.0, px, ed)); }
  if (e1 < 0.85) return s1;
  float s2 = shadow1(sm2, u.svp2, wp + N * u.csm.x * 1.2, 0.00025, u.p2.y * 0.6 / 2048.0, px, e2);
  s2 = mix(s2, 1.0, smoothstep(0.85, 1.0, e2));
  return mix(s1, s2, smoothstep(0.85, 1.0, e1)); }
/* the Remastered sky (host_sky.m): each panorama is a level photograph, so it
   is laid out with a photograph's proportions: horizon on its bottom row,
   35 degrees up on its top row, 60 degrees of heading across, repeated six
   times around the eye; above 35 degrees, the top row's average colour.
   The chase camera sees about 39 x 30 degrees centred on the horizon, so the
   lower half of the picture is what is on screen. */
float3 pano(constant FXU &u, texture2d<float> sky, float3 vd, float lv = 0.0) {
  constexpr sampler ws(filter::linear, mip_filter::linear, s_address::repeat, t_address::clamp_to_edge);
  float e = asin(clamp(vd.z, -1.0, 1.0)) / 0.6109;
  float2 st = float2(atan2(vd.y, vd.x) / 1.0472, 1.0 - saturate(e));
  float3 c = sky.sample(ws, st, level(lv)).rgb;
  float3 z = sky.sample(ws, float2(st.x, 0.0), level(7.0)).rgb;
  return mix(c, z, smoothstep(0.85, 1.25, e)) * u.tm.z; }
/* the colour a reflected ray sees when it leaves the screen: the track's sky
   panorama when one is loaded, else the drawn sky (clear) or the air */
float3 skylight(constant FXU &u, texture2d<float> sky, float3 d) {
  if (u.tm.z > 0.0) return pano(u, sky, d);
  return u.sk2.x > 0.5 ? atmos(u, d) : air(u, d); }
/* the surface's own colour (host_glide.m's: rgb times the baked light in
   alpha, what compfs relights) averaged over two rings (7 and 20 pixels)
   round a ground pixel, from the samples on the same surface only: a car, a
   sign or a post in front must not darken the average, or the ground beside
   it reads as a painted marking and keeps its old texture, a pale rim round
   everything.  Not the game's finished colour: that holds what the game
   blended over the ground, its own car shadows and fog, which the relit
   surface leaves out -- averaged in, a car's old shadow came back as a flat
   dark box on the road under it */
float3 ring_same_surface(constant FXU &u, texture2d<float> col, texture2d<float> gp, texture2d<float> gn,
                         float2 uv, float2 px, float3 wp, float3 N) {
  float d0 = distance(u.eye.xyz, wp), tol = 0.06 * d0 + 0.3;
  float3 acc = 0; float n = 0;
  for (int k = 0; k < 16; k++) {
    float a = float(k >> 1) * 0.785398 + ((k & 1) ? 0.39 : 0.0);
    float2 q = uv + float2(cos(a), sin(a)) * px * ((k & 1) ? 2.9 : 1.0);
    float4 Q = gp.sample(ns, q);
    if (!is_geo(Q.w) || abs(distance(u.eye.xyz, Q.xyz) - d0) > tol) continue;
    if (dot(onormal(gn.sample(ns, q).xyz, Q.xyz, u.eye.xyz), N) < 0.8) continue;   /* a wall or post standing on it */
    float4 c = col.sample(ls, q); acc += c.rgb * c.a; n += 1.0; }
  float4 c0 = col.sample(ns, uv);
  return n > 0.0 ? acc / n : c0.rgb * c0.a; }
/* at half resolution: the average is wide and smooth, it loses nothing */
fragment float4 ringfs(QO in [[stage_in]], constant FXU &u [[buffer(0)]], texture2d<float> col [[texture(0)]],
                       texture2d<float> gp [[texture(1)]], texture2d<float> gn [[texture(2)]]) {
  float4 P = gp.sample(ns, in.uv);
  if (!is_geo(P.w)) { float4 c = col.sample(ns, in.uv); return float4(c.rgb * c.a, 1); }
  float3 N = onormal(gn.sample(ns, in.uv).xyz, P.xyz, u.eye.xyz);
  return float4(ring_same_surface(u, col, gp, gn, in.uv, 7.0 / float2(col.get_width(), col.get_height()), P.xyz, N), 1); }
/* within reach of a car's shadow (the cars' own shadow map is read only there) */
bool near_car(constant MVC &mv, float3 wp) {
  for (int i = 0; i < int(mv.misc.x); i++) if (distance_squared(wp, mv.cur[i][3].xyz) < 225.0) return true;
  return false; }
/* the landscape's light maps (host_terrain.m): how much of the sun gets
   past the mountains, and the light from the sky and the land around; w 0
   outside the maps */
float4 ter_light(constant FXU &u, texture2d<float> tsv, texture2d<float> tgi, float3 wp, thread float &sunv) {
  constexpr sampler ls(filter::linear, address::clamp_to_edge);
  sunv = 1.0;
  if (u.ter[1].w < 0.5) return float4(0);
  float2 g = (wp.xy - u.ter[0].xy) / u.ter[0].z;
  if (any(g < 1.0) || any(g > u.ter[0].w - 1.0)) return float4(0);
  float2 uv = g / u.ter[0].w;
  sunv = tsv.sample(ls, uv).r;
  float edge = saturate(min(min(g.x, g.y), min(u.ter[0].w - g.x, u.ter[0].w - g.y)) / 40.0);
  return float4(tgi.sample(ls, uv).rgb, edge); }
/* aerial perspective over kilometres: air thins with height (a 1.4 km
   scale), so the valley floor hazes and the peaks stay clear */
float air_depth(constant FXU &u, float3 eye, float3 wp) {
  float3 d = wp - eye; float L = length(d);
  float dz = d.z / max(L, 1e-3), H = 1400.0, z0 = eye.z;
  float a = abs(dz) < 1e-4 ? L * exp(-z0 / H) : H / dz * exp(-z0 / H) * (1.0 - exp(-L * dz / H));
  return a; }
fragment float4 compfs(QO in [[stage_in]], constant FXU &u [[buffer(0)]],
                       texture2d<float> gn [[texture(0)]], texture2d<float> gp [[texture(1)]],
                       texture2d<float> col [[texture(2)]], depth2d<float> sm [[texture(3)]],
                       texture2d<float> aot [[texture(4)]], texture2d<float> ssr [[texture(5)]],
                       texture2d<float> sky [[texture(6)]], texture2d<float> trk [[texture(7)]],
                       depth2d<float> sm2 [[texture(8)]], texture2d<float> gsm [[texture(9)]],
                       texture2d_array<float> matA [[texture(10)]], texture2d_array<float> matN [[texture(11)]],
                       texture2d<float> ring [[texture(12)]], texture2d<float> rmap [[texture(13)]],
                       texture2d<float> ga [[texture(14)]], texture2d<float> gmt [[texture(15)]],
                       texture2d<float> cnp [[texture(16)]],
                       texture2d<float> mka [[texture(17)]], texture2d<float> mkp [[texture(18)]],
                       texture2d<float> bndt [[texture(19)]],
                       texture2d<float> tsv [[texture(20)]], texture2d<float> tgi [[texture(21)]],
                       depth2d<float> smd [[texture(22)]],
                       constant HL &hl [[buffer(1)]], constant MVC &mv [[buffer(2)]]) {
  float4 C = col.sample(ns, in.uv), P = gp.sample(ns, in.uv), G = gn.sample(ns, in.uv);
  if (int(u.p3.y) == 15)                             /* debug: the G-buffer's class (red car, green lit, blue sky) */
    return float4(P.w > 3.5 && P.w < 4.5, P.w > 0.5 && P.w < 2.5, P.w > 2.5 && P.w < 3.5, 1) * (0.4 + 0.6 * dot(pow(C.rgb, 2.2), 0.33));
  float3 lin = pow(C.rgb, 2.2);
  if (P.w > 3.5 && P.w < 4.5) {                      /* pre-lit surface (host_car.m): colour holds
                                                        (radiance / 4)^(1/2.2); only the sun's
                                                        shadow is applied here */
    float3 wp = P.xyz, N = normalize(G.xyz);
    float sh = u.sun.w > 0 ? shadow_at(u, sm, wp, N, in.pos.xy, sm2, smd, near_car(mv, wp)) : 1.0;
    { float tsun; ter_light(u, tsv, tgi, wp, tsun); sh = min(sh, tsun); }
    float k = saturate(dot(N, u.sun.xyz) * 4.0) * (1.0 - sh);
    float3 o = lin * 4.0 * (1.0 - 0.6 * k);
    /* the lamps' lenses light up: the red of a tail lamp near its bulb glows
       with the brake, a headlamp's clear glass with its beam */
    for (int i = 0; i < int(hl.gmisc.x); i++) {
      float dl = distance(wp, hl.g[i].xyz);
      if (dl > 0.3) continue;
      float fall = smoothstep(0.3, 0.1, dl);
      if (hl.g[i].w > 0.5) {
        float red = saturate((lin.r - max(lin.g, lin.b) * 1.8) * 6.0);
        o += float3(1.0, 0.05, 0.02) * red * fall * hl.gd[i].w * 1.8; }
      else {
        float clear = saturate((dot(lin, 0.33) - 0.25) * 3.0) * (1.0 - saturate((max(lin.r, max(lin.g, lin.b)) - min(lin.r, min(lin.g, lin.b))) * 4.0));
        o += float3(1.0, 0.95, 0.85) * clear * fall * hl.gd[i].w * hl.misc.z * 3.0; } }
    /* the lamps light it too: host_env.m's trees and props leave their
       surface colour in ga (alpha 1), lit here as the ground is.  Leaves and
       cards are seen from both sides, so either face takes the light */
    { float4 AG = ga.sample(ns, in.uv);
      if (AG.a > 0.5 && hl.misc.x > 0.5) {
        /* foliage is saturated green in its maps; under a lamp it reads as
           lit leaves, not neon */
        float3 albe = min(pow(AG.rgb, 2.2), 0.7), hd = 0;
        albe = mix(float3(dot(albe, float3(0.2126, 0.7152, 0.0722))), albe, 0.6);
        for (int i = 0; i < int(hl.misc.x); i++) {
          float3 Ld; float w = spotw(hl, i, wp + N * 0.05, Ld);
          if (w > 0.0) hd += spotc(hl, i) * w * abs(dot(N, Ld)); }
        hd = hd / (1.0 + 0.2 * hd);
        o += 3.0 * albe * hd; } }
    /* rain on the car, in the car's own frame so it rides with it: beads
       standing on the paint (little lenses: the paint darker under them, a
       rim, the sky and a spark of light in them), drips running down the
       sides, and on the upper panels the small crowns of drops landing */
    if (u.wx.x > 0.0) {
      int ci = -1;
      for (int i = 0; i < int(mv.misc.x); i++) {
        float3 d = wp - mv.cur[i][3].xyz;
        float3 l = float3(dot(d, mv.cur[i][0].xyz), dot(d, mv.cur[i][1].xyz), dot(d, mv.cur[i][2].xyz));
        if (abs(l.x) < 2.8 && abs(l.y) < 1.4 && l.z > -1.2 && l.z < 2.0) { ci = i; break; } }
      if (ci >= 0) {
        float3 F = mv.cur[ci][0].xyz, Lf = mv.cur[ci][1].xyz, Up = mv.cur[ci][2].xyz, d = wp - mv.cur[ci][3].xyz;
        float3 l = float3(dot(d, F), dot(d, Lf), dot(d, Up)), nl = float3(dot(N, F), dot(N, Lf), dot(N, Up));
        float3 an = abs(nl);
        /* the panel's plane: top, side or end */
        float2 q; float3 Ta, Tb; float side;
        if (an.z >= an.x && an.z >= an.y) { q = l.xy; Ta = F; Tb = Lf; side = 0.0; }
        else if (an.y >= an.x) { q = float2(l.x, l.z); Ta = F; Tb = Up; side = 1.0; }
        else { q = float2(l.y, l.z); Ta = Lf; Tb = Up; side = 1.0; }
        float wet = u.wx.x, t = u.tm.x;
        const float S = 0.02;                                  /* 2 cm cells */
        float2 g = q / S;
        /* on the sides a share of the columns run: the beads slide down */
        float colh = hash2(float2(floor(g.x), 3.7));
        float run = side * step(0.72, colh);
        g.y += run * t * (1.5 + 3.0 * colh) / S * 0.02;
        float2 cell = floor(g), f = fract(g) - 0.5;
        float h1 = hash2(cell), h2 = hash2(cell + 5.3), h3 = hash2(cell + 9.1);
        float2 c0 = (float2(h2, h3) - 0.5) * 0.5;
        float2 df = f - c0;
        if (run > 0.0) df.y *= 0.45;                           /* a drip is drawn out down its path */
        float rad = mix(0.18, 0.4, h3) * (side > 0.5 ? 0.85 : 1.0);
        float r = length(df) / rad;
        float bead = step(h1, 0.42 * wet) * step(r, 1.0) * saturate(dot(nl, float3(0, 0, 1)) + 0.9);
        float3 Vv = normalize(u.eye.xyz - wp);
        if (bead > 0.0) {
          /* the bead's surface: a lens over the panel */
          float2 sl = df / rad;
          float3 Nb = normalize(N * sqrt(max(1.0 - dot(sl, sl), 0.02)) + (Ta * sl.x + Tb * sl.y) * 0.9);
          float3 Rb = reflect(-Vv, Nb);
          float Fb = 0.04 + 0.96 * pow(1.0 - saturate(dot(Nb, Vv)), 5.0);
          float3 Hb = normalize(u.sun.xyz + Vv);
          float spark = pow(saturate(dot(Nb, Hb)), 400.0) * 8.0 * u.sun.w;
          float rim = smoothstep(0.55, 1.0, r);
          o *= mix(0.82, 0.6, rim);                            /* the paint seen through it, and its dark edge */
          o += (skylight(u, sky, Rb) * u.p4.w * Fb * 1.6 + u.sunc.rgb * spark) * bead; }
        /* drops landing: a thin ring spreading for a tenth of a second */
        if (side < 0.5) {
          float2 gi = q / 0.06, ic = floor(gi), fi = fract(gi) - 0.5;
          float hi = hash2(ic + 17.0), ph = fract(t * 2.5 + hi * 7.0);
          float2 io = (float2(hash2(ic + 2.0), hash2(ic + 4.0)) - 0.5) * 0.6;
          float rr = length(fi - io), R = ph * 0.45;
          float ring = step(hi, 0.5 * wet) * exp(-pow((rr - R) / 0.03, 2.0)) * (1.0 - ph) * step(ph, 0.35);
          o += skylight(u, sky, reflect(-Vv, N)) * u.p4.w * ring * 0.5; } } }
    if (any(isnan(o)) || any(isinf(o))) o = lin;
    return float4(o, G.a); }
  if (P.w > 2.5 && int(u.p3.y) == 11) return float4(0, 0, 1, G.a);
  if (int(u.p3.y) == 17) {                           /* debug: the catalogued material (grey: none) */
    int m = int(gmt.sample(ns, in.uv).r * 255.0 + 0.5);
    float3 mc = m == 0 ? float3(0.3) : fract(float3(m * 0.37, m * 0.61, m * 0.83)) * 0.8 + 0.2;
    return float4(is_geo(P.w) ? mc : float3(0), G.a); }
  if (P.w > 2.5) {                                   /* sky */
    float3 vd = view_dir(u, in.uv); float g = saturate(dot(vd, u.sun.xyz));
    float3 o = lin * u.p4.x;
    /* the Remastered sky for this track and weather, where it is loaded, in
       place of the game's painted one, everywhere above the horizon: the
       scenery the game paints into its 64x64 backdrop is too coarse to keep
       against it (the real distant geometry is not sky and is untouched).
       The weather's own touches below (the night floor, fog, the sun's glow,
       lightning) apply over it. */
    if (u.tm.z > 0.0 && G.a > 0.01) o = mix(o, pano(u, sky, vd), smoothstep(-0.004, 0.004, vd.z));
    /* the distant land along the horizon (host_sky.m's band), over the sky:
       copies round the eye, every other one mirrored so each join meets its
       own edge, square pixels; lit and hazed by the weather's own horizon */
    if (u.bnd.z > 0.0 && u.tm.z > 0.0 && G.a > 0.01) {
      constexpr sampler bs(filter::linear, mip_filter::linear, address::clamp_to_edge);
      float el = asin(clamp(vd.z, -1.0, 1.0)), t = (el - u.bnd.x) / u.bnd.y;
      if (t > 0.0 && t < 1.0) {
        float az = atan2(vd.y, vd.x) / 6.2831853 * u.bnd.z + 8.0, s = fract(az);
        if ((int(floor(az)) & 1) != 0) s = 1.0 - s;
        float4 b = bndt.sample(bs, float2(s, 1.0 - t));
        /* the ridge's edge a touch soft, as air softens a far skyline */
        b.a = min(b.a, bndt.sample(bs, float2(s, 1.0 - t), level(1.5)).a * 1.15);
        float3 hz = pano(u, sky, normalize(float3(vd.xy, 0.02)), 6.0);
        float lit = saturate(dot(hz, float3(0.2126, 0.7152, 0.0722)) / max(0.62 * u.tm.z, 1e-3));
        float3 bc = mix(b.rgb * u.tm.z * lit, hz, u.bnd.w);
        o = mix(o, bc, b.a); } }
    /* night: the sky is never black; a deep blue, lighter toward the horizon */
    o = max(o, u.nsky.rgb * (0.45 + 0.55 * pow(1.0 - saturate(vd.z), 3.0)));
    /* a luminous horizon and the sun's glow, as the air scatters it */
    /* gentle: translucent distant scenery the game draws over the sky
       shares these pixels */
    o = mix(o, air(u, vd), max(pow(1.0 - saturate(vd.z), 5.0) * 0.12, u.wb.w));
    o += u.sunc.rgb * (pow(g, 900.0) * 6.0 + pow(g, 12.0) * 0.12) * u.p4.y;
    /* clear weather: a real sky in place of the game's painted one, keeping
       its clouds (the whiter texels) and anything the game drew over it */
    if (u.sk2.x > 0.0 && G.a > 0.01 && u.tm.z <= 0.0) {
      float3 own = saturate(G.rgb / G.a);
      float mn = min(own.r, min(own.g, own.b)), cl = dot(own, float3(0.2126, 0.7152, 0.0722));
      /* the sky and its clouds are drawn, not the game's painted texture */
      float4 cv = clouds(u, vd, in.pos.xy);
      float3 ns = atmos(u, vd) * cv.a + cv.rgb;
      /* what the game drew over its sky: scenery near the horizon stays; higher
         up it is the old cloud layer, which the new clouds replace */
      ns += (lin - pow(own, 2.2)) * (1.0 - smoothstep(0.015, 0.04, vd.z));
      /* above the horizon only, fading in over the first few degrees so the
         game's distant scenery painted into its backdrop stays */
      /* sky is the blue texels and the clouds (bright, grey); the hills and
         scenery painted along the horizon are neither, and stay */
      float mx = max(own.r, max(own.g, own.b));
      float blue = smoothstep(1.15, 1.32, own.b / max(own.r, 0.02));
      float grey = smoothstep(0.34, 0.42, mn) * (1.0 - smoothstep(0.1, 0.2, (mx - mn) / max(mx, 0.05)));
      float skyish = max(blue, grey);
      o = mix(o, ns, u.sk2.x * smoothstep(-0.005, 0.02, vd.z) * max(skyish, smoothstep(0.03, 0.08, vd.z))); }
    /* the sea painted into the backdrop, below the horizon: water on a plane
       under the eye, reflecting the sky, with the sun's glitter */
    if (vd.z < 0.006 && u.sk2.w > 0.0 && G.a > 0.01) {
      /* the final colour: in some weathers the sea is a see-through layer
         over the backdrop, which the G-buffer's sky colour does not hold.
         Decided on the backdrop's colour smoothed along the horizon, so its
         big texels do not break the sea into streaks, and carried up to the
         backdrop's own painted horizon, which sits a little above the true one */
      float2 hp = float2(3.0 / col.get_width(), 1.0 / col.get_height());
      float3 cs = (C.rgb + col.sample(ls, in.uv + float2(hp.x, 0)).rgb + col.sample(ls, in.uv - float2(hp.x, 0)).rgb
                 + col.sample(ls, in.uv + float2(2.0 * hp.x, hp.y)).rgb + col.sample(ls, in.uv - float2(2.0 * hp.x, -hp.y)).rgb) * 0.2;
      float m = smoothstep(0.2, 0.7, seamask(saturate(cs))) * u.sk2.w;
      m *= smoothstep(0.006, 0.0, vd.z);
      if (m > 0.0) {
        /* a sea plane fixed in the world (z = 0), not under the camera, so the
           waves stay put as the camera rises and falls */
        float t = min(max(u.eye.z, 1.0) / max(-vd.z, 1e-3), 3000.0);
        float2 xy = u.eye.xy + vd.xy * t;
        /* waves fade to a mirror with distance, before they are finer than a pixel */
        float3 Nw = waveN(xy * 0.5, u.tm.x, 0.22 * smoothstep(500.0, 30.0, t));
        float3 R = reflect(vd, Nw);
        /* never the panorama's bottom rows: they hold its painted horizon */
        /* rough with distance: far water mirrors a blur of the sky, never
           single clouds drawn out into streaks */
        float3 Rd = normalize(float3(R.xy, max(abs(R.z), 0.04)));
        float3 refl = u.tm.z > 0.0 ? pano(u, sky, Rd, mix(1.0, 6.0, smoothstep(40.0, 700.0, t))) : skylight(u, sky, Rd);
        float Fw = 0.02 + 0.98 * pow(1.0 - saturate(dot(Nw, -vd)), 5.0);
        float3 Hw = normalize(u.sun.xyz - vd); float nhw = saturate(dot(Nw, Hw));
        float aw = 0.1 * 0.1 * 0.1 * 0.1, dw = nhw * nhw * (aw - 1.0) + 1.0;
        float glint = aw / (3.14159 * dw * dw) * 0.012 * saturate(dot(Nw, u.sun.xyz)) * u.sun.w;
        float3 ow = mix(lin * 0.7, refl, saturate(Fw * 1.1 + 0.1)) + u.sunc.rgb * glint;
        o = mix(o, ow, m); } }
    o += u.flash.rgb * u.flash.w * 0.35;
    if (any(isnan(o)) || any(isinf(o))) o = lin;
    return float4(o, G.a); }
  if (!is_geo(P.w)) return float4(lin, 0);
  float fogk = saturate((P.w - 1.0) / 0.9);
  /* the surface's own colour, before the game's vertex light and fog
     (host_glide.m): rgb the colour, a the light the game baked in, which
     also carries its darkening for the weather.  bref is that light's mean
     over the race: the sun, moon and lamps light the true colour at that
     level, so the game's baked shading stays in the ambient term only and
     is not laid a second time over the shadow map's */
  float4 AL = ga.sample(ns, in.uv);
  /* what the surface is (host_glide.m's texmat.csv, by its texture; 0
     where the texture is not catalogued: then its colour decides, as before) */
  int mid = int(gmt.sample(ns, in.uv).r * 255.0 + 0.5);
  float bake = pow(AL.a, 2.2), bref = u.mat.y > 0.0 ? u.mat.y : 0.5;
  float rdir = bref / max(bake, 0.3 * bref);
  lin = pow(AL.rgb, 2.2) * bake;
  /* the 1999 textures carry colour noise that relighting would amplify:
     keep every pixel's brightness but take its colour from a 3-pixel
     neighbourhood (a chroma-only filter; edges shift by under 2 pixels) */
  bool terr = mid == MAT_TERRAIN || mid == 11;   /* the field, and its grass */
  if (!terr) { float2 px = 1.5 / float2(col.get_width(), col.get_height());
    float4 a0 = ga.sample(ls, in.uv + float2(px.x, px.y)), a1 = ga.sample(ls, in.uv - float2(px.x, px.y));
    float4 a2 = ga.sample(ls, in.uv + float2(px.x, -px.y)), a3 = ga.sample(ls, in.uv - float2(px.x, -px.y));
    float3 cb = pow(a0.rgb, 2.2) * pow(a0.a, 2.2) + pow(a1.rgb, 2.2) * pow(a1.a, 2.2)
              + pow(a2.rgb, 2.2) * pow(a2.a, 2.2) + pow(a3.rgb, 2.2) * pow(a3.a, 2.2);
    cb *= 0.25;
    const float3 lw = float3(0.2126, 0.7152, 0.0722);
    float yl = dot(lin, lw), yb = dot(cb, lw);
    if (yb > 1e-4) lin = cb * (yl / yb); }
  float3 wp = P.xyz, N = onormal(G.xyz, wp, u.eye.xyz), V = normalize(u.eye.xyz - wp), L = u.sun.xyz;
  if (!terr) { float3 Ns = gsm.sample(ls, in.uv).xyz;
    if (length(Ns) > 0.5) { Ns = onormal(Ns, wp, u.eye.xyz); if (dot(Ns, N) > 0.55) N = Ns; } }
  /* surface detail: the texture's own light and dark read as relief (a bump
     map from its brightness, by the surface gradient of Mikkelsen 2020),
     fading out where the texture is too far away to show it.  The surface's
     own colour, as the ring: the game's car shadow drawn over the road is
     not relief */
  if (!terr) { float2 px = 2.0 / float2(col.get_width(), col.get_height());
    const float3 lw = float3(0.2126, 0.7152, 0.0722);
    float4 b0 = ga.sample(ls, in.uv), b1 = ga.sample(ls, in.uv + float2(px.x, 0)), b2 = ga.sample(ls, in.uv + float2(0, px.y));
    float h0 = dot(b0.rgb, lw) * b0.a;
    float hx = dot(b1.rgb, lw) * b1.a - h0;
    float hy = dot(b2.rgb, lw) * b2.a - h0;
    float3 sx = gp.sample(ns, in.uv + float2(px.x, 0)).xyz - wp, sy = gp.sample(ns, in.uv + float2(0, px.y)).xyz - wp;
    float3 r1 = cross(sy, N), r2 = cross(N, sx);
    float det = dot(sx, r1);
    float k = u.tm.w * smoothstep(60.0, 8.0, distance(u.eye.xyz, wp));
    if (abs(det) > 1e-9 && k > 0.0) {
      float3 grad = sign(det) * (hx * r1 + hy * r2);
      N = normalize(abs(det) * N - grad * k); } }
  float ndl = saturate(dot(N, L)), up = saturate(N.z);
  float sh = u.sun.w > 0 ? shadow_at(u, sm, wp, N, in.pos.xy, sm2, smd, near_car(mv, wp)) : 1.0;
  float tsun; float4 TL = ter_light(u, tsv, tgi, wp, tsun);
  sh = min(sh, tsun);
  float4 A = aot.sample(ls, in.uv);
  float ao = mix(1.0, A.a, u.p0.x);
  /* under the forest's canopy (the Remastered trees' crown cover, host_env.m)
     the ground sees far less sky: its floor is in the trees' shade even
     where the sun comes through (the shadow map has the sun's part) */
  float canopy = u.cmap.z > 0.0 ? cnp.sample(ls, (wp.xy - u.cmap.xy) * u.cmap.zw).r : 0.0;
  ao *= 1.0 - 0.55 * canopy * smoothstep(0.35, 0.75, N.z);
  float wet = u.p0.w * smoothstep(0.55, 0.9, up);
  /* standing water: puddles in the dips of a world-space noise */
  /* more water, more and larger puddles */
  float puddle = wet * smoothstep(0.64 - 0.14 * u.p0.w, 0.74 - 0.1 * u.p0.w, fbm(wp.xy * 0.18));
  /* tyre tracks: pressed snow, darkened dirt, and on a wet road the lines
     the tyres squeezed dry */
  float tk = 0;
  { float4 TK = trk.sample(ls, in.uv);
    if (TK.r > 0.0 && abs(TK.g - wp.z) < 0.45 && N.z > 0.7) tk = saturate(TK.r); }
  wet *= 1.0 - 0.6 * tk * (1.0 - u.wx.z); puddle *= 1.0 - tk;
  /* rain landing on standing water */
  if (u.wx.x > 0.0 && wet > 0.0) {
    float2 rg = ripples(wp.xy, u.tm.x, 0.55 * u.wx.x) * (0.35 + 0.65 * puddle / max(wet, 1e-3));
    N = normalize(N + float3(rg * 0.28 * wet, 0.0)); }
  float mrough = u.p3.z;
  float3 alb = lin * mix(1.0, 0.6, wet) * mix(1.0, 0.7, puddle);
  alb *= mix(float3(1.0), mix(float3(0.82), float3(0.7, 0.74, 0.82), u.wx.z), tk);
  /* asphalt: the old road textures carry a purple cast; flat, unsaturated
     surfaces are pulled toward a neutral, very slightly cool grey */
  { float3 sb = saturate(C.rgb); float mx = max(sb.r, max(sb.g, sb.b)), mn = min(sb.r, min(sb.g, sb.b));
    float grey = 1.0 - smoothstep(0.08, 0.22, (mx - mn) / max(mx, 0.05));
    float road = smoothstep(0.8, 0.95, N.z) * grey * u.tm.y;
    float y = dot(alb, float3(0.2126, 0.7152, 0.0722));
    alb = mix(alb, y * float3(0.97, 1.0, 1.04), road); }
  /* photoscanned ground materials (ambientCG, CC0): asphalt, grass, sand,
     gravel, rock, snow.  Each surface is classified from the game's own
     texture (its broad colour and its slope); the material's photographed
     detail is laid over the game's broad tone in world space, at two scales
     so it does not visibly repeat, with its normal map and roughness.  The
     game's painted road markings stay; far away the game's texture returns */
  bool matdone = false;
  if (u.mat.x > 0.5) {
    constexpr sampler ms(filter::linear, mip_filter::linear, address::repeat, max_anisotropy(16));
    const float3 lw = float3(0.2126, 0.7152, 0.0722);
    /* two rings, 7 and 20 pixels: wide enough that the old textures' colour
       speckle averages out of the classification */
    float3 cbl = ring.sample(ns, in.uv).rgb;
    float mx = max(cbl.r, max(cbl.g, cbl.b)), mn = min(cbl.r, min(cbl.g, cbl.b));
    float sat = (mx - mn) / max(mx, 0.05);
    float grey = 1.0 - smoothstep(0.14, 0.26, sat);
    float green = smoothstep(0.015, 0.06, cbl.g - max(cbl.r, cbl.b));
    float warm = smoothstep(0.03, 0.12, cbl.r - cbl.b) * (1.0 - green) * (1.0 - grey);
    float flat = smoothstep(0.6, 0.85, N.z), steep = 1.0 - flat;
    float dist = distance(u.eye.xyz, wp), fade = 1.0 - smoothstep(140.0, 300.0, dist);
    float snowg = u.wx.z;
    /* the road from the track's own collision surfaces (the road map), in
       every weather; where there is no map, from the colour as before */
    float rm = -1.0, redge = 0.0;
    if (u.rmap.z > 0.0) {
      float2 ru = (wp.xy - u.rmap.xy) * u.rmap.zw;
      float r0 = rmap.sample(ls, ru).r;
      if (r0 > 0.002) {
        rm = smoothstep(0.3, 0.7, r0);
        float re = 0.0;
        for (int k = 0; k < 4; k++) { float a = float(k) * 1.5708 + 0.4; re += rmap.sample(ls, ru + float2(cos(a), sin(a)) * 2.5 * u.rmap.zw).r; }
        redge = 1.0 - saturate(re * 0.25); } }
    bool hasmap = rm >= 0.0;
    float rd = hasmap ? rm : grey;
    /* a race road in snow is ploughed: asphalt, with packed snow along its
       edges and in drifts across it; the rest lies under snow */
    float snowroad = snowg * saturate(redge * 1.4 + (fbm(wp.xy * 0.3) - 0.72) * 1.8);   /* drifts across it: few */
    float offroad = hasmap ? 1.0 - rm : 1.0;
    float w[8];
    w[0] = flat * rd * (1.0 - (hasmap ? snowroad : snowg));               /* asphalt */
    w[1] = flat * green * (1.0 - snowg) * offroad;                        /* grass */
    w[2] = flat * warm * step(0.45, mx) * (1.0 - snowg) * offroad;        /* sand (bright) */
    w[3] = flat * warm * (1.0 - step(0.45, mx)) * (1.0 - snowg) * offroad; /* gravel (darker) */
    /* rock: natural slopes only (earthy or green); buildings, walls and
       banners are painted or plastered and keep the game's own texture */
    w[4] = steep * saturate(warm + green) * smoothstep(0.1, 0.25, N.z);
    w[5] = flat * (hasmap ? mix(snowg, snowroad, rm) : snowg);           /* snow */
    if (mid > 0) {
      /* catalogued: the texture says which ground it is -- or that it is
         no ground at all (a wall, a building, a sign), which keeps its own
         texture; snow still lies where the weather and the road map put it */
      int gs = mid == MAT_ASPHALT || mid == MAT_MARKING ? 0 : mid == MAT_GRASS ? 1 : mid == MAT_SAND ? 2
             : mid == MAT_DIRT ? 3 : mid == MAT_ROCK ? 4 : mid == MAT_SNOW || mid == MAT_ICE ? 5 : -1;
      float sn = w[5];
      for (int m = 0; m < 8; m++) w[m] = 0.0;
      if (gs >= 0) {
        w[gs] = gs == 4 ? 1.0 : flat + (gs == 3 || gs == 1 ? steep * 0.5 : 0.0);
        if (gs != 5 && gs != 4) { w[gs] *= 1.0 - sn; w[5] = sn; }
      } }
    /* the forest floor under the trees: needles, leaf litter and dark earth
       in place of the meadow's grass and, partly, of loose dirt (the litter
       the trees drop at a dirt road's edges) */
    w[6] = 0.0; w[7] = 0.0;
    { float ff = smoothstep(0.06, 0.45, canopy);
      w[6] = (w[1] + w[3] * 0.6) * ff; w[1] *= 1.0 - ff; w[3] *= 1.0 - 0.6 * ff; }
    /* a dirt road is not one soil: stony, washed-out stretches come and go
       along it, a few metres to tens of metres long */
    { float st = smoothstep(0.42, 0.62, fbm(wp.xy * 0.07 + 3.7) * 0.7 + fbm(wp.xy * 0.4) * 0.3);
      w[7] = w[3] * st * flat; w[3] -= w[7]; }   /* roads, not walls */
    /* the asphalt's edge is not a ruled line: within half a metre or so of
       the sealed road's border (the road map's distance) it breaks up along
       a ragged line into gravel and dirt, and the outer metre carries dust
       blown and tracked onto it */
    float edust = 0.0;
    if (hasmap && w[0] > 0.0 && u.tm.y > 0.5) {
      float de = rmap.sample(ls, (wp.xy - u.rmap.xy) * u.rmap.zw).g;
      float jag = (fbm(wp.xy * 0.8) - 0.5) * 1.0 + (vnoise(wp.xy * 5.0) - 0.5) * 0.35 + (vnoise(wp.xy * 19.0) - 0.5) * 0.12;
      float crumble = 1.0 - smoothstep(0.0, 0.12, de + jag - 0.2);
      float mv = w[0] * crumble;
      w[0] -= mv; w[7] += mv * 0.65; w[3] += mv * 0.35;
      edust = (1.0 - smoothstep(0.1, 1.6, de + jag * 0.6)) * w[0]; }
    float ws = 0; for (int m = 0; m < 8; m++) ws += w[m];
    if (ws > 1.0) for (int m = 0; m < 8; m++) w[m] /= ws;
    ws = min(ws, 1.0) * fade;
    const float TILE[8] = { 4.0, 3.0, 4.0, 3.0, 5.0, 4.0, 3.0, 2.5 };
    float3 an = abs(N);
    float2 q = flat > 0.5 ? wp.xy : (an.x > an.y ? wp.yz : wp.xz);
    float3 T1 = flat > 0.5 ? float3(1, 0, 0) : (an.x > an.y ? float3(0, 1, 0) : float3(1, 0, 0));
    float3 T2 = flat > 0.5 ? float3(0, 1, 0) : float3(0, 0, 1);
    float3 tone = pow(cbl, 2.2) * mix(1.0, 0.6, wet) * mix(1.0, 0.7, puddle);
    /* the 1999 road textures bake wear into long light and dark streaks; a
       road is one even grey, so asphalt takes its brightness mostly from a
       far wider average (taps on a different surface -- a wall, the verge,
       their luminance far off -- left out) */
    float ytone;
    { float y0 = dot(tone, lw), ya = y0, na = 1.0;
      float2 rp = 70.0 / float2(ring.get_width(), ring.get_height());
      for (int k = 0; k < 8; k++) {
        float a = float(k) * 0.7854 + 0.2;
        /* a tap on another surface (a car, a tree, the sky) is not this
           road.  The car reads zero here, and the old absolute margin let
           it in at night, where the road's own brightness is below the
           margin: eight dark copies of the car round it on the lit road */
        float2 tq = in.uv + float2(cos(a), sin(a)) * rp;
        if (!is_geo(gp.sample(ns, tq).w)) continue;
        float yq = dot(pow(ring.sample(ls, tq).rgb, 2.2), lw) * mix(1.0, 0.6, wet) * mix(1.0, 0.7, puddle);
        if (abs(yq - y0) < 0.5 * y0) { ya += yq; na += 1.0; } }
      ytone = mix(ya / na, y0, 0.3); }
    float y = dot(lin, lw), yb = dot(pow(cbl, 2.2), lw);
    /* painted markings are white or yellow: bright, and blue lowest */
    float3 tx = saturate(C.rgb);
    float paint = (1.0 - smoothstep(0.12, 0.25, (max(tx.r, max(tx.g, tx.b)) - min(tx.r, min(tx.g, tx.b))) / max(tx.r + tx.g, 0.05) * 2.0))
                + smoothstep(0.1, 0.25, min(tx.r, tx.g) - tx.b);
    float mark = smoothstep(1.4, 1.9, y / max(yb, 1e-3)) * w[0] * saturate(paint)
               * (hasmap ? 1.0 - snowg : 1.0);        /* in snow the game's road is white streaks, not paint */
    if (mid == MAT_MARKING) mark = max(mark, 0.85 * smoothstep(1.15, 1.5, y / max(yb, 1e-3)));
    /* parallax occlusion: the dominant material's height map, marched along
       the view ray in its tangent space, shifts every map's lookup so stones,
       cracks and ridges stand up at grazing angles; then a short march toward
       the sun shades their far sides */
    const float HGT[8] = { 0.012, 0.02, 0.02, 0.07, 0.05, 0.03, 0.03, 0.06 }; /* metres, full range */
    int md = 0; for (int m = 1; m < 8; m++) if (w[m] > w[md]) md = m;
    float2 poff = 0; float hsurf = 0.5, pshadow = 1.0;
    /* a dirt road's stones and ruts stand up further out than asphalt's grain */
    float pk = (1.0 - (md == 3 || md == 7 ? smoothstep(14.0, 40.0, dist) : smoothstep(6.0, 20.0, dist))) * step(0.02, w[md]);
    if (pk > 0.0) {
      float3 Vt = float3(dot(V, T1), dot(V, T2), max(dot(V, N), 0.08));
      float hs = HGT[md] / TILE[md] * pk;
      float2 stepv = -Vt.xy / Vt.z * hs / 12.0;
      float2 uvp = q / TILE[md]; float lay = 1.0, hcur = matA.sample(ms, uvp, md).a;
      for (int i = 0; i < 12 && hcur < lay; i++) { lay -= 1.0 / 12.0; uvp += stepv; hcur = matA.sample(ms, uvp, md).a; }
      poff = (uvp - q / TILE[md]) * TILE[md];
      hsurf = hcur;
      float3 Lt = float3(dot(L, T1), dot(L, T2), max(dot(L, N), 0.05));
      float2 sp = uvp; float sl = hsurf; float2 sst = Lt.xy / Lt.z * hs / 6.0;
      for (int i = 0; i < 6; i++) { sp += sst; sl += 1.0 / 6.0 * (Lt.z < 0.3 ? 0.5 : 1.0);
        float hh = matA.sample(ms, sp, md).a; if (hh > sl) pshadow = min(pshadow, 1.0 - saturate((hh - sl) * 6.0)); }
      pshadow = mix(1.0, pshadow, pk); }
    float3 ma = 0, mnrm = 0; float mr = 0, mao = 0, wt = 0;
    for (int m = 0; m < 8; m++) {
      if (w[m] < 0.02) continue;
      float2 uv1 = (q + poff) / TILE[m], uv2 = (q + poff * 0.29) / TILE[m] * 0.29 + float2(0.37, 0.61);
      /* the scans are millimetre photographs.  Out to several car lengths their
         stones show, crisp, at most of their own contrast; further out, at
         full contrast, every stone is a lone pixel, a grit that TAA cannot
         settle and motion blur draws into streaks.  So with distance they
         are sampled coarser and fade to a mottling of an even surface --
         asphalt most of all, one even grey by sixty metres */
      float near = 1.0 - smoothstep(8.0, 60.0, dist), mb = mix(1.5, 0.0, near);
      float4 c1 = matA.sample(ms, uv1, m, bias(mb)), c2 = matA.sample(ms, uv2, m, bias(mb));
      float4 n1 = matN.sample(ms, uv1, m, bias(2.0)), n2 = matN.sample(ms, uv2, m, bias(2.0));
      float4 n = mix(n1, n2, 0.35);
      float3 c = pow(mix(c1.rgb, c2.rgb, 0.35), 2.2);
      bool dirt = m == 3 || m == 7;
      float3 cr = 1.0 + (c / max(u.matmean[m].rgb, 0.02) - 1.0) * mix(m == 0 ? 0.25 : dirt ? mix(0.35, 0.5, flat) : 0.35, dirt ? mix(0.8, 1.0, flat) : m == 0 ? 1.1 : 0.8, near);
      /* asphalt takes the game's brightness but not its purple cast (in
         snow the game paints the road white: there it is dark wet tarmac);
         rock and snow take the game's colour and only their relief from the
         scan -- the scanned snow lies over grass, the scanned rock is gold */
      float cl = 1.0 + (dot(c, lw) / max(dot(u.matmean[m].rgb, lw), 0.02) - 1.0) * mix(0.35, 0.8, near);
      /* aged asphalt is a mid grey (albedo ~0.12), not the 1999 textures'
         near-black: drawn toward it, keeping the game's light and dark */
      float yasp = u.tmo.w > 0.0 ? mix(ytone, u.tmo.w, 0.7) : ytone;
      if (m == 0) ma += w[m] * float3(mix(yasp, 0.075, hasmap ? snowg : 0.0)) * float3(1.02, 1.0, 0.97) * cr;
      else if (m == 4) ma += w[m] * tone * cl;
      else if (m == 5) ma += w[m] * float3(0.84, 0.87, 0.93) * mix(0.9, 1.05, saturate(cl));
      /* the forest floor is not the game's grass green: its own colour, at
         the brightness of what the game painted there */
      else if (m == 6) ma += w[m] * c * (dot(tone, lw) / max(dot(u.matmean[m].rgb, lw), 0.02)) * 0.85;
      /* dirt: the photographed soil's own colour (the 1999 texture's
         orange is a flat paint), at the game's brightness */
      else if (dirt) ma += w[m] * mix(tone * cr, c * (dot(tone, lw) / max(dot(u.matmean[m].rgb, lw), 0.02)), 0.65 * flat);
      else ma += w[m] * tone * cr;
      mnrm += w[m] * float3(n.xy * 2.0 - 1.0, 0.0);
      mr += w[m] * n.z; mao += w[m] * n.w; wt += w[m]; }
    if (wt > 0.0) {
      ma /= wt; mnrm /= wt; mr /= wt; mao /= wt;
      float k = ws * (1.0 - mark);
      alb = mix(alb, ma * mix(1.0, mao, 0.6), k);
      alb = mix(alb, u.matmean[3].rgb * 0.75 * (0.8 + 0.4 * fbm(wp.xy * 2.3)), edust * 0.45 * k);
      /* dirt: its stones and ruts in full relief, further out; and the
         road's own bumps and dips, a few metres across */
      float dw = (w[3] + w[7]) / max(wt, 1e-3);
      float nk = k * (1.0 - mix(smoothstep(10.0, 60.0, dist), smoothstep(25.0, 90.0, dist), dw));
      N = normalize(N + (T1 * mnrm.x + T2 * mnrm.y) * mix(0.35, 0.8, dw) * nk);
      if (dw > 0.05 && flat > 0.5) {
        float e = 0.25, h0 = fbm(wp.xy * 0.3), hx = fbm((wp.xy + float2(e, 0)) * 0.3), hy = fbm((wp.xy + float2(0, e)) * 0.3);
        float bk = dw * k * (1.0 - smoothstep(30.0, 120.0, dist)) * 0.35 / e;   /* 35 cm of rise and fall */
        N = normalize(N - float3(hx - h0, hy - h0, 0.0) * bk);
        /* the dips hold the damp: darker soil, a little less rough (dry
           weather too; it is earth, so no mirror) */
        float damp = smoothstep(0.42, 0.3, h0) * dw * k;
        alb *= mix(1.0, 0.72, damp);
        mrough = mix(mrough, mrough * 0.8, damp); }
      ndl = saturate(dot(N, L));
      mrough = mix(mrough, mr, k);
      /* rain gathers in the low spots of the surface first */
      float pud2 = wet * u.p0.w * smoothstep(0.38, 0.18, hsurf) * k * (1.0 - tk);
      if (pud2 > puddle) { alb *= mix(1.0, 0.7, pud2 - puddle); puddle = pud2; }
      sh *= mix(1.0, pshadow, k);
      matdone = true; }
    /* the tyres' marks (host_marks.m): ruts pressed into loose ground with
       lips of pushed-out soil, relit from their own relief; rubber on
       asphalt; water standing in the ruts in the rain */
    if (u.mk.z > 0.0 && dist < 90.0 && N.z > 0.5) {
      float e = 1.0 / u.mk.z;
      float2 m0 = mark_at(mkp, mka, u.mk, wp.xy, wp.z);
      float2 mx = mark_at(mkp, mka, u.mk, wp.xy + float2(e, 0), wp.z), my = mark_at(mkp, mka, u.mk, wp.xy + float2(0, e), wp.z);
      float fk = 1.0 - smoothstep(50.0, 90.0, dist);
      float asph = w[0], range = 0.2;                  /* the map's full range: 20 cm */
      float2 g = float2(mx.x - m0.x, my.x - m0.x) * range / e;
      N = normalize(N - float3(g, 0.0) * fk * (1.0 - asph));
      float pressed = saturate((0.5 - m0.x) * 4.0), heaped = saturate((m0.x - 0.5) * 6.0);
      alb *= 1.0 - m0.y * mix(0.15, 0.65, asph) * fk;
      alb *= mix(1.0, 0.82, pressed * (1.0 - asph) * fk);
      alb *= 1.0 + 0.1 * heaped * fk;
      mrough = mix(mrough, mrough * 0.85, m0.y * asph);
      puddle = max(puddle, wet * u.p0.w * smoothstep(0.1, 0.6, pressed) * (1.0 - asph) * fk);
      ndl = saturate(dot(N, L)); }
    if (int(u.p3.y) == 14) return float4(w[0] + w[3] + w[4] * 0.5, w[1] + w[4] * 0.5 + w[5], w[2] + w[3] + w[5], G.a); }
  /* asphalt, resynthesised: the game's road texture is baked streaks and
     colour speckle.  On flat grey surfaces (judged on a wide blur, which the
     speckle averages out of) keep only that blur's tone and the painted
     markings (texels well above it), and lay a world-space asphalt over it:
     patches, fine grain and aggregate */
  if (!matdone) { const float3 lw = float3(0.2126, 0.7152, 0.0722);
    float3 cbl = ring.sample(ns, in.uv).rgb;
    float mx = max(cbl.r, max(cbl.g, cbl.b)), mn = min(cbl.r, min(cbl.g, cbl.b));
    float grey = 1.0 - smoothstep(0.1, 0.2, (mx - mn) / max(mx, 0.05));
    float road = smoothstep(0.85, 0.96, N.z) * grey * (1.0 - u.wx.z) * step(0.5, u.tm.y)
               * (mid == 0 || mid == MAT_ASPHALT || mid == MAT_MARKING ? 1.0 : 0.0);
    if (road > 0.0) {
      float yb = dot(pow(cbl, 2.2), lw);
      float y = dot(lin, lw), dist = distance(u.eye.xyz, wp);
      float mark = smoothstep(1.4, 1.9, y / max(yb, 1e-3));
      float f1 = smoothstep(80.0, 20.0, dist), f2 = smoothstep(30.0, 6.0, dist);
      float n = 1.0 + (fbm(wp.xy * 0.35) - 0.5) * 0.35
              + (fbm(wp.xy * 6.0) - 0.5) * 0.3 * f1
              + (step(0.8, vnoise(wp.xy * 55.0)) * 0.3 - vnoise(wp.xy * 23.0) * 0.12) * f2;
      float3 asph = yb * n * float3(0.98, 1.0, 1.03) * mix(1.0, 0.6, wet) * mix(1.0, 0.7, puddle);
      alb = mix(alb, asph, road * (1.0 - mark)); }
    if (int(u.p3.y) == 13) return float4(road, grey, smoothstep(0.85, 0.96, N.z), G.a); }
  /* close up, the 1999 textures run out of detail: add the material's own
     fine structure, in world space -- aggregate in asphalt, blades in grass,
     grain in sand, stone on walls -- as colour variation and relief */
  { float dist = distance(u.eye.xyz, wp), k = smoothstep(26.0, 3.0, dist) * u.wx.w * (matdone || terr ? 0.0 : 1.0);
    if (k > 0.0) {
      float3 sb = saturate(C.rgb); float mx = max(sb.r, max(sb.g, sb.b)), mn = min(sb.r, min(sb.g, sb.b));
      float sat = (mx - mn) / max(mx, 0.05);
      float green = smoothstep(0.02, 0.08, sb.g - max(sb.r, sb.b));
      float sand = smoothstep(0.04, 0.14, sb.r - sb.b) * (1.0 - green);
      float grey = 1.0 - smoothstep(0.06, 0.18, sat);
      float3 an = abs(N);
      float2 q = an.z > 0.6 ? wp.xy : (an.x > an.y ? wp.yz : wp.xz);
      float f = green > 0.5 ? 60.0 : sand > 0.5 ? 45.0 : 30.0;
      float e = 0.35 / f;
      float h0 = fbm(q * f), hx = fbm((q + float2(e, 0)) * f), hy = fbm((q + float2(0, e)) * f);
      float spk = step(0.78, vnoise(q * 140.0)) * grey;              /* aggregate stones */
      float blades = green * (vnoise(float2(q.x * 180.0, q.y * 22.0)) * 0.5 + vnoise(float2(q.x * 25.0, q.y * 170.0)) * 0.5);
      float amp = grey * 0.22 + green * 0.3 + sand * 0.16 + 0.08;
      alb *= 1.0 + k * ((h0 - 0.5) * amp * 1.6 + spk * 0.25 - blades * 0.25);
      float3 g3 = float3(hx - h0, hy - h0, 0.0) * (0.9 * k);
      float3 t1 = an.z > 0.6 ? float3(1, 0, 0) : (an.x > an.y ? float3(0, 1, 0) : float3(1, 0, 0));
      float3 t2 = an.z > 0.6 ? float3(0, 1, 0) : float3(0, 0, 1);
      N = normalize(N - (t1 * g3.x + t2 * g3.y) * 6.0);
      ndl = saturate(dot(N, L)); } }
  float3 amb = mix(u.grnd.rgb, u.skyc.rgb, N.z * 0.5 + 0.5) * ao;
  /* in the landscape: the light the sky past the horizon and the sunlit land
     around send (host_terrain.m's solve), for this surface's facing (a wall
     sees half of it, the ground below the rest) */
  if (TL.w > 0.0) {
    float3 gi = TL.rgb * mix(0.55, 1.0, saturate(N.z)) + u.grnd.rgb * 0.15 * (1.0 - saturate(N.z));
    amb = mix(amb, gi * ao, TL.w); }
  float3 dl = u.sunc.rgb * ndl * sh;
  float3 fl = u.flash.rgb * u.flash.w * saturate(dot(N, normalize(float3(0.3, 0.2, 1.0))) * 0.7 + 0.3);
  /* bounce light carries the occluders' brightness, not their texture noise */
  float bounce = dot(A.rgb, float3(0.2126, 0.7152, 0.0722));
  float3 o = alb * ((dl * rdir + amb + fl) * u.p4.w + bounce * u.p0.y * ao);
  /* grass (host_terrain.m's leaves): thin enough that the sun shines through
     them from behind, yellow-green; and a waxy sheen toward the sun */
  if (mid == 11) {
    float thru = pow(saturate(dot(-V, L)) * 0.6 + 0.4, 3.0) * (0.35 + 0.65 * saturate(-dot(N, L) + 0.5));
    o += alb * float3(1.1, 1.25, 0.55) * u.sunc.rgb * thru * sh * 0.85 * u.p4.w;
    float3 Hs = normalize(L + V); float ns2 = saturate(dot(N, Hs));
    o += u.sunc.rgb * pow(ns2, 24.0) * 0.12 * ndl * sh * ao; }
  /* sun specular, GGX */
  float3 H = normalize(L + V); float nh = saturate(dot(N, H)), vh = saturate(dot(V, H));
  float rough = mix(mrough, mix(0.3, 0.08, puddle), wet), a2 = rough * rough * rough * rough;
  float d = nh * nh * (a2 - 1.0) + 1.0, D = a2 / (3.14159 * d * d);
  float F = 0.04 + 0.96 * pow(1.0 - vh, 5.0);
  /* dry asphalt is matte: highlights come only from water on it */
  o += u.sunc.rgb * D * F * ndl * sh * 0.25 * wet * ao;
  /* headlights: each car's two beams light what is in front of it, with a
     glint where the road is wet */
  { float3 hd = 0, hs = 0; int n = int(hl.misc.x);
    for (int i = 0; i < n; i++) {
      float3 Ld; float w = spotw(hl, i, wp + N * 0.05, Ld);
      float3 lc = spotc(hl, i);
      if (w <= 0.0) continue;
      /* rough ground lit from beside the eye sends much of the light back
         toward it (the opposition effect of asphalt and dirt): a flattened
         cosine, so the pool on the road reads at grazing angles */
      float nl = saturate(dot(N, Ld));
      if (hl.p[i].w < 0.5) nl = max(nl, 0.35 * saturate(dot(Ld, V)) * step(0.0, dot(N, Ld)));
      float3 H2 = normalize(Ld + V); float n2 = saturate(dot(N, H2));
      float d2 = n2 * n2 * (a2 - 1.0) + 1.0;
      hd += lc * w * nl;
      hs += lc * w * nl * (1.0 - hl.p[i].w) * (a2 / (3.14159 * d2 * d2)) * (0.04 + 0.96 * pow(1.0 - saturate(dot(V, H2)), 5.0)); }
    /* the eye adapted to the night sees the pool on the road far brighter
       than the grazing irradiance alone: a gain on surfaces */
    /* the lamps light the surface's true colour: the game's baked light
       (and its darkening for the night) divided back out */
    float3 albh = min(alb / max(bake, 0.3 * bref), 0.7);
    hd = hd / (1.0 + 0.2 * hd);                      /* a lamp a few metres off lights, not burns */
    o += 3.0 * (albh * hd * ao + hs * 0.25 * wet);
    if (int(u.p3.y) == 16) return float4(float3(hd.x, alb.g * 4.0, ao) , G.a); }   /* debug: headlight irradiance, albedo, occlusion */
  /* reflections */
  float4 S = ssr.sample(ls, in.uv);
  /* open water: flat, and blue-green where everything else is not.  Moving
     waves, the sky and the scene mirrored with Fresnel, the sun's glitter */
  { float3 sb = saturate(C.rgb);
    /* modelled water must be clearly blue-green: snowy and wet roads are
       greenish-grey too */
    /* modelled water: clearly blue-green, or sea-coloured and below the road
       (wet and snowy roads are greenish-grey too, but sit higher) */
    float water = u.sk2.w * step(0.97, N.z) * max(smoothstep(1.12, 1.24, sb.b / max(sb.r, 0.02))
                * smoothstep(1.08, 1.18, sb.g / max(sb.r, 0.02)), smoothstep(1.04, 1.1, sb.g / max(sb.r, 0.02)) * (1.0 - smoothstep(0.2, 0.45, wp.z)));
    if (mid > 0) water = mid == MAT_WATER ? u.sk2.w * smoothstep(0.9, 0.97, N.z) : 0.0;   /* catalogued: no guessing */
    if (water > 0.0) {
      float3 Nw = waveN(wp.xy, u.tm.x, 0.3 * smoothstep(250.0, 15.0, distance(u.eye.xyz, wp)) + 0.05);
      float3 R = reflect(-V, Nw);
      float3 refl = mix(skylight(u, sky, float3(R.xy, abs(R.z))), S.rgb, S.a * 0.8);
      float Fw = 0.02 + 0.98 * pow(1.0 - saturate(dot(Nw, V)), 5.0);
      float3 Hw = normalize(L + V); float nhw = saturate(dot(Nw, Hw));
      float aw = 0.1 * 0.1 * 0.1 * 0.1, dw = nhw * nhw * (aw - 1.0) + 1.0;
      float glint = aw / (3.14159 * dw * dw) * 0.012 * saturate(dot(Nw, L)) * sh;
      float3 body = lin * 0.5 * (amb + dl * 0.4);
      float3 ow = mix(body, refl, saturate(Fw * 1.2 + 0.08)) + u.sunc.rgb * glint;
      o = mix(o, ow, water); }
    if (int(u.p3.y) == 11) return float4(water, is_geo(P.w) ? 0.5 : 0.0, 0, G.a); }
  float Fe = 0.04 + 0.96 * pow(1.0 - saturate(dot(N, V)), 5.0);
  /* and so are reflections: none dry, a damp sheen, mirrors in the puddles */
  float rw = saturate(u.p0.z * wet * mix(0.12 + 0.3 * Fe, 0.45 + 0.55 * Fe, puddle / max(wet, 1e-3)) * S.a) * ao;
  o = mix(o, S.rgb * u.p4.w * 1.1, rw);
  /* aerial perspective: distant surfaces pick up the sky's colour */
  { float dd = distance(u.eye.xyz, wp), hz = (1.0 - exp(-dd * u.fogc.w)) * u.nsky.w;
    /* past the old world's few hundred metres, the landscape: by the air's
       depth along the ray */
    if (u.ter[1].w > 0.5) {
      float od = air_depth(u, u.eye.xyz, wp) * u.fogc.w * u.air2.x;
      float hk = 1.0 - exp(-od);
      hz = mix(hz, max(hk, hz * 0.5), smoothstep(200.0, 800.0, dd)); }
    o = mix(o, air(u, -V), hz); }
  /* the game's own fog, over the relit surface: its colour, by its amount */
  o = mix(o, u.fogc.rgb * u.p4.z, fogk);
  int dbg = int(u.p3.y);
  if (dbg == 1) o = N * 0.5 + 0.5;
  else if (dbg == 2) o = float3(sh);
  else if (dbg == 3) o = float3(A.a);
  else if (dbg == 4) o = fract(wp / 50.0);
  else if (dbg == 5) o = A.rgb;
  else if (dbg == 6) o = S.rgb * S.a;
  else if (dbg == 12) o = float3(wp.z < 0.0, wp.z < 0.6, wp.z < 1.2);
  else if (dbg == 7 || dbg == 8) {
    float4 c = u.svp * float4(wp, 1); float2 st = float2(c.x * 0.5 + 0.5, 0.5 - c.y * 0.5);
    o = dbg == 7 ? float3(sm.sample(ns, st), fract(st * 8.0)) : float3(c.z, fract(st * 8.0)); }
  if (any(isnan(o)) || any(isinf(o))) o = lin;
  return float4(o, G.a); }

/* ---- bloom */
fragment float4 prefs(QO in [[stage_in]], constant FXU &u [[buffer(0)]], texture2d<float> h [[texture(0)]]) {
  float2 px = 1.0 / float2(h.get_width(), h.get_height());
  float4 c = (h.sample(ls, in.uv + px * float2(-1, -1)) + h.sample(ls, in.uv + px * float2(1, -1)) +
              h.sample(ls, in.uv + px * float2(-1, 1)) + h.sample(ls, in.uv + px * float2(1, 1))) * 0.25;
  float l = dot(c.rgb, float3(0.2126, 0.7152, 0.0722));
  float k = max(l - u.scr.z, 0.0); k = k * k / (k + 0.4);
  return float4(c.rgb * (k / max(l, 1e-4)) * c.a, 1); }
fragment float4 downfs(QO in [[stage_in]], texture2d<float> h [[texture(0)]]) {
  float2 px = 1.0 / float2(h.get_width(), h.get_height());
  float3 c = h.sample(ls, in.uv).rgb * 4.0;
  c += h.sample(ls, in.uv + px * float2(-1, -1)).rgb + h.sample(ls, in.uv + px * float2(1, -1)).rgb;
  c += h.sample(ls, in.uv + px * float2(-1, 1)).rgb + h.sample(ls, in.uv + px * float2(1, 1)).rgb;
  return float4(c / 8.0, 1); }
fragment float4 upfs(QO in [[stage_in]], texture2d<float> h [[texture(0)]]) {
  float2 px = 1.0 / float2(h.get_width(), h.get_height());
  float3 c = h.sample(ls, in.uv).rgb * 4.0;
  c += (h.sample(ls, in.uv + float2(px.x, 0)).rgb + h.sample(ls, in.uv - float2(px.x, 0)).rgb +
        h.sample(ls, in.uv + float2(0, px.y)).rgb + h.sample(ls, in.uv - float2(0, px.y)).rgb) * 2.0;
  c += h.sample(ls, in.uv + px).rgb + h.sample(ls, in.uv - px).rgb +
       h.sample(ls, in.uv + float2(px.x, -px.y)).rgb + h.sample(ls, in.uv - float2(px.x, -px.y)).rgb;
  return float4(c / 16.0, 1); }

/* ---- light shafts (half res): radial scattering of the sky around the sun */
/* headlight beams: light the air scatters along the view ray, up to the
   surface the ray meets */
float3 beams(constant FXU &u, constant HL &hl, texture2d<float> gp, float2 uv, float2 pos) {
  int n = int(hl.misc.x);
  float3 acc = 0;
  if (n == 0 || hl.misc.y <= 0.0) return 0;
  float4 P = gp.sample(ns, uv);
  float3 vd = view_dir(u, uv);
  float tmax = is_solid(P.w) ? distance(u.eye.xyz, P.xyz) : 90.0;
  tmax = min(tmax, 90.0);
  const int NS = 20;
  float dt = tmax / float(NS), t = dt * ign(pos);
  for (int k = 0; k < NS; k++, t += dt) {
    float3 x = u.eye.xyz + vd * t, Ld;
    for (int i = 0; i < n; i++) {
      float w = spotw(hl, i, x, Ld);
      /* forward scattering: brighter looking into a beam */
      acc += spotc(hl, i) * w * (0.35 + 0.65 * pow(saturate(dot(-vd, Ld) * 0.5 + 0.5), 4.0)); } }
  return acc * dt * hl.misc.y * hl.misc.w * 0.33; }
/* the lamps themselves: a glow where each lamp faces the eye and nothing
   stands in front of it (white heads, red tails), with a soft halo */
float3 lamps(constant FXU &u, constant HL &hl, texture2d<float> gp, float2 uv) {
  int n = int(hl.gmisc.x);
  float3 acc = 0;
  float2 scr = float2(gp.get_width(), gp.get_height());
  for (int i = 0; i < n; i++) {
    float3 lp = hl.g[i].xyz, toe = u.eye.xyz - lp;
    float d = length(toe), face = saturate(dot(hl.gd[i].xyz, toe / d));
    if (face <= 0.0) continue;
    float cw; float2 lu = to_uv(u, lp, cw);
    if (cw <= 0) continue;
    float4 Q = gp.sample(ns, lu);
    if (is_solid(Q.w) && distance(u.eye.xyz, Q.xyz) < d - 0.35) continue;   /* hidden */
    float px = length((uv - lu) * scr), s = clamp(40.0 / d, 0.8, 8.0);
    float core = exp(-px * px / (s * s)), halo = 1.0 / (1.0 + pow(px / (s * 2.5), 2.0)) * smoothstep(s * 10.0, s * 3.0, px);
    bool tail = hl.g[i].w > 0.5;
    float3 c = tail ? float3(1.0, 0.06, 0.03) * 0.6 * hl.gd[i].w : float3(1.0, 0.95, 0.85) * hl.gd[i].w;
    /* a brake lamp blooms: a wide red glow round each lens, stronger the
       harder it burns (gd.w: 0.5 lit, 2.2 braking) */
    float bloom = tail ? 1.0 / (1.0 + pow(px / (s * 6.0), 2.0)) * smoothstep(s * 24.0, s * 6.0, px) * 0.12 * hl.gd[i].w : 0.0;
    acc += c * (core * 6.0 + halo * 0.08 + bloom) * pow(face, 2.0); }
  return acc * hl.gmisc.y; }
fragment float4 shaftfs(QO in [[stage_in]], constant FXU &u [[buffer(0)]],
                        texture2d<float> gp [[texture(0)]], texture2d<float> col [[texture(1)]],
                        constant HL &hl [[buffer(1)]]) {
  float3 bm = beams(u, hl, gp, in.uv, in.pos.xy) + lamps(u, hl, gp, in.uv);
  float cw; float2 sp = to_uv(u, u.eye.xyz + u.sun.xyz * 8000.0, cw);
  if (cw <= 0 || u.p2.x <= 0) return float4(bm, 1);
  float2 d = (in.uv - sp) / 32.0 * 0.9; float2 q = in.uv; float dec = 1.0, acc = 0;
  float j = ign(in.pos.xy);
  q -= d * j;
  for (int i = 0; i < 32; i++) {
    q -= d;
    if (all(q >= 0.0) && all(q <= 1.0) && abs(gp.sample(ns, q).w - 3.0) < 0.5) {
      float l = dot(pow(col.sample(ls, q).rgb, 2.2), float3(0.3, 0.5, 0.2));
      acc += l * l * dec * 2.0; }
    dec *= 0.94; }
  float g = saturate(1.0 - length((in.uv - sp) * float2(1.333, 1.0)) * 0.9);
  return float4(u.sunc.rgb * acc / 32.0 * (0.4 + g) * u.p2.x + bm, 1); }

/* ---- smooth shading: the game's models are low-poly and its G-buffer
   normals are per face.  Average each pixel's normal with its neighbours on
   the same continuous surface (close to its plane, bent by under ~50 deg),
   across a disc a fixed size in the world, as a modeller's smoothing groups
   would; creases sharper than that stay sharp */
fragment float4 nrmfs(QO in [[stage_in]], constant FXU &u [[buffer(0)]],
                      texture2d<float> gn [[texture(0)]], texture2d<float> gp [[texture(1)]]) {
  float4 G = gn.sample(ns, in.uv), P = gp.sample(ns, in.uv);
  if (!is_geo(P.w)) return G;
  float3 N0 = onormal(G.xyz, P.xyz, u.eye.xyz);
  float de = distance(u.eye.xyz, P.xyz);
  /* far away a face is a few pixels: nothing to smooth */
  if (de > 140.0) return float4(N0, G.a);
  /* a 0.9 m disc, in pixels at this distance; the taps are TAA-rotated, so
     eight a frame accumulate to many */
  float2 scr = float2(gn.get_width(), gn.get_height());
  float rpx = clamp(0.9 * u.scr.w / max(de, 0.5), 2.0, 40.0);
  float3 acc = N0; float wsum = 1.0;
  float rot = (ign(in.pos.xy) + u.p2.w * 0.618) * 6.2831853;
  for (int i = 0; i < 8; i++) {
    float r = sqrt((float(i) + 0.5) / 8.0) * rpx, a = rot + float(i) * 2.3999632;
    float2 q = in.uv + float2(cos(a), sin(a)) * r / scr;
    float4 Q = gp.sample(ns, q);
    float plane = abs(dot(N0, Q.xyz - P.xyz)) / (0.02 * de + 0.05);
    if (!is_geo(Q.w) || plane >= 1.0) continue;
    float3 Nk = onormal(gn.sample(ns, q).xyz, Q.xyz, u.eye.xyz);
    float w = (1.0 - plane) * smoothstep(0.62, 0.9, dot(N0, Nk));
    acc += Nk * w; wsum += w; }
  return float4(normalize(acc), G.a); }

/* ---- tyre tracks: ribbons along each wheel's path, drawn into a mask the
   lighting pass reads (r strength, g the ground's height, to reject pixels
   the ribbon passes in front of) */
struct TV { float4 p; float4 a; };                 /* world xyz + strength; across */
struct TO { float4 pos [[position]]; float k; float z; float a; };
vertex TO trkvs(uint vid [[vertex_id]], const device TV *v [[buffer(0)]], constant FXU &u [[buffer(1)]]) {
  TV g = v[vid]; TO o;
  float4 c = u.vp * float4(g.p.xyz, 1);
  float sx = u.vpt.x * c.x + u.vpt.y * c.w, sy = u.vpt.z * c.y + u.vpt.w * c.w;
  float yd = u.p3.x > 0.5 ? 480.0 * c.w - sy : sy;
  o.pos = float4(u.m3.x * sx + u.m3.y * c.w, u.m3.z * yd + u.m3.w * c.w, 0.5 * c.w, c.w);
  o.k = g.p.w; o.z = g.p.z; o.a = g.a.x; return o; }
fragment float4 trkfs(TO in [[stage_in]]) {
  float e = 1.0 - in.a * in.a;
  /* the tread: fine bands across the ribbon */
  float tread = 0.8 + 0.2 * step(0.5, fract(in.a * 3.0 + 0.5));
  return float4(in.k * e * tread, in.z, 0, 1); }

/* ---- volumetric particles (smoke, dust, snow and water spray) and falling
   rain and snow, ray-marched per pixel at half resolution; premultiplied.
   The particles are not drawn one by one: each is a soft kernel of density,
   the kernels sum into one field, and a tiling billow noise carved into that
   field gives the plume its rolls and wisps.  Light is the sun through the
   plume (Beer-Lambert toward the sun, with a powder term), the sky through
   the plume above, and the headlights. */
struct PF { float4 p[256]; float4 c[256]; float4 s[256]; float4 misc; float4 amb; };   /* s: the plume's depth toward the sun, and above */
constexpr sampler rs3(filter::linear, address::repeat);
fragment float4 pfxfs(QO in [[stage_in]], constant FXU &u [[buffer(0)]], constant HL &hl [[buffer(1)]],
                      const device PF &pf [[buffer(2)]], texture2d<float> gp [[texture(0)]],
                      texture3d<float> nz [[texture(1)]]) {
  float3 vd = view_dir(u, in.uv), eye = u.eye.xyz;
  float4 P = gp.sample(ns, in.uv);
  float dmax = is_solid(P.w) ? distance(eye, P.xyz) : 1e5;
  float T = 1.0; float3 Lc = 0;
  float t = u.tm.x, j0 = ign(in.pos.xy);
  int n = int(pf.misc.x);
  /* the particles this ray passes through */
  int ix[32]; int nh = 0; float ta = 1e9, tb = 0;
  /* the plume's rectangle on screen (CPU): nothing to do outside it */
  if (any(in.uv < pf.misc.yz) || in.uv.x > pf.misc.w || in.uv.y > pf.amb.w) n = 0;
  for (int i = 0; i < n && nh < 32; i++) {
    float3 c = pf.p[i].xyz; float r = pf.p[i].w;
    float3 oc = c - eye; float b = dot(oc, vd), dd = dot(oc, oc) - b * b;
    if (dd >= r * r) continue;
    float h = sqrt(r * r - dd), t0 = max(b - h, 0.05), t1 = min(b + h, dmax);
    if (t1 <= t0) continue;
    ix[nh++] = i; ta = min(ta, t0); tb = max(tb, t1); }
  if (nh > 0) {
    float mu = dot(vd, u.sun.xyz);
    /* forward scattering (Henyey-Greenstein, g 0.55) over an even floor */
    float g = 0.55, hg = (1.0 - g * g) / pow(1.0 + g * g - 2.0 * g * mu, 1.5) * 0.0796;
    float phase = 0.35 + 3.0 * hg;
    float3 hd = 0, Ld;
    { float3 xm = eye + vd * (0.5 * (ta + tb));
      for (int m = 0; m < int(hl.misc.x); m++) hd += spotc(hl, m) * spotw(hl, m, xm, Ld) * 0.33; }
    float3 sunL = u.sunc.rgb * phase, ambL = pf.amb.rgb, hdL = hd * 1.5;
    const int NS = 8;
    float dt = (tb - ta) / float(NS);
    for (int s = 0; s < NS && T > 0.01; s++) {
      float3 x = eye + vd * (ta + (float(s) + j0) * dt);
      float D = 0; float3 alb = 0; float ods = 0, odu = 0;
      for (int k = 0; k < nh; k++) {
        int i = ix[k]; float3 d = x - pf.p[i].xyz; float r = pf.p[i].w;
        float q = dot(d, d) / (r * r);
        if (q < 1.0) {
          float w = 1.0 - q; w *= w * pf.c[i].w; D += w; alb += w * pf.c[i].rgb;
          /* the other puffs between here and the light (CPU), and this
             puff's own depth on the far side of the point */
          float3 dn = d / r;
          ods += w * (pf.s[i].x + pf.c[i].w * r * 0.5 * (1.0 - dot(dn, u.sun.xyz)));
          odu += w * (pf.s[i].y + pf.c[i].w * r * 0.5 * (1.0 - dn.z)); } }
      if (D < 0.002) continue;
      alb /= D; ods /= D; odu /= D;
      /* billows, rolling up and outward; fine wisps eat the thin edges */
      /* two scales that never line up, the second turned off the axes, so
         the tiling volume shows no period across a plume */
      float3 xr = float3(0.8 * x.x - 0.6 * x.y, 0.6 * x.x + 0.8 * x.y, x.z);
      float bil = nz.sample(rs3, x * 0.11 + float3(0.0, 0.0, -t * 0.03)).r * 0.55
                + nz.sample(rs3, xr * 0.317 + float3(0.37, 0.71, -t * 0.09)).r * 0.45;
      float wisp = nz.sample(rs3, xr * 0.93 + float3(t * 0.05, 0.13, -t * 0.2)).g;
      /* the noise shapes the density, it never cuts it into islands: thin
         mist stays a mist, thick smoke gets its rolls, the edges fray */
      float edge = 1.0 - saturate(D * 1.5);
      float b2 = smoothstep(0.3, 0.7, bil);
      float sh = mix(0.12, 1.5, b2 * b2) * (1.0 - wisp * 0.8 * edge);
      float sig = 2.0 * min(D, 1.5) * sh;
      if (sig < 1e-3) continue;
      float ts = exp(-ods * 0.7), tu = exp(-odu * 0.45);
      float powder = 1.0 - exp(-sig * 2.0);
      float3 L = alb * (sunL * ts * mix(0.6, 1.0, powder) + ambL * (0.45 + 0.55 * tu) + hdL);
      float a = 1.0 - exp(-sig * dt);
      Lc += T * a * L; T *= 1.0 - a; } }
  /* falling rain: thin streaks on columns fixed in the world, in four
     layers of distance; lit by the air's light and the headlights */
  if (u.wx.x > 0.0 || u.wx.y > 0.0) {
    float2 fw = normalize(vd.xy + float2(1e-5, 0)), side = float2(-fw.y, fw.x);
    for (int l = 0; l < 4; l++) {
      float d = 1.6 * exp2(float(l)) * (0.85 + 0.3 * j0);
      if (d >= dmax) break;
      float3 p = eye + vd * d;
      bool snow = u.wx.y > 0.0;
      float g = snow ? 0.5 : 0.28;
      float2 cell = floor(p.xy / g);
      float cover = 0;
      for (int k = 0; k < 2; k++) {
        float2 ck = cell + float2(k * 41, l * 17);
        float h = hash2(ck), h2 = hash2(ck + 9.1);
        float2 col = (cell + float2(h, h2)) * g;
        if (snow) col += float2(sin(t * 0.9 + h * 30.0), cos(t * 0.7 + h2 * 30.0)) * 0.15;
        float2 dv = p.xy - col;
        float along = dot(dv, fw), lat = abs(dot(dv, side));
        if (abs(along) > g * 0.5) continue;
        float w = 0.0018 * d + 0.0015;
        if (snow) {
          /* a flake every S metres down the column, each one only sometimes
             there and each at its own sideways offset */
          float S = 1.4, zz = (p.z + t * 1.1) / S + h * 3.0, rep = floor(zz), z = fract(zz) - 0.5;
          float hr = hash2(ck + rep * 1.37);
          if (hr > 0.45) continue;
          float lat2 = abs(lat - (hr - 0.22) * g * 0.8);
          float rr = length(float2(lat2, z * S)) / (w * 1.6);
          cover += smoothstep(1.0, 0.35, rr) * 0.85;
        } else {
          /* a drop every S metres down the column, each one only sometimes
             there and each at its own sideways offset: short streaks, no dashes */
          float S = 2.6, len = 0.42, zz = (p.z + t * 9.5) / S + h * 5.0, rep = floor(zz);
          float hr = hash2(ck + rep * 1.37);
          if (hr > 0.35) continue;
          float lat2 = abs(lat - (hr - 0.17) * g * 1.2);
          float z = fract(zz) * S;
          cover += smoothstep(w, w * 0.2, lat2) * smoothstep(0.0, 0.1, z) * smoothstep(len, len * 0.5, z) * 0.4;
        } }
      cover = saturate(cover) * (snow ? u.wx.y : u.wx.x) / (1.0 + float(l) * 0.35);
      if (cover <= 0.0) continue;
      float3 hd = 0, Ld;
      for (int m = 0; m < int(hl.misc.x); m++) hd += spotc(hl, m) * spotw(hl, m, p, Ld) * 0.33;
      float3 lum = (snow ? float3(0.95) : float3(0.7, 0.75, 0.8)) * (pf.amb.rgb * 1.3 + u.sunc.rgb * 0.3)
                 + hd * 2.5;
      Lc += T * cover * lum; T *= 1.0 - cover; } }
  return float4(Lc, 1.0 - T); }

/* ---- temporal anti-aliasing and motion blur.  Every 3D triangle is drawn
   with a sub-pixel jitter (host_glide.m), so over frames each pixel sees its
   whole area; each pixel's position last frame comes from its world position
   through last frame's camera, or through its car's last transform when it
   is on a car. */
float2 prev_uv(constant FXU &u, constant MVC &mv, float4 P, float2 uv) {
  float3 wp;
  if (is_solid(P.w)) {
    wp = P.xyz;
    for (int i = 0; i < int(mv.misc.x); i++) {
      float3 d = wp - mv.cur[i][3].xyz;
      float3 l = float3(dot(d, mv.cur[i][0].xyz), dot(d, mv.cur[i][1].xyz), dot(d, mv.cur[i][2].xyz));
      /* the car's own pixels ride with it; the road under and around the
         tyres (below the contact plane in .w) does not */
      float fl = mv.cur[i][3].w + (P.w > 3.5 ? -0.1 : 0.35);   /* the HD car, or an original body well above the road */
      if (abs(l.x) < 2.6 && abs(l.y) < 1.3 && l.z > fl && l.z < 1.8) {
        wp = mv.prev[i][3].xyz + l.x * mv.prev[i][0].xyz + l.y * mv.prev[i][1].xyz + l.z * mv.prev[i][2].xyz;
        break; } }
  } else wp = u.eye.xyz + view_dir(u, uv) * 20000.0;
  float4 c = u.pvp * float4(wp, 1);
  if (c.w <= 0) return float2(-1);
  return scr_uv(u, u.vpt.x * c.x / c.w + u.vpt.y, u.vpt.z * c.y / c.w + u.vpt.w); }
float3 ycc(float3 c) { return float3(0.25 * c.r + 0.5 * c.g + 0.25 * c.b, 0.5 * c.r - 0.5 * c.b, -0.25 * c.r + 0.5 * c.g - 0.25 * c.b); }
float3 rgb_(float3 y) { return float3(y.x + y.y - y.z, y.x + y.z, y.x - y.y - y.z); }
/* Catmull-Rom history fetch, 5 taps (Jimenez) */
float3 hist_cr(texture2d<float> h, float2 uv) {
  float2 sz = float2(h.get_width(), h.get_height()), p = uv * sz - 0.5, t1 = floor(p) + 0.5, f = p - floor(p);
  float2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f)), w1 = 1.0 + f * f * (-2.5 + 1.5 * f), w2 = f * (0.5 + f * (2.0 - 1.5 * f)), w3 = f * f * (-0.5 + 0.5 * f);
  float2 w12 = w1 + w2, t0 = (t1 - 1.0) / sz, t3 = (t1 + 2.0) / sz, t12 = (t1 + w2 / w12) / sz;
  float3 c = h.sample(ls, float2(t12.x, t0.y)).rgb * (w12.x * w0.y) + h.sample(ls, float2(t0.x, t12.y)).rgb * (w0.x * w12.y)
           + h.sample(ls, t12).rgb * (w12.x * w12.y) + h.sample(ls, float2(t3.x, t12.y)).rgb * (w3.x * w12.y)
           + h.sample(ls, float2(t12.x, t3.y)).rgb * (w12.x * w3.y);
  float ws = w12.x * w0.y + w0.x * w12.y + w12.x * w12.y + w3.x * w12.y + w12.x * w3.y;
  return max(c / ws, 0.0); }
/* the lit scene with the particles and precipitation over it */
float4 withpfx(texture2d<float> cur, texture2d<float> pf, float2 uv) {
  float4 c = cur.sample(ns, uv), p = pf.sample(ls, uv);
  return float4(c.rgb * (1.0 - p.a) + p.rgb, c.a); }
/* the half-resolution particles brought up to this pixel from the texels
   that saw the same surface, so a plume behind the car neither bleeds onto
   the car nor leaves a clear outline around it */
float4 pfx_up(constant FXU &u, texture2d<float> pf, texture2d<float> gp, float2 uv) {
  float2 hs = float2(pf.get_width(), pf.get_height()), p = uv * hs - 0.5, b = floor(p), f = p - b;
  float4 P = gp.sample(ns, uv);
  float dc = is_solid(P.w) ? distance(u.eye.xyz, P.xyz) : 1e4;
  float4 acc = 0; float ws = 0, best = 1e9; float4 nearest = 0;
  for (int k = 0; k < 4; k++) {
    float2 o = float2(k & 1, k >> 1), tuv = (b + o + 0.5) / hs;
    float4 Q = gp.sample(ns, tuv);
    float dq = is_solid(Q.w) ? distance(u.eye.xyz, Q.xyz) : 1e4;
    float e = abs(dq - dc) / (0.04 * dc + 0.15);
    float w = (o.x > 0.5 ? f.x : 1.0 - f.x) * (o.y > 0.5 ? f.y : 1.0 - f.y) * exp(-e * e);
    float4 s = pf.sample(ns, tuv);
    acc += s * w; ws += w;
    if (e < best) { best = e; nearest = s; } }
  return ws > 1e-3 ? acc / ws : nearest; }
/* MetalFX's temporal upscaler (host_glide.m): each pixel's move since the
   last frame, in UV (where it was minus where it is) */
fragment float4 mvfs(QO in [[stage_in]], constant FXU &u [[buffer(0)]], constant MVC &mv [[buffer(1)]],
                     texture2d<float> gp [[texture(0)]]) {
  float4 P = gp.sample(ns, in.uv);
  if (P.w < 0.5 && !is_solid(P.w)) return float4(0);   /* 2D: still */
  float2 pu = prev_uv(u, mv, P, in.uv);
  return float4(pu - in.uv, 0, 1); }
fragment float4 taafs(QO in [[stage_in]], constant FXU &u [[buffer(0)]], constant MVC &mv [[buffer(1)]],
                      texture2d<float> cur [[texture(0)]], texture2d<float> hist [[texture(1)]], texture2d<float> gp [[texture(2)]],
                      texture2d<float> pf [[texture(3)]]) {
  float4 C = cur.sample(ns, in.uv), pc = pfx_up(u, pf, gp, in.uv);
  C.rgb = C.rgb * (1.0 - pc.a) + pc.rgb;
  float4 P0 = gp.sample(ns, in.uv);
  bool car0 = P0.w > 3.5 && P0.w < 4.5;
  /* the history remembers which kind of surface each pixel was (2 added to
     its alpha on a car; mbfs takes it off again) */
  float4 Ct = float4(C.rgb, C.a + (car0 ? 2.0 : 0.0));
  if (u.jit.w < 0.5) return Ct;                      /* no history yet */
  float2 px = 1.0 / float2(cur.get_width(), cur.get_height());
  /* the clamp's neighbourhood is only the pixels on the same kind of
     surface: road just uncovered beside a tyre must not accept the tyre's
     colour as history, nor the car the road's */
  float3 m1 = 0, m2 = 0; float nw = 0;
  for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
    float2 q = in.uv + float2(x, y) * px;
    float wq = gp.sample(ns, q).w;
    if ((wq > 3.5 && wq < 4.5) != car0) continue;
    float3 cc = withpfx(cur, pf, q).rgb;
    float3 c = ycc(cc / (1.0 + dot(cc, float3(0.2126, 0.7152, 0.0722))));
    m1 += c; m2 += c * c; nw += 1.0; }
  m1 /= nw; float3 sd = sqrt(max(m2 / nw - m1 * m1, 0.0));
  float2 pu = prev_uv(u, mv, P0, in.uv);
  if (any(pu < 0.0) || any(pu > 1.0)) return Ct;
  /* disocclusion: road coming out from under a moving car finds the car
     where it was last frame.  At night the two are too alike in colour for
     the clamp to tell, and the car's underside was copied down the road
     behind it frame after frame -- so history from the other kind of
     surface is not used at all */
  if ((hist.sample(ns, pu).a > 1.5) != car0) return Ct;
  float3 H = hist_cr(hist, pu);
  float3 hy = ycc(H / (1.0 + dot(H, float3(0.2126, 0.7152, 0.0722))));
  float3 lo = m1 - sd * 1.25, hi = m1 + sd * 1.25;
  /* clip the history toward the neighbourhood's mean */
  float3 d = hy - m1, e = max(abs(d) / max(hi - m1, 1e-4), 1.0);
  hy = m1 + d / max(e.x, max(e.y, e.z));
  float3 cy = ycc(C.rgb / (1.0 + dot(C.rgb, float3(0.2126, 0.7152, 0.0722))));
  float speed = length((in.uv - pu) / px);
  float a = mix(0.08, 0.25, saturate(speed / 30.0));
  float3 r = mix(hy, cy, a);
  float3 o = rgb_(r); o = o / max(1.0 - dot(o, float3(0.2126, 0.7152, 0.0722)), 1e-3);
  return float4(max(o, 0.0), Ct.a); }
fragment float4 mbfs(QO in [[stage_in]], constant FXU &u [[buffer(0)]], constant MVC &mv [[buffer(1)]],
                     texture2d<float> src [[texture(0)]], texture2d<float> gp [[texture(1)]],
                     texture2d<float> pf [[texture(2)]]) {
  float4 C = src.sample(ns, in.uv);
  if (C.a > 1.5) C.a -= 2.0;                         /* taafs' surface mark */
  if (C.a < 0.5 || u.jit.w < 0.5) return C;
  /* smoke drifts on its own, not with the surface behind it: where it is
     thick there is nothing to smear */
  float pa = pf.sample(ls, in.uv).a;
  float2 pu = prev_uv(u, mv, gp.sample(ns, in.uv), in.uv);
  if (any(pu < -0.5)) return C;
  float2 sz = float2(src.get_width(), src.get_height());
  float2 v = (in.uv - pu) * u.jit.z * (1.0 - saturate(pa * 1.5));   /* shutter */
  float l = length(v * sz);
  if (l < 1.0) return C;
  v *= min(l, 0.04 * sz.x) / l;
  float3 acc = C.rgb; float n = 1.0, j = ign(in.pos.xy) - 0.5;
  float4 P = gp.sample(ns, in.uv);
  float de = is_solid(P.w) ? distance(u.eye.xyz, P.xyz) : 1e5;
  bool car0 = P.w > 3.5 && P.w < 4.5;
  for (int i = 1; i <= 8; i++) {
    float t = (float(i) + j) / 8.0 - 0.5;
    float2 q = in.uv + v * t;
    float4 s = src.sample(ls, q);
    if (s.a < 0.5) continue;
    /* nearer surfaces (the car over the streaming road) do not smear
       into what is behind them */
    float4 Q = gp.sample(ns, q);
    if ((Q.w > 3.5 && Q.w < 4.5) != car0) continue;          /* the car and the world move apart */
    if (is_solid(Q.w) && distance(u.eye.xyz, Q.xyz) < de * 0.9 - 0.3) continue;
    acc += s.rgb; n += 1.0; }
  return float4(acc / n, C.a); }

fragment float4 pfxcfs(QO in [[stage_in]], constant FXU &u [[buffer(0)]], texture2d<float> p [[texture(0)]],
                       texture2d<float> gp [[texture(1)]]) { return pfx_up(u, p, gp, in.uv); }

/* ---- tonemap and grade, over the untouched 2D */
/* Khronos PBR Neutral: colours below ~0.76 come through exactly, highlights
   roll off toward white without the hue shifts and greying of a film curve */
float3 neutral(float3 c) {
  const float start = 0.76, desat = 0.15;
  float x = min(c.r, min(c.g, c.b)), off = x < 0.08 ? x - 6.25 * x * x : 0.04;
  /* the toe darkens near-black by subtracting the smallest channel, which
     on a grey a hair bluer than neutral left only the blue: night's unlit
     road came out saturated blue.  Down there a near-grey darkens by
     scaling, which keeps the hue; a strong colour (its smallest channel far
     below its largest, where scaling by it would crush the colour to black)
     and everything brighter keep the original curve */
  float peak0 = max(c.r, max(c.g, c.b));
  float ks = (1.0 - smoothstep(0.03, 0.08, x)) * smoothstep(0.5, 0.85, x / max(peak0, 1e-6));
  c = mix(c - off, c * (6.25 * x), ks);
  float peak = max(c.r, max(c.g, c.b));
  if (peak < start) return c;
  float d = 1.0 - start, np = 1.0 - d * d / (peak + d - start);
  c *= np / peak;
  float g = 1.0 - 1.0 / (desat * (peak - np) + 1.0);
  return mix(c, float3(np), g); }
/* Uchimura's filmic curve (Gran Turismo Sport, CEDEC 2017): a toe that
   takes the near-blacks down to black, a straight middle of slope a, and a
   shoulder that reaches white at P -- per channel, so bright saturated
   colours desaturate toward white as film does */
float3 gt_tone(float3 x, float a, float c) {
  const float P = 1.0, m = 0.22, l = 0.4, b = 0.0;
  float l0 = ((P - m) * l) / a, S0 = m + l0, S1 = m + a * l0, C2 = (a * P) / (P - S1), CP = -C2 / P;
  float3 w0 = 1.0 - smoothstep(0.0, m, x), w2 = step(m + l0, x), w1 = 1.0 - w0 - w2;
  float3 T = m * pow(x / m, c) + b, L = m + a * (x - m), S = P - (P - S1) * exp(CP * (x - S0));
  return T * w0 + L * w1 + S * w2; }
/* the scene's tone map and grade, shared by the passes below */
float3 grade(constant FXU &u, float4 H, texture2d<float> bl, texture2d<float> sh, float2 uv, float2 pos) {
  float2 in_uv = uv;
  float3 x = H.rgb + bl.sample(ls, in_uv).rgb * u.p1.w + sh.sample(ls, in_uv).rgb * H.a;
  float3 xe = max(x * u.p1.x * u.wb.rgb, 0.0);
  float3 c = pow(saturate(u.tmo.x > 0.5 ? gt_tone(xe, u.tmo.y, u.tmo.z) : neutral(xe)), 1.0 / 2.2);
  float l = dot(c, float3(0.2126, 0.7152, 0.0722));
  c = mix(float3(l), c, u.p1.y);
  c = saturate((c - 0.5) * u.p1.z + 0.5);
  float v = length((in_uv - 0.5) * float2(1.2, 1.0));
  c *= mix(1.0, smoothstep(1.05, 0.35, v), 0.12);
  c += (ign(pos) - 0.5) / 255.0;
  return c; }
fragment float4 finfs(QO in [[stage_in]], constant FXU &u [[buffer(0)]],
                      texture2d<float> h [[texture(0)]], texture2d<float> col [[texture(1)]],
                      texture2d<float> bl [[texture(2)]], texture2d<float> sh [[texture(3)]]) {
  float4 H = h.sample(ns, in.uv), C = col.sample(ns, in.uv);
  if (H.a <= 0.001) return C;
  if (u.p3.y > 8.5 && u.p3.y < 9.5) return C;
  if ((u.p3.y > 0.5 && u.p3.y < 8.5) || u.p3.y > 10.5) return float4(mix(C.rgb, H.rgb, H.a), 1);
  if (u.p3.y > 9.5 && u.p3.y < 10.5) {
    float3 hb = H.rgb, bb = bl.sample(ls, in.uv).rgb, sb = sh.sample(ls, in.uv).rgb;
    return float4(any(isnan(hb)) || any(isinf(hb)) ? 1.0 : (any(hb < 0.0) ? 0.5 : 0.0),
                  any(isnan(bb)) || any(isinf(bb)) ? 1.0 : (any(bb < 0.0) ? 0.5 : 0.0),
                  any(isnan(sb)) || any(isinf(sb)) ? 1.0 : (any(sb < 0.0) ? 0.5 : 0.0), 1); }
  float3 c = grade(u, H, bl, sh, in.uv, in.pos.xy);
  return float4(mix(C.rgb, c, H.a), 1); }
/* the graded scene alone, for the anti-aliasing pass: no 2D in it */
fragment float4 scenefs(QO in [[stage_in]], constant FXU &u [[buffer(0)]],
                        texture2d<float> h [[texture(0)]], texture2d<float> bl [[texture(2)]], texture2d<float> sh [[texture(3)]]) {
  float4 H = h.sample(ns, in.uv);
  if (H.a <= 0.001) return float4(0);
  return float4(grade(u, H, bl, sh, in.uv, in.pos.xy), 1); }
/* the 2D layer (the game's own colour) over the anti-aliased scene */
fragment float4 mixfs(QO in [[stage_in]], texture2d<float> a [[texture(0)]], texture2d<float> h [[texture(1)]],
                      texture2d<float> col [[texture(2)]]) {
  float4 H = h.sample(ns, in.uv), C = col.sample(ns, in.uv);
  if (H.a <= 0.001) return C;
  return float4(mix(C.rgb, a.sample(ns, in.uv).rgb, H.a), 1); }

/* ---- anti-aliasing: FXAA (quality preset) on the graded scene layer, before the
   2D is composited over it, so the HUD, text and menus never pass through it */
float fl_(float3 c) { return dot(c, float3(0.299, 0.587, 0.114)); }
fragment float4 aafs(QO in [[stage_in]], texture2d<float> t [[texture(0)]], texture2d<float> hd [[texture(1)]]) {
  float2 rc = 1.0 / float2(t.get_width(), t.get_height()), uv = in.uv;
  float4 C = t.sample(ls, uv);
  if (hd.sample(ns, uv).a <= 0.001) return C;
  float lM = fl_(C.rgb);
  float lN = fl_(t.sample(ls, uv + float2(0, -rc.y)).rgb), lS = fl_(t.sample(ls, uv + float2(0, rc.y)).rgb);
  float lW = fl_(t.sample(ls, uv + float2(-rc.x, 0)).rgb), lE = fl_(t.sample(ls, uv + float2(rc.x, 0)).rgb);
  float lo = min(lM, min(min(lN, lS), min(lW, lE))), hi = max(lM, max(max(lN, lS), max(lW, lE)));
  float range = hi - lo;
  if (range < max(0.0312, hi * 0.125)) return C;
  float lNW = fl_(t.sample(ls, uv + float2(-rc.x, -rc.y)).rgb), lNE = fl_(t.sample(ls, uv + float2(rc.x, -rc.y)).rgb);
  float lSW = fl_(t.sample(ls, uv + float2(-rc.x, rc.y)).rgb), lSE = fl_(t.sample(ls, uv + float2(rc.x, rc.y)).rgb);
  float eH = abs(-2.0 * lW + lNW + lSW) + 2.0 * abs(-2.0 * lM + lN + lS) + abs(-2.0 * lE + lNE + lSE);
  float eV = abs(-2.0 * lN + lNW + lNE) + 2.0 * abs(-2.0 * lM + lW + lE) + abs(-2.0 * lS + lSW + lSE);
  bool horz = eH >= eV;
  float l1 = horz ? lN : lW, l2 = horz ? lS : lE, g1 = l1 - lM, g2 = l2 - lM;
  bool steep1 = abs(g1) >= abs(g2);
  float gs = 0.25 * max(abs(g1), abs(g2)), st = horz ? rc.y : rc.x, la;
  if (steep1) { st = -st; la = 0.5 * (l1 + lM); } else la = 0.5 * (l2 + lM);
  float2 cur = uv; if (horz) cur.y += st * 0.5; else cur.x += st * 0.5;
  float2 off = horz ? float2(rc.x, 0) : float2(0, rc.y), u1 = cur - off, u2 = cur + off;
  float e1 = fl_(t.sample(ls, u1).rgb) - la, e2 = fl_(t.sample(ls, u2).rgb) - la;
  bool r1 = abs(e1) >= gs, r2 = abs(e2) >= gs;
  const float Q[12] = { 1, 1, 1, 1, 1, 1.5, 2, 2, 2, 2, 4, 8 };
  for (int i = 0; i < 12 && !(r1 && r2); i++) {
    if (!r1) { u1 -= off * Q[i]; e1 = fl_(t.sample(ls, u1).rgb) - la; r1 = abs(e1) >= gs; }
    if (!r2) { u2 += off * Q[i]; e2 = fl_(t.sample(ls, u2).rgb) - la; r2 = abs(e2) >= gs; } }
  float d1 = horz ? uv.x - u1.x : uv.y - u1.y, d2 = horz ? u2.x - uv.x : u2.y - uv.y;
  float pxo = -min(d1, d2) / (d1 + d2) + 0.5;
  bool good = ((d1 < d2 ? e1 : e2) < 0.0) != (lM < la);
  float fin = good ? pxo : 0.0;
  float lA = (2.0 * (lN + lS + lW + lE) + lNW + lNE + lSW + lSE) / 12.0;
  float sb = saturate(abs(lA - lM) / range), s2 = (-2.0 * sb + 3.0) * sb * sb;
  fin = max(fin, s2 * s2 * 0.75);
  float2 f = uv; if (horz) f.y += fin * st; else f.x += fin * st;
  return float4(t.sample(ls, f).rgb, 1); }
);

typedef struct {
    float vp[16], ivp[16], svp[16];
    float eye[4], sun[4], sunc[4], skyc[4], grnd[4], fogc[4], vpt[4], scr[4];
    float p0[4], p1[4], p2[4], p3[4], p4[4], flash[4], wb[4], nsky[4], sk2[4], tm[4], wx[4];
    float pvp[16], jit[4];   /* last frame's view x projection; this frame's jitter (ndc) and the last one's */
    float svp2[16], csm[4];  /* the far shadow cascade; csm.x its texel size */
    float mat[4], matmean[8][4], rmap[4];   /* ground materials: mat.x loaded; each one's mean colour (linear) */   /* wx: rain, snowfall, snow ground, - */
    float m3[4];                            /* the screen map (host_glide.m) */
    float cmap[4];                          /* the canopy grid's placing (host_env.m) */
    float mk[4];                            /* the tyre marks map's placing (host_marks.m) */
    float bnd[4];                           /* the horizon band's placing (host_sky.m) */
    float ter[3][4];                        /* the landscape's light maps (host_terrain.m) */
    float tmo[4];                           /* the tone curve */
    float air2[4];                          /* the landscape's haze */
} fxu;
typedef struct { float p[64][4], d[64][4], col[4], misc[4]; float g[64][4], gd[64][4], gmisc[4]; } hlu;
static float g_hl_int, g_hl_beam, g_hl_air, g_wdark = 1.0f, g_brake[64];
typedef struct { int at_fn, at_ref, use_tex, pad; float su, sv, pad2, pad3; float svp[16]; } shu;

/* BR_FX_PROF=1: GPU time of every pass (stage-boundary timestamps), summed
 * over 120 frames and printed as a table */
static int g_prof = -1, g_prof_n, g_prof_buf;
static id<MTLCounterSampleBuffer> g_psb[2];
static const char *g_plab[2][64];
static double g_pacc[64], g_pvacc[64], g_pgap[64]; static const char *g_pacc_lab[64]; static int g_pacc_n, g_pacc_frames;
static void prof_attach(MTLRenderPassDescriptor *rp, const char *label)
{
    if (g_prof < 0) g_prof = getenv("BR_FX_PROF") != NULL;
    if (!g_prof || !g_psb[g_prof_buf] || g_prof_n >= 64) return;
    rp.sampleBufferAttachments[0].sampleBuffer = g_psb[g_prof_buf];
    rp.sampleBufferAttachments[0].startOfVertexSampleIndex = (NSUInteger)g_prof_n * 4;
    rp.sampleBufferAttachments[0].endOfVertexSampleIndex = (NSUInteger)g_prof_n * 4 + 1;
    rp.sampleBufferAttachments[0].startOfFragmentSampleIndex = (NSUInteger)g_prof_n * 4 + 2;
    rp.sampleBufferAttachments[0].endOfFragmentSampleIndex = (NSUInteger)g_prof_n * 4 + 3;
    g_plab[g_prof_buf][g_prof_n++] = label;
}
static id<MTLDevice> D;
static id<MTLLibrary> L;
static id<MTLRenderPipelineState> p_trk, p_pfx, p_pfxc, p_nrm, p_taa, p_mb, p_ring, p_mvec;
static id<MTLTexture> t_mv;                 /* motion, for MetalFX's temporal upscaler */
static float g_jit_used[2];                 /* the jitter this frame was drawn with (pixels) */
static int g_hist_ok_mfx;                   /* MetalFX's history is good (0 after a cut or a resize) */
int hglide_temporal(void);                  /* host_glide.m: MetalFX temporal upscaling this frame */
static id<MTLRenderPipelineState> p_sh, p_ao, p_blur, p_ssr, p_comp, p_pre, p_down, p_up, p_shaft, p_fin, p_aa, p_scene, p_mix;
static id<MTLDepthStencilState> ds_sh;
static id<MTLTexture> t_trk, t_pfx, t_nrm, t_hist[2], t_mb, t_pnz, t_ring;
static int g_hist_ok, g_hist_i;
static id<MTLTexture> t_sm2;
static double g_shanc[2][3], g_shsun[3]; static int g_shanc_ok[2], g_shdirty[2]; static unsigned g_shframe[2];
static id<MTLTexture> t_smD;          /* the near cascade's moving casters (the cars), drawn every frame */
static id<MTLTexture> t_sm, t_ao, t_ao2, t_ssr, t_hdr, t_bl[6], t_shaft, t_out, t_aa;
static int tw, th;
static int g_on = -1;

typedef struct { __unsafe_unretained id<MTLBuffer> buf; size_t off; int n; __unsafe_unretained id<MTLTexture> tex;
                 __unsafe_unretained id<MTLSamplerState> smp; shu s; int dyn; } shrec;
static shrec *g_rec; static int g_nrec, g_caprec, g_rec_dyn;
static NSMutableArray *g_keep;           /* keeps recorded buffers/textures alive for the frame */
static int g_havecam;
static float g_P[16], g_vpt[4];
static unsigned g_frame;
static double g_last_ms, g_clock, g_dt;   /* the game clock at the last frame; effect time; this frame's step (s) */

int hfx_on(void)
{
    if (g_on < 0) {
        const char *e = getenv("BR_FX");
        if (e) g_on = atoi(e) != 0;
        else if (happ_metal_layer() == nil) g_on = 0;
        else {
            NSString *r = [[NSUserDefaults standardUserDefaults] stringForKey:@"Renderer"];
            g_on = !r || ![r isEqualToString:@"original"];
        }
    }
    return g_on;
}

/* A word in the top left corner, faded in, held and out: which renderer. */
NSWindow *happ_window(void);
static CATextLayer *g_badge;
static void badge(void)
{
    NSView *v = happ_window().contentView;
    CAKeyframeAnimation *a;
    CGFloat h, fs;
    if (!v) return;
    if (!v.layer) v.wantsLayer = YES;
    if (!g_badge) {
        g_badge = [CATextLayer layer];
        g_badge.foregroundColor = NSColor.whiteColor.CGColor;
        g_badge.shadowColor = NSColor.blackColor.CGColor;
        g_badge.shadowOpacity = 0.7f; g_badge.shadowRadius = 3; g_badge.shadowOffset = CGSizeMake(0, -1);
        g_badge.font = (__bridge CFTypeRef)[NSFont systemFontOfSize:12 weight:NSFontWeightHeavy];
        g_badge.opacity = 0;
        [v.layer addSublayer:g_badge];
    }
    h = v.bounds.size.height;
    fs = fmin(fmax(h * 0.035, 14), 34);
    g_badge.contentsScale = happ_window().backingScaleFactor;
    g_badge.fontSize = fs;
    g_badge.string = g_on ? @"REMASTERED" : @"ORIGINAL";
    g_badge.frame = CGRectMake(fmax(h * 0.025, 8), h - fmax(h * 0.025, 8) - fs * 1.3, fs * 12, fs * 1.3);
    a = [CAKeyframeAnimation animationWithKeyPath:@"opacity"];
    a.values = @[ @0, @0.95, @0.95, @0 ];
    a.keyTimes = @[ @0, @0.06, @0.75, @1 ];
    a.duration = 2.0;
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    g_badge.opacity = 0;
    [g_badge removeAnimationForKey:@"show"];
    [g_badge addAnimation:a forKey:@"show"];
    [CATransaction commit];
}

void hfx_toggle(void)
{
    g_on = !hfx_on();
    [[NSUserDefaults standardUserDefaults] setObject:g_on ? @"remastered" : @"original" forKey:@"Renderer"];
    fprintf(stderr, "renderer: %s\n", g_on ? "Remastered" : "Original");
    nmusic_remastered(g_on);
    badge();
}

/* host_app.m, for every key event: 1 if it was the switch's (~ / `, the key
 * left of 1, with no Command held) and must not reach the game. */
int hfx_key(NSEvent *e)
{
    /* 0x32 is ` ~ on ANSI keyboards; ISO ones put § there (0x0A) and move ` */
    if ((e.keyCode != 0x32 && e.keyCode != 0x0A) || (e.modifierFlags & NSEventModifierFlagCommand)) return 0;
    if (e.type == NSEventTypeKeyDown && !e.isARepeat) hfx_toggle();
    return 1;
}

/* The main camera's view x projection (the game's row-vector matrix) and
 * the viewport mapping clip space to the 640x480 screen. */
void hfx_set_cam(const float *P, float sx, float tx, float sy, float ty)
{
    memcpy(g_P, P, sizeof g_P);
    g_vpt[0] = sx; g_vpt[1] = tx; g_vpt[2] = sy; g_vpt[3] = ty;
    g_havecam = 1;
}

static void inv4(const double *m, double *o);
static void rowmul(const double *v, const double *M, double *h);
/* host_glide.m: the eye of this frame's camera (0 before it has one) */
int hfx_cam_eye(float e[3])
{
    double P[16], IP[16], v[4] = { 0, 0, 1, 0 }, h[4];
    int i;
    if (!g_havecam) return 0;
    for (i = 0; i < 16; i++) P[i] = g_P[i];
    inv4(P, IP);
    rowmul(v, IP, h);
    if (h[3] == 0) return 0;
    for (i = 0; i < 3; i++) e[i] = (float)(h[i] / h[3]);
    return 1;
}

void hfx_shadow_batch(id<MTLBuffer> buf, size_t off, int n, id<MTLTexture> tex, id<MTLSamplerState> smp,
                      int at_fn, int at_ref, int use_tex, float su, float sv)
{
    if (!g_havecam) return;
    if (g_nrec == g_caprec) { g_caprec = g_caprec ? g_caprec * 2 : 1024; g_rec = realloc(g_rec, sizeof *g_rec * (size_t)g_caprec); }
    if (!g_keep) g_keep = [NSMutableArray new];
    [g_keep addObject:buf]; if (tex) [g_keep addObject:tex]; if (smp) [g_keep addObject:smp];
    g_rec[g_nrec++] = (shrec){ buf, off, n, tex, smp, { at_fn, at_ref, use_tex, 0, su, sv, 0, 0, {0} }, g_rec_dyn };
}
/* host_car.m: a caster that moves (a car), drawn into the shadow maps every
 * frame; everything else the game draws goes into the cascades' cache */
void hfx_shadow_batch_dyn(id<MTLBuffer> buf, size_t off, int n, id<MTLTexture> tex, id<MTLSamplerState> smp,
                          int at_fn, int at_ref, int use_tex, float su, float sv)
{
    g_rec_dyn = 1;
    hfx_shadow_batch(buf, off, n, tex, smp, at_fn, at_ref, use_tex, su, sv);
    g_rec_dyn = 0;
}

static void inv4(const double *m, double *o)
{
    double a[4][8];
    int i, j, k;
    for (i = 0; i < 4; i++) for (j = 0; j < 8; j++) a[i][j] = j < 4 ? m[i * 4 + j] : (j - 4 == i);
    for (i = 0; i < 4; i++) {
        int p = i; double t;
        for (k = i + 1; k < 4; k++) if (fabs(a[k][i]) > fabs(a[p][i])) p = k;
        for (j = 0; j < 8; j++) { t = a[i][j]; a[i][j] = a[p][j]; a[p][j] = t; }
        t = a[i][i]; if (t == 0) t = 1e-30;
        for (j = 0; j < 8; j++) a[i][j] /= t;
        for (k = 0; k < 4; k++) if (k != i) { t = a[k][i]; for (j = 0; j < 8; j++) a[k][j] -= t * a[i][j]; }
    }
    for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) o[i * 4 + j] = a[i][j + 4];
}
/* row vector h = v * M */
static void rowmul(const double *v, const double *M, double *h)
{
    int j;
    for (j = 0; j < 4; j++) h[j] = v[0] * M[j] + v[1] * M[4 + j] + v[2] * M[8 + j] + v[3] * M[12 + j];
}
static void norm3(double *v) { double l = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l > 0) { v[0] /= l; v[1] /= l; v[2] /= l; } }
static void cross3(const double *a, const double *b, double *o)
{ o[0] = a[1] * b[2] - a[2] * b[1]; o[1] = a[2] * b[0] - a[0] * b[2]; o[2] = a[0] * b[1] - a[1] * b[0]; }

static id<MTLRenderPipelineState> mkpipe(NSString *vs, NSString *fs, MTLPixelFormat f, int add)
{
    NSError *err = nil;
    MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
    d.vertexFunction = [L newFunctionWithName:vs];
    d.fragmentFunction = [L newFunctionWithName:fs];
    if (f == MTLPixelFormatDepth32Float) d.depthAttachmentPixelFormat = f;
    else {
        d.colorAttachments[0].pixelFormat = f;
        if (add) {
            d.colorAttachments[0].blendingEnabled = YES;
            d.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
            d.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
            d.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorZero;
            d.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
        }
    }
    d.label = fs;
    id<MTLRenderPipelineState> p = [D newRenderPipelineStateWithDescriptor:d error:&err];
    if (!p) { fprintf(stderr, "fx pipeline %s: %s\n", fs.UTF8String, err.localizedDescription.UTF8String); exit(1); }
    return p;
}

static id<MTLTexture> mktex(int w, int h, MTLPixelFormat f, int shared)
{
    MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:f
                                   width:(NSUInteger)(w < 1 ? 1 : w) height:(NSUInteger)(h < 1 ? 1 : h) mipmapped:NO];
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    td.storageMode = shared ? MTLStorageModeShared : MTLStorageModePrivate;
    return [D newTextureWithDescriptor:td];
}

static void setup(id<MTLDevice> dev)
{
    NSError *err = nil;
    if (L) return;
    D = dev;
    L = [D newLibraryWithSource:[NSString stringWithUTF8String:FXSRC] options:nil error:&err];
    if (!L) { fprintf(stderr, "fx shader: %s\n", err.localizedDescription.UTF8String); exit(1); }
    {
        MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
        d.vertexFunction = [L newFunctionWithName:@"shvs"];
        d.fragmentFunction = [L newFunctionWithName:@"shfs"];
        d.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
        p_sh = [D newRenderPipelineStateWithDescriptor:d error:&err];
        if (!p_sh) { fprintf(stderr, "fx shadow pipeline: %s\n", err.localizedDescription.UTF8String); exit(1); }
    }
    if (getenv("BR_FX_PROF") && [D supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary]) {
        for (id<MTLCounterSet> cs in D.counterSets)
            if ([cs.name isEqualToString:MTLCommonCounterSetTimestamp]) {
                MTLCounterSampleBufferDescriptor *sd = [MTLCounterSampleBufferDescriptor new];
                sd.counterSet = cs; sd.storageMode = MTLStorageModeShared; sd.sampleCount = 256;
                g_psb[0] = [D newCounterSampleBufferWithDescriptor:sd error:&err];
                g_psb[1] = [D newCounterSampleBufferWithDescriptor:sd error:&err];
            }
        if (!g_psb[0]) fprintf(stderr, "fx prof: no timestamp counters\n");
    }
    p_ao = mkpipe(@"fsq", @"aofs", MTLPixelFormatRGBA16Float, 0);
    p_blur = mkpipe(@"fsq", @"blurfs", MTLPixelFormatRGBA16Float, 0);
    p_ssr = mkpipe(@"fsq", @"ssrfs", MTLPixelFormatRGBA16Float, 0);
    p_comp = mkpipe(@"fsq", @"compfs", MTLPixelFormatRGBA16Float, 0);
    p_pre = mkpipe(@"fsq", @"prefs", MTLPixelFormatRGBA16Float, 0);
    p_down = mkpipe(@"fsq", @"downfs", MTLPixelFormatRGBA16Float, 0);
    p_up = mkpipe(@"fsq", @"upfs", MTLPixelFormatRGBA16Float, 1);
    p_shaft = mkpipe(@"fsq", @"shaftfs", MTLPixelFormatRGBA16Float, 0);
    p_fin = mkpipe(@"fsq", @"finfs", MTLPixelFormatRGBA8Unorm, 0);
    p_aa = mkpipe(@"fsq", @"aafs", MTLPixelFormatRGBA8Unorm, 0);
    p_scene = mkpipe(@"fsq", @"scenefs", MTLPixelFormatRGBA8Unorm, 0);
    p_mix = mkpipe(@"fsq", @"mixfs", MTLPixelFormatRGBA8Unorm, 0);
    p_pfx = mkpipe(@"fsq", @"pfxfs", MTLPixelFormatRGBA16Float, 0);
    p_nrm = mkpipe(@"fsq", @"nrmfs", MTLPixelFormatRGBA16Float, 0);
    p_ring = mkpipe(@"fsq", @"ringfs", MTLPixelFormatRGBA16Float, 0);
    p_taa = mkpipe(@"fsq", @"taafs", MTLPixelFormatRGBA16Float, 0);
    p_mvec = mkpipe(@"fsq", @"mvfs", MTLPixelFormatRG16Float, 0);
    p_mb = mkpipe(@"fsq", @"mbfs", MTLPixelFormatRGBA16Float, 0);
    {   /* track ribbons: the strongest wins where they overlap */
        MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
        d.vertexFunction = [L newFunctionWithName:@"trkvs"];
        d.fragmentFunction = [L newFunctionWithName:@"trkfs"];
        d.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA16Float;
        d.colorAttachments[0].blendingEnabled = YES;
        d.colorAttachments[0].rgbBlendOperation = d.colorAttachments[0].alphaBlendOperation = MTLBlendOperationMax;
        p_trk = [D newRenderPipelineStateWithDescriptor:d error:&err];
        if (!p_trk) { fprintf(stderr, "fx track pipeline: %s\n", err.localizedDescription.UTF8String); exit(1); }
        /* particles over the lit scene, premultiplied; the scene's coverage kept */
        d = [MTLRenderPipelineDescriptor new];
        d.vertexFunction = [L newFunctionWithName:@"fsq"];
        d.fragmentFunction = [L newFunctionWithName:@"pfxcfs"];
        d.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA16Float;
        d.colorAttachments[0].blendingEnabled = YES;
        d.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
        d.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        d.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorZero;
        d.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
        p_pfxc = [D newRenderPipelineStateWithDescriptor:d error:&err];
        if (!p_pfxc) { fprintf(stderr, "fx particle pipeline: %s\n", err.localizedDescription.UTF8String); exit(1); }
    }
    {
        MTLDepthStencilDescriptor *d = [MTLDepthStencilDescriptor new];
        d.depthCompareFunction = MTLCompareFunctionLess;
        d.depthWriteEnabled = YES;
        ds_sh = [D newDepthStencilStateWithDescriptor:d];
    }
    {
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                                     width:2048 height:2048 mipmapped:NO];
        td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        td.storageMode = MTLStorageModePrivate;
        t_sm = [D newTextureWithDescriptor:td];
        t_sm2 = [D newTextureWithDescriptor:td];
        t_smD = [D newTextureWithDescriptor:td];
    }
}

static dispatch_once_t g_setup_once;
static void setup_once(id<MTLDevice> dev) { dispatch_once(&g_setup_once, ^{ setup(dev); }); }

static void size(int w, int h)
{
    int i, hw = (w + 1) / 2, hh = (h + 1) / 2;
    if (w == tw && h == th) return;
    tw = w; th = h;
    t_mv = mktex(w, h, MTLPixelFormatRG16Float, 0);
    t_ao = mktex(hw, hh, MTLPixelFormatRGBA16Float, 0);
    t_ao2 = mktex(hw, hh, MTLPixelFormatRGBA16Float, 0);
    t_ssr = mktex(hw, hh, MTLPixelFormatRGBA16Float, 0);
    t_shaft = mktex(hw, hh, MTLPixelFormatRGBA16Float, 0);
    t_hdr = mktex(w, h, MTLPixelFormatRGBA16Float, 0);
    for (i = 0; i < 6; i++) t_bl[i] = mktex(hw >> i, hh >> i, MTLPixelFormatRGBA16Float, 0);
    /* the CPU reads the output only for scripted screenshots; otherwise it
     * stays in GPU memory, where the GPU can compress it */
    {
        int shots = getenv("BR_SHOTS") || getenv("BR_SHOT_DIR") || getenv("BR_SCRIPT");
        t_out = mktex(w, h, MTLPixelFormatRGBA8Unorm, shots);
        t_aa = mktex(w, h, MTLPixelFormatRGBA8Unorm, shots);
    }
    t_trk = mktex(hw, hh, MTLPixelFormatRGBA16Float, 0);
    t_pfx = mktex(hw, hh, MTLPixelFormatRGBA16Float, 0);
    t_nrm = mktex(hw, hh, MTLPixelFormatRGBA16Float, 0);
    t_ring = mktex(hw, hh, MTLPixelFormatRGBA16Float, 0);
    t_hist[0] = mktex(w, h, MTLPixelFormatRGBA16Float, 0);
    t_hist[1] = mktex(w, h, MTLPixelFormatRGBA16Float, 0);
    t_mb = mktex(w, h, MTLPixelFormatRGBA16Float, 0);
    g_hist_ok = 0; g_hist_ok_mfx = 0;
}

static const char *g_next_label;
void hfx_prof_attach(MTLRenderPassDescriptor *rp, const char *label) { prof_attach(rp, label); }
static id<MTLRenderCommandEncoder> pass(id<MTLCommandBuffer> cb, id<MTLTexture> t, int load)
{
    MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
    prof_attach(rp, g_next_label ? g_next_label : "?"); g_next_label = NULL;
    rp.colorAttachments[0].texture = t;
    rp.colorAttachments[0].loadAction = load ? MTLLoadActionLoad : MTLLoadActionClear;
    rp.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 0);
    rp.colorAttachments[0].storeAction = MTLStoreActionStore;
    return [cb renderCommandEncoderWithDescriptor:rp];
}
/* The plume noise: a 64^3 tiling volume.  R is billows (Perlin-like value
 * fbm lifted by inverted Worley cells, the look of rolling smoke), G is the
 * finer Worley fbm that erodes the edges into wisps. */
static float pn_hash(int x, int y, int z, int s)
{
    unsigned h = (unsigned)(x * 73856093) ^ (unsigned)(y * 19349663) ^ (unsigned)(z * 83492791) ^ (unsigned)(s * 2654435761u);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return (float)(h & 0xFFFFFF) / 16777216.0f;
}
static float pn_value(float x, float y, float z, int per, int s)
{
    int ix = (int)floorf(x), iy = (int)floorf(y), iz = (int)floorf(z), i;
    float fx = x - ix, fy = y - iy, fz = z - iz, c[8];
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy); fz = fz * fz * (3 - 2 * fz);
    for (i = 0; i < 8; i++)
        c[i] = pn_hash(((ix + (i & 1)) % per + per) % per, ((iy + ((i >> 1) & 1)) % per + per) % per, ((iz + (i >> 2)) % per + per) % per, s);
    return (((c[0] + (c[1] - c[0]) * fx) * (1 - fy) + (c[2] + (c[3] - c[2]) * fx) * fy) * (1 - fz)
          + ((c[4] + (c[5] - c[4]) * fx) * (1 - fy) + (c[6] + (c[7] - c[6]) * fx) * fy) * fz);
}
static float pn_worley(float x, float y, float z, int per, int s)
{
    int ix = (int)floorf(x), iy = (int)floorf(y), iz = (int)floorf(z), a, b, c;
    float best = 9;
    for (a = -1; a <= 1; a++) for (b = -1; b <= 1; b++) for (c = -1; c <= 1; c++) {
        int cx = ix + a, cy = iy + b, cz = iz + c;
        int wx = (cx % per + per) % per, wy = (cy % per + per) % per, wz = (cz % per + per) % per;
        float px = cx + pn_hash(wx, wy, wz, s), py = cy + pn_hash(wx, wy, wz, s + 1), pz = cz + pn_hash(wx, wy, wz, s + 2);
        float d = (px - x) * (px - x) + (py - y) * (py - y) + (pz - z) * (pz - z);
        if (d < best) best = d;
    }
    return fminf(sqrtf(best), 1.0f);
}
static id<MTLTexture> mk_plume_noise(id<MTLDevice> dev)
{
    enum { N = 64 };
    unsigned char *px = malloc(N * N * N * 2);
    int x, y, z;
    for (z = 0; z < N; z++) for (y = 0; y < N; y++) for (x = 0; x < N; x++) {
        float u = (float)x / N, v = (float)y / N, w = (float)z / N;
        float pv = pn_value(u * 4, v * 4, w * 4, 4, 1) * 0.5f + pn_value(u * 8, v * 8, w * 8, 8, 2) * 0.3f + pn_value(u * 16, v * 16, w * 16, 16, 3) * 0.2f;
        float wl = 1.0f - (pn_worley(u * 3, v * 3, w * 3, 3, 10) * 0.6f + pn_worley(u * 6, v * 6, w * 6, 6, 20) * 0.4f);
        float bil = pv * 0.55f + wl * 0.65f - 0.2f;
        float wisp = 1.0f - (pn_worley(u * 8, v * 8, w * 8, 8, 30) * 0.55f + pn_worley(u * 16, v * 16, w * 16, 16, 40) * 0.3f
                           + pn_worley(u * 32, v * 32, w * 32, 32, 50) * 0.15f);
        bil = bil < 0 ? 0 : bil > 1 ? 1 : bil;
        wisp = wisp < 0 ? 0 : wisp > 1 ? 1 : wisp;
        px[((z * N + y) * N + x) * 2] = (unsigned char)(bil * 255.0f + 0.5f);
        px[((z * N + y) * N + x) * 2 + 1] = (unsigned char)(wisp * 255.0f + 0.5f);
    }
    {
        MTLTextureDescriptor *d = [MTLTextureDescriptor new];
        id<MTLTexture> t;
        d.textureType = MTLTextureType3D; d.pixelFormat = MTLPixelFormatRG8Unorm;
        d.width = N; d.height = N; d.depth = N; d.usage = MTLTextureUsageShaderRead;
        t = [dev newTextureWithDescriptor:d];
        [t replaceRegion:MTLRegionMake3D(0, 0, 0, N, N, N) mipmapLevel:0 slice:0 withBytes:px bytesPerRow:N * 2 bytesPerImage:N * N * 2];
        free(px);
        return t;
    }
}

/* The road map: the track's collision mesh (the same triangles and surface
 * codes the wheels find, br_collgrid.c) drawn from above into a texture over
 * the track, 1 where the ground is sealed (surface 3), 0 where it is loose.
 * The lighting pass knows the road from it in every weather, snow included,
 * where the game paints road and verge alike. */
static id<MTLTexture> t_rmap;
static float g_rmap[4];            /* x0, y0, 1/width, 1/height (metres) */
static unsigned char *g_rmap_px;   /* the map as built, for hfx_road_sealed */
static int g_rmap_w;
/* host_marks.m: the ground under a point: 1 sealed, 0 loose, -1 off the
 * collision mesh, -2 no map built yet */
int hfx_road_sealed(float x, float y)
{
    int ix, iy;
    unsigned char v;
    if (!g_rmap_px) return -2;
    ix = (int)((x - g_rmap[0]) * g_rmap[2] * g_rmap_w); iy = (int)((y - g_rmap[1]) * g_rmap[3] * g_rmap_w);
    if (ix < 0 || iy < 0 || ix >= g_rmap_w || iy >= g_rmap_w) return -1;
    v = g_rmap_px[iy * g_rmap_w + ix];
    return v == 255 ? 1 : v ? 0 : -1;
}
static u32 g_rmap_key;
static void roadmap(id<MTLDevice> dev)
{
    u32 faces = H32(0x106EECE4u), verts = H32(0x106EECECu), flags = H32(0x106EED6Cu);
    u32 key = faces ^ (verts * 31u) ^ (flags * 131u);
    int nt, i, W = 2048;
    float lo[2] = { 1e9f, 1e9f }, hi[2] = { -1e9f, -1e9f };
    unsigned char *px;
    if (!faces || !verts || !flags || flags <= faces) return;
    if (key == g_rmap_key && t_rmap) return;
    nt = (int)((flags - faces) / 8u);
    if (nt <= 1 || nt > 200000) return;
    for (i = 1; i < nt; i++) {
        int k;
        for (k = 0; k < 3; k++) {
            u32 vi = W_LD(u16, faces, i * 8 + k * 2);
            float x = W_LD(f32, verts, vi * 12), y = W_LD(f32, verts, vi * 12 + 4);
            if (x < lo[0]) lo[0] = x; if (x > hi[0]) hi[0] = x;
            if (y < lo[1]) lo[1] = y; if (y > hi[1]) hi[1] = y;
        }
    }
    if (!(hi[0] > lo[0]) || !(hi[1] > lo[1])) return;
    lo[0] -= 4; lo[1] -= 4; hi[0] += 4; hi[1] += 4;
    px = calloc((size_t)W * W, 1);
    {
        float sx = W / (hi[0] - lo[0]), sy = W / (hi[1] - lo[1]);
        int nsealed = 0;
        for (i = 1; i < nt; i++) {
            float P[3][3], e1[3], e2[3], nz;
            int k, x0, x1, y0, y1, x, y;
            unsigned char v;
            for (k = 0; k < 3; k++) {
                u32 vi = W_LD(u16, faces, i * 8 + k * 2);
                P[k][0] = W_LD(f32, verts, vi * 12); P[k][1] = W_LD(f32, verts, vi * 12 + 4); P[k][2] = W_LD(f32, verts, vi * 12 + 8);
            }
            for (k = 0; k < 3; k++) { e1[k] = P[1][k] - P[0][k]; e2[k] = P[2][k] - P[0][k]; }
            nz = e1[0] * e2[1] - e1[1] * e2[0];
            {   /* only ground: walls and banks stand up from it */
                float nx = e1[1] * e2[2] - e1[2] * e2[1], ny = e1[2] * e2[0] - e1[0] * e2[2];
                if (fabsf(nz) < 0.3f * sqrtf(nx * nx + ny * ny + nz * nz)) continue;
            }
            v = (W_LD(u8, flags, i) & 7) == 3 ? 255 : 1;
            if (v == 255) nsealed++;
            for (k = 0; k < 3; k++) { P[k][0] = (P[k][0] - lo[0]) * sx; P[k][1] = (P[k][1] - lo[1]) * sy; }
            x0 = (int)floorf(fminf(P[0][0], fminf(P[1][0], P[2][0]))); x1 = (int)ceilf(fmaxf(P[0][0], fmaxf(P[1][0], P[2][0])));
            y0 = (int)floorf(fminf(P[0][1], fminf(P[1][1], P[2][1]))); y1 = (int)ceilf(fmaxf(P[0][1], fmaxf(P[1][1], P[2][1])));
            if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > W - 1) x1 = W - 1; if (y1 > W - 1) y1 = W - 1;
            {
                float d = (P[1][0] - P[0][0]) * (P[2][1] - P[0][1]) - (P[1][1] - P[0][1]) * (P[2][0] - P[0][0]);
                if (fabsf(d) < 1e-6f) continue;
                for (y = y0; y <= y1; y++) for (x = x0; x <= x1; x++) {
                    float qx = x + 0.5f - P[0][0], qy = y + 0.5f - P[0][1];
                    float b1 = (qx * (P[2][1] - P[0][1]) - qy * (P[2][0] - P[0][0])) / d;
                    float b2 = (qy * (P[1][0] - P[0][0]) - qx * (P[1][1] - P[0][1])) / d;
                    if (b1 < -0.02f || b2 < -0.02f || b1 + b2 > 1.02f) continue;
                    if (v > px[y * W + x]) px[y * W + x] = v;
                }
            }
        }
        if (getenv("BR_FX_CARPROBE"))
            fprintf(stderr, "roadmap: %d triangles, %d sealed, %.0f x %.0f m at %.2f m\n", nt - 1, nsealed,
                    hi[0] - lo[0], hi[1] - lo[1], (hi[0] - lo[0]) / W);
        if (getenv("BR_FX_RMAPDUMP")) {
            FILE *f = fopen(getenv("BR_FX_RMAPDUMP"), "wb");
            if (f) { fprintf(f, "P5\n%d %d\n255\n", W, W); fwrite(px, 1, (size_t)W * W, f); fclose(f); }
        }
    }
    {   /* r: the map; g: how far inside the sealed road (+) or outside it
         * (-) each texel is, in metres (a two-pass chamfer distance), for
         * the asphalt's broken edge */
        MTLTextureDescriptor *d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRG16Float width:W height:W mipmapped:NO];
        float ex = (hi[0] - lo[0]) / W, ey = (hi[1] - lo[1]) / W, ed = sqrtf(ex * ex + ey * ey);
        float *din = malloc(sizeof(float) * W * W), *dout = malloc(sizeof(float) * W * W);
        __fp16 *rg = malloc(sizeof(__fp16) * 2 * W * W);
        int pass, x, y;
        for (i = 0; i < W * W; i++) { din[i] = px[i] == 255 ? 1e9f : 0.0f; dout[i] = px[i] == 255 ? 0.0f : 1e9f; }
        for (pass = 0; pass < 2; pass++) {
            float *D2 = pass ? dout : din;
            for (y = 0; y < W; y++) for (x = 0; x < W; x++) {
                float v = D2[y * W + x];
                if (x > 0) v = fminf(v, D2[y * W + x - 1] + ex);
                if (y > 0) { v = fminf(v, D2[(y - 1) * W + x] + ey);
                    if (x > 0) v = fminf(v, D2[(y - 1) * W + x - 1] + ed);
                    if (x < W - 1) v = fminf(v, D2[(y - 1) * W + x + 1] + ed); }
                D2[y * W + x] = v; }
            for (y = W - 1; y >= 0; y--) for (x = W - 1; x >= 0; x--) {
                float v = D2[y * W + x];
                if (x < W - 1) v = fminf(v, D2[y * W + x + 1] + ex);
                if (y < W - 1) { v = fminf(v, D2[(y + 1) * W + x] + ey);
                    if (x < W - 1) v = fminf(v, D2[(y + 1) * W + x + 1] + ed);
                    if (x > 0) v = fminf(v, D2[(y + 1) * W + x - 1] + ed); }
                D2[y * W + x] = v; }
        }
        for (i = 0; i < W * W; i++) {
            float sd = px[i] == 255 ? din[i] - 0.5f * fminf(ex, ey) : -(dout[i] - 0.5f * fminf(ex, ey));
            rg[i * 2] = (__fp16)(px[i] / 255.0f); rg[i * 2 + 1] = (__fp16)fmaxf(fminf(sd, 60.0f), -60.0f); }
        d.usage = MTLTextureUsageShaderRead;
        t_rmap = [dev newTextureWithDescriptor:d];
        [t_rmap replaceRegion:MTLRegionMake2D(0, 0, W, W) mipmapLevel:0 withBytes:rg bytesPerRow:W * 4];
        free(din); free(dout); free(rg);
    }
    free(g_rmap_px); g_rmap_px = px; g_rmap_w = W;     /* kept for hfx_road_sealed */
    g_rmap[0] = lo[0]; g_rmap[1] = lo[1]; g_rmap[2] = 1.0f / (hi[0] - lo[0]); g_rmap[3] = 1.0f / (hi[1] - lo[1]);
    g_rmap_key = key;
}

/* The forest's canopy (host_env.m's grid of the Remastered trees' crown
 * cover) as a texture, rebuilt whenever the track's placements change */
int henv_canopy(const unsigned char **px, int *w, int *h, float *x0, float *y0, float *cell, int *gen);   /* host_env.m */
int hmarks_bind(id<MTLDevice> dev, id<MTLTexture> *atlas, id<MTLTexture> *page, float *u);   /* host_marks.m */
void hmarks_frame(void);
static id<MTLTexture> t_mka, t_mkp;
id<MTLTexture> hsky_band(id<MTLDevice> dev);   /* host_sky.m */
static id<MTLTexture> g_bandt;
static id<MTLTexture> t_cnp;
static float g_cmap[4];
static int g_cnp_gen = -1;
static void canopymap(id<MTLDevice> dev)
{
    const unsigned char *px; int w, h, gen; float x0, y0, cell;
    int ok = henv_canopy(&px, &w, &h, &x0, &y0, &cell, &gen);
    if (gen == g_cnp_gen && (ok != 0) == (t_cnp != nil)) return;
    g_cnp_gen = gen; t_cnp = nil; g_cmap[2] = 0;
    if (!ok) return;
    {
        MTLTextureDescriptor *d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                                                     width:(NSUInteger)w height:(NSUInteger)h mipmapped:NO];
        d.usage = MTLTextureUsageShaderRead;
        t_cnp = [dev newTextureWithDescriptor:d];
        [t_cnp replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)w, (NSUInteger)h) mipmapLevel:0 withBytes:px bytesPerRow:(NSUInteger)w];
    }
    g_cmap[0] = x0; g_cmap[1] = y0; g_cmap[2] = 1.0f / ((float)w * cell); g_cmap[3] = 1.0f / ((float)h * cell);
}

static void quad(id<MTLRenderCommandEncoder> e, id<MTLRenderPipelineState> p, const fxu *u, NSArray *tex)
{
    if (g_prof > 0 && g_prof_n > 0 && p.label) g_plab[g_prof_buf][g_prof_n - 1] = p.label.UTF8String;
    NSUInteger i;
    [e setRenderPipelineState:p];
    if (u) [e setFragmentBytes:u length:sizeof *u atIndex:0];
    for (i = 0; i < tex.count; i++) [e setFragmentTexture:tex[i] atIndex:i];
    [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [e endEncoding];
}

/* Every car the frame drew (BrCarDrawVehicle, native/car.m): its record
 * starts with its car -> world matrix (row vectors; the model's own units are
 * 1/255 of the world's).  Headlights are placed from it. */
#define FX_CARS 16
static float g_cars[FX_CARS][16];
static u32 g_carptr[FX_CARS], g_carmodel[FX_CARS];
static int g_ncars;
void hfx_car_seen(u32 car)
{
    int i;
    for (i = 0; i < g_ncars; i++) if (g_carptr[i] == car) return;   /* the mirror draws them again */
    if (g_ncars == FX_CARS) return;
    g_carptr[g_ncars] = car;
    g_carmodel[g_ncars] = W_LD(u32, car, 0x29C4);                    /* its .rca model record */
    for (i = 0; i < 16; i++) g_cars[g_ncars][i] = W_LD(f32, car, 4 * i);
    g_ncars++;
}

/* A car model's extent in its own frame (x forward, y left, z up; world
 * metres), measured once from the vertices of its full-detail body display
 * list: G_VTX loads (0x04) of eight floats per vertex, x y z first, followed
 * into called lists (0x06) until G_ENDDL (0xB8).  The lamps sit on its faces. */
typedef struct { u32 model; float lo[3], hi[3]; int ok; } carbox;
static carbox g_box[32];
static int g_nbox;
static void box_dl(u32 dl, carbox *b, int depth)
{
    int k;
    if (!dl || depth > 6) return;
    for (k = 0; k < 4096; k++, dl += 8) {
        u32 w0 = W_LD(u32, dl, 0), w1 = W_LD(u32, dl, 4), op = w0 >> 24;
        if (op == 0xB8) return;
        if (op == 0x06) { box_dl(w1, b, depth + 1); if (w0 & 0x00010000u) return; continue; }
        if (op == 0x04) {
            int n = (int)((w0 >> 10) & 0x3F), v, c;
            for (v = 0; v < n; v++)
                for (c = 0; c < 3; c++) {
                    float x = W_LD(f32, w1 + (u32)v * 0x20u, 4 * c) / 255.0f;
                    if (!(x > -100 && x < 100)) continue;
                    if (!b->ok || x < b->lo[c]) b->lo[c] = x;
                    if (!b->ok || x > b->hi[c]) b->hi[c] = x;
                    if (c == 2) b->ok = 1;
                }
        }
    }
}
static const carbox *car_box(u32 model)
{
    int i;
    for (i = 0; i < g_nbox; i++) if (g_box[i].model == model) return &g_box[i];
    if (g_nbox == 32 || !model) return NULL;
    memset(&g_box[g_nbox], 0, sizeof g_box[g_nbox]);
    g_box[g_nbox].model = model;
    box_dl(W_LD(u32, model, 0x8024), &g_box[g_nbox], 0);   /* LOD 0 body */
    if (getenv("BR_FX_CARPROBE"))
        fprintf(stderr, "carbox %08X: x %.2f..%.2f y %.2f..%.2f z %.2f..%.2f\n", model,
                g_box[g_nbox].lo[0], g_box[g_nbox].hi[0], g_box[g_nbox].lo[1], g_box[g_nbox].hi[1],
                g_box[g_nbox].lo[2], g_box[g_nbox].hi[2]);
    return &g_box[g_nbox++];
}
/* The Remastered car can say exactly where its lamps are (car frame, metres:
 * front left, front right, rear left, rear right). */
__attribute__((weak)) int hcar_lamps(u32 car, float out[4][3]) { (void)car; (void)out; return 0; }

/* The light rig for physically lit models drawn into the scene (the
 * Remastered car).  The scene rig above is relative to the game's own
 * shading, which already darkens its textures and vertex colours at night
 * and in storms; a model with true albedo gets no such darkening, so its sky
 * light is scaled by how dark the game makes the world in that weather.
 * Filled at every swap; zero before the first. */
static float g_car_rig[4][4];
static int g_car_rig_ok;
int hfx_car_rig(float *sun, float *sunc, float *skyc, float *grnd)
{
    if (!g_car_rig_ok) return 0;
    memcpy(sun, g_car_rig[0], 16); memcpy(sunc, g_car_rig[1], 16);
    memcpy(skyc, g_car_rig[2], 16); memcpy(grnd, g_car_rig[3], 16);
    return 1;
}

static float rdf(u32 a) { float f; u32 v = H32(a); memcpy(&f, &v, 4); return f; }

/* The light rig for the game's weather. */
static void weather(fxu *u, const float *fogc)
{
    int w = (int)H32(0x104B15E8u);
    int lightning = (int)H32(0x100A79CCu);
    const char *ov = getenv("BR_FX_WEATHER");
    float sunk, sk[3], gr[3], sc[3];
    if (ov) w = atoi(ov);
    /* p0: ao, gi, ssr, wet   p1: exposure, saturation, contrast, bloom
     * p2: shafts, shadow softness (texels), ao radius, frame
     * p3: origin_ll, -, rough, shadow texel   p4: sky gain, sun glow, fog light, light scale */
    /* Levels are relative to the game's own shading, which the textures and
     * vertex colours already carry: a surface facing the sun ends near 1.15x,
     * one in shadow near 0.6x, and shadow is filled with the sky's colour.
     * hz: the air's brightness at the horizon; hf: 1 = the air takes the
     * game's fog colour (night, fog) instead of the sky's. */
    float hz = 1.3f, hf = 0.0f;
    float tm0[4] = { 0.0f, 1.0f, 1.33f, 0.0f };   /* the tone curve: PBR Neutral unless the weather says */
    float wb[3] = { 1.03f, 1.0f, 0.96f }, ns[3] = { 0, 0, 0 };
    switch (w) {
    case 4:  /* night, clear and dry (the game's grip tables give it the dry
              * row): moonlight, lit windows glowing */
        /* the moon and the sky's fill dim enough that a headlamp's pool is
         * the brightest thing on the road by far, and what lies outside it
         * still reads, darkly */
        /* and neutral: the dark is black-grey, not blue (a blue cast reads
         * as murk; a neutral one keeps what is in it visible) */
        sunk = 0.09f; sc[0] = 0.85f; sc[1] = 0.9f; sc[2] = 1.0f;
        sk[0] = 0.7f; sk[1] = 0.72f; sk[2] = 0.77f; gr[0] = 0.4f; gr[1] = 0.4f; gr[2] = 0.42f;
        memcpy(u->p0, (float[4]){ 1.0f, 0.5f, 0.0f, 0.0f }, 16);
        /* bloom as by day: stronger, it laid the sky's blue and the lamps'
         * glow over every dark surface, a blue haze instead of black */
        memcpy(u->p1, (float[4]){ 1.25f, 0.95f, 1.04f, 0.07f }, 16);
        u->p2[0] = 0.0f; u->p2[1] = 7.0f; u->p3[2] = 0.35f; u->p4[0] = 1.0f; u->p4[1] = 0.0f;
        u->scr[2] = 0.3f; hf = 1.0f;
        wb[0] = 0.92f; wb[1] = 0.97f; wb[2] = 1.1f;      /* moonlight: cool */
        /* filmic (the Forza Horizon 6 night set: median 0.01, greens
         * desaturated), the toe for true black */
        u->p1[0] = 0.8f; u->p1[1] = 0.75f; tm0[0] = 1.0f; tm0[1] = 1.15f; tm0[2] = 1.7f;
        ns[0] = 0.018f; ns[1] = 0.03f; ns[2] = 0.065f;
        break;
    case 2:  /* storm: overcast, wet, lightning */
        sunk = 0.10f; sc[0] = 0.8f; sc[1] = 0.85f; sc[2] = 1.0f;
        sk[0] = 0.78f; sk[1] = 0.82f; sk[2] = 0.9f; gr[0] = 0.42f; gr[1] = 0.42f; gr[2] = 0.45f;
        memcpy(u->p0, (float[4]){ 1.0f, 0.35f, 0.75f, 1.0f }, 16);
        memcpy(u->p1, (float[4]){ 1.12f, 0.92f, 1.04f, 0.1f }, 16);
        u->p2[0] = 0.0f; u->p2[1] = 9.0f; u->p3[2] = 0.35f; u->p4[0] = 1.05f; u->p4[1] = 0.0f;
        hz = 1.0f; wb[0] = 0.94f; wb[1] = 1.0f; wb[2] = 1.1f;
        u->p1[0] = 0.95f; u->p1[1] = 1.1f; tm0[0] = 1.0f; tm0[1] = 1.25f; tm0[2] = 1.7f;
        break;
    case 3:  /* snow: bright overcast, strong bounce */
        sunk = 0.55f; sc[0] = 1.0f; sc[1] = 0.97f; sc[2] = 0.93f;
        sk[0] = 0.62f; sk[1] = 0.68f; sk[2] = 0.8f; gr[0] = 0.5f; gr[1] = 0.52f; gr[2] = 0.58f;
        memcpy(u->p0, (float[4]){ 1.0f, 0.35f, 0.1f, 0.0f }, 16);
        memcpy(u->p1, (float[4]){ 1.0f, 1.0f, 1.08f, 0.08f }, 16);
        u->p2[0] = 0.12f; u->p2[1] = 3.0f; u->p3[2] = 0.45f; u->p4[0] = 1.1f; u->p4[1] = 0.5f;
        hz = 1.25f; wb[0] = 0.97f; wb[1] = 1.0f; wb[2] = 1.06f;
        /* exposures measured in real races of each weather (the game's own
         * fog colour and sky) against the Forza Horizon 6 sets */
        u->p1[0] = 0.7f; u->p1[1] = 0.85f; tm0[0] = 1.0f; tm0[1] = 1.15f; tm0[2] = 1.7f;
        break;
    case 1:  /* fog: soft and diffuse */
        sunk = 0.3f; sc[0] = 1.0f; sc[1] = 0.97f; sc[2] = 0.92f;
        sk[0] = 0.8f; sk[1] = 0.82f; sk[2] = 0.86f; gr[0] = 0.5f; gr[1] = 0.49f; gr[2] = 0.47f;
        memcpy(u->p0, (float[4]){ 0.9f, 0.35f, 0.12f, 0.2f }, 16);
        memcpy(u->p1, (float[4]){ 1.08f, 0.95f, 1.0f, 0.1f }, 16);
        u->p2[0] = 0.0f; u->p2[1] = 10.0f; u->p3[2] = 0.5f; u->p4[0] = 1.05f; u->p4[1] = 0.2f;
        hf = 1.0f; wb[0] = 1.0f; wb[1] = 1.0f; wb[2] = 1.02f;
        u->p1[0] = 0.65f; u->p1[1] = 0.8f; tm0[0] = 1.0f; tm0[1] = 1.4f; tm0[2] = 1.7f;
        break;
    default: /* sunny: clean high-key daylight.  Measured against the Forza
              * Horizon 6 daylight set (median luminance 0.10, saturation
              * 0.40, greens toward yellow): a sun twice the sky's old ratio, the
              * filmic curve's toe for real blacks, aged-asphalt grey roads */
        sunk = 3.4f; sc[0] = 1.0f; sc[1] = 0.93f; sc[2] = 0.8f;
        sk[0] = 0.4f; sk[1] = 0.5f; sk[2] = 0.72f; gr[0] = 0.36f; gr[1] = 0.31f; gr[2] = 0.25f;
        memcpy(u->p0, (float[4]){ 1.0f, 0.4f, 0.07f, 0.0f }, 16);
        memcpy(u->p1, (float[4]){ 1.9f, 1.0f, 1.06f, 0.07f }, 16);
        tm0[0] = 1.0f; tm0[1] = 1.15f; tm0[2] = 1.7f; tm0[3] = 0.085f;
        wb[0] = 1.07f; wb[1] = 1.0f; wb[2] = 0.88f;
        u->p2[0] = 0.3f; u->p2[1] = 2.2f; u->p3[2] = 0.45f; u->p4[0] = 1.15f; u->p4[1] = 1.0f;
        u->scr[2] = 1.1f;
        break;
    }
    /* tuning: BR_FX_SUNK (the sun's strength), BR_FX_SKYK (sky and ground fill), BR_FX_EXP, BR_FX_SAT,
     * BR_FX_CON, BR_FX_TM (curve), BR_FX_TMA / BR_FX_TOE (its contrast, toe),
     * BR_FX_WB r,g,b */
    {
        const char *e;
        memcpy(u->tmo, tm0, 16);
        u->air2[0] = w == 0 ? 0.25f : w == 1 ? 0.3f : w == 3 ? 0.2f : 0.055f;   /* clear air still layers the ridges: a third gone by 2 km */
        if ((e = getenv("BR_FX_HAZE"))) u->air2[0] = (float)atof(e);
        if ((e = getenv("BR_FX_SUNK"))) sunk = (float)atof(e);
        if ((e = getenv("BR_FX_SKYK"))) { float k = (float)atof(e); int i2; for (i2 = 0; i2 < 3; i2++) { sk[i2] *= k; gr[i2] *= k; } }
        if ((e = getenv("BR_FX_EXP"))) u->p1[0] = (float)atof(e);
        if ((e = getenv("BR_FX_SAT"))) u->p1[1] = (float)atof(e);
        if ((e = getenv("BR_FX_CON"))) u->p1[2] = (float)atof(e);
        if ((e = getenv("BR_FX_TM"))) u->tmo[0] = (float)atof(e);
        if ((e = getenv("BR_FX_TMA"))) u->tmo[1] = (float)atof(e);
        if ((e = getenv("BR_FX_TOE"))) u->tmo[2] = (float)atof(e);
        if ((e = getenv("BR_FX_ASPH"))) u->tmo[3] = (float)atof(e);
        if ((e = getenv("BR_FX_WB"))) sscanf(e, "%f,%f,%f", &wb[0], &wb[1], &wb[2]);
    }
    u->sunc[0] = sc[0] * sunk; u->sunc[1] = sc[1] * sunk; u->sunc[2] = sc[2] * sunk;
    memcpy(u->skyc, sk, 12); memcpy(u->grnd, gr, 12);
    u->skyc[3] = hz; u->grnd[3] = hf;
    /* haze: how far toward the air colour distance takes a surface, and the
     * sky's share of fog; headlights: on at night, in storms and in fog */
    u->nsky[3] = w == 1 ? 0.9f : 0.3f;
    /* falling rain (storm heaviest), falling snow, snow on the ground */
    u->wx[0] = w == 2 ? 1.0f : 0.0f;
    u->wx[1] = w == 3 ? 1.0f : 0.0f;
    u->wx[2] = w == 3 ? 1.0f : 0.0f;
    u->wx[3] = getenv("BR_FX_DETAIL") ? (float)atof(getenv("BR_FX_DETAIL")) : 1.0f;
    /* a real sky in clear weather (clouds kept from texels whose darkest
     * channel is between sk2.y and sk2.z), and water shading */
    u->sk2[0] = w == 0 ? 1.0f : 0.0f; u->sk2[1] = 0.36f; u->sk2[2] = 0.55f;
    u->sk2[3] = w == 1 ? 0.5f : 1.0f;
    u->wb[3] = w == 1 ? 0.8f : 0.0f;
    g_hl_int = w == 4 ? 1.0f : w == 2 ? 0.6f : w == 1 ? 0.5f : 0.0f;
    g_hl_beam = w == 4 ? 1.0f : w == 2 ? 0.8f : w == 1 ? 1.4f : 0.0f;
    /* how much the air scatters a beam: clear night air barely, rain a
     * little, fog most; any more and the scene reads as seen through film */
    g_hl_air = w == 1 ? 0.0035f : w == 2 ? 0.0012f : 0.0007f;
    {   /* the world's darkness per weather, for the car rig (see hfx_car_rig) */
        float wk = w == 4 ? 0.15f : w == 2 ? 0.7f : w == 1 ? 0.9f : 1.0f;
        int i;
        g_wdark = wk;
        for (i = 0; i < 3; i++) {
            g_car_rig[1][i] = u->sunc[i];
            g_car_rig[2][i] = u->skyc[i] * wk * (w == 4 ? 0.9f : 1.0f);
            g_car_rig[3][i] = u->grnd[i] * wk;
        }
        g_car_rig_ok = 1;
    }
    memcpy(u->wb, wb, 12); memcpy(u->nsky, ns, 12);
    u->p4[2] = 1.0f;      /* fogged pixels keep the game's own light */
    if (u->scr[2] == 0) u->scr[2] = 0.8f;    /* bloom threshold */
    u->p4[3] = 1.0f;
    u->sun[3] = sunk > 0.2f ? 1.0f : 0.0f;
    u->fogc[0] = fogc[0]; u->fogc[1] = fogc[1]; u->fogc[2] = fogc[2];
    /* haze density per metre: clear air, then rain, snow, storm, fog */
    u->fogc[3] = w == 1 ? 0.014f : w == 2 ? 0.003f : w == 4 ? 0.0025f : w == 3 ? 0.002f : 0.0012f;
    /* lightning: the strike counts 3..1 while it lights the sky */
    if (w == 2 && lightning > 0 && lightning <= 3) {
        u->flash[0] = 0.8f; u->flash[1] = 0.85f; u->flash[2] = 1.0f; u->flash[3] = 2.5f * (float)lightning / 3.0f;
    }
    { const char *e;
      if ((e = getenv("BR_FX_AOR"))) u->p2[2] = (float)atof(e);
      if ((e = getenv("BR_FX_DEBUG"))) u->p3[1] = (float)atof(e);
      if (getenv("BR_FX_NOSHAFT")) u->p2[0] = 0;
      if (getenv("BR_FX_NOSHADOW")) u->sun[3] = 0;
      if (getenv("BR_FX_NOBLOOM")) u->p1[3] = 0; }
}

    /* BR_FX_TESTKEY=N,M,...: post a real ~ key press at those swaps (a
     * windowed check of the switch's keyboard path) */
static void materials_once(id<MTLDevice> dev);
void henv_prefetch(void);   /* host_env.m */
void hcar_prefetch(void);   /* host_car.m */
void hfx_tick(void)
{
    if (!hfx_on()) g_ncars = 0;
    else {
        /* what Remastered draws, loaded in the background before the race
         * needs it: the shaders and ground materials and the car once, the
         * environment for whichever track is chosen */
        static int pre;
        if (!pre) {
            id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
            pre = 1;
            dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{ @autoreleasepool {
                setup_once(dev);
                materials_once(dev);
            } });
            hcar_prefetch();
        }
        henv_prefetch();
    }
    { static unsigned n; const char *t = getenv("BR_FX_TESTKEY"); n++;
      if (t && happ_window()) {
          char buf[256], *q, *sv; snprintf(buf, sizeof buf, "%s", t);
          for (q = strtok_r(buf, ",", &sv); q; q = strtok_r(NULL, ",", &sv))
              if ((unsigned)atoi(q) == n) {
                  NSEventType ty[2] = { NSEventTypeKeyDown, NSEventTypeKeyUp }; int k;
                  for (k = 0; k < 2; k++)
                      [NSApp postEvent:[NSEvent keyEventWithType:ty[k] location:NSZeroPoint modifierFlags:0
                                          timestamp:NSProcessInfo.processInfo.systemUptime windowNumber:happ_window().windowNumber
                                          context:nil characters:@"`" charactersIgnoringModifiers:@"`" isARepeat:NO keyCode:0x32] atStart:NO];
              } } }
}

/* ---- the ground materials: photoscanned sets (ambientCG, CC0) from the app's
 * Resources/materials, else the tree's ports/common/models/materials/src,
 * each colour map and its normal+roughness+AO packed into two 1024x1024
 * texture arrays with full mip chains.  Missing sets leave the procedural
 * detail in place. */
#import <ImageIO/ImageIO.h>
/* Poly Haven (CC0) for the dirt road and the forest floor, converted to the
 * same layout by name */
enum { NMAT = 8 };
static const char *MATS[NMAT] = { "Asphalt031", "Grass004", "Ground054", "forest_ground_04", "Rock064", "Snow015", "mud_forest",
                                  "stony_dirt_path" };
static id<MTLTexture> g_matA, g_matN;
static float g_matmean[NMAT][4];
static int g_mat_state;              /* 0 not tried, 1 loaded, -1 unavailable */
static int load_map(const char *name, const char *kind, u8 *out, int sz)
{
    NSString *base = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"materials"];
    NSString *f = [NSString stringWithFormat:@"%s/%s_2K-JPG_%s.jpg", name, name, kind];
    NSString *p = [base stringByAppendingPathComponent:f];
    CGImageSourceRef src;
    CGImageRef img;
    CGContextRef cx;
    if (![[NSFileManager defaultManager] fileExistsAtPath:p]) {
        NSString *root = @(getenv("BR_ROOT") ? getenv("BR_ROOT") : ".");
        p = [[root stringByAppendingPathComponent:@"ports/common/models/materials/src"] stringByAppendingPathComponent:f];
    }
    src = CGImageSourceCreateWithURL((__bridge CFURLRef)[NSURL fileURLWithPath:p], NULL);
    if (!src) return 0;
    img = CGImageSourceCreateImageAtIndex(src, 0, NULL);
    CFRelease(src);
    if (!img) return 0;
    {
        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        cx = CGBitmapContextCreate(out, (size_t)sz, (size_t)sz, 8, (size_t)sz * 4, cs, kCGImageAlphaNoneSkipLast);
        CGColorSpaceRelease(cs);
    }
    CGContextSetInterpolationQuality(cx, kCGInterpolationHigh);
    CGContextDrawImage(cx, CGRectMake(0, 0, sz, sz), img);
    CGContextRelease(cx);
    CGImageRelease(img);
    return 1;
}
void hload_for(int n, void (^f)(int i));   /* host_load.m */
/* every map decodes on its own core, then each set packs on its own; the
 * mip chains are built on a queue of their own, waited for (this may run on
 * a prefetch thread, ahead of the first frame that samples them) */
static void load_materials(id<MTLDevice> dev)
{
    enum { SZ = 1024, NK = 5 };
    static const char *const KIND[NK] = { "Color", "NormalGL", "Roughness", "AmbientOcclusion", "Displacement" };
    int i, ok = 0;
    u8 **buf = calloc(NMAT * NK, sizeof *buf);
    int *got = calloc(NMAT * NK, sizeof *got), *setok = calloc(NMAT, sizeof *setok);
    MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:SZ height:SZ mipmapped:YES];
    id<MTLTexture> ta, tn;
    td.textureType = MTLTextureType2DArray; td.arrayLength = NMAT;
    g_mat_state = -1;
    if (getenv("BR_FX_NOMAT")) { free(buf); free(got); free(setok); return; }
    ta = [dev newTextureWithDescriptor:td];
    tn = [dev newTextureWithDescriptor:td];
    hload_for(NMAT * NK, ^(int j) { @autoreleasepool {
        buf[j] = malloc((size_t)SZ * SZ * 4);
        got[j] = load_map(MATS[j / NK], KIND[j % NK], buf[j], SZ);
    } });
    hload_for(NMAT, ^(int m) {
        u8 *c = buf[m * NK], *n = buf[m * NK + 1], *r = buf[m * NK + 2], *a = buf[m * NK + 3], *h = buf[m * NK + 4];
        double sum[3] = { 0, 0, 0 };
        int i2, k;
        if (!got[m * NK] || !got[m * NK + 1]) return;
        if (!got[m * NK + 2]) memset(r, 180, (size_t)SZ * SZ * 4);
        if (!got[m * NK + 3]) memset(a, 255, (size_t)SZ * SZ * 4);
        if (!got[m * NK + 4]) memset(h, 128, (size_t)SZ * SZ * 4);
        for (i2 = 0; i2 < SZ * SZ; i2++) {
            for (k = 0; k < 3; k++) sum[k] += pow(c[i2 * 4 + k] / 255.0, 2.2);
            c[i2 * 4 + 3] = h[i2 * 4];           /* height, for parallax and puddles */
            n[i2 * 4 + 2] = r[i2 * 4]; n[i2 * 4 + 3] = a[i2 * 4];
        }
        for (k = 0; k < 3; k++) g_matmean[m][k] = (float)(sum[k] / (SZ * SZ));
        [ta replaceRegion:MTLRegionMake2D(0, 0, SZ, SZ) mipmapLevel:0 slice:(NSUInteger)m withBytes:c bytesPerRow:SZ * 4 bytesPerImage:0];
        [tn replaceRegion:MTLRegionMake2D(0, 0, SZ, SZ) mipmapLevel:0 slice:(NSUInteger)m withBytes:n bytesPerRow:SZ * 4 bytesPerImage:0];
        setok[m] = 1;
    });
    for (i = 0; i < NMAT; i++) ok += setok[i];
    if (ok == NMAT) {
        id<MTLCommandBuffer> b = [[dev newCommandQueue] commandBuffer];
        id<MTLBlitCommandEncoder> e = [b blitCommandEncoder];
        [e generateMipmapsForTexture:ta]; [e generateMipmapsForTexture:tn];
        [e endEncoding]; [b commit]; [b waitUntilCompleted];
        g_matA = ta; g_matN = tn;
        g_mat_state = 1;
    } else fprintf(stderr, "fx: ground materials missing (%d of %d)\n", ok, NMAT);
    for (i = 0; i < NMAT * NK; i++) free(buf[i]);
    free(buf); free(got); free(setok);
}
static dispatch_once_t g_mat_once;
static void materials_once(id<MTLDevice> dev) { dispatch_once(&g_mat_once, ^{ load_materials(dev); }); }

/* ---- temporal: the jitter this frame's 3D is drawn with (Halton 2,3 over
 * 8 frames, in the target's pixels, as clip-space NDC for host_glide.m), last
 * frame's camera, and each car's last transform */
static float g_jit[2], g_pvp[16];
static int g_pvp_ok, g_jit_i;
static int g_taa = -1;
static int taa_on(void) { if (g_taa < 0) g_taa = !getenv("BR_FX_TAA") || atoi(getenv("BR_FX_TAA")); return g_taa && !hglide_temporal(); }
static float halton(int i, int b) { float f = 1, r = 0; while (i > 0) { f /= (float)b; r += f * (float)(i % b); i /= b; } return r; }
/* host_glide.m: the offset (NDC) to add to every 3D corner this frame */
void hfx_jitter(float *jx, float *jy, int rw, int rh)
{
    if (!hfx_on() || !(taa_on() || hglide_temporal())) { *jx = *jy = 0; return; }
    *jx = g_jit[0] * 2.0f / (float)rw; *jy = g_jit[1] * 2.0f / (float)rh;
}
static void next_jitter(void)
{
    g_jit_i = (g_jit_i + 1) % 8;
    g_jit[0] = halton(g_jit_i + 1, 2) - 0.5f; g_jit[1] = halton(g_jit_i + 1, 3) - 0.5f;
}
typedef struct { float cur[16][4][4], prev[16][4][4], misc[4]; } mvu;
static u32 g_prevcar[FX_CARS]; static float g_prevmat[FX_CARS][16]; static int g_nprev;

/* ---- weather effects the port draws itself in Remastered: particles
 * (the game's smoke, dust and snow spray, and water spray and splashes from
 * every wheel in the wet), falling rain and snow, tyre tracks.  The game's
 * own particle and rain draws are left out (hfx_game_particles). */
int hfx_game_particles(void) { return !hfx_on() || getenv("BR_FX_GAMEPFX") != NULL; }

static float fhash2(float x, float y)
{
    float v = sinf(x * 127.1f + y * 311.7f) * 43758.5453f;
    return v - floorf(v);
}
static float fvnoise(float x, float y)
{
    float ix = floorf(x), iy = floorf(y), fx = x - ix, fy = y - iy;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    float a = fhash2(ix, iy), b = fhash2(ix + 1, iy), c = fhash2(ix, iy + 1), d = fhash2(ix + 1, iy + 1);
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}
static float ffbm(float x, float y)   /* the shader's fbm, for where its puddles are */
{
    return fvnoise(x, y) * 0.55f + fvnoise(x * 2.03f + 17.1f, y * 2.03f + 17.1f) * 0.3f
         + fvnoise(x * 4.1f + 5.3f, y * 4.1f + 5.3f) * 0.15f;
}
static float frand(void) { return (float)rand() / (float)RAND_MAX; }

typedef struct { float p[3], v[3], age, life, r0, r1, dens, col[3], grav, drag; } sprt;
#define NSPR 1400
static sprt g_spr[NSPR];
static int g_nspr;
static float g_emit[FX_CARS][4], g_semit[FX_CARS][4];

#define TRK_N 320
typedef struct { float p[TRK_N][3], k[TRK_N], t[TRK_N]; int head, n; u32 car; } trkw;
static trkw g_trk[FX_CARS * 4];
static float g_time;

static const u32 WHEEL_REC[4] = { 0x994, 0x57C, 0x370, 0x788 };

static void emit(const float *p, const float *v, float life, float r0, float r1, float dens,
                 float cr, float cg, float cb, float grav, float drag)
{
    sprt *s;
    if (g_nspr == NSPR) return;
    s = &g_spr[g_nspr++];
    memcpy(s->p, p, 12); memcpy(s->v, v, 12);
    s->age = 0; s->life = life; s->r0 = r0; s->r1 = r1; s->dens = dens;
    s->col[0] = cr; s->col[1] = cg; s->col[2] = cb; s->grav = grav; s->drag = drag;
}

/* One frame of the port's own weather effects: spray and splashes from each
 * wheel, and the tyre-track samples. */
static void fx_sim(int w, float wetness, float dt)
{
    int c, k, i;
    float wetroad = wetness;
    g_time += dt;
    /* age and move the spray */
    for (i = 0; i < g_nspr; ) {
        sprt *s = &g_spr[i];
        s->age += dt;
        if (s->age >= s->life) { g_spr[i] = g_spr[--g_nspr]; continue; }
        for (k = 0; k < 3; k++) { s->v[k] *= 1.0f - s->drag * dt; s->p[k] += s->v[k] * dt; }
        s->v[2] -= s->grav * dt;
        i++;
    }
    for (c = 0; c < g_ncars; c++) {
        u32 car = g_carptr[c];
        const float *m = g_cars[c];
        float up[3] = { m[8], m[9], m[10] }, fw[3] = { m[0], m[1], m[2] };
        float vel[3] = { W_LD(f32, car, 0x1024), W_LD(f32, car, 0x1028), W_LD(f32, car, 0x102C) };
        float spd = sqrtf(vel[0] * vel[0] + vel[1] * vel[1] + vel[2] * vel[2]);
        for (k = 0; k < 4; k++) {
            float hub[3] = { W_LD(f32, car, 0x70 + 0x40 * k), W_LD(f32, car, 0x74 + 0x40 * k), W_LD(f32, car, 0x78 + 0x40 * k) };
            float gp[3];
            int on = W_LD(s32, car, WHEEL_REC[k] + 0x1B4) != 0;
            int surf = W_LD(s8, car, WHEEL_REC[k] + 0x1A0);
            int j;
            for (j = 0; j < 3; j++) gp[j] = hub[j] - up[j] * 0.3f;
            /* tracks: a sample every 0.3 m while the tyre is down on something
             * that keeps a mark */
            {
                trkw *t = &g_trk[c * 4 + k];
                float kk = 0;
                if (t->car != car) { memset(t, 0, sizeof *t); t->car = car; }
                if (on) kk = (surf >= 1 && surf <= 3) ? (w == 3 ? 1.0f : 0.8f) : (wetroad > 0.5f ? 0.6f : 0.0f);
                if (kk > 0) {
                    int last = (t->head + TRK_N - 1) % TRK_N;
                    float dx = gp[0] - t->p[last][0], dy = gp[1] - t->p[last][1], dz = gp[2] - t->p[last][2];
                    float d2 = dx * dx + dy * dy + dz * dz;
                    if (t->n == 0 || d2 > 0.09f) {
                        if (t->n > 0 && d2 > 4.0f) t->k[last] = -1;     /* a gap: the ribbon breaks here */
                        memcpy(t->p[t->head], gp, 12); t->k[t->head] = kk; t->t[t->head] = g_time;
                        t->head = (t->head + 1) % TRK_N; if (t->n < TRK_N) t->n++;
                    }
                } else if (t->n > 0) t->k[(t->head + TRK_N - 1) % TRK_N] = -1;
            }
            /* water: a fine spray off every tyre on a wet road, and a splash
             * where the tyre goes through standing water */
            if (on && wetroad > 0.0f && spd > 4.0f && w != 3) {
                float pud = ffbm(gp[0] * 0.18f, gp[1] * 0.18f), lo = 0.64f - 0.14f * wetroad;
                int inpud = pud > lo + 0.04f;
                float rate = (inpud ? 50.0f : 50.0f * wetroad) * fminf(spd / 30.0f, 1.6f);
                g_emit[c][k] += rate * dt;
                while (g_emit[c][k] >= 1.0f) {
                    float p[3], v[3], sd = (k == 0 || k == 3) ? 1.0f : -1.0f;
                    float lf[3] = { m[4], m[5], m[6] };
                    g_emit[c][k] -= 1.0f;
                    for (j = 0; j < 3; j++) {
                        p[j] = gp[j] - fw[j] * 0.35f + up[j] * 0.05f;
                        v[j] = vel[j] * (inpud ? 0.45f : 0.55f) - fw[j] * spd * 0.05f
                             + up[j] * (inpud ? 1.6f + 2.2f * frand() : 0.5f + 0.8f * frand())
                             + lf[j] * sd * (inpud ? 1.5f + 1.5f * frand() : 0.4f * frand());
                    }
                    if (inpud) emit(p, v, 0.45f + 0.25f * frand(), 0.3f, 0.9f, 0.7f, 0.78f, 0.82f, 0.86f, 7.0f, 0.8f);
                    else emit(p, v, 0.6f + 0.4f * frand(), 0.7f, 2.0f + 0.5f * frand(), 0.1f, 0.8f, 0.83f, 0.87f, 0.4f, 1.6f);
                }
            } else g_emit[c][k] = 0;
            /* tyre smoke from sliding on a hard surface; dust (or powder
             * snow) thrown up behind every tyre on a loose one, more with
             * speed and more again when sliding */
            {
                float lf[3] = { m[4], m[5], m[6] };
                float lat = fabsf(vel[0] * lf[0] + vel[1] * lf[1] + vel[2] * lf[2]);
                float slide = fminf(fmaxf((lat - 2.5f) / 7.0f, 0.0f), 1.0f);
                /* the game's grip tables: 3 and 11 are the sealed surfaces */
                int loose = surf != 3 && surf != 11, rear = k == 1 || k == 2;
                float rate = 0;
                int rain = w == 2 || wetroad > 0.3f;   /* wet: neither smoke nor dust */
                if (rain) rate = 0;
                else if (on && loose) rate = (fminf(spd / 25.0f, 1.3f) * 14.0f + slide * 22.0f) * (rear ? 1.0f : 0.35f);
                else if (on) rate = fminf(slide * 2.0f, 1.0f) * (rear ? 30.0f : 12.0f);
                if (spd < 1.5f && slide <= 0) rate = 0;
                g_semit[c][k] += rate * dt;
                while (g_semit[c][k] >= 1.0f) {
                    float p[3], v[3], sd = (k == 0 || k == 3) ? 1.0f : -1.0f;
                    g_semit[c][k] -= 1.0f;
                    for (j = 0; j < 3; j++) {
                        p[j] = gp[j] - fw[j] * 0.3f + up[j] * 0.2f + (frand() - 0.5f) * 0.25f;
                        v[j] = vel[j] * (loose ? 0.35f : 0.2f) + lf[j] * sd * (0.3f + 0.8f * frand())
                             + (frand() - 0.5f) * 1.2f;
                    }
                    v[2] += 0.4f + 0.6f * frand();
                    if (!loose) emit(p, v, 3.0f + 1.5f * frand(), 0.55f, 3.2f + 1.2f * frand(), 1.2f, 0.9f, 0.9f, 0.91f, -0.12f, 1.2f);
                    else if (w == 3) emit(p, v, 1.8f + 1.0f * frand(), 0.55f, 2.8f + 1.0f * frand(), 1.1f, 0.93f, 0.95f, 0.98f, 0.4f, 1.4f);
                    else emit(p, v, 2.6f + 1.6f * frand(), 0.55f, 3.4f + 1.4f * frand(), 0.8f, 0.74f, 0.64f, 0.5f, 0.08f, 1.2f);
                }
            }
        }
    }
}

/* The particles for this frame, nearest first: the game's (read from its
 * pool) and the port's spray. */
typedef struct { float p[256][4], c[256][4], s[256][4], misc[4], amb[4]; } pfu;
static u32 g_pseen[512]; static u8 g_pfade[512]; static u32 g_pframe;
static int cand_cmp(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;
    return x < y ? -1 : x > y;
}
static int fx_particles(pfu *out, const float *eye, const float *sun, const float *vp, const float *vpt, const float *m3, int flip, int w)
{
    typedef struct { float d, p[4], c[4]; } cand;
    static cand cs[2000];
    int n = 0, i, l;
    static const u32 HEADS[3] = { 0x10AC0C40u, 0x10AC0C3Cu, 0x10AC0C44u };
    g_pframe++;
    for (l = 0; l < 3; l++) {
        u32 idx = W_LD(u16, HEADS[l], 0), guard = 0;
        while (idx && idx < 512 && guard++ < 512 && n < 2000) {
            u32 r = 0x10AC0C48u + idx * 0x20u;
            float p[3] = { W_LD(f32, r, 0), W_LD(f32, r, 4), W_LD(f32, r, 8) };
            /* smoke and dust are the port's own (fx_sim), from the tyres' slip
             * and the surface; the game's puffs are not used */
            if (l < 2) {
                idx = W_LD(u16, r, 0x1C);
                continue;
            }
            /* the game fades each particle out through its byte at +0x1E
             * (255 new, 0 gone): that is its opacity, and its spread */
            float a = (float)W_LD(u8, r, 0x1E) / 255.0f, age = 1.0f - a;
            {
                cand *q = &cs[n++];
                float dx = p[0] - eye[0], dy = p[1] - eye[1], dz = p[2] - eye[2];
                q->d = dx * dx + dy * dy + dz * dz;
                memcpy(q->p, p, 12);
                q->p[3] = l == 2 ? 0.35f + 1.4f * age : (l == 0 ? 0.45f : 0.5f) + (l == 0 ? 2.4f : 2.8f) * sqrtf(age);
                if (l == 1) { q->c[0] = 0.78f; q->c[1] = 0.68f; q->c[2] = 0.55f; }             /* dust */
                else if (l == 2 && w == 3) { q->c[0] = 0.92f; q->c[1] = 0.94f; q->c[2] = 0.97f; } /* snow */
                else if (l == 2) { q->c[0] = 0.8f; q->c[1] = 0.83f; q->c[2] = 0.87f; }           /* water */
                else { q->c[0] = 0.9f; q->c[1] = 0.9f; q->c[2] = 0.91f; }                         /* smoke */
                q->c[3] = (l == 2 ? 1.4f : l == 0 ? 0.8f : 0.9f) * a;
            }
            idx = W_LD(u16, r, 0x1C);
        }
    }
    if (getenv("BR_FX_PFXPROBE") && ((int)(g_time * 60) % 30) == 0) {
        for (l = 0; l < 3; l++) {
            u32 idx = W_LD(u16, HEADS[l], 0); int cnt = 0;
            fprintf(stderr, "pfx list %d:", l);
            while (idx && idx < 512 && cnt < 200) {
                u32 r = 0x10AC0C48u + idx * 0x20u;
                if (cnt < 3) fprintf(stderr, " [%u p %.1f %.1f %.2f v %.1f %.1f %.1f age %.3f e %u f %u]", idx, W_LD(f32, r, 0), W_LD(f32, r, 4), W_LD(f32, r, 8),
                                     W_LD(f32, r, 12), W_LD(f32, r, 16), W_LD(f32, r, 20), W_LD(f32, r, 0x18), W_LD(u8, r, 0x1E), W_LD(u8, r, 0x1F));
                cnt++; idx = W_LD(u16, r, 0x1C); }
            fprintf(stderr, " n=%d\n", cnt); }
        fprintf(stderr, "pfx eye %.1f %.1f %.1f own %d\n", eye[0], eye[1], eye[2], g_nspr);
        for (i = 0; i < g_nspr && i < 6; i++)
            fprintf(stderr, "  own %d: age %.2f/%.2f r %.2f->%.2f dens %.2f col %.2f grav %.2f\n", i, g_spr[i].age, g_spr[i].life,
                    g_spr[i].r0, g_spr[i].r1, g_spr[i].dens, g_spr[i].col[0], g_spr[i].grav);
    }
    { static int src = -2; if (src == -2) src = getenv("BR_FX_PFXSRC") ? atoi(getenv("BR_FX_PFXSRC")) : 3;   /* debug: 1 the game's, 2 the port's */
      if (!(src & 1)) n = 0;
      if (!(src & 2)) goto own_done; }
    for (i = 0; i < g_nspr && n < 2000; i++) {
        const sprt *s = &g_spr[i];
        float f = s->age / s->life;
        cand *q = &cs[n++];
        float dx = s->p[0] - eye[0], dy = s->p[1] - eye[1], dz = s->p[2] - eye[2];
        q->d = dx * dx + dy * dy + dz * dz;
        memcpy(q->p, s->p, 12);
        q->p[3] = s->r0 + (s->r1 - s->r0) * sqrtf(f);
        memcpy(q->c, s->col, 12);
        q->c[3] = s->dens * (1.0f - f) * (1.0f - f);
    }
own_done:
    /* nearest 256, nearest first */
    {
        int cnt = 0, j;
        for (i = 0; i < n; i++) if (cs[i].d < 140.0f * 140.0f && cs[i].c[3] > 0.01f) cs[cnt++] = cs[i];
        (void)j;
        qsort(cs, cnt, sizeof *cs, cand_cmp);
        if (cnt > 256) cnt = 256;
        for (i = 0; i < cnt; i++) { memcpy(out->p[i], cs[i].p, 16); memcpy(out->c[i], cs[i].c, 16); }
        /* for each puff, the density of the others on the way to the sun and
         * to the open sky: the plume's shadow on itself, once per frame
         * instead of per sample */
        for (i = 0; i < cnt; i++) {
            float od[2] = { 0, 0 }; int a;
            for (a = 0; a < 2; a++) {
                const float *dir = a == 0 ? sun : (const float[3]){ 0, 0, 1 };
                for (j = 0; j < cnt; j++) {
                    float ox, oy, oz, b, dd, r;
                    if (j == i) continue;
                    ox = cs[j].p[0] - cs[i].p[0]; oy = cs[j].p[1] - cs[i].p[1]; oz = cs[j].p[2] - cs[i].p[2];
                    b = ox * dir[0] + oy * dir[1] + oz * dir[2];
                    if (b <= 0) continue;
                    r = cs[j].p[3]; dd = ox * ox + oy * oy + oz * oz - b * b;
                    if (dd >= r * r) continue;
                    od[a] += cs[j].c[3] * sqrtf(r * r - dd) * 0.53f;   /* mean of the kernel along a chord */
                }
            }
            out->s[i][0] = od[0]; out->s[i][1] = od[1];
        }
        /* their rectangle on screen, from the corners of each one's box */
        {
            float lo[2] = { 1, 1 }, hi[2] = { 0, 0 };
            int all = 0, cx;
            for (i = 0; i < cnt && !all; i++)
                for (cx = 0; cx < 8; cx++) {
                    float x = cs[i].p[0] + ((cx & 1) ? 1 : -1) * cs[i].p[3], y = cs[i].p[1] + ((cx & 2) ? 1 : -1) * cs[i].p[3];
                    float z = cs[i].p[2] + ((cx & 4) ? 1 : -1) * cs[i].p[3], c[4], uv[2];
                    int k;
                    for (k = 0; k < 4; k++) c[k] = vp[k] * x + vp[4 + k] * y + vp[8 + k] * z + vp[12 + k];
                    if (c[3] < 0.05f) { all = 1; break; }
                    {   /* the game's screen, then the screen map, to uv */
                        float sx = vpt[0] * c[0] / c[3] + vpt[1], sy = vpt[2] * c[1] / c[3] + vpt[3];
                        float yd = flip ? 480.0f - sy : sy;
                        uv[0] = (m3[0] * sx + m3[1]) * 0.5f + 0.5f;
                        uv[1] = 0.5f - 0.5f * (m3[2] * yd + m3[3]);
                    }
                    for (k = 0; k < 2; k++) { if (uv[k] < lo[k]) lo[k] = uv[k]; if (uv[k] > hi[k]) hi[k] = uv[k]; }
                }
            if (all) { lo[0] = lo[1] = 0; hi[0] = hi[1] = 1; }
            out->misc[1] = lo[0]; out->misc[2] = lo[1]; out->misc[3] = hi[0]; out->amb[3] = hi[1];
        }
        return cnt;
    }
}

/* the tyre-track ribbons near the camera */
static id<MTLBuffer> fx_tracks(id<MTLDevice> dev, const float *eye, int *nv)
{
    static float *v; static int cap;
    int n = 0, i, s;
    for (i = 0; i < FX_CARS * 4; i++) {
        trkw *t = &g_trk[i];
        for (s = 1; s < t->n; s++) {
            int a = (t->head - t->n + s - 1 + 2 * TRK_N) % TRK_N, b = (a + 1) % TRK_N;
            float ka, kb, dx, dy, len, ax, ay, fa, fb;
            if (t->k[a] < 0 || t->k[b] < 0) continue;
            dx = t->p[b][0] - eye[0]; dy = t->p[b][1] - eye[1];
            if (dx * dx + dy * dy > 110.0f * 110.0f) continue;
            fa = 1.0f - (g_time - t->t[a]) / 120.0f; fb = 1.0f - (g_time - t->t[b]) / 120.0f;
            if (fa <= 0 || fb <= 0) continue;
            ka = t->k[a] * fa; kb = t->k[b] * fb;
            dx = t->p[b][0] - t->p[a][0]; dy = t->p[b][1] - t->p[a][1]; len = sqrtf(dx * dx + dy * dy);
            if (len < 1e-3f) continue;
            ax = -dy / len * 0.11f; ay = dx / len * 0.11f;
            if (n + 6 > cap) { cap = cap ? cap * 2 : 6144; v = realloc(v, (size_t)cap * 8 * sizeof(float)); }
            {
                float q[4][8] = {
                    { t->p[a][0] - ax, t->p[a][1] - ay, t->p[a][2], ka, -1, 0, 0, 0 },
                    { t->p[a][0] + ax, t->p[a][1] + ay, t->p[a][2], ka,  1, 0, 0, 0 },
                    { t->p[b][0] - ax, t->p[b][1] - ay, t->p[b][2], kb, -1, 0, 0, 0 },
                    { t->p[b][0] + ax, t->p[b][1] + ay, t->p[b][2], kb,  1, 0, 0, 0 } };
                memcpy(v + (n + 0) * 8, q[0], 32); memcpy(v + (n + 1) * 8, q[1], 32); memcpy(v + (n + 2) * 8, q[2], 32);
                memcpy(v + (n + 3) * 8, q[1], 32); memcpy(v + (n + 4) * 8, q[3], 32); memcpy(v + (n + 5) * 8, q[2], 32);
                n += 6;
            }
        }
    }
    *nv = n;
    if (!n) return nil;
    return [dev newBufferWithBytes:v length:(NSUInteger)n * 8 * sizeof(float) options:MTLResourceStorageModeShared];
}

/* Relight the finished frame.  Returns the picture to present, or nil when
 * this frame had no main-camera 3D (menus). */
id<MTLTexture> hfx_run(id<MTLDevice> dev, id<MTLCommandBuffer> cb, id<MTLTexture> col, id<MTLTexture> gn,
                       id<MTLTexture> gp, id<MTLTexture> ga, id<MTLTexture> gm, int w, int h, int origin_ll, const float *fogc)
{
    fxu u;
    hlu hl;
    double P[16], IP[16], v[4], e[4], f[4], sun[3], cf[3];
    int i, aa = 0;
    id<MTLRenderCommandEncoder> enc;
    if (!g_havecam || getenv("BR_FX_SKIP")) { g_havecam = 0; g_nrec = 0; g_ncars = 0; [g_keep removeAllObjects]; g_prof_n = 0; return nil; }
    setup_once(dev);
    size(w, h);
    memset(&u, 0, sizeof u);
    g_frame++;
    for (i = 0; i < 16; i++) P[i] = g_P[i];
    inv4(P, IP);
    memcpy(u.vp, g_P, sizeof u.vp);
    for (i = 0; i < 16; i++) u.ivp[i] = (float)IP[i];
    /* the eye: the clip-space point at infinity along z */
    v[0] = 0; v[1] = 0; v[2] = 1; v[3] = 0; rowmul(v, IP, e);
    for (i = 0; i < 3; i++) u.eye[i] = (float)(e[i] / e[3]);
    /* the forward direction through the screen centre */
    v[0] = 0; v[1] = 0; v[2] = 0.5; v[3] = 1; rowmul(v, IP, f);
    for (i = 0; i < 3; i++) cf[i] = f[i] / f[3] - u.eye[i];
    norm3(cf);
    memcpy(u.vpt, g_vpt, sizeof u.vpt);
    u.scr[0] = (float)w; u.scr[1] = (float)h;
    /* pixels per metre at 1 m: the projection's x scale times half the width */
    u.scr[3] = (float)(sqrt(P[0] * P[0] + P[4] * P[4] + P[8] * P[8]) * w * 0.5);
    u.tm[3] = getenv("BR_FX_BUMP") ? (float)atof(getenv("BR_FX_BUMP")) : 0.18f;
    memcpy(u.pvp, g_pvp_ok ? g_pvp : g_P, sizeof u.pvp);
    u.jit[2] = getenv("BR_FX_SHUTTER") ? (float)atof(getenv("BR_FX_SHUTTER")) : 0.5f;
    /* a camera cut (a new view, a replay angle): no history, no blur */
    {
        static float le[3], lf[3]; static int have;
        double d2 = 0, dotf = 0;
        v[0] = 0; v[1] = 0; v[2] = 0.5; v[3] = 1; rowmul(v, IP, f);
        for (i = 0; i < 3; i++) { double ff = f[i] / f[3] - u.eye[i]; cf[i] = ff; }
        norm3(cf);
        for (i = 0; i < 3; i++) { d2 += (u.eye[i] - le[i]) * (u.eye[i] - le[i]); dotf += cf[i] * lf[i]; }
        if (have && (d2 > 4.0 * 4.0 || dotf < 0.94)) { g_hist_ok = 0; g_hist_ok_mfx = 0; }
        for (i = 0; i < 3; i++) { le[i] = u.eye[i]; lf[i] = (float)cf[i]; }
        have = 1;
    }
    u.jit[3] = (g_pvp_ok && g_hist_ok && taa_on()) ? 1.0f : 0.0f;
    u.p3[0] = (float)origin_ll;
    u.p2[2] = 6.0f;
    u.p2[3] = (float)(g_frame % 64);
    hglide_map(0, u.m3);
    /* effects run on the game's clock (the time of the frame being shown),
     * however many frames a second are drawn: the step since the last
     * frame, held under a tenth of a second across a stall or a pause */
    {
        double ms = hframe_game_ms();
        g_dt = g_last_ms > 0 && ms >= g_last_ms ? fmin((ms - g_last_ms) / 1000.0, 0.1) : 0.0;
        g_last_ms = ms;
        g_clock += g_dt;
    }
    u.tm[0] = (float)g_clock;
    u.tm[1] = 0.75f;                           /* asphalt neutralising */
    materials_once(dev);                       /* waits for a prefetch still loading */
    if (g_mat_state > 0) { u.mat[0] = 1; memcpy(u.matmean, g_matmean, sizeof u.matmean); }
    u.mat[1] = hglide_bake_ref();
    roadmap(dev);
    if (t_rmap && !getenv("BR_FX_NORMAP")) memcpy(u.rmap, g_rmap, sizeof u.rmap);
    canopymap(dev);
    if (t_cnp) memcpy(u.cmap, g_cmap, sizeof u.cmap);
    if (g_ncars > 0) hmarks_frame();
    {   /* the horizon band: 90 degrees a copy, its bottom row 11 degrees under
           the horizon, haze by weather (sunny, fog, storm, snow, night) */
        static const float HAZE[5] = { 0.22f, 0.85f, 0.6f, 0.5f, 0.45f };
        int wx = (int)H32(0x104B15E8u);
        g_bandt = hsky_band(dev);
        {   id<MTLTexture> fh = nil, sv = nil, gi = nil;
            memset(u.ter, 0, sizeof u.ter);
            if (getenv("BR_FX_NOTERLIGHT") || !hter_light(u.ter, &sv, &gi, &fh)) { sv = gi = nil; memset(u.ter, 0, sizeof u.ter); }
            g_tsv = sv; g_tgi = gi; }
        /* host_terrain.m's real mountains replace the painted band */
        if (g_bandt && !hter_ready()) {
            u.bnd[0] = -11.0f * 3.14159265f / 180.0f;
            u.bnd[1] = (float)g_bandt.height / (float)g_bandt.width * (3.14159265f / 2.0f);
            u.bnd[2] = 4.0f;
            u.bnd[3] = HAZE[wx >= 0 && wx < 5 ? wx : 0];
        }
    }           /* a race is being drawn: its cars' tyres mark the ground */
    {
        id<MTLTexture> ma = nil, mp = nil;
        if (hmarks_bind(dev, &ma, &mp, u.mk)) { t_mka = ma; t_mkp = mp; } else { t_mka = t_mkp = nil; u.mk[2] = 0; }
    }
    weather(&u, fogc);
    /* the Remastered sky for the track and weather (tm.z: its gain, 0 = none) */
    { const char *ov = getenv("BR_FX_WEATHER");
      g_skyt = hsky_tex(dev, ov ? atoi(ov) : (int)H32(0x104B15E8u));
      u.tm[2] = g_skyt ? 1.0f : 0.0f; }
    /* the sun: low in the afternoon sky (19 degrees up), from the side of
     * the game's own light direction (1,1,1), so trees and walls lay long
     * shadows across the road (host_car.m's rig: the same) */
    { const char *s = getenv("BR_FX_SUN");
      sun[0] = 3; sun[1] = 1; sun[2] = 1.1;
      if (s) sscanf(s, "%lf,%lf,%lf", &sun[0], &sun[1], &sun[2]); }
    norm3(sun);
    for (i = 0; i < 3; i++) u.sun[i] = (float)sun[i];
    memcpy(g_car_rig[0], u.sun, 16);
    /* lamps: each car's two headlights and two tail lights, where the car's
     * own model puts them (its measured front and rear faces, or the exact
     * places the Remastered car reports), carried by its car -> world matrix
     * (row 0 forward, row 1 left, row 2 up) */
    memset(&hl, 0, sizeof hl);
    /* braking, from the car's own controls as the physics applies them
     * (br_carstep.c): the brake torque at car+0xE6C (negative when held: the
     * handbrake, the grid hold), or drive torque at car+0xE68 against the
     * direction of travel (the brake pedal: the down key while moving
     * forward); the same for the player and the AI */
    {
        int c;
        for (c = 0; c < g_ncars; c++) {
            u32 car = g_carptr[c];
            const float *m = g_cars[c];
            float fl = sqrtf(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
            float vf = (W_LD(f32, car, 0x1024) * m[0] + W_LD(f32, car, 0x1028) * m[1] + W_LD(f32, car, 0x102C) * m[2]) / (fl > 0 ? fl : 1);
            float thr = W_LD(f32, car, 0xE68), brk = W_LD(f32, car, 0xE6C);
            g_brake[c] = (brk < 0.0f || (fabsf(vf) > 0.3f && thr * vf < 0.0f)) ? 1.0f : 0.0f;
            if (getenv("BR_FX_BRAKEPROBE"))
                fprintf(stderr, "brake: car %d v %.1f thr %.1f brk %.1f -> %.0f\n", c, vf, thr, brk, g_brake[c]);
        }
    }
    if (!getenv("BR_FX_NOLIGHTS")) {
        int c, k, n = 0, ng = 0;
        for (c = 0; c < g_ncars && n + 4 <= 64 && ng + 4 <= 64; c++) {
            const float *m = g_cars[c];
            double f[3] = { m[0], m[1], m[2] }, lf[3] = { m[4], m[5], m[6] }, up[3] = { m[8], m[9], m[10] }, dv[3];
            float L[4][3];
            const carbox *b = NULL;
            int j, have = hcar_lamps(g_carptr[c], L);
            if (!have && (b = car_box(g_carmodel[c])) && b->ok) {
                float hw = (b->hi[1] - b->lo[1]) * 0.5f * 0.72f, yc = (b->hi[1] + b->lo[1]) * 0.5f;
                float zh = b->lo[2] + (b->hi[2] - b->lo[2]) * 0.36f, zt = b->lo[2] + (b->hi[2] - b->lo[2]) * 0.48f;
                float L0[4][3] = { { b->hi[0] - 0.08f, yc + hw, zh }, { b->hi[0] - 0.08f, yc - hw, zh },
                                   { b->lo[0] + 0.05f, yc + hw, zt }, { b->lo[0] + 0.05f, yc - hw, zt } };
                memcpy(L, L0, sizeof L);
                have = 1;
            }
            if (!have) continue;
            norm3(f); norm3(lf); norm3(up);
            for (k = 0; k < 3; k++) dv[k] = f[k];
            dv[2] = 0; norm3(dv);
            for (j = 0; j < 4; j++) {
                float w[3];
                /* heads burn in the dark; tails glow dimly in the dark and
                 * full when braking, in any light */
                float lev = j < 2 ? g_hl_int : fmaxf(g_hl_int > 0 ? 0.22f : 0.0f, g_brake[c]);
                if (lev <= 0) continue;
                for (k = 0; k < 3; k++) w[k] = (float)(m[12 + k] + f[k] * L[j][0] + lf[k] * L[j][1] + up[k] * L[j][2]);
                memcpy(hl.g[ng], w, 12); hl.g[ng][3] = j < 2 ? 0.0f : 1.0f;
                for (k = 0; k < 3; k++) hl.gd[ng][k] = (float)(j < 2 ? f[k] : -f[k]);
                hl.gd[ng][3] = j < 2 ? 1.0f : lev * 2.2f;
                ng++;
                memcpy(hl.p[n], w, 12); hl.p[n][3] = j < 2 ? 0.0f : 1.0f;
                for (k = 0; k < 3; k++) hl.d[n][k] = (float)(j < 2 ? dv[k] : -f[k]);
                hl.d[n][3] = lev;
                n++;
            }
        }
        hl.misc[0] = (float)n;
        hl.gmisc[0] = (float)ng; hl.gmisc[1] = 1.0f;
        if (getenv("BR_FX_CARPROBE") && g_frame % 200 == 0)
            fprintf(stderr, "headlights: %d of %d cars, lamp0 %.2f %.2f %.2f dir %.2f %.2f %.2f\n", n / 2, g_ncars,
                    hl.p[0][0], hl.p[0][1], hl.p[0][2], hl.d[0][0], hl.d[0][1], hl.d[0][2]);
    }
    /* halogen-warm, and bright: at night the pool is what the eye adapts to */
    hl.col[0] = 1.6f; hl.col[1] = 1.4f; hl.col[2] = 1.05f;   /* halogen-warm */
    hl.misc[1] = g_hl_beam; hl.misc[2] = g_hl_int; hl.misc[3] = g_hl_air;

    /* the sun's view: an orthographic box ahead of the camera, snapped to
     * whole shadow-map texels so the edges do not crawl as the camera moves */
    {
        double R = getenv("BR_FX_SHADOWR") ? atof(getenv("BR_FX_SHADOWR")) : 110.0, depth = 800.0;
        double c[3], fw[3] = { -sun[0], -sun[1], -sun[2] }, upw[3] = { 0, 0, 1 }, rt[3], up[3], M[16], tx, ty, tz, tex;
        double fl[3] = { cf[0], cf[1], 0 };
        norm3(fl);
        for (i = 0; i < 3; i++) c[i] = u.eye[i] + fl[i] * R * 0.7;
        /* the cascade stays where it is until the point it should centre on
         * has moved a quarter of its radius: the static casters' map
         * (host_env.m's models, host_terrain.m's field) is then reused */
        {
            double d0 = 0;
            for (i = 0; i < 3; i++) d0 += (c[i] - g_shanc[0][i]) * (c[i] - g_shanc[0][i]);
            if (!g_shanc_ok[0] || d0 > R * R * 0.0625 || memcmp(g_shsun, sun, sizeof g_shsun) || g_frame - g_shframe[0] > 600) {
                if (getenv("BR_FX_SHLOG")) fprintf(stderr, "shcache: near re-anchor frame %u ok %d move %.1f/%.1f sun %d (%.6f %.6f %.6f)\n", g_frame, g_shanc_ok[0], sqrt(d0), R * 0.25,
                                                   memcmp(g_shsun, sun, sizeof g_shsun) != 0, sun[0], sun[1], sun[2]);
                for (i = 0; i < 3; i++) g_shanc[0][i] = c[i];
                if (memcmp(g_shsun, sun, sizeof g_shsun)) g_shanc_ok[1] = 0;   /* a new sun: both cascades */
                memcpy(g_shsun, sun, sizeof g_shsun);
                g_shanc_ok[0] = 1; g_shdirty[0] = 1; g_shframe[0] = g_frame;
            }
            for (i = 0; i < 3; i++) c[i] = g_shanc[0][i];
        }
        cross3(fw, upw, rt); norm3(rt); cross3(rt, fw, up); norm3(up);
        tx = rt[0] * c[0] + rt[1] * c[1] + rt[2] * c[2];
        ty = up[0] * c[0] + up[1] * c[1] + up[2] * c[2];
        tz = fw[0] * c[0] + fw[1] * c[1] + fw[2] * c[2];
        tex = 2.0 * R / 2048.0;
        tx = floor(tx / tex) * tex; ty = floor(ty / tex) * tex;
        /* row-vector: clip = [x y z 1] * M; x' = (dot(rt,p) - tx)/R, z' = (dot(fw,p) - tz + depth/2)/depth */
        memset(M, 0, sizeof M);
        for (i = 0; i < 3; i++) { M[i * 4 + 0] = rt[i] / R; M[i * 4 + 1] = up[i] / R; M[i * 4 + 2] = fw[i] / depth; }
        M[12] = -tx / R; M[13] = -ty / R; M[14] = (-tz + depth * 0.5) / depth; M[15] = 1;
        for (i = 0; i < 16; i++) u.svp[i] = (float)M[i];
        u.p3[3] = (float)tex;
        /* the far cascade: the same view, 4x wider */
        {
            double R2 = R * 4.0, c2[3], tex2 = 2.0 * R2 / 2048.0;
            for (i = 0; i < 3; i++) c2[i] = u.eye[i] + fl[i] * R2 * 0.6;
            {
                double d1 = 0;
                for (i = 0; i < 3; i++) d1 += (c2[i] - g_shanc[1][i]) * (c2[i] - g_shanc[1][i]);
                if (!g_shanc_ok[1] || d1 > R2 * R2 * 0.0625 || g_shdirty[0] == 2 || g_frame - g_shframe[1] > 600) {
                    for (i = 0; i < 3; i++) g_shanc[1][i] = c2[i];
                    g_shanc_ok[1] = 1; g_shdirty[1] = 1; g_shframe[1] = g_frame;
                }
                for (i = 0; i < 3; i++) c2[i] = g_shanc[1][i];
            }
            tx = rt[0] * c2[0] + rt[1] * c2[1] + rt[2] * c2[2];
            ty = up[0] * c2[0] + up[1] * c2[1] + up[2] * c2[2];
            tz = fw[0] * c2[0] + fw[1] * c2[1] + fw[2] * c2[2];
            tx = floor(tx / tex2) * tex2; ty = floor(ty / tex2) * tex2;
            memset(M, 0, sizeof M);
            for (i = 0; i < 3; i++) { M[i * 4 + 0] = rt[i] / R2; M[i * 4 + 1] = up[i] / R2; M[i * 4 + 2] = fw[i] / (depth * 2); }
            M[12] = -tx / R2; M[13] = -ty / R2; M[14] = (-tz + depth) / (depth * 2); M[15] = 1;
            for (i = 0; i < 16; i++) u.svp2[i] = (float)M[i];
            u.csm[0] = (float)tex2;
        }
    }

    if (getenv("BR_FX_STAT") && g_frame % 500 == 0) {
        long tris = 0; for (i = 0; i < g_nrec; i++) tris += g_rec[i].n / 3;
        fprintf(stderr, "fx: frame %u shadow batches %d tris %ld eye %.1f %.1f %.1f fwd %.2f %.2f %.2f weather %d chosen %d rain %d storm %d snow %d carlight %d %d %d amb %d %d %d f661c %d\n",
                g_frame, g_nrec, tris, u.eye[0], u.eye[1], u.eye[2], cf[0], cf[1], cf[2], (int)H32(0x104B15E8u),
                (int)H32(0x10226E80u), (int)H32(0x106C6620u), (int)H32(0x106C6624u), (int)H32(0x106ED6B0u),
                W_LD(u8, 0x106C1580u, 0), W_LD(u8, 0x106C335Cu, 0), W_LD(u8, 0x106C0968u, 0),
                W_LD(u8, 0x106B7C80u, 0), W_LD(u8, 0x106C0960u, 0), W_LD(u8, 0x106C65BCu, 0), (int)H32(0x106C661Cu));
    }
    if (getenv("BR_FX_CARPROBE") && g_frame % 200 == 0) {
        u32 vc = H32(0x106E9D88u);
        for (i = 0; i < g_ncars; i++) if (g_carptr[i] == vc) {
            static float lp[3]; float *m = g_cars[i];
            { u32 c = g_carptr[i]; int k; static const u32 WO[4] = { 0x994, 0x57C, 0x370, 0x788 };
              for (k = 0; k < 4; k++)
                  fprintf(stderr, "wheel%d: %.2f %.2f %.2f contact %d surf %d\n", k, W_LD(f32, c, 0x70 + 0x40 * k), W_LD(f32, c, 0x74 + 0x40 * k),
                          W_LD(f32, c, 0x78 + 0x40 * k), W_LD(s32, c, WO[k] + 0x1B4), W_LD(s8, c, WO[k] + 0x1A0));
              fprintf(stderr, "speed %.1f h %.2f vel %.2f %.2f %.2f\n", W_LD(f32, c, 0x1030), W_LD(f32, c, 0x2994),
                      W_LD(f32, c, 0x1024), W_LD(f32, c, 0x1028), W_LD(f32, c, 0x102C)); }
            fprintf(stderr, "car: r0 %.2f %.2f %.2f r1 %.2f %.2f %.2f r2 %.2f %.2f %.2f pos %.1f %.1f %.1f d %.2f %.2f %.2f camfwd %.2f %.2f %.2f\n",
                    m[0], m[1], m[2], m[4], m[5], m[6], m[8], m[9], m[10], m[12], m[13], m[14],
                    m[12] - lp[0], m[13] - lp[1], m[14] - lp[2], cf[0], cf[1], cf[2]);
            lp[0] = m[12]; lp[1] = m[13]; lp[2] = m[14];
        }
    }
    /* 1. shadow maps: the static casters into the cascade's cache when it
     * moved (or the sun did), then each frame the cache copied and the
     * game's own triangles (the cars among them) drawn over it */
    if (u.sun[3] > 0) for (int cas = 0; cas < 2; cas++) {
        float rng = (float)((getenv("BR_FX_SHADOWR") ? atof(getenv("BR_FX_SHADOWR")) : 110.0) * (cas ? 4.0 : 1.0) * 1.6);
        if (g_shdirty[cas] || getenv("BR_FX_NOSHCACHE")) {
            MTLRenderPassDescriptor *rs = [MTLRenderPassDescriptor renderPassDescriptor];
            float anc[3] = { (float)g_shanc[cas][0], (float)g_shanc[cas][1], (float)g_shanc[cas][2] };
            rs.depthAttachment.texture = cas ? t_sm2 : t_sm;
            prof_attach(rs, cas ? "shadow far static" : "shadow near static");
            rs.depthAttachment.loadAction = MTLLoadActionClear;
            rs.depthAttachment.clearDepth = 1.0;
            rs.depthAttachment.storeAction = MTLStoreActionStore;
            enc = [cb renderCommandEncoderWithDescriptor:rs];
            [enc setDepthStencilState:ds_sh];
            [enc setCullMode:MTLCullModeNone];
            [enc setDepthBias:2.0f slopeScale:3.0f clamp:0.02f];
            [enc setRenderPipelineState:p_sh];
            for (i = 0; i < g_nrec; i++) {
                shrec *r = &g_rec[i];
                if (r->dyn) continue;
                memcpy(r->s.svp, cas ? u.svp2 : u.svp, sizeof u.svp);
                [enc setVertexBuffer:r->buf offset:r->off atIndex:0];
                [enc setVertexBytes:&r->s length:sizeof r->s atIndex:1];
                [enc setFragmentBytes:&r->s length:sizeof r->s atIndex:0];
                if (r->tex) [enc setFragmentTexture:r->tex atIndex:0];
                if (r->smp) [enc setFragmentSamplerState:r->smp atIndex:0];
                [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:(NSUInteger)r->n];
            }
            if (!getenv("BR_SH_NOENV")) henv_shadow(enc, cas ? u.svp2 : u.svp, anc, rng * 1.25f);
            hter_shadow(enc, cas ? u.svp2 : u.svp, anc, rng * 1.25f);
            [enc endEncoding];
            g_shdirty[cas] = 0;
        }
        if (cas == 0) {   /* the cars, every frame, into their own map */
            MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
            rp.depthAttachment.texture = t_smD;
            prof_attach(rp, "shadow cars");
            rp.depthAttachment.loadAction = MTLLoadActionClear;
            rp.depthAttachment.clearDepth = 1.0;
            rp.depthAttachment.storeAction = MTLStoreActionStore;
            enc = [cb renderCommandEncoderWithDescriptor:rp];
            [enc setRenderPipelineState:p_sh];
            [enc setDepthStencilState:ds_sh];
            [enc setCullMode:MTLCullModeNone];
            [enc setDepthBias:2.0f slopeScale:3.0f clamp:0.02f];
            for (i = 0; i < g_nrec; i++) {
                shrec *r = &g_rec[i];
                if (!r->dyn) continue;
                memcpy(r->s.svp, u.svp, sizeof u.svp);
                [enc setVertexBuffer:r->buf offset:r->off atIndex:0];
                [enc setVertexBytes:&r->s length:sizeof r->s atIndex:1];
                [enc setFragmentBytes:&r->s length:sizeof r->s atIndex:0];
                if (r->tex) [enc setFragmentTexture:r->tex atIndex:0];
                if (r->smp) [enc setFragmentSamplerState:r->smp atIndex:0];
                [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:(NSUInteger)r->n];
            }
            [enc endEncoding];
        }
    }
    /* 2. occlusion + bounce, blurred */
    /* 1b. smoothed normals: every pass after this reads them */
    /* (at half resolution: a smoothed normal has no fine detail to lose;
     * the lighting pass takes it only where it agrees with the face's own) */
    /* host_terrain.m's field: its look, once per pixel, before anything reads it */
    hter_resolve(cb, gn, ga, gp, gm);
    id<MTLTexture> gface = gn;
    quad(pass(cb, t_ring, 0), p_ring, &u, @[ga, gp, gn]);
    /* the G-buffer's normals are the meshes' smooth ones (host_glide.m); the
     * old screen-space smoothing stays behind BR_FX_SSNRM=1 */
    if (getenv("BR_FX_SSNRM")) { quad(pass(cb, t_nrm, 0), p_nrm, &u, @[gn, gp]); gn = t_nrm; }
    quad(pass(cb, t_ao, 0), p_ao, &u, @[gn, gp, col]);
    if (!taa_on()) {
        float d1[2] = { 1, 0 }, d2[2] = { 0, 1 };
        enc = pass(cb, t_ao2, 0);
        [enc setFragmentBytes:d1 length:8 atIndex:1];
        quad(enc, p_blur, &u, @[t_ao, gp, gn]);
        enc = pass(cb, t_ao, 0);
        [enc setFragmentBytes:d2 length:8 atIndex:1];
        quad(enc, p_blur, &u, @[t_ao2, gp, gn]);
    }
    /* 3. reflections, blurred like the occlusion; dry roads get none */
    if (u.p0[3] <= 0) [pass(cb, t_ssr, 0) endEncoding];
    else {
    quad(pass(cb, t_ssr, 0), p_ssr, &u, @[gn, gp, col]);
        float d1[2] = { 1, 0 }, d2[2] = { 0, 1 };
        enc = pass(cb, t_ao2, 0);
        [enc setFragmentBytes:d1 length:8 atIndex:1];
        quad(enc, p_blur, &u, @[t_ssr, gp, gn]);
        enc = pass(cb, t_ssr, 0);
        [enc setFragmentBytes:d2 length:8 atIndex:1];
        quad(enc, p_blur, &u, @[t_ao2, gp, gn]);
    }
    /* 3b. this frame's spray and track samples, and the track mask */
    {
        int w = (int)H32(0x104B15E8u), nv = 0;
        id<MTLBuffer> tb;
        if (getenv("BR_FX_WEATHER")) w = atoi(getenv("BR_FX_WEATHER"));
        fx_sim(w, u.p0[3], (float)g_dt);
        tb = fx_tracks(dev, u.eye, &nv);
        {
            MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
            rp.colorAttachments[0].texture = t_trk;
            prof_attach(rp, "tracks");
            rp.colorAttachments[0].loadAction = MTLLoadActionClear;
            rp.colorAttachments[0].clearColor = MTLClearColorMake(0, -10000, 0, 0);
            rp.colorAttachments[0].storeAction = MTLStoreActionStore;
            enc = [cb renderCommandEncoderWithDescriptor:rp];
            if (tb) {
                if (!g_keep) g_keep = [NSMutableArray new];
                [g_keep addObject:tb];
                [enc setRenderPipelineState:p_trk];
                [enc setVertexBuffer:tb offset:0 atIndex:0];
                [enc setVertexBytes:&u length:sizeof u atIndex:1];
                [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:(NSUInteger)nv];
            }
            [enc endEncoding];
        }
    }
    /* each car's frame now and last frame (motion vectors, and the rain on
     * the cars in the lighting pass) */
    id<MTLBuffer> mb;
    {
        mvu *mv = calloc(1, sizeof *mv);
        int c, k, nm = 0;
        for (c = 0; c < g_ncars && nm < 16; c++) {
            int j2;
            for (j2 = 0; j2 < g_nprev && g_prevcar[j2] != g_carptr[c]; j2++) ;
            if (j2 == g_nprev) continue;
            for (k = 0; k < 4; k++) {
                int r;
                for (r = 0; r < 3; r++) { mv->cur[nm][k][r] = g_cars[c][k * 4 + r]; mv->prev[nm][k][r] = g_prevmat[j2][k * 4 + r]; }
                if (k < 3) {       /* unit rows */
                    float l1 = sqrtf(mv->cur[nm][k][0] * mv->cur[nm][k][0] + mv->cur[nm][k][1] * mv->cur[nm][k][1] + mv->cur[nm][k][2] * mv->cur[nm][k][2]);
                    float l2 = sqrtf(mv->prev[nm][k][0] * mv->prev[nm][k][0] + mv->prev[nm][k][1] * mv->prev[nm][k][1] + mv->prev[nm][k][2] * mv->prev[nm][k][2]);
                    for (r = 0; r < 3; r++) { mv->cur[nm][k][r] /= l1 > 0 ? l1 : 1; mv->prev[nm][k][r] /= l2 > 0 ? l2 : 1; }
                }
            }
            {   /* the tyres' contact plane, in the car's up axis */
                u32 car = g_carptr[c]; float lo = 1e9f;
                for (k = 0; k < 4; k++) {
                    float h = 0; int r;
                    for (r = 0; r < 3; r++) h += (W_LD(f32, car, 0x70 + 0x40 * k + 4 * r) - mv->cur[nm][3][r]) * mv->cur[nm][2][r];
                    if (h < lo) lo = h;
                }
                mv->cur[nm][3][3] = lo < 1e8f ? lo - 0.3f : -1.2f;
            }
            nm++;
        }
        mv->misc[0] = (float)nm;
        mb = [dev newBufferWithBytes:mv length:sizeof *mv options:MTLResourceStorageModeShared];
        free(mv);
        if (!g_keep) g_keep = [NSMutableArray new];
        [g_keep addObject:mb];
    }
    /* BR_FX_PICK="fx,fy;...": the world position under those points of the
     * frame (fractions of its width and height), printed every frame */
    { const char *pk = getenv("BR_FX_PICK");
      if (pk && gp) {
          float q[16][2]; int nq = 0; const char *c2 = pk;
          while (nq < 16 && sscanf(c2, "%f,%f", &q[nq][0], &q[nq][1]) == 2) { nq++; c2 = strchr(c2, ';'); if (!c2) break; c2++; }
          if (nq) {
              id<MTLBuffer> pb = [dev newBufferWithLength:16 * 16 options:MTLResourceStorageModeShared];
              id<MTLBlitCommandEncoder> be = [cb blitCommandEncoder];
              int k2, fr = g_frame;
              for (k2 = 0; k2 < nq; k2++)
                  [be copyFromTexture:gp sourceSlice:0 sourceLevel:0
                         sourceOrigin:MTLOriginMake((NSUInteger)(q[k2][0] * (gp.width - 1)), (NSUInteger)(q[k2][1] * (gp.height - 1)), 0)
                           sourceSize:MTLSizeMake(1, 1, 1) toBuffer:pb destinationOffset:(NSUInteger)k2 * 16 destinationBytesPerRow:16 destinationBytesPerImage:16];
              [be endEncoding];
              [cb addCompletedHandler:^(id<MTLCommandBuffer> b) { (void)b;
                  const float *v2 = pb.contents; int k3;
                  for (k3 = 0; k3 < nq; k3++) fprintf(stderr, "pick %d %d: %.2f %.2f %.2f %.2f\n", fr, k3, v2[k3 * 4], v2[k3 * 4 + 1], v2[k3 * 4 + 2], v2[k3 * 4 + 3]); }];
          } } }
    /* 4. lighting */
    enc = pass(cb, t_hdr, 0);
    [enc setFragmentBytes:&hl length:sizeof hl atIndex:1];
    [enc setFragmentBuffer:mb offset:0 atIndex:2];
    quad(enc, p_comp, &u, @[gface, gp, col, t_sm, t_ao, t_ssr, g_skyt ? g_skyt : col, t_trk, t_sm2, gn, g_matA ? g_matA : t_trk, g_matN ? g_matN : t_trk, t_ring, t_rmap ? t_rmap : t_trk, ga, gm, t_cnp ? t_cnp : t_trk, t_mka ? t_mka : t_trk, t_mkp ? t_mkp : t_trk, g_bandt ? g_bandt : t_trk,
                            g_tsv ? g_tsv : t_trk, g_tgi ? g_tgi : t_trk, t_smD]);
    /* 4b. particles and falling rain/snow over the lit scene */
    if (getenv("BR_FX_NOPFX")) [pass(cb, t_pfx, 0) endEncoding];
    else {
        pfu *pu = calloc(1, sizeof *pu);
        id<MTLBuffer> pb;
        pu->misc[0] = (float)fx_particles(pu, u.eye, u.sun, u.vp, u.vpt, u.m3, u.p3[0] > 0.5f, (int)H32(0x104B15E8u));
        for (i = 0; i < 3; i++) pu->amb[i] = g_car_rig[2][i] * 0.8f + g_car_rig[3][i] * 0.4f;
        pb = [dev newBufferWithBytes:pu length:sizeof *pu options:MTLResourceStorageModeShared];
        free(pu);
        if (!g_keep) g_keep = [NSMutableArray new];
        [g_keep addObject:pb];
        {
            enc = pass(cb, t_pfx, 0);
            [enc setFragmentBytes:&hl length:sizeof hl atIndex:1];
            [enc setFragmentBuffer:pb offset:0 atIndex:2];
            if (!t_pnz) t_pnz = mk_plume_noise(dev);
            quad(enc, p_pfx, &u, @[gp, t_pnz]);
            /* with TAA the resolve lays the particles over the scene */
            if (!taa_on()) quad(pass(cb, t_hdr, 1), p_pfxc, &u, @[t_pfx, gp]);
        }
    }
    /* 4c. temporal anti-aliasing, then motion blur */
    {
        {
            if (taa_on()) {
                id<MTLTexture> dst = t_hist[g_hist_i], his = t_hist[g_hist_i ^ 1];
                enc = pass(cb, dst, 0);
                [enc setFragmentBuffer:mb offset:0 atIndex:1];
                quad(enc, p_taa, &u, @[t_hdr, his, gp, t_pfx]);
                g_hist_i ^= 1; g_hist_ok = 1;
                enc = pass(cb, t_hdr, 0);
                [enc setFragmentBuffer:mb offset:0 atIndex:1];
                if (getenv("BR_FX_NOMB")) u.jit[2] = 0;       /* shutter 0: a copy */
                quad(enc, p_mb, &u, @[dst, gp, t_pfx]);
            }
        }
        if (hglide_temporal()) {   /* the moves, for MetalFX */
            enc = pass(cb, t_mv, 0);
            [enc setFragmentBuffer:mb offset:0 atIndex:1];
            quad(enc, p_mvec, &u, @[gp]);
        }
        g_jit_used[0] = g_jit[0]; g_jit_used[1] = g_jit[1];
        /* remember this frame for the next */
        memcpy(g_pvp, g_P, sizeof g_pvp); g_pvp_ok = 1;
        g_nprev = g_ncars;
        { int c; for (c = 0; c < g_ncars; c++) { g_prevcar[c] = g_carptr[c]; memcpy(g_prevmat[c], g_cars[c], sizeof g_prevmat[c]); } }
        next_jitter();
    }
    /* 5. bloom */
    quad(pass(cb, t_bl[0], 0), p_pre, &u, @[t_hdr]);
    for (i = 1; i < 6; i++) quad(pass(cb, t_bl[i], 0), p_down, NULL, @[t_bl[i - 1]]);
    for (i = 4; i >= 0; i--) quad(pass(cb, t_bl[i], 1), p_up, NULL, @[t_bl[i + 1]]);
    /* 6. shafts */
    {   /* sun shafts only while the sun is on or near the screen */
        double sp[4], sv[4] = { u.eye[0] + sun[0] * 8000.0, u.eye[1] + sun[1] * 8000.0, u.eye[2] + sun[2] * 8000.0, 1 };
        rowmul(sv, P, sp);
        if (sp[3] <= 0 || fabs(sp[0] / sp[3]) > 1.6 || fabs(sp[1] / sp[3]) > 1.6) u.p2[0] = 0;
    }
    if (u.p2[0] > 0 || hl.gmisc[0] > 0) {
        enc = pass(cb, t_shaft, 0);
        [enc setFragmentBytes:&hl length:sizeof hl atIndex:1];
        quad(enc, p_shaft, &u, @[gp, col]);
    }
    else [pass(cb, t_shaft, 0) endEncoding];
    /* 7. tone map and grade; with anti-aliasing (BR_FX_AA=0 leaves it out,
     * the debug views skip it) the scene is graded alone, anti-aliased, and
     * the game's 2D laid over it afterwards */
    if ((!getenv("BR_FX_AA") || atoi(getenv("BR_FX_AA"))) && u.p3[1] < 0.5 && !taa_on() && !hglide_temporal()) {
        quad(pass(cb, t_aa, 0), p_scene, &u, @[t_hdr, col, t_bl[0], t_shaft]);
        quad(pass(cb, t_out, 0), p_aa, NULL, @[t_aa, t_hdr]);
        quad(pass(cb, t_aa, 0), p_mix, NULL, @[t_out, t_hdr, col]);
        aa = 1;
    } else
        quad(pass(cb, t_out, 0), p_fin, &u, @[t_hdr, col, t_bl[0], t_shaft]);

    /* keep this frame's shadow inputs alive until the GPU is done */
    {
        NSArray *keep = [g_keep copy];
        int stat = getenv("BR_FX_STAT") != NULL;
        int pb = g_prof_buf, pn = g_prof_n;
        const char **labs = malloc(sizeof(char *) * 64);
        memcpy(labs, g_plab[pb], sizeof(char *) * 64);
        [cb addCompletedHandler:^(id<MTLCommandBuffer> b) {
            static double acc; static int n;
            (void)keep;
            if (pn > 0 && g_psb[pb]) {
                NSData *d = [g_psb[pb] resolveCounterRange:NSMakeRange(0, (NSUInteger)pn * 4)];
                const MTLCounterResultTimestamp *t = d.bytes;
                int k;
                if (!g_pacc_n) { g_pacc_n = pn; for (k = 0; k < pn; k++) g_pacc_lab[k] = labs[k]; }
                for (k = 0; k < pn && k < g_pacc_n; k++) {
                    /* passes overlap on the tile GPU: charge each one the time
                       from the previous pass's end to its own end */
                    uint64_t a = t[k * 4 + 2].timestamp, e = t[k * 4 + 3].timestamp;   /* fragment stage span */
                    uint64_t va = t[k * 4].timestamp, ve = t[k * 4 + 1].timestamp;     /* vertex stage span */
                    if (e > a && e - a < 1000000000ull) g_pacc[k] += (double)(e - a) / 1e6;
                    if (ve > va && ve - va < 1000000000ull) g_pvacc[k] += (double)(ve - va) / 1e6;
                    if (k + 1 < pn) { uint64_t nx = t[(k + 1) * 4].timestamp; if (nx > e && nx - e < 1000000000ull) g_pgap[k] += (double)(nx - e) / 1e6; }
                }
                if (++g_pacc_frames == 120) {
                    (void)0;
                    double tot = 0;
                    for (k = 0; k < g_pacc_n; k++) tot += g_pacc[k];
                    fprintf(stderr, "fx prof (ms/frame over 120 frames, sum %.2f):\n", tot / 120);
                    for (k = 0; k < g_pacc_n; k++) fprintf(stderr, "  %-16s frag %6.2f  vert %6.2f  gap-after %6.2f\n", g_pacc_lab[k], g_pacc[k] / 120, g_pvacc[k] / 120, g_pgap[k] / 120);
                    memset(g_pacc, 0, sizeof g_pacc); memset(g_pvacc, 0, sizeof g_pvacc); memset(g_pgap, 0, sizeof g_pgap); g_pacc_frames = 0; g_pacc_n = 0;
                }
            }
            free(labs);
            if (!stat) return;
            acc += (b.GPUEndTime - b.GPUStartTime) * 1000.0;
            if (++n == 240) { fprintf(stderr, "fx: gpu %.2f ms per frame (last 240, whole frame)\n", acc / n); acc = 0; n = 0; }
        }];
        g_prof_buf ^= 1; g_prof_n = 0;       /* the next frame's passes, the game's scene first */
    }
    [g_keep removeAllObjects];
    g_nrec = 0;
    g_havecam = 0;
    g_ncars = 0;
    (void)rdf;
    return aa ? t_aa : t_out;
}

/* host_glide.m's temporal upscale: this frame's motion and jitter (pixels),
 * and whether the history is usable (0 after a cut) */
id<MTLTexture> hfx_motion(float *jx, float *jy, int *history)
{
    *jx = g_jit_used[0]; *jy = g_jit_used[1];
    *history = g_hist_ok_mfx;
    g_hist_ok_mfx = 1;
    return t_mv;
}
