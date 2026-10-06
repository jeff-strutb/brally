/* host_terrain.m -- the Remastered landscape (port code).
 *
 * While the Remastered renderer is on (host_fx.m, the ~ key), the track's
 * natural ground -- the grass, earth and rock the game draws as a few
 * thousand flat triangles -- is replaced by one heightfield that runs on to
 * the horizon (ports/brally-wasm/tools/remaster_terrain.py builds it per track).
 * The road, walls, buildings, water and signs are still the game's own.
 * Only what is drawn changes: the collision mesh is a separate array the game
 * never draws, and the heightfield passes through the game's ground wherever
 * a car can be (to the centimetre), stays under everything else it draws,
 * and falls away past the edges a car can go off.
 *
 *   hiding      the natural ground's triangle commands are blanked in
 *               host_env.m's Remastered copies of the instance lists (the
 *               "hide" lines of placements/<track>.terrain, one triangle of a
 *               two-triangle command where only one is ground).
 *   drawing     once per view, at the first instance list the frame jumps
 *               into (native/env.m's seam): a quadtree of 32x32-quad patches
 *               over the whole field, finer near the eye, each vertex morphing
 *               toward its parent's grid before the switch so nothing pops;
 *               heights read in the vertex shader from the 1 m grid over the
 *               track and the 8 m grid out to the horizon.  Projected through
 *               the view the list machine is drawing (main view or mirror),
 *               depth as the Voodoo W-buffer word like every other surface.
 *   shading     photoscanned ground (Poly Haven, CC0): two grasses, conifer
 *               forest ground, dirt, mossy rock, scree, plain rock, snow --
 *               weighted by slope, height, the water courses the erosion cut
 *               and the forest's canopy, height-blended so stones stand out
 *               of the grass, sampled at two scales (and from the side on
 *               steep ground) so nothing repeats.  In the main view it goes to
 *               host_fx.m's G-buffer as lit ground (class 1, material
 *               MAT_TERRAIN) and is relit with everything else: sun, shadows,
 *               sky light, bounce, wet, headlights, fog, aerial perspective.
 *               In the mirror it is lit here.  It casts into the sun's shadow
 *               map (hter_shadow, from the fx shadow pass).
 *
 * BR_TERRAIN=0 turns it off.  Files: ports/common/models/terrain/<track>.ter,
 * placements/<track>.terrain, env/polyhaven/textures/<set>/ (or the app's
 * Resources/env).
 */
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <ImageIO/ImageIO.h>
#include "host.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

int hfx_on(void);
int hfx_car_rig(float *sun, float *sunc, float *skyc, float *grnd);
unsigned hglide_swaps(void);
id<MTLRenderCommandEncoder> hglide_native_pass(id<MTLDevice> *dev, MTLScissorRect *sc, int *origin_ll,
                                               int *fogmode, float *fogcolor, float *fogtab, int *rw, int *rh);
void hfx_jitter(float *jx, float *jy, int rw, int rh);
void hglide_map(int view, float T[4]);
MTLScissorRect hglide_scissor(int view);
int hrender_view(void);
float hglide_bake_ref(void);
double hframe_game_ms(void);
void hload_for(int n, void (^f)(int i));
int henv_canopy(const unsigned char **px, int *w, int *h, float *x0, float *y0, float *cell, int *gen);
void henv_add_hide(int inst, const int *o, int n);
void hfx_prof_attach(MTLRenderPassDescriptor *rp, const char *label);
void henv_models_find(int n, const char *const *asset, const char *const *var, int *out);
float henv_model_h(int m);
int henv_model_card(int m);
int henv_model_tris(int m, int lod);
typedef struct { int model, lod; long first; int n; } henv_set;
void henv_draw_sets(const henv_set *sets, int nsets, id<MTLBuffer> buf);
void henv_shadow_sets(id<MTLRenderCommandEncoder> e, const float *svp, const henv_set *sets, int nsets, id<MTLBuffer> buf);

#define PROJ   0x105CCD00u
#define PATCH  32                          /* quads along a patch's side */
#define LEAF   4.0f                        /* the finest patch, metres (12.5 cm quads) */
#define BLVL   3                           /* the level whose patches are 32 m: height bounds from here up */
#define MAXNODE 16384

#define STR(...) #__VA_ARGS__
static const char *TERSRC = "#include <metal_stdlib>\n" STR(
using namespace metal;
struct TU {
  float4x4 P;
  float4 vpt, eye, map, jit, misc;   /* misc: origin_ll, main, fog on, bake ref */
  float4 nd;   /* near grid: x0, y0, cell, - */
  float4 ns;   /* near grid: w, h */
  float4 fd;   /* far grid: x0, y0, cell */
  float4 fs;   /* far grid: w, h */
  float4 sun, sunc, skyc, grnd, fogc;
  float4 cmap;  /* canopy grid: x0, y0, 1/w, 1/h (metres); z>0 present */
  float4 wx;    /* weather: snow on the ground, wet, time, debug */
  float4 lod;   /* morph: k (range of level 0), start fraction, leaf size */
  float4 mm[8]; /* each material's mean colour (linear) */
  float fogtab[64];
};
struct NV { float4 o; };              /* a patch: x0, y0, size, level */
struct VO { float4 pos [[position]]; float3 wp; float oow; float morph; };

/* ---- the field's height ---- */
float4 crw(float t) {                 /* Catmull-Rom: passes through every sample */
  return float4(t * ((2.0 - t) * t - 1.0), t * t * (3.0 * t - 5.0) + 2.0, t * ((4.0 - 3.0 * t) * t + 1.0), (t - 1.0) * t * t) * 0.5; }
float hbil(texture2d<float> t, float2 g, float2 sz) {
  g = clamp(g, float2(0), sz - 1.001);
  float2 i = floor(g), f = g - i;
  uint2 a = uint2(i);
  float h00 = t.read(a).r, h10 = t.read(a + uint2(1, 0)).r, h01 = t.read(a + uint2(0, 1)).r, h11 = t.read(a + uint2(1, 1)).r;
  return mix(mix(h00, h10, f.x), mix(h01, h11, f.x), f.y); }
float hcub(texture2d<float> t, float2 g, float2 sz) {
  g = clamp(g, float2(1), sz - 2.001);
  float2 i = floor(g), f = g - i;
  float4 wx = crw(f.x), wy = crw(f.y);
  int2 a = int2(i) - 1;
  float s = 0;
  for (int y = 0; y < 4; y++) {
    float r = 0;
    for (int x = 0; x < 4; x++) r += wx[x] * t.read(uint2(a + int2(x, y))).r;
    s += wy[y] * r; }
  return s; }
bool in_near(constant TU &u, float2 p) {
  float2 g = (p - u.nd.xy) / u.nd.z;
  return all(g >= 1.0) && all(g <= u.ns.xy - 2.001); }
/* near the road, the walls and the lakes the field must stay under what the
   game draws: there the samples are joined straight (a curve between them
   could rise over its bound by a few centimetres) */
float height(constant TU &u, texture2d<float> nt, texture2d<float> ft, texture2d<uint> cls, float2 p) {
  if (in_near(u, p)) {
    float2 g = (p - u.nd.xy) / u.nd.z;
    uint2 a = uint2(floor(g));
    uint c00 = cls.read(a).r, c10 = cls.read(a + uint2(1, 0)).r, c01 = cls.read(a + uint2(0, 1)).r, c11 = cls.read(a + uint2(1, 1)).r;
    bool under = (c00 >= 2u && c00 <= 4u) || (c10 >= 2u && c10 <= 4u) || (c01 >= 2u && c01 <= 4u) || (c11 >= 2u && c11 <= 4u);
    return under ? hbil(nt, g, u.ns.xy) : hcub(nt, g, u.ns.xy); }
  return hcub(ft, (p - u.fd.xy) / u.fd.z, u.fs.xy); }
uint cls_at(constant TU &u, texture2d<uint> cls, float2 p) {
  if (!in_near(u, p)) return 0u;
  float2 g = (p - u.nd.xy) / u.nd.z;
  return cls.read(uint2(g + 0.5)).r; }
/* the slope, from the precomputed gradients (smoothed over the track's flat
   1999 triangles), filtered */
float3 gnormal(constant TU &u, texture2d<float> ng, texture2d<float> fg, float2 p) {
  constexpr sampler ls(filter::linear, address::clamp_to_edge);
  float2 d;
  if (in_near(u, p)) d = ng.sample(ls, ((p - u.nd.xy) / u.nd.z + 0.5) / u.ns.xy).rg;
  else d = fg.sample(ls, ((p - u.fd.xy) / u.fd.z + 0.5) / u.fs.xy).rg;
  return normalize(float3(-d, 1.0)); }

/* ---- noise ---- */
float h21(float2 p) { return fract(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
float vn(float2 p) {
  float2 i = floor(p), f = p - i; f = f * f * (3.0 - 2.0 * f);
  return mix(mix(h21(i), h21(i + float2(1, 0)), f.x), mix(h21(i + float2(0, 1)), h21(i + float2(1, 1)), f.x), f.y); }
float fbm(float2 p) { float s = 0, a = 0.5; for (int i = 0; i < 5; i++) { s += a * vn(p); p = p * 2.03 + 17.1; a *= 0.5; } return s; }
float fbm3(float2 p) { float s = 0, a = 0.5; for (int i = 0; i < 3; i++) { s += a * vn(p); p = p * 2.03 + 17.1; a *= 0.5; } return s / 0.875; }

/* ---- what the ground is: 0 meadow grass, 1 lush grass, 2 conifer forest
   ground, 3 dirt, 4 mossy rock, 5 scree, 6 plain rock, 7 snow ---- */
struct GC { float w[8]; float snow, gully, alpine; };
GC compose(constant TU &u, float3 wp, float3 Ng, float flow, float canopy) {
  GC c;
  float slope = 1.0 - Ng.z, z = wp.z;
  float n1 = fbm(wp.xy * 0.011), n2 = fbm(wp.xy * 0.045 + 5.3), n3 = fbm(wp.xy * 0.2 + 9.1);
  c.gully = smoothstep(2.5, 5.5, log2(1.0 + flow));
  float north = saturate(-Ng.y);
  float snowline = 1120.0 + (n1 - 0.5) * 300.0 - north * 180.0;   /* ~2900 m */
  float snow = smoothstep(snowline, snowline + 140.0, z) * smoothstep(0.75, 0.45, slope);
  /* fresh snow lies unbroken on anything a drift can hold; only the steep
     faces (and the odd wind-scoured gully) show through */
  snow = max(snow, u.wx.x * smoothstep(0.72, 0.5, slope + (n3 - 0.5) * 0.15) * smoothstep(0.15, 0.4, n3 * 0.5 + 0.6 - c.gully * 0.3));
  c.alpine = smoothstep(650.0, 950.0, z + (n1 - 0.5) * 200.0);    /* ~2450-2750 m */
  float rock = smoothstep(0.30, 0.48, slope + (n2 - 0.5) * 0.12 - c.alpine * 0.0);
  float bare = rock * (1.0 - snow);
  c.w[4] = bare * (1.0 - c.alpine) * smoothstep(0.3, 0.7, n2);
  c.w[6] = bare * max(c.alpine, 1.0 - smoothstep(0.3, 0.7, n2));
  c.w[5] = (1.0 - rock) * (1.0 - snow) * saturate(c.gully * smoothstep(0.1, 0.3, slope) + c.alpine * smoothstep(0.15, 0.35, slope) * 0.8);
  float soil = (1.0 - rock) * (1.0 - snow) * (1.0 - c.w[5]);
  /* bare earth where the slope is too steep to hold turf, in the gullies,
     and in the odd worn patch -- an alpine meadow is mostly unbroken grass */
  c.w[3] = soil * saturate(smoothstep(0.28, 0.42, slope) * 0.7 + smoothstep(0.74, 0.86, n2) * 0.25 + c.gully * 0.35);
  float green = soil - c.w[3];
  c.w[2] = green * smoothstep(0.08, 0.4, canopy);
  float meadow = green - c.w[2];
  c.w[1] = meadow * smoothstep(0.6, 0.78, n2 * 0.7 + n3 * 0.3 + c.alpine * 0.3) * 0.7;
  c.w[0] = meadow - c.w[1];
  c.w[7] = snow;
  c.snow = snow;
  return c; }

constant float TILE[8] = { 2.5, 2.5, 3.0, 3.0, 5.0, 3.0, 5.0, 4.0 };
/* how far each material's own relief stands out of the field, metres */
constant float RELIEF[8] = { 0.05, 0.06, 0.07, 0.07, 0.35, 0.16, 0.45, 0.08 };
float2 tq1(float2 q, int m) { return q / TILE[m]; }
float2 tq2(float2 q, int m) { return float2(q.x * 0.8 - q.y * 0.6, q.x * 0.6 + q.y * 0.8) / (TILE[m] * 4.3) + float2(0.37, 0.71); }

/* ---- the baked material weights (kcomp): 8 weights in two RGBA textures,
   1 m over the near field, 6 m out to the horizon; and a tiling noise ---- */
void weights(constant TU &u, texture2d<float> cn0, texture2d<float> cn1, texture2d<float> cf0, texture2d<float> cf1,
             float2 p, thread float *w, float lv) {
  constexpr sampler ls(filter::linear, address::clamp_to_edge);
  float4 a, b;
  if (in_near(u, p)) { float2 t = ((p - u.nd.xy) / u.nd.z + 0.5) / u.ns.xy; a = cn0.sample(ls, t, level(lv)); b = cn1.sample(ls, t, level(lv)); }
  else { float2 t = ((p - u.fd.xy) / u.fd.z + 0.5) / u.fs.xy; a = cf0.sample(ls, t, level(lv)); b = cf1.sample(ls, t, level(lv)); }
  w[0] = a.r; w[1] = a.g; w[2] = a.b; w[3] = a.a; w[4] = b.r; w[5] = b.g; w[6] = b.b; w[7] = b.a; }
/* the noise, sampled: channel c of a 64-unit tile (features about 1 unit) */
float nz(texture2d<float> nt, float2 x, int c) {
  constexpr sampler rs(filter::linear, mip_filter::linear, address::repeat);
  return nt.sample(rs, x / 64.0)[c]; }

/* ---- vertex: the patch, morphed, at its height, with the ground's relief ---- */
vertex VO tvs(uint vid [[vertex_id]], uint iid [[instance_id]], constant TU &u [[buffer(1)]],
              const device NV *nodes [[buffer(2)]],
              texture2d<float> nt [[texture(0)]], texture2d<float> ft [[texture(1)]],
              texture2d<uint> cls [[texture(2)]], texture2d_array<float> A [[texture(5)]],
              texture2d<float> ng [[texture(8)]], texture2d<float> fg [[texture(9)]],
              texture2d<float> cn0 [[texture(11)]], texture2d<float> cn1 [[texture(12)]],
              texture2d<float> cf0 [[texture(13)]], texture2d<float> cf1 [[texture(14)]], texture2d<float> noise [[texture(15)]]) {
  constexpr sampler ms(filter::linear, mip_filter::linear, address::repeat);
  float4 nd = nodes[iid].o;
  uint gx = vid % 33u, gy = vid / 33u;
  float2 g = float2(gx, gy);
  float s = nd.z / 32.0;
  float2 p = nd.xy + g * s;
  /* CDLOD: past `start` of this level's range a vertex slides onto its
     parent's grid, so the switch to the coarser patch changes nothing */
  float range = u.lod.x * exp2(nd.w);
  float h0 = in_near(u, p) ? hbil(nt, (p - u.nd.xy) / u.nd.z, u.ns.xy) : hbil(ft, (p - u.fd.xy) / u.fd.z, u.fs.xy);
  float d = distance(float3(p, h0), u.eye.xyz);
  float k = saturate((d - range * u.lod.y) / (range * (1.0 - u.lod.y)));
  float2 odd = fmod(g, 2.0);
  p -= odd * s * k;
  float h = height(u, nt, ft, cls, p);
  float3 w = float3(p, h);
  /* the ground's own relief, as real geometry near the eye: each material's
     scanned height (stones, clods, tussocks, rock faces), by the baked
     weights, out along the slope's normal */
  float fade = 1.0 - smoothstep(35.0, 110.0, d);
  if (fade > 0.0) {
    float3 Ng = gnormal(u, ng, fg, p);
    float wt[8];
    weights(u, cn0, cn1, cf0, cf1, p, wt, 0.0);
    float2 an = abs(Ng.xy);
    bool side = Ng.z < 0.65;
    float2 q = side ? (an.x > an.y ? float2(w.y, w.z) : float2(w.x, w.z)) : w.xy;
    float disp = 0, ws = 0;
    for (int m = 0; m < 8; m++) {
      if (wt[m] < 0.05) continue;
      float lv1 = max(log2(s / TILE[m] * 2048.0), 0.0);
      float h1 = A.sample(ms, tq1(q, m), m, level(lv1)).a;
      disp += wt[m] * (h1 - 0.5) * 2.0 * RELIEF[m];
      ws += wt[m]; }
    disp /= max(ws, 1e-3);
    uint ci = cls_at(u, cls, p);
    disp += (nz(noise, p * 0.7, 1) - 0.5) * 0.35 * (ci == 0u ? 1.0 : ci == 5u ? 0.6 : 0.0);
    if (ci == 1u) disp = clamp(disp, -0.03, 0.03);
    else if (ci >= 2u && ci <= 4u) disp = min(disp, 0.0);
    else if (ci == 5u) disp = clamp(disp, -0.15, 0.3);
    w += Ng * disp * fade;
  }
  float4 c = u.P * float4(w, 1);
  float X = u.vpt.x * c.x + u.vpt.y * c.w, Y = u.vpt.z * c.y + u.vpt.w * c.w;
  float Yd = u.misc.x > 0.5 ? 480.0 * c.w - Y : Y;
  VO o;
  o.pos = float4(u.map.x * X + u.map.y * c.w, u.map.z * Yd + u.map.w * c.w, 0.01, c.w);
  o.pos.xy += u.jit.xy * c.w;
  o.wp = w; o.oow = 1.0 / c.w; o.morph = k;
  return o; }

struct FO { float4 c [[color(0)]]; float4 n [[color(1)]]; float4 g [[color(2)]]; float4 a [[color(3)]];
            float m [[color(4)]]; };
uint wfloat(float oow) {
  if (oow >= 1.0) return 0;
  if (oow <= 0.0) return 0xFFFF;
  uint t = uint(min(oow * 4294967296.0, 4294967295.0));
  if (t == 0) return 0xFFFF;
  int e = int(clz(t));
  uint m = e <= 19 ? (~t >> uint(19 - e)) : (~t << uint(e - 19));
  uint w = (uint(e) << 12) | (m & 0xFFF);
  return w < 0xFFFF ? w + 1 : w; }
float fogtw(int i) { return exp2(3.0 + float(i >> 2)) / float(8 - (i & 3)); }
float fogof(constant TU &u, float w) {
  if (w <= 1.0) return u.fogtab[0];
  int g = int(floor(log2(w))); float m = w / exp2(float(g));
  int k = m > 1.6 ? 3 : m > 8.0 / 6.0 ? 2 : m > 8.0 / 7.0 ? 1 : 0;
  int i = 4 * g + k + 1;
  if (i > 1 && w <= fogtw(i - 1)) i--;
  if (i < 63 && w > fogtw(i)) i++;
  if (i > 63) return u.fogtab[63];
  float pw = fogtw(i - 1), tw = fogtw(i), prev = u.fogtab[i - 1];
  return prev + (u.fogtab[i] - prev) * (w - pw) / (tw - pw); }

struct MS { float3 c; float h; float2 n; float r; float ao; float3 nw; };
MS mat_sample1(texture2d_array<float> A, texture2d_array<float> N, int m, float2 q, float dist);
/* the material on any slope: from above on flat ground; on steep ground the
   three projections (along x, y, z) blended by how much the surface faces
   each, so a bank is never stretched */
MS mat_tri(texture2d_array<float> A, texture2d_array<float> N, int m, float3 wp, float3 Nr, float dist) {
  float3 b = pow(abs(Nr), float3(4.0)); b /= b.x + b.y + b.z;
  if (b.z > 0.97) { MS s = mat_sample1(A, N, m, wp.xy, dist); s.nw = float3(s.n, 0.0); return s; }
  MS o; o.c = 0; o.h = 0; o.n = 0; o.r = 0; o.ao = 0;
  float3 tn = 0;
  if (b.x > 0.01) { MS s = mat_sample1(A, N, m, wp.yz, dist); o.c += s.c * b.x; o.h += s.h * b.x; o.r += s.r * b.x; o.ao += s.ao * b.x;
                    tn += float3(0.0, s.n.x, s.n.y) * b.x; }
  if (b.y > 0.01) { MS s = mat_sample1(A, N, m, wp.xz, dist); o.c += s.c * b.y; o.h += s.h * b.y; o.r += s.r * b.y; o.ao += s.ao * b.y;
                    tn += float3(s.n.x, 0.0, s.n.y) * b.y; }
  if (b.z > 0.01) { MS s = mat_sample1(A, N, m, wp.xy, dist); o.c += s.c * b.z; o.h += s.h * b.z; o.r += s.r * b.z; o.ao += s.ao * b.z;
                    tn += float3(s.n.x, s.n.y, 0.0) * b.z; }
  /* the detail's tilt, in world space, folded back to the two components the
     caller lays along its own frame */
  o.nw = tn;
  return o; }
MS mat_sample1(texture2d_array<float> A, texture2d_array<float> N, int m, float2 q, float dist) {
  constexpr sampler ms(filter::linear, mip_filter::linear, address::repeat, max_anisotropy(16));
  /* two scales, the second turned and offset: no visible repeat */
  float2 q1 = tq1(q, m), q2 = tq2(q, m);
  float4 a1 = A.sample(ms, q1, m), a2 = A.sample(ms, q2, m);
  float4 n1 = N.sample(ms, q1, m), n2 = N.sample(ms, q2, m);
  float b = mix(0.35, 0.6, saturate(dist / 120.0));
  MS o;
  o.c = pow(mix(a1.rgb, a2.rgb, b), 2.2);
  o.h = mix(a1.a, a2.a, b);
  o.n = mix(n1.xy, n2.xy, b) * 2.0 - 1.0;
  o.r = mix(n1.z, n2.z, b); o.ao = mix(n1.w, n2.w, b);
  o.nw = 0;
  return o; }
/* the ground's look at a point: its two strongest materials, height-blended,
   from above on gentle ground and three ways on steep; the forest's canopy
   past the trees' cards; broad variation.  Returns the albedo (linear);
   *Nn the shading normal */
float3 ground(constant TU &u, float3 wp, float dist, thread float3 &Nn,
              texture2d<float> ng, texture2d<float> fg, texture2d_array<float> A, texture2d_array<float> N,
              texture2d<float> cnp, texture2d<float> cn0, texture2d<float> cn1, texture2d<float> cf0, texture2d<float> cf1,
              texture2d<float> noise) {
  constexpr sampler ls(filter::linear, address::clamp_to_edge);
  float3 Ng = gnormal(u, ng, fg, wp.xy);
  float slope = 1.0 - Ng.z;
  float w[8];
  weights(u, cn0, cn1, cf0, cf1, wp.xy, w, 0.0);
  int m1 = 0, m2 = -1;
  for (int m = 1; m < 8; m++) if (w[m] > w[m1]) m1 = m;
  for (int m = 0; m < 8; m++) if (m != m1 && (m2 < 0 || w[m] > w[m2])) m2 = m;
  float3 alb; float3 nn = 0; float ao = 1;
  float3 mean = 0; float wsum = 0;
  for (int m = 0; m < 8; m++) { mean += u.mm[m].rgb * w[m]; wsum += w[m]; }
  mean /= max(wsum, 1e-4);
  if (dist < 2500.0) {
    MS s1 = mat_tri(A, N, m1, wp, Ng, dist);
    float3 c = s1.c; float3 n = s1.nw; float a = s1.ao;
    if (m2 >= 0 && w[m2] > 0.03) {
      MS s2 = mat_tri(A, N, m2, wp, Ng, dist);
      float h1 = s1.h * 0.6 + w[m1], h2 = s2.h * 0.6 + w[m2], hm = max(h1, h2);
      float b1 = max(h1 - hm + 0.25, 0.0), b2 = max(h2 - hm + 0.25, 0.0), bs = b1 + b2;
      c = (s1.c * b1 + s2.c * b2) / bs; n = (s1.nw * b1 + s2.nw * b2) / bs; a = (s1.ao * b1 + s2.ao * b2) / bs; }
    float far = smoothstep(900.0, 2500.0, dist);
    alb = mix(c, mean, far); nn = n * (1.0 - far); ao = mix(a, 1.0, far);
  } else alb = mean;
  float n1 = nz(noise, wp.xy * 0.011, 0), n2 = nz(noise, wp.xy * 0.045 + 5.3, 1);
  /* forest: the crowns of a conifer forest as one dark, clumped canopy --
     where the placed trees stand once their cards are behind (the near
     field), and below the tree line beyond it */
  { float canopy = u.cmap.z > 0.0 ? cnp.sample(ls, (wp.xy - u.cmap.xy) * u.cmap.zw).r : 0.0;
    float treeline = 420.0 + (n1 - 0.5) * 160.0;
    float snow = w[7];
    float fz = smoothstep(treeline + 80.0, treeline - 80.0, wp.z) * smoothstep(0.78, 0.55, slope) * (1.0 - snow);
    float cover = smoothstep(0.36, 0.5, nz(noise, wp.xy * 0.0016 + 4.4, 2));
    float fk = in_near(u, wp.xy) ? saturate(canopy * 1.6) * smoothstep(250.0, 850.0, dist) * (1.0 - snow) : fz * cover;
    if (fk > 0.0) {
      float cl = nz(noise, wp.xy / 7.0, 3);
      float ck = 1.0 - smoothstep(600.0, 1800.0, dist);
      float3 crown = mix(float3(0.018, 0.032, 0.016), float3(0.05, 0.08, 0.035), mix(0.5, cl, ck));
      crown *= mix(0.85, 1.15, n2);
      alb = mix(alb, crown, fk);
      nn = mix(nn, float3(nz(noise, wp.xy / 7.0 + 0.5, 0) - 0.5, nz(noise, wp.xy / 7.0 + 9.5, 1) - 0.5, 0.0) * 1.6 * ck, fk);
      ao = mix(ao, 0.75, fk); }
    if (int(u.wx.w + 0.5) == 3) alb = float3(fk, in_near(u, wp.xy) ? 0.5 : 0.0, fz); }
  /* a meadow seen through its grass: what shows between the leaves is the
     shade and old thatch at their feet, not a lawn -- darkest where the
     leaves are drawn, and still dimmed beyond them, where the grass is only
     this colour */
  { float gw = saturate(w[0] + w[1] * 0.8 + w[2] * 0.25);
    float nearg = 1.0 - smoothstep(30.0, 62.0, dist);
    float3 thatch = float3(0.05, 0.052, 0.026);
    alb = mix(alb, mix(alb * 0.78, thatch, 0.55), gw * nearg);
    alb *= mix(1.0, 0.85, gw * (1.0 - nearg)); }
  alb *= mix(0.82, 1.12, n1) * mix(0.9, 1.06, n2);
  { float3 warm = alb * float3(1.08, 1.0, 0.85), cool = alb * float3(0.92, 1.0, 1.06);
    alb = mix(warm, cool, smoothstep(0.3, 0.7, nz(noise, wp.xy * 0.004 + 2.2, 0))); }
  float nk = 1.0 - smoothstep(40.0, 260.0, dist);
  Nn = normalize(Ng + nn * nk);
  return alb * mix(1.0, ao, 0.7); }

/* the scene pass: in the main view only what the resolve needs (where, which
   way, that it is the field) -- the shading runs once per pixel afterwards;
   in the mirror, the whole look here */
fragment FO tfs(VO in [[stage_in]], constant TU &u [[buffer(0)]],
                texture2d<uint> cls [[texture(2)]], texture2d_array<float> A [[texture(5)]], texture2d_array<float> N [[texture(6)]],
                texture2d<float> cnp [[texture(7)]], texture2d<float> ng [[texture(8)]], texture2d<float> fg [[texture(9)]],
                texture2d<uint> fcls [[texture(10)]],
                texture2d<float> cn0 [[texture(11)]], texture2d<float> cn1 [[texture(12)]],
                texture2d<float> cf0 [[texture(13)]], texture2d<float> cf1 [[texture(14)]], texture2d<float> noise [[texture(15)]]) {
  float3 wp = in.wp;
  FO o;
  float kf = u.misc.z > 0.5 ? fogof(u, 1.0 / in.oow) / 255.0 : 0.0;
  bool water;
  if (in_near(u, wp.xy)) water = cls.read(uint2((wp.xy - u.nd.xy) / u.nd.z + 0.5)).r == 6u;
  else { float2 g = clamp((wp.xy - u.fd.xy) / u.fd.z + 0.5, float2(0), u.fs.xy - 1.0); water = fcls.read(uint2(g)).r == 1u; }
  float bref = u.misc.w > 0.0 ? u.misc.w : 0.5;
  if (u.misc.y > 0.5) {
    if (water) {
      float3 deep = float3(0.012, 0.03, 0.035);
      o.a = float4(pow(deep, 1.0 / 2.2), pow(bref, 1.0 / 2.2));
      o.n = float4(0, 0, 1, 1); o.g = float4(wp, 1.0 + 0.9 * kf); o.m = 9.0 / 255.0;
      o.c = float4(0.12, 0.22, 0.26, 1);
      return o; }
    float3 Ng = gnormal(u, ng, fg, wp.xy);
    float w[8]; weights(u, cn0, cn1, cf0, cf1, wp.xy, w, 2.0);
    float3 mean = 0; for (int m = 0; m < 8; m++) mean += u.mm[m].rgb * w[m];
    o.a = float4(pow(saturate(mean), 1.0 / 2.2), pow(bref, 1.0 / 2.2));   /* the resolve replaces it */
    o.n = float4(Ng, 1);
    o.g = float4(wp, 1.0 + 0.9 * kf);
    o.m = 10.0 / 255.0;
    float ndl = saturate(dot(Ng, u.sun.xyz));
    o.c = float4(pow(saturate(mix(mean * (u.sunc.rgb * ndl + u.skyc.rgb) * 0.5, u.fogc.rgb, kf)), 1.0 / 2.2), 1);
    if (int(u.wx.w + 0.5) == 9) { o.c = float4(1, 0, 1, 1); o.n = float4(0, 0, 1, 1); o.g = float4(wp, 4.0); o.a = 0; o.m = 0; }
    return o; }
  float3 Nn;
  float dist = distance(u.eye.xyz, wp);
  float3 alb = water ? float3(0.02, 0.04, 0.05) : ground(u, wp, dist, Nn, ng, fg, A, N, cnp, cn0, cn1, cf0, cf1, noise);
  if (water) Nn = float3(0, 0, 1);
  float ndl = saturate(dot(Nn, u.sun.xyz));
  float3 lit = alb * (u.sunc.rgb * ndl + mix(u.grnd.rgb, u.skyc.rgb, Nn.z * 0.5 + 0.5)) * 1.35;
  lit = saturate(lit * (2.51 * lit + 0.03) / (lit * (2.43 * lit + 0.59) + 0.14));
  lit = mix(lit, u.fogc.rgb, kf);
  o.c = float4(pow(lit, 1.0 / 2.2), 1);
  o.n = float4(0, 0, 0, 1); o.g = float4(0); o.a = float4(0); o.m = 0;
  return o; }

/* the resolve: once per screen pixel the field covers, after the scene --
   the ground's full look into the G-buffer's colour and normal */
struct RO { float4 pos [[position]]; };
vertex RO trvs(uint vid [[vertex_id]]) {
  float2 p = float2((vid << 1) & 2, vid & 2); RO o; o.pos = float4(p * 2.0 - 1.0, 0, 1); return o; }
struct RF { float4 n [[color(0)]]; float4 a [[color(1)]]; };
fragment RF trfs(RO in [[stage_in]], constant TU &u [[buffer(0)]],
                 texture2d_array<float> A [[texture(5)]], texture2d_array<float> N [[texture(6)]],
                 texture2d<float> cnp [[texture(7)]], texture2d<float> ng [[texture(8)]], texture2d<float> fg [[texture(9)]],
                 texture2d<float> cn0 [[texture(11)]], texture2d<float> cn1 [[texture(12)]],
                 texture2d<float> cf0 [[texture(13)]], texture2d<float> cf1 [[texture(14)]], texture2d<float> noise [[texture(15)]],
                 texture2d<float> gp [[texture(16)]], texture2d<float> gm [[texture(17)]]) {
  uint2 px = uint2(in.pos.xy);
  int mid = int(gm.read(px).r * 255.0 + 0.5);
  if (mid != 10) discard_fragment();
  float3 wp = gp.read(px).xyz;
  float3 Nn;
  float3 alb = ground(u, wp, distance(u.eye.xyz, wp), Nn, ng, fg, A, N, cnp, cn0, cn1, cf0, cf1, noise);
  RF o;
  float bref = u.misc.w > 0.0 ? u.misc.w : 0.5;
  o.n = float4(Nn, 1);
  o.a = float4(pow(saturate(alb), 1.0 / 2.2), pow(bref, 1.0 / 2.2));
  return o; }

/* the material weights, baked over a grid (near or far: p.x 0/1) from the
   composition rules: once per track and weather */
kernel void kcomp(texture2d<float> nt [[texture(0)]], texture2d<float> ft [[texture(1)]],
                  texture2d<float> nflow [[texture(3)]], texture2d<float> fflow [[texture(4)]],
                  texture2d<float> cnp [[texture(7)]], texture2d<float> ng [[texture(8)]], texture2d<float> fg [[texture(9)]],
                  texture2d<float, access::write> o0 [[texture(20)]], texture2d<float, access::write> o1 [[texture(21)]],
                  texture2d<float> troad [[texture(22)]],
                  constant TU &u [[buffer(0)]], constant float4 &kp [[buffer(1)]], uint2 id [[thread_position_in_grid]]) {
  constexpr sampler ls(filter::linear, address::clamp_to_edge);
  bool nr = kp.x < 0.5;
  float2 sz = nr ? u.ns.xy : u.fs.xy;
  if (id.x >= uint(sz.x) || id.y >= uint(sz.y)) return;
  float2 p = nr ? u.nd.xy + float2(id) * u.nd.z : u.fd.xy + float2(id) * u.fd.z;
  float h = nr ? nt.read(id).r : ft.read(id).r;
  float3 Ng = nr ? normalize(float3(-ng.read(id).rg, 1.0)) : normalize(float3(-fg.read(id).rg, 1.0));
  float flow = nr ? nflow.read(id).r : fflow.read(id).r * 0.12;
  float canopy = u.cmap.z > 0.0 ? cnp.sample(ls, (p - u.cmap.xy) * u.cmap.zw, level(0)).r : 0.0;
  GC c = compose(u, float3(p, h), Ng, flow, canopy);
  /* the road's shoulder: packed dirt and spilled gravel along its edge,
     as wide as the traffic has worn it -- here barely a hand's width, there
     a couple of metres -- and broken by tufts the grass holds on to */
  if (nr) {
    float rd = troad.read(id).r * 25.5;
    float wide = 0.3 + 2.0 * smoothstep(0.35, 0.75, fbm(p * 0.06 + 1.3)) * fbm(p * 0.33 + 7.1);
    float sh = 1.0 - smoothstep(0.15, wide + 0.35, rd);
    sh *= smoothstep(0.25, 0.55, fbm(p * 0.9 + 2.9) + (1.0 - smoothstep(0.0, 0.6, rd)) * 0.6);
    if (sh > 0.0) {
      float g = c.w[0] + c.w[1] + c.w[2];
      c.w[0] *= 1.0 - sh; c.w[1] *= 1.0 - sh; c.w[2] *= 1.0 - sh;
      c.w[5] += g * sh * 0.25; c.w[3] += g * sh * 0.75; } }
  float s = 0; for (int m = 0; m < 8; m++) s += c.w[m];
  s = max(s, 1e-4);
  o0.write(float4(c.w[0], c.w[1], c.w[2], c.w[3]) / s, id);
  o1.write(float4(c.w[4], c.w[5], c.w[6], c.w[7]) / s, id); }

/* ---- grass: clumps of blades on a grid round the eye ---- */
struct GU { float4 ring;   /* cell size, grid side, inner radius, outer radius */
            float4 orig;   /* the grid's corner (snapped to cells); where the grid's edge fades */
            float4 blades; /* blades a clump, vertices a blade, height scale */ };
struct GO { float4 pos [[position]]; float3 wp; float3 fn; float3 ac; float2 uv; float t; float dry; float3 tint; float oow; };
struct GC2 { float4 a, b, c; };   /* a: root x y, height, r3; b: normal xy, edge, snow; c: cell key xy, dry */
struct GARGS { uint vcount, icount, vstart, istart; };
kernel void kgcull(constant TU &u [[buffer(0)]], constant GU &gu [[buffer(1)]],
                   device GC2 *out [[buffer(2)]], device atomic_uint *args [[buffer(3)]], constant uint &cap [[buffer(4)]],
                   texture2d<float> nt [[texture(0)]], texture2d<uint> cls [[texture(2)]],
                   texture2d<float> ng [[texture(8)]], texture2d<float> fg [[texture(9)]],
                   texture2d<float> cn0 [[texture(11)]], texture2d<float> cn1 [[texture(12)]],
                   texture2d<float> cf0 [[texture(13)]], texture2d<float> cf1 [[texture(14)]], texture2d<float> noise [[texture(15)]],
                   uint2 id [[thread_position_in_grid]]) {
  uint side = uint(gu.ring.y);
  if (id.x >= side || id.y >= side) return;
  float2 cell = float2(id);
  float2 ck = gu.orig.xy / gu.ring.x + cell;
  float r0 = h21(ck * 1.37 + 0.5), r1 = h21(ck * 2.11 + 7.3), r2 = h21(ck * 0.73 + 3.9), r3 = h21(ck * 3.3 + 1.1);
  float2 root = gu.orig.xy + (cell + float2(r0, r1)) * gu.ring.x;
  float dxy = distance(root, u.eye.xy);
  if (dxy < gu.ring.z || dxy > gu.ring.w || !in_near(u, root)) return;
  float edge = 1.0 - smoothstep(gu.orig.z, gu.orig.w, dxy);
  if (r2 > 1.15 * (0.4 + 0.6 * edge)) return;
  float w[8]; weights(u, cn0, cn1, cf0, cf1, root, w, 0.0);
  /* tufts hold on in the dirt and gravel of a shoulder, shorter */
  float grass = w[0] + w[1] * 0.8 + w[2] * 0.25 + (w[3] + w[5] * 0.5) * 0.45 * smoothstep(0.45, 0.65, nz(noise, root * 0.7 + 5.0, 1));
  if (r2 > grass * 1.15 * (0.4 + 0.6 * edge) || w[7] > 0.9) return;
  uint ci = cls_at(u, cls, root);
  if (ci >= 2u && ci <= 4u) return;
  float h = hbil(nt, (root - u.nd.xy) / u.nd.z, u.ns.xy);
  float4 cr = u.P * float4(root, h + 0.3, 1);
  if (cr.w < 0.05 || abs(cr.x) > cr.w * 1.35 + 1.5 || abs(cr.y) > cr.w * 1.35 + 1.5) return;
  float3 Ng = gnormal(u, ng, fg, root);
  uint slot = atomic_fetch_add_explicit(&args[1], 1u, memory_order_relaxed);
  if (slot >= cap) return;
  float dry = smoothstep(0.55, 0.85, nz(noise, root * 0.05 + 2.0, 2)) * 0.6 + r3 * 0.2;
  float lush = saturate((w[0] + w[1] + w[2]) * 1.5);
  GC2 g; g.a = float4(root, h, r3); g.b = float4(Ng.xy, edge, w[7]); g.c = float4(ck, dry, lush);
  out[slot] = g; }
/* one grass leaf: a ribbon of gu.blades.w segments carrying one of the
   photoscanned leaves (remaster_grass_atlas.py: ten 96-texel columns, tip
   at the top), rising from the clump's centre and arching outward under its
   own weight -- some barely, some folded right over -- with the wind adding
   to the bend toward the tip */
constant uint NLEAF = 10;
vertex GO gvs(uint vid [[vertex_id]], uint iid [[instance_id]], constant TU &u [[buffer(1)]], constant GU &gu [[buffer(3)]],
              const device GC2 *clumps [[buffer(4)]]) {
  GO o; o.pos = float4(0, 0, -1, 1); o.wp = 0; o.fn = 0; o.ac = 0; o.uv = 0; o.t = 0; o.dry = 0; o.tint = 0; o.oow = 1;
  GC2 g = clumps[iid];
  float2 root = g.a.xy, ck = g.c.xy; float h = g.a.z, r3 = g.a.w, edge = g.b.z;
  uint S = uint(gu.blades.w), vpb = S * 6u;
  uint bi = vid / vpb, vi = vid % vpb;
  float rb0 = h21(ck + float2(bi * 1.7, 0.3)), rb1 = h21(ck + float2(bi * 2.9, 5.1)), rb2 = h21(ck + float2(bi * 0.61, 9.7));
  float rb3 = h21(ck + float2(bi * 3.7, 2.2)), rb4 = h21(ck + float2(bi * 5.3, 7.9));
  float L = (0.22 + 0.5 * rb2 * rb2) * mix(0.75, 1.2, r3) * gu.blades.z * mix(0.55, 1.0, edge) * (1.0 - g.b.w) * mix(0.45, 1.0, g.c.w);
  if (L < 0.05) return o;
  /* where in the clump it rises, and which way it leans: away from the centre */
  float ang = (float(bi) + rb0 * 0.8) * 6.2831853 / max(gu.blades.x, 1.0) + r3 * 6.2831853;
  float2 lean = float2(cos(ang), sin(ang));
  float2 base = root + lean * (0.015 + 0.06 * rb1);
  /* its bend: the angle from upright at the root and how far it turns by the tip */
  float flop = rb4 > 0.75 ? 1.0 : 0.0;
  float th0 = 0.18 + 0.4 * rb1, dth = mix(0.5 + 0.9 * rb3, 1.9 + 0.5 * rb3, flop);
  float tm = u.wx.z * 0.001;
  float wind = sin(tm * 1.7 + dot(base, float2(0.21, 0.13))) * 0.5 + sin(tm * 3.1 + dot(base, float2(0.5, 0.37)) + rb0 * 3.0) * 0.25;
  float2 wdir = float2(0.8, 0.6);
  /* the corner this vertex is: two triangles a segment */
  uint seg = vi / 6u, c = vi % 6u;
  uint row = seg + ((c == 2u || c == 3u || c == 5u) ? 1u : 0u);
  float side = (c == 1u || c == 4u || c == 5u) ? 0.5 : -0.5;
  float t = float(row) / float(S);
  float3 p = float3(base, h - 0.02), tg = float3(0, 0, 1);
  float sl = L / float(S);
  for (uint i = 0; i < row; i++) {
    float ti = (float(i) + 0.5) / float(S);
    float th = th0 + dth * ti * ti + 0.0;
    float3 d = float3(lean * sin(th), cos(th));
    d.xy += wdir * wind * 0.35 * ti;
    d = normalize(d);
    p += d * sl; tg = d; }
  if (row == 0u) { float th = th0; tg = normalize(float3(lean * sin(th), cos(th))); }
  /* the leaf's width lies across its lean, turning a little along it */
  float tw = (rb0 - 0.5) * 1.2 * t;
  float2 a2 = float2(-lean.y, lean.x) * cos(tw) + lean * sin(tw);
  float3 ac = normalize(float3(a2, 0) - tg * dot(float3(a2, 0), tg));
  float bw = (0.022 + 0.022 * rb1) * mix(1.0, 1.35, flop);
  p += ac * side * bw;
  float4 cc = u.P * float4(p, 1);
  float X = u.vpt.x * cc.x + u.vpt.y * cc.w, Y = u.vpt.z * cc.y + u.vpt.w * cc.w;
  float Yd = u.misc.x > 0.5 ? 480.0 * cc.w - Y : Y;
  o.pos = float4(u.map.x * X + u.map.y * cc.w, u.map.z * Yd + u.map.w * cc.w, 0.01, cc.w);
  o.pos.xy += u.jit.xy * cc.w;
  o.wp = p; o.t = t; o.oow = 1.0 / cc.w;
  o.fn = normalize(cross(ac, tg)); o.ac = ac;
  /* living leaves (the scan's green ones); its dead ones only where it is dry */
  const uint LIVE[7] = { 0, 1, 3, 4, 5, 8, 9 }, DEAD[3] = { 2, 6, 7 };
  float dryk = saturate(g.c.z * 0.7 + (rb4 < 0.06 ? 0.9 : 0.0));
  uint leaf = dryk > 0.6 && rb3 < dryk * 0.6 ? DEAD[uint(rb2 * 2.99)] : LIVE[uint(rb2 * 6.99)];
  o.uv = float2((float(leaf) * 96.0 + (side + 0.5) * 96.0) / 1024.0, 1.0 - t);
  /* dry leaves: more of them where the meadow is drying, a few everywhere */
  o.dry = dryk;
  /* each clump its own green: a little yellower or bluer, lighter or darker */
  float hv = h21(ck * 0.37 + 4.1);
  o.tint = mix(float3(1.0, 1.02, 0.86), float3(0.86, 1.0, 1.05), hv) * mix(0.8, 1.12, r3);
  return o; }
fragment FO gfs(GO in [[stage_in]], bool front [[front_facing]], constant TU &u [[buffer(0)]],
                texture2d_array<float> lv [[texture(0)]]) {
  constexpr sampler s(filter::linear, mip_filter::linear, address::clamp_to_edge, max_anisotropy(4));
  FO o;
  float4 NM = lv.sample(s, in.uv, 2);
  /* the outline, held sharp as the leaf shrinks with distance (mip averaging
     would thin it away) */
  float lod = lv.calculate_clamped_lod(s, in.uv);
  float cut = NM.b * (1.0 + 0.35 * lod);
  if (cut < 0.45) discard_fragment();
  float3 green = pow(lv.sample(s, in.uv, 0).rgb, 2.2), dry = pow(lv.sample(s, in.uv, 1).rgb, 2.2);
  /* the scan's leaves are a late-summer olive: keep their light and dark
     (veins, midrib, blemishes) and give the living ones a meadow's green */
  const float3 lw = float3(0.2126, 0.7152, 0.0722);
  float3 lush = float3(0.055, 0.105, 0.022) * dot(green, lw) / 0.30;
  float3 alb = mix(mix(lush, green * 0.8, 0.25), dry * 0.75, in.dry) * in.tint;
  /* low in the clump the leaves shade one another: dark at the root */
  alb *= mix(0.22, 1.0, smoothstep(0.0, 0.65, in.t));
  /* the leaf is folded along its midrib (a shallow V), and its scan's relief */
  float3 fn = normalize(in.fn), ac = normalize(in.ac);
  float3 V = normalize(u.eye.xyz - in.wp);
  if (dot(fn, V) < 0.0) fn = -fn;
  float across = in.uv.x * 1024.0 / 96.0; across = fract(across) - 0.5;
  float2 nm = NM.rg * 2.0 - 1.0;
  float3 along = normalize(cross(fn, ac));
  float3 N = normalize(fn + ac * (across * 0.9 + nm.x * 0.6) + along * nm.y * 0.4);
  N = normalize(mix(N, float3(0, 0, 1), 0.25));
  float kf = u.misc.z > 0.5 ? fogof(u, 1.0 / in.oow) / 255.0 : 0.0;
  float bref = u.misc.w > 0.0 ? u.misc.w : 0.5;
  o.a = float4(pow(saturate(alb), 1.0 / 2.2), pow(bref, 1.0 / 2.2));
  o.n = float4(N, 1);
  o.g = float4(in.wp, 1.0 + 0.9 * kf);
  o.m = 11.0 / 255.0;          /* grass: lit as ground with the sun through its leaves (host_fx.m) */
  float ndl = saturate(dot(N, u.sun.xyz));
  o.c = float4(pow(saturate(alb * (u.sunc.rgb * ndl + u.skyc.rgb) * 0.5), 1.0 / 2.2), 1);
  return o; }

struct LU { float4 g;     /* map: x0, y0, texel (m), side */
            float4 fd, fs; /* far grid */
            float4 sun, sunc, skyc, misc; /* misc: snow on the ground */ };
float lh(texture2d<float> h, constant LU &u, float2 p) {
  constexpr sampler ls(filter::linear, address::clamp_to_edge);
  return h.sample(ls, ((p - u.fd.xy) / u.fd.z + 0.5) / u.fs.xy, level(0)).r; }
float3 lnorm(texture2d<float> h, constant LU &u, float2 p, float e) {
  float hx = lh(h, u, p + float2(e, 0)) - lh(h, u, p - float2(e, 0)), hy = lh(h, u, p + float2(0, e)) - lh(h, u, p - float2(0, e));
  return normalize(float3(-hx, -hy, 2.0 * e)); }
/* how much of the sun a point sees past the land toward it (soft: the
   nearest miss, by its angle) */
kernel void ksunvis(texture2d<float> h [[texture(0)]], texture2d<float, access::write> out [[texture(1)]],
                    constant LU &u [[buffer(0)]], uint2 id [[thread_position_in_grid]]) {
  float2 p = u.g.xy + (float2(id) + 0.5) * u.g.z;
  float3 L = u.sun.xyz;
  float2 dir = normalize(L.xy + 1e-6);
  float tanL = L.z / max(length(L.xy), 1e-4);
  float z0 = lh(h, u, p) + 1.5, vis = 1.0, t = u.g.z;
  for (int i = 0; i < 72 && t < 9000.0; i++) {
    float zt = lh(h, u, p + dir * t);
    float over = z0 + t * tanL - zt;                 /* the ray's height above the land */
    vis = min(vis, saturate(0.5 + over / (0.02 * t + 2.0)));
    if (vis <= 0.0) break;
    t += max(u.g.z * 0.75, t * 0.07); }
  out.write(float4(vis, 0, 0, 1), id); }
/* the ground's colour, for what it reflects (the shading's rules, broad) */
float3 lalbedo(constant LU &u, float z, float3 n, uint water) {
  if (water == 1u) return float3(0.02, 0.03, 0.035);
  float slope = 1.0 - n.z;
  float snow = max(smoothstep(1100.0, 1250.0, z) * smoothstep(0.75, 0.45, slope), u.misc.x * smoothstep(0.55, 0.3, slope));
  float rock = max(smoothstep(0.3, 0.48, slope), smoothstep(700.0, 950.0, z) * 0.6);
  float forest = smoothstep(480.0, 360.0, z) * (1.0 - rock) * 0.6;
  float3 g = mix(float3(0.07, 0.11, 0.035), float3(0.03, 0.045, 0.025), forest);
  float3 a = mix(g, float3(0.16, 0.15, 0.13), rock);
  return mix(a, float3(0.75, 0.78, 0.82), snow); }
/* light from the sky and the land around: per 16 headings, the horizon
   (the highest the land rises seen from here); the sky above it lights the
   point, the land below it lights it with what that land reflects of the sun
   and sky.  Stored as the light a flat-lit surface takes (radiance units,
   as host_fx.m's ambient: open level ground = the sky colour) */
kernel void kgi(texture2d<float> h [[texture(0)]], texture2d<float> sv [[texture(1)]], texture2d<uint> wat [[texture(2)]],
                texture2d<float, access::write> out [[texture(3)]],
                constant LU &u [[buffer(0)]], uint2 id [[thread_position_in_grid]]) {
  constexpr sampler ls(filter::linear, address::clamp_to_edge);
  float2 p = u.g.xy + (float2(id) + 0.5) * u.g.z;
  float3 n0 = lnorm(h, u, p, u.g.z);
  float z0 = lh(h, u, p) + 1.0;
  float3 E = 0; float skyv = 0;
  const int ND = 16;
  for (int d = 0; d < ND; d++) {
    float a = (float(d) + 0.5) * 6.2831853 / float(ND);
    float2 dir = float2(cos(a), sin(a));
    /* the horizon in this heading, and the land that makes it */
    float best = -1.0, t = u.g.z, tb = 0;
    for (int i = 0; i < 40 && t < 7000.0; i++) {
      float zt = lh(h, u, p + dir * t);
      float s = (zt - z0) / t;
      if (s > best) { best = s; tb = t; }
      t += max(u.g.z, t * 0.11); }
    float el = atan(max(best, -1.0));                /* the horizon's elevation */
    /* the slice's share of a cosine-weighted hemisphere, tilted by the
       ground's own normal (the slope facing this heading sees more of what is
       below the horizontal, less of the sky behind it) */
    float tilt = dot(n0.xy, dir);                    /* + facing this way */
    float elg = atan(-tilt / max(n0.z, 0.2));         /* where the ground's own plane cuts this heading */
    float lo = max(el, elg);
    float skyw = 0.5 * (1.0 - sin(lo) * abs(sin(lo)));     /* sky from the horizon up, cosine weighted */
    float landw = 0.5 * max(sin(lo) * abs(sin(lo)) - sin(elg) * abs(sin(elg)), 0.0) * step(elg, el);
    float3 sk = u.skyc.rgb * mix(1.15, 0.85, saturate(sin(max(lo, 0.0)) * 1.5));   /* the sky paler low down */
    E += sk * skyw;
    skyv += skyw;
    if (landw > 0.0 && tb > 0.0) {
      float2 q = p + dir * tb;
      float3 nq = lnorm(h, u, q, u.g.z * 2.0);
      float zq = lh(h, u, q);
      float2 gq = (q - u.g.xy) / u.g.z;
      float vis = sv.sample(ls, (gq + 0.5) / u.g.w, level(0)).r;
      float2 fq = clamp((q - u.fd.xy) / u.fd.z + 0.5, float2(0), u.fs.xy - 1.0);
      float3 alb = lalbedo(u, zq, nq, wat.read(uint2(fq)).r);
      float3 Lq = alb * (u.sunc.rgb * saturate(dot(nq, u.sun.xyz)) * vis + u.skyc.rgb * 0.8);
      E += Lq * landw; } }
  E *= 2.0 / float(ND);                              /* the 16 slices of a hemisphere (open flat ground: the sky colour) */
  out.write(float4(E, skyv * 2.0 / float(ND)), id); }

/* the sun's shadow map: depth only (the field without its fine relief) */
struct SU { float4x4 svp; float4 lod; float4 nd, ns, fd, fs; };
struct SO { float4 pos [[position]]; };
vertex SO tsvs(uint vid [[vertex_id]], uint iid [[instance_id]], constant SU &s [[buffer(1)]],
               const device NV *nodes [[buffer(2)]],
               texture2d<float> nt [[texture(0)]], texture2d<float> ft [[texture(1)]]) {
  float4 nd = nodes[iid].o;
  uint gx = vid % 33u, gy = vid / 33u;
  float2 p = nd.xy + float2(gx, gy) * (nd.z / 32.0);
  float h;
  { float2 g = (p - s.nd.xy) / s.nd.z;
    if (all(g >= 0.0) && all(g <= s.ns.xy - 1.001)) h = hbil(nt, g, s.ns.xy);
    else h = hbil(ft, (p - s.fd.xy) / s.fd.z, s.fs.xy); }
  SO o; o.pos = s.svp * float4(p, h, 1); return o; }
);

typedef struct {
    float P[16];
    float vpt[4], eye[4], map[4], jit[4], misc[4];
    float nd[4], ns[4], fd[4], fs[4];
    float sun[4], sunc[4], skyc[4], grnd[4], fogc[4];
    float cmap[4], wx[4], lod[4], mm[8][4];
    float fogtab[64];
} tu_t;
typedef struct { float svp[16]; float lod[4]; float nd[4], ns[4], fd[4], fs[4]; } su_t;

static id<MTLDevice> D;
static id<MTLRenderPipelineState> g_pipe, g_spipe, g_gpipe;
static id<MTLComputePipelineState> g_ksv, g_kgi, g_kcomp, g_kgcull;
#define GCAP (1 << 17)                      /* grass clumps a ring can draw */
static id<MTLBuffer> g_gclump[3][3], g_gargs[3][3];
static int g_gring = -1;                    /* the slot the last cull wrote */
static float g_grass_gu[3][12];
static id<MTLRenderPipelineState> g_rpipe;
static id<MTLTexture> g_noise;
static tu_t g_lastu; static int g_lastu_ok;   /* the main view's uniforms this frame, for the resolve */
static int g_comp_snow = -1;
static id<MTLCommandQueue> g_lq;
static float g_lsun[3] = { 0, 0, -9 };     /* the sun the light maps were solved for */
static volatile int g_lbusy;
enum { LMAP = 2048 };                       /* light maps: texels a side */
static const float LHALF = 8000.0f;         /* metres from the far grid's centre */
static id<MTLDepthStencilState> g_ds, g_dsall;
static id<MTLBuffer> g_ib, g_nodes[3], g_snodes[6];
static int g_nib;
static int g_state;                        /* 0 not tried, 1 ready, -1 unavailable */
static int g_mat_state;
static id<MTLTexture> g_matA, g_matN, g_dummy, g_dummyu, g_leaves;
static float g_mm[8][4];

/* the loaded field */
typedef struct {
    int ok;
    char name[64];
    float nx0, ny0, ncell; int nw, nh;
    float fx0, fy0, fcell; int fw, fh;
    float *near, *far;
    id<MTLTexture> tn, tf, tcls, tnflow, tfflow, tng, tfg, tfcls, tfh;
    id<MTLTexture> tsv, tgi;               /* the light maps (hter_light); nil until solved */
    id<MTLTexture> cn0, cn1, cf0, cf1;     /* the baked material weights (kcomp) */
    id<MTLTexture> troad;                  /* metres to the road / 10, over the near grid */
    /* the quadtree's height bounds: per level, per node */
    int levels;                            /* root at levels-1, leaves (LEAF m) at 0 */
    float rx0, ry0, rsize;                 /* the root square */
    float *zmin[16], *zmax[16]; int dim[16];
} tfield;
static tfield g_t;

static int off_env(void)
{
    static int o = -1;
    if (o < 0) o = getenv("BR_TERRAIN") && !atoi(getenv("BR_TERRAIN"));
    return o;
}

static NSString *models_dir(void)
{
    const char *e = getenv("BR_ENV_DIR");
    NSString *r;
    if (e) return [NSString stringWithUTF8String:e];
    r = [[NSBundle mainBundle].resourcePath stringByAppendingPathComponent:@"env/models"];
    if ([[NSFileManager defaultManager] fileExistsAtPath:r]) return r;
    return @"ports/common/models";
}

/* ---- the ground materials ------------------------------------------------ */

static const char *const MATS[8] = { "Grass004", "sparse_grass", "forest_ground_04", "dirt",
                                     "mossy_rock", "gray_rocks", "rock_01", "Snow015" };
enum { MSZ = 2048 };

static int load_img(NSString *p, u8 *out, int sz)
{
    CGImageSourceRef src = CGImageSourceCreateWithURL((__bridge CFURLRef)[NSURL fileURLWithPath:p], NULL);
    CGImageRef img;
    CGContextRef cx;
    CGColorSpaceRef cs;
    if (!src) return 0;
    img = CGImageSourceCreateImageAtIndex(src, 0, NULL);
    CFRelease(src);
    if (!img) return 0;
    cs = CGColorSpaceCreateDeviceRGB();
    cx = CGBitmapContextCreate(out, (size_t)sz, (size_t)sz, 8, (size_t)sz * 4, cs, kCGImageAlphaNoneSkipLast);
    CGColorSpaceRelease(cs);
    CGContextSetInterpolationQuality(cx, kCGInterpolationHigh);
    CGContextDrawImage(cx, CGRectMake(0, 0, sz, sz), img);
    CGContextRelease(cx);
    CGImageRelease(img);
    return 1;
}

/* a set's maps: Poly Haven (<name>_diff_4k.jpg, _nor_gl_4k.png, _arm_4k.jpg,
 * _disp_4k.png) or ambientCG (<name>_2K-JPG_Color.jpg ...) */
static NSString *map_path(const char *name, int kind)
{
    NSString *res = [[NSBundle mainBundle].resourcePath stringByAppendingPathComponent:@"env/textures"];
    NSString *ph = [[NSFileManager defaultManager] fileExistsAtPath:res] ? res
                 : [models_dir() stringByAppendingPathComponent:@"env/polyhaven/textures"];
    static const char *const PH[4] = { "diff_4k.jpg", "nor_gl_4k.png", "arm_4k.jpg", "disp_4k.png" };
    static const char *const AC[4] = { "Color.jpg", "NormalGL.jpg", "Roughness.jpg", "Displacement.jpg" };
    NSString *p = [ph stringByAppendingPathComponent:[NSString stringWithFormat:@"%s/%s_%s", name, name, PH[kind]]];
    if ([[NSFileManager defaultManager] fileExistsAtPath:p]) return p;
    {
        NSString *mres = [[NSBundle mainBundle].resourcePath stringByAppendingPathComponent:@"materials"];
        NSString *mdir = [[NSFileManager defaultManager] fileExistsAtPath:mres] ? mres : [models_dir() stringByAppendingPathComponent:@"materials/src"];
        return [mdir stringByAppendingPathComponent:[NSString stringWithFormat:@"%s/%s_2K-JPG_%s", name, name, AC[kind]]];
    }
}

static void load_materials(void)
{
    enum { NK = 4 };
    u8 **buf = calloc(8 * NK, sizeof *buf);
    int *got = calloc(8 * NK, sizeof *got), *ok = calloc(8, sizeof *ok), n = 0, i;
    MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:MSZ height:MSZ mipmapped:YES];
    id<MTLTexture> ta, tnm;
    td.textureType = MTLTextureType2DArray; td.arrayLength = 8;
    ta = [D newTextureWithDescriptor:td]; tnm = [D newTextureWithDescriptor:td];
    hload_for(8 * NK, ^(int j) { @autoreleasepool {
        buf[j] = malloc((size_t)MSZ * MSZ * 4);
        got[j] = load_img(map_path(MATS[j / NK], j % NK), buf[j], MSZ);
    } });
    hload_for(8, ^(int m) {
        u8 *c = buf[m * NK], *nm = buf[m * NK + 1], *arm = buf[m * NK + 2], *h = buf[m * NK + 3];
        double sum[3] = { 0, 0, 0 };
        int ac = MATS[m][0] >= 'A' && MATS[m][0] <= 'Z';   /* ambientCG: roughness alone, no packed ARM */
        long k;
        if (!got[m * NK] || !got[m * NK + 1]) return;
        for (k = 0; k < (long)MSZ * MSZ; k++) {
            int ch;
            for (ch = 0; ch < 3; ch++) sum[ch] += pow(c[k * 4 + ch] / 255.0, 2.2);
            c[k * 4 + 3] = got[m * NK + 3] ? h[k * 4] : 128;
            if (got[m * NK + 2]) {
                if (ac) { nm[k * 4 + 2] = arm[k * 4]; nm[k * 4 + 3] = 255; }
                else { nm[k * 4 + 2] = arm[k * 4 + 1]; nm[k * 4 + 3] = arm[k * 4]; }
            } else { nm[k * 4 + 2] = 220; nm[k * 4 + 3] = 255; }
        }
        for (k = 0; k < 3; k++) g_mm[m][k] = (float)(sum[k] / ((double)MSZ * MSZ));
        [ta replaceRegion:MTLRegionMake2D(0, 0, MSZ, MSZ) mipmapLevel:0 slice:(NSUInteger)m withBytes:c bytesPerRow:MSZ * 4 bytesPerImage:0];
        [tnm replaceRegion:MTLRegionMake2D(0, 0, MSZ, MSZ) mipmapLevel:0 slice:(NSUInteger)m withBytes:nm bytesPerRow:MSZ * 4 bytesPerImage:0];
        ok[m] = 1;
    });
    for (i = 0; i < 8; i++) { n += ok[i]; if (!ok[i]) fprintf(stderr, "terrain: material %s missing\n", MATS[i]); }
    if (n == 8) {
        id<MTLCommandBuffer> b = [[D newCommandQueue] commandBuffer];
        id<MTLBlitCommandEncoder> e = [b blitCommandEncoder];
        [e generateMipmapsForTexture:ta]; [e generateMipmapsForTexture:tnm];
        [e endEncoding]; [b commit]; [b waitUntilCompleted];
        g_matA = ta; g_matN = tnm; g_mat_state = 1;
    } else g_mat_state = -1;
    for (i = 0; i < 8 * NK; i++) free(buf[i]);
    free(buf); free(got); free(ok);
}

/* the grass's photoscanned leaves (remaster_grass_atlas.py): colour, dry
 * colour, normal and outline, as three slices */
static void load_leaves(void)
{
    static const char *const F[3] = { "grass_blades_col.png", "grass_blades_dry.png", "grass_blades_nma.png" };
    NSString *res = [[NSBundle mainBundle].resourcePath stringByAppendingPathComponent:@"env/textures/grass_blades"];
    NSString *dir = [[NSFileManager defaultManager] fileExistsAtPath:res] ? res
                  : [models_dir() stringByAppendingPathComponent:@"env/polyhaven/textures/grass_blades"];
    MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1024 height:1024 mipmapped:YES];
    u8 *buf = malloc(1024 * 1024 * 4);
    id<MTLTexture> t;
    int i;
    td.textureType = MTLTextureType2DArray; td.arrayLength = 3;
    t = [D newTextureWithDescriptor:td];
    for (i = 0; i < 3; i++) {
        if (!load_img([dir stringByAppendingPathComponent:@(F[i])], buf, 1024)) {
            fprintf(stderr, "terrain: grass leaves missing (%s)\n", F[i]); free(buf); return; }
        [t replaceRegion:MTLRegionMake2D(0, 0, 1024, 1024) mipmapLevel:0 slice:(NSUInteger)i withBytes:buf bytesPerRow:1024 * 4 bytesPerImage:0];
    }
    free(buf);
    {
        id<MTLCommandBuffer> b = [[D newCommandQueue] commandBuffer];
        id<MTLBlitCommandEncoder> e = [b blitCommandEncoder];
        [e generateMipmapsForTexture:t]; [e endEncoding]; [b commit]; [b waitUntilCompleted];
    }
    g_leaves = t;
}

/* ---- setup --------------------------------------------------------------- */

static void setup(void)
{
    NSError *err = nil;
    id<MTLLibrary> lib;
    MTLRenderPipelineDescriptor *pd;
    g_state = -1;
    D = MTLCreateSystemDefaultDevice();
    lib = [D newLibraryWithSource:[NSString stringWithUTF8String:TERSRC] options:nil error:&err];
    if (!lib) { fprintf(stderr, "terrain shader: %s\n", err.localizedDescription.UTF8String); return; }
    pd = [MTLRenderPipelineDescriptor new];
    pd.vertexFunction = [lib newFunctionWithName:@"tvs"];
    pd.fragmentFunction = [lib newFunctionWithName:@"tfs"];
    pd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
    pd.colorAttachments[1].pixelFormat = MTLPixelFormatRGBA16Float;
    pd.colorAttachments[2].pixelFormat = MTLPixelFormatRGBA32Float;
    pd.colorAttachments[3].pixelFormat = MTLPixelFormatRGBA8Unorm;
    pd.colorAttachments[4].pixelFormat = MTLPixelFormatR8Unorm;
    pd.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    g_pipe = [D newRenderPipelineStateWithDescriptor:pd error:&err];
    if (!g_pipe) { fprintf(stderr, "terrain pipeline: %s\n", err.localizedDescription.UTF8String); return; }
    pd.vertexFunction = [lib newFunctionWithName:@"gvs"];
    pd.fragmentFunction = [lib newFunctionWithName:@"gfs"];
    g_gpipe = [D newRenderPipelineStateWithDescriptor:pd error:&err];
    if (!g_gpipe) { fprintf(stderr, "grass pipeline: %s\n", err.localizedDescription.UTF8String); return; }
    g_kgcull = [D newComputePipelineStateWithFunction:[lib newFunctionWithName:@"kgcull"] error:&err];
    if (!g_kgcull) { fprintf(stderr, "grass cull kernel: %s\n", err.localizedDescription.UTF8String); return; }
    for (int q = 0; q < 3; q++) for (int r = 0; r < 3; r++) {
        g_gclump[q][r] = [D newBufferWithLength:48 * (size_t)GCAP options:MTLResourceStorageModePrivate];
        g_gargs[q][r] = [D newBufferWithLength:16 options:MTLResourceStorageModeShared];
    }
    g_kcomp = [D newComputePipelineStateWithFunction:[lib newFunctionWithName:@"kcomp"] error:&err];
    if (!g_kcomp) { fprintf(stderr, "terrain comp kernel: %s\n", err.localizedDescription.UTF8String); return; }
    {
        MTLRenderPipelineDescriptor *rd = [MTLRenderPipelineDescriptor new];
        rd.vertexFunction = [lib newFunctionWithName:@"trvs"];
        rd.fragmentFunction = [lib newFunctionWithName:@"trfs"];
        rd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA16Float;
        rd.colorAttachments[1].pixelFormat = MTLPixelFormatRGBA8Unorm;
        g_rpipe = [D newRenderPipelineStateWithDescriptor:rd error:&err];
        if (!g_rpipe) { fprintf(stderr, "terrain resolve pipeline: %s\n", err.localizedDescription.UTF8String); return; }
    }
    {   /* the tiling noise: four channels of 5-octave value noise, periodic
         * over the texture (64 units, 16 texels a unit) */
        enum { NS = 1024 };
        u8 *px = malloc((size_t)NS * NS * 4);
        hload_for(NS, ^(int y) {
            for (int x = 0; x < NS; x++)
                for (int c = 0; c < 4; c++) {
                    double sum = 0, amp = 0.5, norm = 0;
                    for (int o = 0; o < 5; o++) {
                        int per = 64 << o;
                        double fx = (double)x / NS * per, fy = (double)y / NS * per;
                        int ix = (int)floor(fx), iy = (int)floor(fy);
                        double tx = fx - ix, ty = fy - iy;
                        #define HSH(i, j) ((double)(((unsigned)(((i) % per + per) % per) * 374761393u + (unsigned)(((j) % per + per) % per) * 668265263u + (unsigned)(c * 97 + o * 13) * 2246822519u) * 2654435761u >> 8) / 16777216.0)
                        double sx = tx * tx * (3 - 2 * tx), sy = ty * ty * (3 - 2 * ty);
                        double a0 = HSH(ix, iy) + (HSH(ix + 1, iy) - HSH(ix, iy)) * sx;
                        double a1 = HSH(ix, iy + 1) + (HSH(ix + 1, iy + 1) - HSH(ix, iy + 1)) * sx;
                        #undef HSH
                        sum += amp * (a0 + (a1 - a0) * sy); norm += amp; amp *= 0.5;
                    }
                    px[((long)y * NS + x) * 4 + c] = (u8)fmin(255.0, sum / norm * 255.0);
                }
        });
        MTLTextureDescriptor *nd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:NS height:NS mipmapped:YES];
        g_noise = [D newTextureWithDescriptor:nd];
        [g_noise replaceRegion:MTLRegionMake2D(0, 0, NS, NS) mipmapLevel:0 withBytes:px bytesPerRow:NS * 4];
        free(px);
        id<MTLCommandBuffer> mb = [[D newCommandQueue] commandBuffer];
        id<MTLBlitCommandEncoder> be = [mb blitCommandEncoder];
        [be generateMipmapsForTexture:g_noise]; [be endEncoding]; [mb commit]; [mb waitUntilCompleted];
    }
    g_ksv = [D newComputePipelineStateWithFunction:[lib newFunctionWithName:@"ksunvis"] error:&err];
    g_kgi = [D newComputePipelineStateWithFunction:[lib newFunctionWithName:@"kgi"] error:&err];
    if (!g_ksv || !g_kgi) { fprintf(stderr, "terrain light kernels: %s\n", err.localizedDescription.UTF8String); return; }
    g_lq = [D newCommandQueue];
    pd = [MTLRenderPipelineDescriptor new];
    pd.vertexFunction = [lib newFunctionWithName:@"tsvs"];
    pd.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    g_spipe = [D newRenderPipelineStateWithDescriptor:pd error:&err];
    if (!g_spipe) { fprintf(stderr, "terrain shadow pipeline: %s\n", err.localizedDescription.UTF8String); return; }
    {
        MTLDepthStencilDescriptor *d = [MTLDepthStencilDescriptor new];
        d.depthCompareFunction = MTLCompareFunctionGreater;   /* host_glide.m's reversed depth (Remastered) */
        d.depthWriteEnabled = YES;
        g_ds = [D newDepthStencilStateWithDescriptor:d];
        d.depthCompareFunction = MTLCompareFunctionAlways;
        g_dsall = [D newDepthStencilStateWithDescriptor:d];
    }
    {   /* one patch: 33x33 vertices, 32x32 quads, the diagonals alternating */
        u16 *ix = malloc(sizeof(u16) * PATCH * PATCH * 6);
        int x, y, n = 0;
        for (y = 0; y < PATCH; y++)
            for (x = 0; x < PATCH; x++) {
                u16 a = (u16)(y * 33 + x), b = (u16)(a + 1), c = (u16)(a + 33), d = (u16)(c + 1);
                if ((x + y) & 1) { ix[n++] = a; ix[n++] = b; ix[n++] = d; ix[n++] = a; ix[n++] = d; ix[n++] = c; }
                else { ix[n++] = a; ix[n++] = b; ix[n++] = c; ix[n++] = b; ix[n++] = d; ix[n++] = c; }
            }
        g_ib = [D newBufferWithBytes:ix length:sizeof(u16) * (size_t)n options:MTLResourceStorageModeShared];
        g_nib = n;
        free(ix);
    }
    for (int i = 0; i < 3; i++) g_nodes[i] = [D newBufferWithLength:16 * MAXNODE * 4 options:MTLResourceStorageModeShared];
    for (int i = 0; i < 6; i++) g_snodes[i] = [D newBufferWithLength:16 * MAXNODE options:MTLResourceStorageModeShared];
    {
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Float width:1 height:1 mipmapped:NO];
        float z = 0; u8 zu = 0;
        g_dummy = [D newTextureWithDescriptor:td];
        [g_dummy replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&z bytesPerRow:4];
        td.pixelFormat = MTLPixelFormatR8Uint;
        g_dummyu = [D newTextureWithDescriptor:td];
        [g_dummyu replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&zu bytesPerRow:1];
    }
    g_state = 1;
}

/* ---- the field ----------------------------------------------------------- */

static float bil(const float *a, int w, int h, float gx, float gy)
{
    int i, j; float fx, fy;
    if (gx < 0) gx = 0; if (gy < 0) gy = 0;
    if (gx > w - 1.001f) gx = w - 1.001f; if (gy > h - 1.001f) gy = h - 1.001f;
    i = (int)gx; j = (int)gy; fx = gx - i; fy = gy - j;
    return (a[j * w + i] * (1 - fx) + a[j * w + i + 1] * fx) * (1 - fy) + (a[(j + 1) * w + i] * (1 - fx) + a[(j + 1) * w + i + 1] * fx) * fy;
}

static id<MTLTexture> tex_f32(const float *a, int w, int h)
{
    MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Float width:(NSUInteger)w height:(NSUInteger)h mipmapped:NO];
    id<MTLTexture> t = [D newTextureWithDescriptor:td];
    [t replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)w, (NSUInteger)h) mipmapLevel:0 withBytes:a bytesPerRow:(NSUInteger)w * 4];
    return t;
}

/* the field's slope per sample (the Catmull-Rom curve's: central
 * differences), as half floats a sampler can filter; over the track's own
 * flat triangles (class 1 and 5) from a field smoothed over 1.5 m, so their
 * creases do not show in the light */
static id<MTLTexture> grad_tex(const float *h, int w, int hh, float cell, const u8 *cls)
{
    __fp16 *g = malloc(sizeof(__fp16) * 2 * (size_t)w * hh);
    float *sm = NULL;
    if (cls) {
        static const float K[7] = { 0.035f, 0.11f, 0.215f, 0.28f, 0.215f, 0.11f, 0.035f };
        float *tmp = malloc(sizeof(float) * (size_t)w * hh);
        sm = malloc(sizeof(float) * (size_t)w * hh);
        hload_for(hh, ^(int y) {
            for (int x = 0; x < w; x++) { float a = 0; for (int k = -3; k <= 3; k++) { int xx = x + k < 0 ? 0 : x + k >= w ? w - 1 : x + k; a += K[k + 3] * h[(long)y * w + xx]; } tmp[(long)y * w + x] = a; }
        });
        hload_for(hh, ^(int y) {
            for (int x = 0; x < w; x++) { float a = 0; for (int k = -3; k <= 3; k++) { int yy = y + k < 0 ? 0 : y + k >= hh ? hh - 1 : y + k; a += K[k + 3] * tmp[(long)yy * w + x]; } sm[(long)y * w + x] = a; }
        });
        free(tmp);
    }
    hload_for(hh, ^(int y) {
        for (int x = 0; x < w; x++) {
            long i = (long)y * w + x;
            const float *src = (cls && (cls[i] == 1 || cls[i] == 5)) ? sm : h;
            int x0 = x > 0 ? x - 1 : x, x1 = x < w - 1 ? x + 1 : x, y0 = y > 0 ? y - 1 : y, y1 = y < hh - 1 ? y + 1 : y;
            g[i * 2] = (__fp16)((src[(long)y * w + x1] - src[(long)y * w + x0]) / ((x1 - x0) * cell));
            g[i * 2 + 1] = (__fp16)((src[(long)y1 * w + x] - src[(long)y0 * w + x]) / ((y1 - y0) * cell));
        }
    });
    {
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRG16Float width:(NSUInteger)w height:(NSUInteger)hh mipmapped:NO];
        id<MTLTexture> t = [D newTextureWithDescriptor:td];
        [t replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)w, (NSUInteger)hh) mipmapLevel:0 withBytes:g bytesPerRow:(NSUInteger)w * 4];
        free(g); free(sm);
        return t;
    }
}

static void field_free(tfield *t)
{
    int l;
    free(t->near); free(t->far);
    for (l = 0; l < 16; l++) { free(t->zmin[l]); free(t->zmax[l]); }
    memset(t, 0, sizeof *t);
}

/* read <name>.ter and build the quadtree's bounds; any thread */
static int field_load(tfield *t, const char *name)
{
    NSString *p = [[models_dir() stringByAppendingPathComponent:@"terrain"] stringByAppendingPathComponent:[NSString stringWithFormat:@"%s.ter", name]];
    FILE *f = fopen(p.UTF8String, "rb");
    char mg[4];
    u8 *cls;
    float *nflow, *fflow, *fdep;
    long nn, nf;
    int l, i, j;
    memset(t, 0, sizeof *t);
    if (!f) return 0;
    if (fread(mg, 1, 4, f) != 4 || memcmp(mg, "TER3", 4) ||
        fread(&t->nx0, 4, 1, f) != 1 || fread(&t->ny0, 4, 1, f) != 1 || fread(&t->ncell, 4, 1, f) != 1 ||
        fread(&t->nw, 4, 1, f) != 1 || fread(&t->nh, 4, 1, f) != 1 ||
        fread(&t->fx0, 4, 1, f) != 1 || fread(&t->fy0, 4, 1, f) != 1 || fread(&t->fcell, 4, 1, f) != 1 ||
        fread(&t->fw, 4, 1, f) != 1 || fread(&t->fh, 4, 1, f) != 1) { fclose(f); return 0; }
    nn = (long)t->nw * t->nh; nf = (long)t->fw * t->fh;
    t->near = malloc(sizeof(float) * (size_t)nn); t->far = malloc(sizeof(float) * (size_t)nf);
    cls = malloc((size_t)nn); fflow = malloc(sizeof(float) * (size_t)nf); fdep = malloc(sizeof(float) * (size_t)nf);
    nflow = malloc(sizeof(float) * (size_t)nn);
    if (fread(t->near, 4, (size_t)nn, f) != (size_t)nn || fread(t->far, 4, (size_t)nf, f) != (size_t)nf ||
        fread(cls, 1, (size_t)nn, f) != (size_t)nn || fread(fflow, 4, (size_t)nf, f) != (size_t)nf ||
        fread(fdep, 4, (size_t)nf, f) != (size_t)nf || fread(nflow, 4, (size_t)nn, f) != (size_t)nn ||
        fread(fdep, 1, (size_t)nf, f) != (size_t)nf || fread(cls, 1, 0, f) != 0) {
        fclose(f); free(cls); free(fflow); free(fdep); free(nflow); field_free(t); return 0;
    }
    fclose(f);
    snprintf(t->name, sizeof t->name, "%s", name);
    t->tn = tex_f32(t->near, t->nw, t->nh);
    t->tf = tex_f32(t->far, t->fw, t->fh);
    t->tnflow = tex_f32(nflow, t->nw, t->nh);
    t->tng = grad_tex(t->near, t->nw, t->nh, t->ncell, cls);
    t->tfg = grad_tex(t->far, t->fw, t->fh, t->fcell, NULL);
    {   /* the far heights as half floats, for filtered reads (light, shadow) */
        __fp16 *hh = malloc(sizeof(__fp16) * (size_t)nf);
        long q;
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR16Float width:(NSUInteger)t->fw height:(NSUInteger)t->fh mipmapped:NO];
        for (q = 0; q < nf; q++) hh[q] = (__fp16)t->far[q];
        t->tfh = [D newTextureWithDescriptor:td];
        [t->tfh replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)t->fw, (NSUInteger)t->fh) mipmapLevel:0 withBytes:hh bytesPerRow:(NSUInteger)t->fw * 2];
        free(hh);
    }
    t->tfflow = tex_f32(fflow, t->fw, t->fh);
    {
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Uint width:(NSUInteger)t->nw height:(NSUInteger)t->nh mipmapped:NO];
        t->tcls = [D newTextureWithDescriptor:td];
        [t->tcls replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)t->nw, (NSUInteger)t->nh) mipmapLevel:0 withBytes:cls bytesPerRow:(NSUInteger)t->nw];
    }
    {   /* the far class bytes (read into fdep's buffer): 1 open water */
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Uint width:(NSUInteger)t->fw height:(NSUInteger)t->fh mipmapped:NO];
        t->tfcls = [D newTextureWithDescriptor:td];
        [t->tfcls replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)t->fw, (NSUInteger)t->fh) mipmapLevel:0 withBytes:fdep bytesPerRow:(NSUInteger)t->fw];
    }
    {   /* the distance to the road (read into cls's buffer, after it is uploaded) */
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm width:(NSUInteger)t->nw height:(NSUInteger)t->nh mipmapped:NO];
        u8 *rd = malloc((size_t)nn);
        FILE *g = fopen(p.UTF8String, "rb");
        long at = 4 + 40 + (long)nn * 4 + (long)nf * 4 + nn + (long)nf * 8 + (long)nn * 4 + nf;
        if (g && fseek(g, at, SEEK_SET) == 0 && fread(rd, 1, (size_t)nn, g) == (size_t)nn) {
            t->troad = [D newTextureWithDescriptor:td];
            [t->troad replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)t->nw, (NSUInteger)t->nh) mipmapLevel:0 withBytes:rd bytesPerRow:(NSUInteger)t->nw];
        }
        if (g) fclose(g);
        free(rd);
    }
    free(cls); free(fflow); free(fdep); free(nflow);
    /* the quadtree: the root covers the far grid; leaves are LEAF metres */
    t->rx0 = t->fx0; t->ry0 = t->fy0;
    {
        float ext = fmaxf((t->fw - 1) * t->fcell, (t->fh - 1) * t->fcell);
        int lv = 0; float s = LEAF;
        while (s < ext) { s *= 2; lv++; }
        t->rsize = s; t->levels = lv + 1;
    }
    /* height bounds of the 32 m patches (level BLVL), sampled every 2 m; the
     * finer levels take their 32 m ancestor's (with the relief's margin) */
    {
        int d0 = 1 << (t->levels - 1 - BLVL);
        const float L32 = LEAF * (1 << BLVL);
        t->dim[BLVL] = d0;
        t->zmin[BLVL] = malloc(sizeof(float) * (size_t)d0 * d0); t->zmax[BLVL] = malloc(sizeof(float) * (size_t)d0 * d0);
        hload_for(d0, ^(int jj) {
            for (int ii = 0; ii < d0; ii++) {
                float x0 = t->rx0 + ii * L32, y0 = t->ry0 + jj * L32, lo = 1e30f, hi = -1e30f;
                for (int b = 0; b <= 16; b++)
                    for (int a = 0; a <= 16; a++) {
                        float x = x0 + a * (L32 / 16), y = y0 + b * (L32 / 16), z;
                        float gx = (x - t->nx0) / t->ncell, gy = (y - t->ny0) / t->ncell;
                        if (gx >= 0 && gy >= 0 && gx <= t->nw - 1.001f && gy <= t->nh - 1.001f) z = bil(t->near, t->nw, t->nh, gx, gy);
                        else z = bil(t->far, t->fw, t->fh, (x - t->fx0) / t->fcell, (y - t->fy0) / t->fcell);
                        if (z < lo) lo = z; if (z > hi) hi = z;
                    }
                /* the curve between the samples, and the ground's relief */
                t->zmin[BLVL][jj * d0 + ii] = lo - 3.0f; t->zmax[BLVL][jj * d0 + ii] = hi + 3.0f;
            }
        });
    }
    for (l = BLVL + 1; l < t->levels; l++) {
        int d = t->dim[l - 1] / 2;
        t->dim[l] = d;
        t->zmin[l] = malloc(sizeof(float) * (size_t)d * d); t->zmax[l] = malloc(sizeof(float) * (size_t)d * d);
        for (j = 0; j < d; j++)
            for (i = 0; i < d; i++) {
                const float *a = t->zmin[l - 1], *b = t->zmax[l - 1];
                int D2 = t->dim[l - 1], o = 2 * j * D2 + 2 * i;
                t->zmin[l][j * d + i] = fminf(fminf(a[o], a[o + 1]), fminf(a[o + D2], a[o + D2 + 1]));
                t->zmax[l][j * d + i] = fmaxf(fmaxf(b[o], b[o + 1]), fmaxf(b[o + D2], b[o + D2 + 1]));
            }
    }
    t->ok = 1;
    fprintf(stderr, "terrain: %s: near %dx%d at %.0f m, far %dx%d at %.0f m, %d levels\n", name, t->nw, t->nh,
            t->ncell, t->fw, t->fh, t->fcell, t->levels);
    return 1;
}

static void scatter_load(const char *name);
/* BR_TER_CPU=1: CPU time in the field's work, per frame (ms), every 240 frames */
#include <mach/mach_time.h>
static double g_cpu_acc[4]; static int g_cpu_n;
static double now_ms(void) { static mach_timebase_info_data_t tb; if (!tb.denom) mach_timebase_info(&tb); return (double)mach_absolute_time() * tb.numer / tb.denom / 1e6; }
void hter_cpu_frame(void)
{
    static int on = -1;
    if (on < 0) on = getenv("BR_TER_CPU") != NULL;
    if (!on) return;
    if (++g_cpu_n == 240) {
        fprintf(stderr, "tercpu: seam %.2f scatter %.2f shadow %.2f resolve %.2f ms/frame\n", g_cpu_acc[0] / 240, g_cpu_acc[1] / 240, g_cpu_acc[2] / 240, g_cpu_acc[3] / 240);
        memset(g_cpu_acc, 0, sizeof g_cpu_acc); g_cpu_n = 0;
    }
}
static void comp_bake(int snow);
/* the race's weather (host_fx.m's BR_FX_WEATHER override too, so the ground
 * matches the sky it is lit under) */
static int ter_weather(void)
{
    const char *ov = getenv("BR_FX_WEATHER");
    return ov ? atoi(ov) : (int)H32(0x104B15E8u);
}

/* host_env.m, as a track's placements are applied: its terrain, if it has
 * one -- the hidden ground merged into the env's lists now (before the
 * instances are registered), the field loaded on a queue */
void hter_track(const char *name)
{
    NSString *p;
    FILE *f;
    char ln[1 << 16];
    if (off_env()) return;
    if (!g_state) setup();
    if (g_state != 1) return;
    if (g_t.ok && !strcmp(g_t.name, name)) goto hides;
    if (g_t.ok) field_free(&g_t);
    /* loaded here, while the game loads the track: the ground is hidden
     * only once the field that replaces it is in memory */
    {
        tfield t;
        if (g_mat_state == 0) { load_materials(); load_leaves(); }
        if (field_load(&t, name)) { g_t = t; g_lsun[2] = -9.0f; scatter_load(name); comp_bake(ter_weather() == 3); }
        else return;
    }
hides:
    p = [[models_dir() stringByAppendingPathComponent:@"placements"] stringByAppendingPathComponent:[NSString stringWithFormat:@"%s.terrain", name]];
    if (!(f = fopen(p.UTF8String, "r"))) return;
    {
        int nh = 0;
        while (fgets(ln, sizeof ln, f)) {
            int inst, n = 0, cap = 64; char *s, *e; int *lst;
            if (strncmp(ln, "hide ", 5)) continue;
            inst = (int)strtol(ln + 5, &e, 10); s = e;
            lst = malloc(sizeof(int) * (size_t)cap);
            for (;;) {
                long o = strtol(s, &e, 10);
                if (e == s) break;
                s = e;
                if (n == cap) lst = realloc(lst, sizeof(int) * (size_t)(cap *= 2));
                lst[n++] = (int)o;
            }
            henv_add_hide(inst, lst, n);
            nh += n;
            free(lst);
        }
        fclose(f);
        fprintf(stderr, "terrain: %s: %d ground triangles handed to the field\n", name, nh);
    }
}

/* is the field drawing (it hides ground only when it is) */
int hter_ready(void)
{
    return g_state == 1 && g_t.ok && g_mat_state == 1 && hfx_on() && !off_env();
}

/* ---- the quadtree, per view ------------------------------------------------ */

typedef struct { float o[4]; } tnode;

static int frustum_out(const float *P, float x0, float y0, float z0, float x1, float y1, float z1)
{
    /* clip space of the 8 corners: outside if all past one plane */
    int k, out[5] = { 0, 0, 0, 0, 0 };
    for (k = 0; k < 8; k++) {
        float x = k & 1 ? x1 : x0, y = k & 2 ? y1 : y0, z = k & 4 ? z1 : z0, c[4];
        int j;
        for (j = 0; j < 4; j++) c[j] = x * P[j] + y * P[4 + j] + z * P[8 + j] + P[12 + j];
        if (c[0] < -c[3]) out[0]++;
        if (c[0] > c[3]) out[1]++;
        if (c[1] < -c[3]) out[2]++;
        if (c[1] > c[3]) out[3]++;
        if (c[3] < 0.1f) out[4]++;
    }
    for (k = 0; k < 5; k++) if (out[k] == 8) return 1;
    return 0;
}

static float box_dist(const float *e, float x0, float y0, float z0, float x1, float y1, float z1)
{
    float dx = fmaxf(fmaxf(x0 - e[0], 0), e[0] - x1), dy = fmaxf(fmaxf(y0 - e[1], 0), e[1] - y1), dz = fmaxf(fmaxf(z0 - e[2], 0), e[2] - z1);
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

static int g_nsel;
static void select_node(const tfield *t, const float *P, const float *eye, float k0, int lvl, int i, int j, tnode *out, int cull, int minlvl)
{
    float size = LEAF * (float)(1 << lvl);
    float x0 = t->rx0 + i * size, y0 = t->ry0 + j * size;
    int bl = lvl < BLVL ? BLVL : lvl, sh = bl - lvl, bi = i >> sh, bj = j >> sh;
    int d = t->dim[bl];
    float z0, z1;
    if (bi >= d || bj >= d || g_nsel >= MAXNODE) return;
    z0 = t->zmin[bl][bj * d + bi]; z1 = t->zmax[bl][bj * d + bi];
    if (cull && frustum_out(P, x0, y0, z0, x0 + size, y0 + size, z1)) return;
    if (lvl <= minlvl || box_dist(eye, x0, y0, z0, x0 + size, y0 + size, z1) > k0 * (float)(1 << (lvl - 1))) {
        tnode *n = &out[g_nsel++];
        n->o[0] = x0; n->o[1] = y0; n->o[2] = size; n->o[3] = (float)lvl;
        return;
    }
    select_node(t, P, eye, k0, lvl - 1, 2 * i, 2 * j, out, cull, minlvl);
    select_node(t, P, eye, k0, lvl - 1, 2 * i + 1, 2 * j, out, cull, minlvl);
    select_node(t, P, eye, k0, lvl - 1, 2 * i, 2 * j + 1, out, cull, minlvl);
    select_node(t, P, eye, k0, lvl - 1, 2 * i + 1, 2 * j + 1, out, cull, minlvl);
}

static void inv_eye(const float *P, float *eye)
{
    double a[4][8];
    int i, j, k;
    for (i = 0; i < 4; i++) for (j = 0; j < 8; j++) a[i][j] = j < 4 ? P[i * 4 + j] : (j - 4 == i);
    for (i = 0; i < 4; i++) {
        int p = i; double t;
        for (k = i + 1; k < 4; k++) if (fabs(a[k][i]) > fabs(a[p][i])) p = k;
        for (j = 0; j < 8; j++) { t = a[i][j]; a[i][j] = a[p][j]; a[p][j] = t; }
        t = a[i][i]; if (t == 0) t = 1e-30;
        for (j = 0; j < 8; j++) a[i][j] /= t;
        for (k = 0; k < 4; k++) if (k != i) { t = a[k][i]; for (j = 0; j < 8; j++) a[k][j] -= t * a[i][j]; }
    }
    for (j = 0; j < 3; j++) eye[j] = (float)(a[2][j + 4] / a[2][7]);
}

static float g_k0 = 9.0f * LEAF;          /* level 0 (12.5 cm) holds out to this; each level twice the last */
static unsigned g_drawn_serial = ~0u; static float g_drawn_P[4][16]; static int g_ndrawn;
static int g_ring;

/* ---- the landscape's trees and rocks (placements/<track>.scatter) ---- */
#define SCH 32.0f                           /* chunk side, metres */
#define SNM 64                              /* models at most */
#define SRING (1 << 18)                     /* instances a frame can draw */
typedef struct {
    int n, nm, model[SNM], card[SNM], tree[SNM], heavy[SNM], small[SNM], tris[SNM][3];
    float *M;                               /* n world matrices, sorted by chunk then model */
    int cw, ch; float cx0, cy0;             /* the chunk grid */
    int *cstart;                            /* per chunk: first instance, per model: cw*ch*nm counts follow */
    u16 *ccount;
    float *czmin, *czmax;
    id<MTLBuffer> ring[3], sring[6];
    int ri, sri;
    id<MTLTexture> canopy; float cmap[4];
} tscatter;
static tscatter g_sc;
static int g_cards_only = -1;

static void scatter_free(void)
{
    free(g_sc.M); free(g_sc.cstart); free(g_sc.ccount); free(g_sc.czmin); free(g_sc.czmax);
    g_sc.M = NULL; g_sc.cstart = NULL; g_sc.ccount = NULL; g_sc.czmin = g_sc.czmax = NULL; g_sc.n = 0; g_sc.canopy = nil;
}

static void scatter_load(const char *name)
{
    NSString *p = [[models_dir() stringByAppendingPathComponent:@"placements"] stringByAppendingPathComponent:[NSString stringWithFormat:@"%s.scatter", name]];
    NSData *d = [NSData dataWithContentsOfFile:p];
    const u8 *b; long o = 8, i, n;
    int na, k;
    const char *an[SNM], *vn[SNM];
    char names[SNM][96];
    typedef struct { u16 a, pad; float x, y, z, yaw, h; } rec;
    const rec *r;
    scatter_free();
    if (!d || d.length < 12 || memcmp(d.bytes, "SCT1", 4)) return;
    b = d.bytes;
    memcpy(&na, b + 4, 4);
    if (na > SNM) na = SNM;
    for (k = 0; k < na; k++) {
        char *sp;
        snprintf(names[k], sizeof names[k], "%s", (const char *)b + o);
        o += (long)strlen((const char *)b + o) + 1;
        sp = strchr(names[k], ' ');
        if (sp) *sp = 0;
        an[k] = names[k]; vn[k] = sp ? sp + 1 : "a";
    }
    henv_models_find(na, an, vn, g_sc.model);
    {   /* BR_SC_ONLY=<text>: only the scatter models whose asset names hold it (diagnosis) */
        const char *only = getenv("BR_SC_ONLY");
        if (only && only[0] == '!') { for (k = 0; k < na; k++) if (strstr(an[k], only + 1)) g_sc.model[k] = -1; }   /* !text: all but those */
        else if (only) for (k = 0; k < na; k++) if (!strstr(an[k], only)) g_sc.model[k] = -1;
    }
    g_sc.nm = na;
    for (k = 0; k < na; k++) { g_sc.card[k] = g_sc.model[k] >= 0 && henv_model_card(g_sc.model[k]); g_sc.tree[k] = g_sc.card[k];
                               g_sc.heavy[k] = g_sc.model[k] >= 0 && henv_model_tris(g_sc.model[k], 2) > 60000;
                               g_sc.small[k] = 0;
                               for (int q2 = 0; q2 < 3; q2++) g_sc.tris[k][q2] = henv_model_tris(g_sc.model[k], q2);
                               if (getenv("BR_ENV_STAT")) fprintf(stderr, "terrain: scatter model %s %s: model %d card %d tris %d/%d/%d heavy %d\n", an[k], vn[k], g_sc.model[k], g_sc.card[k],
                                   henv_model_tris(g_sc.model[k], 0), henv_model_tris(g_sc.model[k], 1), henv_model_tris(g_sc.model[k], 2), g_sc.heavy[k]); }
    memcpy(&n, b + o, 4); n = (long)(int)n; o += 4;
    r = (const rec *)(b + o);
    if ((long)d.length < o + n * (long)sizeof(rec)) return;
    /* the chunk grid over the near field */
    g_sc.cx0 = g_t.nx0; g_sc.cy0 = g_t.ny0;
    g_sc.cw = (int)ceilf(g_t.nw * g_t.ncell / SCH); g_sc.ch = (int)ceilf(g_t.nh * g_t.ncell / SCH);
    {
        long nc = (long)g_sc.cw * g_sc.ch, c;
        long *cnt = calloc((size_t)nc * na, sizeof(long)), *pos = calloc((size_t)nc * na + 1, sizeof(long));
        g_sc.cstart = malloc(sizeof(int) * (size_t)(nc + 1));
        g_sc.ccount = calloc((size_t)nc * na, sizeof(u16));
        g_sc.czmin = malloc(sizeof(float) * (size_t)nc); g_sc.czmax = malloc(sizeof(float) * (size_t)nc);
        for (c = 0; c < nc; c++) { g_sc.czmin[c] = 1e30f; g_sc.czmax[c] = -1e30f; }
        for (i = 0; i < n; i++) {
            int ci = (int)((r[i].x - g_sc.cx0) / SCH), cj = (int)((r[i].y - g_sc.cy0) / SCH);
            if (ci < 0 || cj < 0 || ci >= g_sc.cw || cj >= g_sc.ch || r[i].a >= na || g_sc.model[r[i].a] < 0) continue;
            cnt[((long)cj * g_sc.cw + ci) * na + r[i].a]++;
        }
        for (c = 0; c < nc * na; c++) pos[c + 1] = pos[c] + cnt[c];
        g_sc.n = (int)pos[nc * na];
        g_sc.M = malloc(sizeof(float) * 16 * (size_t)(g_sc.n + 1));
        for (c = 0; c < nc; c++) {
            g_sc.cstart[c] = (int)pos[c * na];
            for (k = 0; k < na; k++) g_sc.ccount[c * na + k] = (u16)(cnt[c * na + k] > 65535 ? 65535 : cnt[c * na + k]);
        }
        g_sc.cstart[nc] = g_sc.n;
        {   /* undergrowth (ferns and the like, under 2.5 m): drawn near only */
            float hmax[SNM] = { 0 };
            for (i = 0; i < n; i++) if (r[i].a < na && r[i].h > hmax[r[i].a]) hmax[r[i].a] = r[i].h;
            for (k = 0; k < na; k++) g_sc.small[k] = g_sc.tree[k] && hmax[k] > 0.0f && hmax[k] < 2.5f;
        }
        memset(cnt, 0, sizeof(long) * (size_t)nc * na);
        for (i = 0; i < n; i++) {
            int ci = (int)((r[i].x - g_sc.cx0) / SCH), cj = (int)((r[i].y - g_sc.cy0) / SCH);
            long cc, slot;
            float s, cy, sy, *M;
            if (ci < 0 || cj < 0 || ci >= g_sc.cw || cj >= g_sc.ch || r[i].a >= na || g_sc.model[r[i].a] < 0) continue;
            cc = (long)cj * g_sc.cw + ci;
            slot = pos[cc * na + r[i].a] + cnt[cc * na + r[i].a]++;
            s = r[i].h / fmaxf(henv_model_h(g_sc.model[r[i].a]), 0.01f);
            if (!g_sc.tree[r[i].a]) s = r[i].h;          /* rocks: the record is a scale */
            cy = cosf(r[i].yaw); sy = sinf(r[i].yaw);
            M = g_sc.M + slot * 16;
            if (g_sc.tree[r[i].a] || r[i].pad == 1) {
                M[0] = cy * s; M[1] = sy * s; M[2] = 0; M[3] = 0;
                M[4] = -sy * s; M[5] = cy * s; M[6] = 0; M[7] = 0;
                M[8] = 0; M[9] = 0; M[10] = s; M[11] = 0;
            } else {
                /* a rock lies on its slope: up along the ground's normal */
                float gx = (r[i].x - g_t.nx0) / g_t.ncell, gy = (r[i].y - g_t.ny0) / g_t.ncell, e = 2.0f;
                float hx = bil(g_t.near, g_t.nw, g_t.nh, gx + e, gy) - bil(g_t.near, g_t.nw, g_t.nh, gx - e, gy);
                float hy = bil(g_t.near, g_t.nw, g_t.nh, gx, gy + e) - bil(g_t.near, g_t.nw, g_t.nh, gx, gy - e);
                float n[3] = { -hx, -hy, 2 * e * g_t.ncell }, l = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
                float xa[3] = { cy, sy, 0 }, d, ya[3];
                n[0] /= l; n[1] /= l; n[2] /= l;
                d = xa[0] * n[0] + xa[1] * n[1];
                xa[0] -= n[0] * d; xa[1] -= n[1] * d; xa[2] -= n[2] * d;
                l = sqrtf(xa[0] * xa[0] + xa[1] * xa[1] + xa[2] * xa[2]); xa[0] /= l; xa[1] /= l; xa[2] /= l;
                ya[0] = n[1] * xa[2] - n[2] * xa[1]; ya[1] = n[2] * xa[0] - n[0] * xa[2]; ya[2] = n[0] * xa[1] - n[1] * xa[0];
                M[0] = xa[0] * s; M[1] = xa[1] * s; M[2] = xa[2] * s; M[3] = 0;
                M[4] = ya[0] * s; M[5] = ya[1] * s; M[6] = ya[2] * s; M[7] = 0;
                M[8] = n[0] * s; M[9] = n[1] * s; M[10] = n[2] * s; M[11] = 0;
            }
            M[12] = r[i].x; M[13] = r[i].y; M[14] = r[i].z; M[15] = 1;
            if (r[i].z < g_sc.czmin[cc]) g_sc.czmin[cc] = r[i].z;
            if (r[i].z + r[i].h > g_sc.czmax[cc]) g_sc.czmax[cc] = r[i].z + (g_sc.tree[r[i].a] ? r[i].h : 3.0f);
        }
        /* each chunk's instances of a model in random order: a prefix of the
         * run is then a random thinning (the far cards) */
        for (c = 0; c < nc * na; c++) {
            long a0 = pos[c], n2 = pos[c + 1] - pos[c], q;
            for (q = n2 - 1; q > 0; q--) {
                long j = (long)(((unsigned long)(a0 + q) * 2654435761ul >> 7) % (unsigned long)(q + 1));
                float tmp[16];
                memcpy(tmp, g_sc.M + (a0 + q) * 16, 64); memcpy(g_sc.M + (a0 + q) * 16, g_sc.M + (a0 + j) * 16, 64); memcpy(g_sc.M + (a0 + j) * 16, tmp, 64);
            }
        }
        free(cnt); free(pos);
    }
    for (k = 0; k < 3; k++) if (!g_sc.ring[k]) g_sc.ring[k] = [D newBufferWithLength:64 * (size_t)SRING options:MTLResourceStorageModeShared];
    for (k = 0; k < 6; k++) if (!g_sc.sring[k]) g_sc.sring[k] = [D newBufferWithLength:64 * (size_t)(SRING / 4) options:MTLResourceStorageModeShared];
    /* the forest's crowns (the ground under them, the forest past the cards) */
    {
        NSString *cp = [[models_dir() stringByAppendingPathComponent:@"terrain"] stringByAppendingPathComponent:[NSString stringWithFormat:@"%s.canopy", name]];
        FILE *f = fopen(cp.UTF8String, "rb");
        float x0, y0, cell; int w, h;
        if (f && fscanf(f, "CANOPY1 %f %f %f %d %d", &x0, &y0, &cell, &w, &h) == 5 && fgetc(f) == '\n' && w > 0 && h > 0) {
            u8 *px = malloc((size_t)w * h);
            if (fread(px, 1, (size_t)w * h, f) == (size_t)w * h) {
                MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm width:(NSUInteger)w height:(NSUInteger)h mipmapped:NO];
                g_sc.canopy = [D newTextureWithDescriptor:td];
                [g_sc.canopy replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)w, (NSUInteger)h) mipmapLevel:0 withBytes:px bytesPerRow:(NSUInteger)w];
                g_sc.cmap[0] = x0; g_sc.cmap[1] = y0; g_sc.cmap[2] = 1.0f / (w * cell); g_sc.cmap[3] = 1.0f / (h * cell);
            }
            free(px);
        }
        if (f) fclose(f);
    }
    fprintf(stderr, "terrain: %s: %d trees and rocks in %d x %d chunks\n", name, g_sc.n, g_sc.cw, g_sc.ch);
}

/* the instances to draw for a view (or a shadow cascade): per chunk in reach,
 * by its distance, a level per model -- meshes near, the trees' cards far --
 * copied into `buf`; the sets returned in `sets` */
static int scatter_select(const float *P, const float *eye, float range, int shadow, id<MTLBuffer> buf, long cap, henv_set *sets, int maxsets)
{
    enum { NLV = 4 };                       /* levels 0-2 meshes, 3 the card */
    long cnt[SNM][NLV], off[SNM][NLV], used = 0;
    int ci0, ci1, cj0, cj1, ci, cj, k, l, ns = 0;
    float *out = buf.contents;
    if (!g_sc.n) return 0;
    if (g_cards_only < 0) g_cards_only = getenv("BR_TER_CARDSONLY") != NULL;
    memset(cnt, 0, sizeof cnt);
    ci0 = (int)floorf((eye[0] - range - g_sc.cx0) / SCH); ci1 = (int)floorf((eye[0] + range - g_sc.cx0) / SCH);
    cj0 = (int)floorf((eye[1] - range - g_sc.cy0) / SCH); cj1 = (int)floorf((eye[1] + range - g_sc.cy0) / SCH);
    if (ci0 < 0) ci0 = 0; if (cj0 < 0) cj0 = 0;
    if (ci1 >= g_sc.cw) ci1 = g_sc.cw - 1; if (cj1 >= g_sc.ch) cj1 = g_sc.ch - 1;
    /* two passes: count, then copy */
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 1) {
            for (k = 0; k < g_sc.nm; k++) for (l = 0; l < NLV; l++) {
                if (used + cnt[k][l] > cap) cnt[k][l] = cap - used > 0 ? cap - used : 0;
                off[k][l] = used; used += cnt[k][l]; cnt[k][l] = 0;
            }
        }
        for (cj = cj0; cj <= cj1; cj++)
            for (ci = ci0; ci <= ci1; ci++) {
                long c = (long)cj * g_sc.cw + ci;
                float x0 = g_sc.cx0 + ci * SCH, y0 = g_sc.cy0 + cj * SCH, d;
                int first;
                if (g_sc.cstart[c + 1] == g_sc.cstart[c]) continue;
                d = box_dist(eye, x0, y0, g_sc.czmin[c], x0 + SCH, y0 + SCH, g_sc.czmax[c]);
                if (d > range) continue;
                if (P && frustum_out(P, x0, y0, g_sc.czmin[c], x0 + SCH, y0 + SCH, g_sc.czmax[c])) continue;
                first = g_sc.cstart[c];
                for (k = 0; k < g_sc.nm; k++) {
                    int n = g_sc.ccount[c * g_sc.nm + k], nd;
                    if (!n) continue;
                    /* far cards thin out: the canopy under them carries the forest */
                    nd = n;
                    if (!shadow && g_sc.tree[k] && d > 250.0f) {
                        float f = d < 450.0f ? 0.5f : d < 700.0f ? 0.33f : 0.2f;
                        nd = (int)ceilf(n * f);
                    }
                    if (g_sc.tree[k] && !shadow && d > 900.0f) { first += n; continue; }
                    if (g_sc.small[k]) {
                        if (d > (shadow ? 25.0f : 60.0f)) { first += n; continue; }
                        nd = n;
                        l = shadow ? 3 : d < 5.0f ? 0 : d < 10.0f ? 1 : 3;
                    } else if (g_sc.tree[k]) {
                        l = shadow || g_cards_only ? 3 : d < 12.0f ? 0 : d < 20.0f ? 1 : d < 30.0f ? 2 : 3;
                        while (l < 3 && g_sc.tris[k][l] > 60000) l++;   /* a level over the triangle budget: the next, or the card */
                    } else { l = shadow ? 2 : d < 8.0f ? 0 : d < 25.0f ? 1 : 2; if (!shadow && d > 350.0f) { first += n; continue; } }
                    if (pass == 0) cnt[k][l] += nd;
                    else {
                        long dst = off[k][l] + cnt[k][l];
                        if (dst + nd <= cap) { memcpy(out + dst * 16, g_sc.M + (long)first * 16, sizeof(float) * 16 * (size_t)nd); cnt[k][l] += nd; }
                    }
                    first += n;
                }
            }
    }
    for (k = 0; k < g_sc.nm; k++) for (l = 0; l < NLV; l++) {
        long n = cnt[k][l];
        if (off[k][l] + n > cap) n = cap - off[k][l];
        if (n <= 0 || ns == maxsets) continue;
        sets[ns].model = g_sc.model[k]; sets[ns].lod = l; sets[ns].first = off[k][l]; sets[ns].n = (int)n; ns++;
    }
    return ns;
}

/* the light maps for the sun now in the sky: solved on the GPU on a queue
 * of their own (about a second), swapped in when done; host_fx.m reads them */
/* the material weights, baked over both grids from the composition rules
 * (kcomp): at load, and whenever snow comes or goes */
static void comp_bake(int snow)
{
    tu_t u;
    id<MTLCommandBuffer> cb;
    id<MTLComputeCommandEncoder> ce;
    MTLTextureDescriptor *td;
    int far;
    memset(&u, 0, sizeof u);
    u.nd[0] = g_t.nx0; u.nd[1] = g_t.ny0; u.nd[2] = g_t.ncell; u.ns[0] = (float)g_t.nw; u.ns[1] = (float)g_t.nh;
    u.fd[0] = g_t.fx0; u.fd[1] = g_t.fy0; u.fd[2] = g_t.fcell; u.fs[0] = (float)g_t.fw; u.fs[1] = (float)g_t.fh;
    u.wx[0] = snow ? 1.0f : 0.0f;
    if (g_sc.canopy) memcpy(u.cmap, g_sc.cmap, sizeof u.cmap);
    cb = [g_lq commandBuffer];
    ce = [cb computeCommandEncoder];
    [ce setComputePipelineState:g_kcomp];
    [ce setTexture:g_t.tn atIndex:0]; [ce setTexture:g_t.tf atIndex:1];
    [ce setTexture:g_t.tnflow atIndex:3]; [ce setTexture:g_t.tfflow atIndex:4];
    [ce setTexture:g_sc.canopy ? g_sc.canopy : g_dummy atIndex:7];
    [ce setTexture:g_t.tng atIndex:8]; [ce setTexture:g_t.tfg atIndex:9];
    [ce setTexture:g_t.troad ? g_t.troad : g_dummy atIndex:22];
    [ce setBytes:&u length:sizeof u atIndex:0];
    for (far = 0; far < 2; far++) {
        int w = far ? g_t.fw : g_t.nw, h = far ? g_t.fh : g_t.nh;
        float kp[4] = { (float)far, 0, 0, 0 };
        id<MTLTexture> t0, t1;
        td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:(NSUInteger)w height:(NSUInteger)h mipmapped:YES];
        td.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
        t0 = [D newTextureWithDescriptor:td]; t1 = [D newTextureWithDescriptor:td];
        [ce setTexture:t0 atIndex:20]; [ce setTexture:t1 atIndex:21];
        [ce setBytes:kp length:sizeof kp atIndex:1];
        [ce dispatchThreads:MTLSizeMake((NSUInteger)w, (NSUInteger)h, 1) threadsPerThreadgroup:MTLSizeMake(16, 16, 1)];
        if (far) { g_t.cf0 = t0; g_t.cf1 = t1; } else { g_t.cn0 = t0; g_t.cn1 = t1; }
    }
    [ce endEncoding];
    {
        id<MTLBlitCommandEncoder> be = [cb blitCommandEncoder];
        [be generateMipmapsForTexture:g_t.cn0]; [be generateMipmapsForTexture:g_t.cn1];
        [be generateMipmapsForTexture:g_t.cf0]; [be generateMipmapsForTexture:g_t.cf1];
        [be endEncoding];
    }
    [cb commit]; [cb waitUntilCompleted];
    g_comp_snow = snow;
}

typedef struct { float g[4], fd[4], fs[4], sun[4], sunc[4], skyc[4], misc[4]; } lu_t;
static void light_solve(const float *sun, const float *sunc, const float *skyc, float snow)
{
    lu_t lu;
    MTLTextureDescriptor *td;
    id<MTLTexture> sv, gi;
    id<MTLCommandBuffer> cb;
    id<MTLComputeCommandEncoder> ce;
    MTLSize grid = MTLSizeMake(LMAP, LMAP, 1), tg = MTLSizeMake(16, 16, 1);
    float cx = g_t.fx0 + (g_t.fw - 1) * g_t.fcell * 0.5f, cy = g_t.fy0 + (g_t.fh - 1) * g_t.fcell * 0.5f;
    memset(&lu, 0, sizeof lu);
    lu.g[0] = cx - LHALF; lu.g[1] = cy - LHALF; lu.g[2] = 2 * LHALF / LMAP; lu.g[3] = LMAP;
    lu.fd[0] = g_t.fx0; lu.fd[1] = g_t.fy0; lu.fd[2] = g_t.fcell; lu.fs[0] = (float)g_t.fw; lu.fs[1] = (float)g_t.fh;
    memcpy(lu.sun, sun, 12); memcpy(lu.sunc, sunc, 12); memcpy(lu.skyc, skyc, 12); lu.misc[0] = snow;
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR16Float width:LMAP height:LMAP mipmapped:NO];
    td.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    sv = [D newTextureWithDescriptor:td];
    td.pixelFormat = MTLPixelFormatRGBA16Float;
    gi = [D newTextureWithDescriptor:td];
    cb = [g_lq commandBuffer];
    ce = [cb computeCommandEncoder];
    [ce setComputePipelineState:g_ksv];
    [ce setTexture:g_t.tfh atIndex:0]; [ce setTexture:sv atIndex:1];
    [ce setBytes:&lu length:sizeof lu atIndex:0];
    [ce dispatchThreads:grid threadsPerThreadgroup:tg];
    [ce setComputePipelineState:g_kgi];
    [ce setTexture:g_t.tfh atIndex:0]; [ce setTexture:sv atIndex:1]; [ce setTexture:g_t.tfcls atIndex:2]; [ce setTexture:gi atIndex:3];
    [ce setBytes:&lu length:sizeof lu atIndex:0];
    [ce dispatchThreads:grid threadsPerThreadgroup:tg];
    [ce endEncoding];
    g_lbusy = 1;
    {
        double t0 = hframe_game_ms();
        float s0 = sun[0], s1 = sun[1], s2 = sun[2];   /* the caller's copy is gone by completion */
        [cb addCompletedHandler:^(id<MTLCommandBuffer> b) {
            (void)b;
            g_t.tsv = sv; g_t.tgi = gi;
            g_lbusy = 0;
            fprintf(stderr, "terrain: light solved (sun %.2f %.2f %.2f)\n", s0, s1, s2);
            (void)t0;
        }];
    }
    [cb commit];
}

/* host_fx.m: the landscape's light, if solved: the sun-visibility and light
 * maps, the far heights; p: map x0 y0 texel side, far x0 y0 cell on, far w h */
int hter_light(float p[3][4], id<MTLTexture> *sv, id<MTLTexture> *gi, id<MTLTexture> *fh)
{
    if (!hter_ready() || !g_t.tsv || !g_t.tgi) return 0;
    {
        float cx = g_t.fx0 + (g_t.fw - 1) * g_t.fcell * 0.5f, cy = g_t.fy0 + (g_t.fh - 1) * g_t.fcell * 0.5f;
        p[0][0] = cx - LHALF; p[0][1] = cy - LHALF; p[0][2] = 2 * LHALF / LMAP; p[0][3] = LMAP;
    }
    p[1][0] = g_t.fx0; p[1][1] = g_t.fy0; p[1][2] = g_t.fcell; p[1][3] = 1;
    p[2][0] = (float)g_t.fw; p[2][1] = (float)g_t.fh; p[2][2] = 0; p[2][3] = 0;
    *sv = g_t.tsv; *gi = g_t.tgi; *fh = g_t.tfh;
    return 1;
}

/* host_fx.m, before its first pass reads the G-buffer: the field's look,
 * once per pixel it covers (the scene pass left only where it is) */
static void hter_resolve_(id<MTLCommandBuffer> cb, id<MTLTexture> gn, id<MTLTexture> ga, id<MTLTexture> gp, id<MTLTexture> gm);
void hter_resolve(id<MTLCommandBuffer> cb, id<MTLTexture> gn, id<MTLTexture> ga, id<MTLTexture> gp, id<MTLTexture> gm)
{ double t0 = now_ms(); hter_resolve_(cb, gn, ga, gp, gm); g_cpu_acc[3] += now_ms() - t0; hter_cpu_frame(); }
static void hter_resolve_(id<MTLCommandBuffer> cb, id<MTLTexture> gn, id<MTLTexture> ga, id<MTLTexture> gp, id<MTLTexture> gm)
{
    MTLRenderPassDescriptor *rp;
    id<MTLRenderCommandEncoder> e;
    tu_t u;
    if (!g_lastu_ok || !hter_ready() || !g_rpipe || getenv("BR_TER_NORESOLVE")) return;
    g_lastu_ok = 0;
    u = g_lastu;
    rp = [MTLRenderPassDescriptor renderPassDescriptor];
    rp.colorAttachments[0].texture = gn; rp.colorAttachments[0].loadAction = MTLLoadActionLoad; rp.colorAttachments[0].storeAction = MTLStoreActionStore;
    rp.colorAttachments[1].texture = ga; rp.colorAttachments[1].loadAction = MTLLoadActionLoad; rp.colorAttachments[1].storeAction = MTLStoreActionStore;
    hfx_prof_attach(rp, "terrain resolve");
    e = [cb renderCommandEncoderWithDescriptor:rp];
    [e setRenderPipelineState:g_rpipe];
    [e setFragmentBytes:&u length:sizeof u atIndex:0];
    [e setFragmentTexture:g_matA atIndex:5]; [e setFragmentTexture:g_matN atIndex:6];
    [e setFragmentTexture:g_sc.canopy ? g_sc.canopy : g_dummy atIndex:7];
    [e setFragmentTexture:g_t.tng atIndex:8]; [e setFragmentTexture:g_t.tfg atIndex:9];
    [e setFragmentTexture:g_t.cn0 atIndex:11]; [e setFragmentTexture:g_t.cn1 atIndex:12];
    [e setFragmentTexture:g_t.cf0 atIndex:13]; [e setFragmentTexture:g_t.cf1 atIndex:14];
    [e setFragmentTexture:g_noise atIndex:15];
    [e setFragmentTexture:gp atIndex:16]; [e setFragmentTexture:gm atIndex:17];
    [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [e endEncoding];
    /* the next frame's grass: the clumps of both rings in view of this one */
    if (g_kgcull && !getenv("BR_TER_NOGRASS")) {
        /* cell, side, inner, outer, leaves, height, segments, fade from, fade to:
         * the first two are one grid (the same clumps), its leaves curved in
         * five segments within 8 m and three beyond */
        static const float RING[3][9] = {
            { 0.13f, 126.0f, 0.0f, 8.0f, 12.0f, 1.0f, 5.0f, 16.5f, 22.0f },
            { 0.13f, 340.0f, 8.0f, 22.0f, 12.0f, 1.0f, 3.0f, 16.5f, 22.0f },
            { 0.42f, 300.0f, 20.0f, 62.0f, 8.0f, 1.1f, 2.0f, 46.5f, 62.0f } };
        int slot = (g_gring + 1) % 3, r;
        id<MTLComputeCommandEncoder> ce = [cb computeCommandEncoder];
        [ce setComputePipelineState:g_kgcull];
        [ce setBytes:&u length:sizeof u atIndex:0];
        [ce setTexture:g_t.tn atIndex:0]; [ce setTexture:g_t.tcls atIndex:2];
        [ce setTexture:g_t.tng atIndex:8]; [ce setTexture:g_t.tfg atIndex:9];
        [ce setTexture:g_t.cn0 atIndex:11]; [ce setTexture:g_t.cn1 atIndex:12];
        [ce setTexture:g_t.cf0 atIndex:13]; [ce setTexture:g_t.cf1 atIndex:14];
        [ce setTexture:g_noise atIndex:15];
        for (r = 0; r < 3; r++) {
            float *gu = g_grass_gu[r];
            u32 *args = g_gargs[slot][r].contents, cap = GCAP;
            float cell = RING[r][0], side = RING[r][1];
            gu[0] = cell; gu[1] = side; gu[2] = RING[r][2]; gu[3] = RING[r][3];
            gu[4] = floorf(u.eye[0] / cell - side * 0.5f) * cell; gu[5] = floorf(u.eye[1] / cell - side * 0.5f) * cell;
            gu[6] = RING[r][7]; gu[7] = RING[r][8];
            gu[8] = RING[r][4]; gu[9] = RING[r][6] * 6.0f; gu[10] = RING[r][5]; gu[11] = RING[r][6];
            args[0] = (u32)(RING[r][4] * RING[r][6] * 6.0f); args[1] = 0; args[2] = 0; args[3] = 0;
            [ce setBytes:gu length:sizeof(float) * 12 atIndex:1];
            [ce setBuffer:g_gclump[slot][r] offset:0 atIndex:2];
            [ce setBuffer:g_gargs[slot][r] offset:0 atIndex:3];
            [ce setBytes:&cap length:4 atIndex:4];
            [ce dispatchThreads:MTLSizeMake((NSUInteger)side, (NSUInteger)side, 1) threadsPerThreadgroup:MTLSizeMake(16, 16, 1)];
        }
        [ce endEncoding];
        g_gring = slot;
    }
}

/* native/render.m, at every depth-buffered triangle of a 3D view: the
 * field, once per view, before the first */
static void hter_seam_(void);
void hter_seam(void) { double t0 = now_ms(); hter_seam_(); g_cpu_acc[0] += now_ms() - t0; }
static void hter_seam_(void)
{
    id<MTLDevice> dev;
    id<MTLRenderCommandEncoder> e;
    MTLScissorRect sc;
    int origin_ll, fogmode, rw, rh, view, main, i;
    float fogc[4], P[16];
    tu_t u;
    tnode *nodes;
    if (!g_t.ok || !hter_ready()) return;
    /* native/render.m's view: 0 the camera, 1 flat 2D (nothing to draw), 2 the mirror */
    if (getenv("BR_TER_TRACE") && hglide_swaps() % 600 == 300) fprintf(stderr, "trace: frame %u seam view %d\n", hglide_swaps(), hrender_view());
    if ((view = hrender_view()) == 1) return;
    for (i = 0; i < 16; i++) P[i] = W_LD(f32, PROJ, 4 * i);
    if (g_drawn_serial != hglide_swaps()) { g_drawn_serial = hglide_swaps(); g_ndrawn = 0; }
    for (i = 0; i < g_ndrawn; i++) if (!memcmp(g_drawn_P[i], P, sizeof P)) return;
    if (g_ndrawn < 4) memcpy(g_drawn_P[g_ndrawn++], P, sizeof P);
    memset(&u, 0, sizeof u);
    e = hglide_native_pass(&dev, &sc, &origin_ll, &fogmode, fogc, u.fogtab, &rw, &rh);
    if (!e) return;
    hfx_jitter(&u.jit[0], &u.jit[1], rw, rh);
    hglide_map(view, u.map);
    sc = hglide_scissor(view);
    memcpy(u.P, P, sizeof P);
    u.vpt[0] = W_LD(f32, 0x105CCD48u, 0); u.vpt[1] = W_LD(f32, 0x105CD9F8u, 0);
    u.vpt[2] = W_LD(f32, 0x105CCFDCu, 0); u.vpt[3] = W_LD(f32, 0x105CD9FCu, 0);
    inv_eye(P, u.eye);
    main = view == 0;
    u.misc[0] = (float)origin_ll; u.misc[1] = (float)main; u.misc[2] = (float)(fogmode & 1); u.misc[3] = hglide_bake_ref();
    u.nd[0] = g_t.nx0; u.nd[1] = g_t.ny0; u.nd[2] = g_t.ncell; u.ns[0] = (float)g_t.nw; u.ns[1] = (float)g_t.nh;
    u.fd[0] = g_t.fx0; u.fd[1] = g_t.fy0; u.fd[2] = g_t.fcell; u.fs[0] = (float)g_t.fw; u.fs[1] = (float)g_t.fh;
    if (!hfx_car_rig(u.sun, u.sunc, u.skyc, u.grnd)) {
        u.sun[0] = 0.57f; u.sun[1] = 0.57f; u.sun[2] = 0.6f;
        u.sunc[0] = 1.6f; u.sunc[1] = 1.45f; u.sunc[2] = 1.2f;
        u.skyc[0] = 0.4f; u.skyc[1] = 0.48f; u.skyc[2] = 0.62f;
        u.grnd[0] = 0.34f; u.grnd[1] = 0.28f; u.grnd[2] = 0.21f;
    }
    for (i = 0; i < 3; i++) u.fogc[i] = powf(fogc[i] / 255.0f, 2.2f);
    if (main) {
        int w = ter_weather();
        if ((w == 3) != g_comp_snow) comp_bake(w == 3);
    }
    if (main && !g_lbusy && g_ksv) {
        float dd = u.sun[0] * g_lsun[0] + u.sun[1] * g_lsun[1] + u.sun[2] * g_lsun[2];
        if (dd < 0.9995f) {
            int w = ter_weather();
            memcpy(g_lsun, u.sun, sizeof g_lsun);
            light_solve(u.sun, u.sunc, u.skyc, w == 3 ? 1.0f : 0.0f);
        }
    }
    {
        int w = (int)H32(0x104B15E8u);
        const char *ov = getenv("BR_FX_WEATHER");
        if (ov) w = atoi(ov);
        u.wx[0] = w == 3 ? 1.0f : 0.0f;
        u.wx[1] = w == 2 || w == 4 ? 1.0f : 0.0f;
        u.wx[2] = (float)fmod(hframe_game_ms(), 1e6);
        u.wx[3] = getenv("BR_TER_DEBUG") ? (float)atoi(getenv("BR_TER_DEBUG")) : 0.0f;
    }
    u.lod[0] = g_k0; u.lod[1] = 0.7f; u.lod[2] = LEAF;
    if (getenv("BR_TER_FOG")) { static int fl; if (fl++ % 600 == 0) { fprintf(stderr, "terrain fog mode %d colour %.0f %.0f %.0f table:", fogmode, fogc[0], fogc[1], fogc[2]);
        for (i = 0; i < 64; i += 4) fprintf(stderr, " w%.0f=%.0f", exp2f(3.0f + (float)(i >> 2)) / (float)(8 - (i & 3)), u.fogtab[i]); fprintf(stderr, "\n"); } }
    memcpy(u.mm, g_mm, sizeof u.mm);
    {
        const unsigned char *px; int cw, ch, gen; float cx0, cy0, ccell;
        static id<MTLTexture> ct; static int cgen = -1;
        if (g_sc.canopy) {
            memcpy(u.cmap, g_sc.cmap, sizeof u.cmap);
            [e setFragmentTexture:g_sc.canopy atIndex:7]; [e setVertexTexture:g_sc.canopy atIndex:7];
        } else if (henv_canopy(&px, &cw, &ch, &cx0, &cy0, &ccell, &gen)) {
            if (gen != cgen || !ct) {
                MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm width:(NSUInteger)cw height:(NSUInteger)ch mipmapped:NO];
                ct = [D newTextureWithDescriptor:td];
                [ct replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)cw, (NSUInteger)ch) mipmapLevel:0 withBytes:px bytesPerRow:(NSUInteger)cw];
                cgen = gen;
            }
            u.cmap[0] = cx0; u.cmap[1] = cy0; u.cmap[2] = 1.0f / (cw * ccell); u.cmap[3] = 1.0f / (ch * ccell);
            [e setFragmentTexture:ct atIndex:7]; [e setVertexTexture:ct atIndex:7];
        } else { u.cmap[2] = 0; [e setFragmentTexture:g_dummy atIndex:7]; [e setVertexTexture:g_dummy atIndex:7]; }
    }
    g_ring = (g_ring + 1) % 3;
    nodes = (tnode *)g_nodes[g_ring].contents + 0;
    {
        static int slot;   /* views within one frame each take their own part of the ring buffer */
        size_t base = (size_t)(slot++ & 3) * MAXNODE;
        nodes += base;
        g_nsel = 0;
        select_node(&g_t, P, u.eye, main ? g_k0 : g_k0 * 0.5f, g_t.levels - 1, 0, 0, nodes, 1, main ? 0 : 2);
        if (getenv("BR_TER_STAT") && main) { static int nl; if (nl++ % 240 == 0) fprintf(stderr, "terrain: patches %d (%d tris)\n", g_nsel, g_nsel * PATCH * PATCH * 2); }
        if (getenv("BR_TER_TRACE") && hglide_swaps() % 600 == 300) {
            int q;
            fprintf(stderr, "trace: frame %u terrain drawn view %d patches %d map %.4f %.4f %.4f %.4f vpt %.2f %.2f %.2f %.2f ll %d\n", hglide_swaps(), view, g_nsel,
                    u.map[0], u.map[1], u.map[2], u.map[3], u.vpt[0], u.vpt[1], u.vpt[2], u.vpt[3], origin_ll);
            for (q = 0; q < g_nsel && q < 40; q++) {
                float x = nodes[q].o[0] + nodes[q].o[2] * 0.5f, y = nodes[q].o[1] + nodes[q].o[2] * 0.5f, z, c[4];
                float gx = (x - g_t.nx0) / g_t.ncell, gy = (y - g_t.ny0) / g_t.ncell;
                int j;
                if (gx >= 0 && gy >= 0 && gx < g_t.nw - 1 && gy < g_t.nh - 1) z = bil(g_t.near, g_t.nw, g_t.nh, gx, gy);
                else z = bil(g_t.far, g_t.fw, g_t.fh, (x - g_t.fx0) / g_t.fcell, (y - g_t.fy0) / g_t.fcell);
                for (j = 0; j < 4; j++) c[j] = x * P[j] + y * P[4 + j] + z * P[8 + j] + P[12 + j];
                {
                    float X = u.vpt[0] * c[0] + u.vpt[1] * c[3], Y = u.vpt[2] * c[1] + u.vpt[3] * c[3];
                    float Yd = u.misc[0] > 0.5f ? 480.0f * c[3] - Y : Y;
                    fprintf(stderr, "trace:   node %.0f %.0f size %.0f lvl %.0f z %.1f clip %.1f %.1f %.1f %.1f ndc %.3f %.3f\n",
                            nodes[q].o[0], nodes[q].o[1], nodes[q].o[2], nodes[q].o[3], z, c[0], c[1], c[2], c[3],
                            (u.map[0] * X + u.map[1] * c[3]) / c[3], (u.map[2] * Yd + u.map[3] * c[3]) / c[3]);
                }
            }
        }
        if (!g_nsel) return;
        [e setRenderPipelineState:g_pipe];
        [e setDepthStencilState:u.wx[3] == 9 ? g_dsall : g_ds];
        [e setScissorRect:sc];
        [e setCullMode:MTLCullModeNone];
        [e setVertexBytes:&u length:sizeof u atIndex:1];
        [e setVertexBuffer:g_nodes[g_ring] offset:base * sizeof(tnode) atIndex:2];
        [e setVertexTexture:g_t.tn atIndex:0];
        [e setVertexTexture:g_t.tf atIndex:1];
        [e setVertexTexture:g_t.tcls atIndex:2];
        [e setVertexTexture:g_t.tnflow atIndex:3];
        [e setVertexTexture:g_t.tfflow atIndex:4];
        [e setVertexTexture:g_matA atIndex:5];
        [e setVertexTexture:g_t.tng atIndex:8];
        [e setVertexTexture:g_t.tfg atIndex:9];
        [e setFragmentTexture:g_t.tng atIndex:8];
        [e setFragmentTexture:g_t.tfg atIndex:9];
        [e setFragmentTexture:g_t.tfcls atIndex:10];
        {
            id<MTLTexture> ct[5] = { g_t.cn0, g_t.cn1, g_t.cf0, g_t.cf1, g_noise };
            for (int q = 0; q < 5; q++) { [e setVertexTexture:ct[q] atIndex:(NSUInteger)(11 + q)]; [e setFragmentTexture:ct[q] atIndex:(NSUInteger)(11 + q)]; }
        }
        if (main) { g_lastu = u; g_lastu_ok = 1; }
        [e setFragmentBytes:&u length:sizeof u atIndex:0];
        [e setFragmentTexture:g_t.tn atIndex:0];
        [e setFragmentTexture:g_t.tf atIndex:1];
        [e setFragmentTexture:g_t.tcls atIndex:2];
        [e setFragmentTexture:g_t.tnflow atIndex:3];
        [e setFragmentTexture:g_t.tfflow atIndex:4];
        [e setFragmentTexture:g_matA atIndex:5];
        [e setFragmentTexture:g_matN atIndex:6];
        [e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:(NSUInteger)g_nib indexType:MTLIndexTypeUInt16
                     indexBuffer:g_ib indexBufferOffset:0 instanceCount:(NSUInteger)g_nsel];
        /* the meadows' grass, in the main view: the clumps the last frame's cull
         * found in view (hter_resolve), drawn from its list */
        if (main && !getenv("BR_TER_NOGRASS") && g_gring >= 0 && g_leaves) {
            int r;
            [e setRenderPipelineState:g_gpipe];
            [e setFragmentTexture:g_leaves atIndex:0];
            for (r = 0; r < 3; r++) {
                [e setVertexBytes:g_grass_gu[r] length:sizeof g_grass_gu[r] atIndex:3];
                [e setVertexBuffer:g_gclump[g_gring][r] offset:0 atIndex:4];
                [e drawPrimitives:MTLPrimitiveTypeTriangle indirectBuffer:g_gargs[g_gring][r] indirectBufferOffset:0];
            }
        }
        /* the forests and rocks (host_env.m draws them) */
        if (!getenv("BR_TER_NOSCATTER") && g_sc.n) {
            henv_set sets[SNM * 4];
            int ns;
            g_sc.ri = (g_sc.ri + 1) % 3;
            double ts = now_ms();
            ns = scatter_select(P, u.eye, main ? 1500.0f : 300.0f, 0, g_sc.ring[g_sc.ri], SRING, sets, SNM * 4);
            henv_draw_sets(sets, ns, g_sc.ring[g_sc.ri]);
            g_cpu_acc[1] += now_ms() - ts;
        }
    }
}

/* host_fx.m's shadow pass, per cascade: the field within `range` of the eye,
 * coarser than the view's own (the shadow map's texels are larger) */
static void hter_shadow_(id<MTLRenderCommandEncoder> e, const float *svp, const float *eye, float range);
void hter_shadow(id<MTLRenderCommandEncoder> e, const float *svp, const float *eye, float range)
{ double t0 = now_ms(); hter_shadow_(e, svp, eye, range); g_cpu_acc[2] += now_ms() - t0; }
static void hter_shadow_(id<MTLRenderCommandEncoder> e, const float *svp, const float *eye, float range)
{
    static int ring;
    su_t s;
    tnode *nodes;
    float Pinf[16];
    int i, n = 0;
    if (!hter_ready()) return;
    ring = (ring + 1) % 6;
    nodes = (tnode *)g_snodes[ring].contents;
    /* every node within range, at a level by its distance */
    memset(Pinf, 0, sizeof Pinf);
    g_nsel = 0;
    select_node(&g_t, Pinf, eye, g_k0 * 0.5f, g_t.levels - 1, 0, 0, nodes, 0, BLVL);
    for (i = 0; i < g_nsel; i++) {
        float cx = nodes[i].o[0] + nodes[i].o[2] * 0.5f - eye[0], cy = nodes[i].o[1] + nodes[i].o[2] * 0.5f - eye[1];
        if (sqrtf(cx * cx + cy * cy) - nodes[i].o[2] * 0.71f < range) nodes[n++] = nodes[i];
    }
    if (g_sc.n && !getenv("BR_TER_NOSCATTER") && !getenv("BR_SH_NOSCAT")) {
        henv_set sets[SNM * 4];
        int ns;
        g_sc.sri = (g_sc.sri + 1) % 6;
        ns = scatter_select(NULL, eye, range, 1, g_sc.sring[g_sc.sri], SRING / 4, sets, SNM * 4);
        henv_shadow_sets(e, svp, sets, ns, g_sc.sring[g_sc.sri]);
    }
    if (!n || getenv("BR_SH_NOTER")) return;
    memcpy(s.svp, svp, sizeof s.svp);
    s.nd[0] = g_t.nx0; s.nd[1] = g_t.ny0; s.nd[2] = g_t.ncell; s.ns[0] = (float)g_t.nw; s.ns[1] = (float)g_t.nh;
    s.fd[0] = g_t.fx0; s.fd[1] = g_t.fy0; s.fd[2] = g_t.fcell; s.fs[0] = (float)g_t.fw; s.fs[1] = (float)g_t.fh;
    [e setRenderPipelineState:g_spipe];
    [e setVertexBytes:&s length:sizeof s atIndex:1];
    [e setVertexBuffer:g_snodes[ring] offset:0 atIndex:2];
    [e setVertexTexture:g_t.tn atIndex:0];
    [e setVertexTexture:g_t.tf atIndex:1];
    [e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:(NSUInteger)g_nib indexType:MTLIndexTypeUInt16
                 indexBuffer:g_ib indexBufferOffset:0 instanceCount:(NSUInteger)n];
}
