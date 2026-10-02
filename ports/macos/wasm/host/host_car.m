/* host_car.m -- the Remastered player car (port code).
 *
 * A modern model of the player's car -- body, and one wheel drawn four
 * times -- replaces the .rca model while the Remastered renderer is on
 * (host_fx.m; ~ switches back to the original, model and all), for every car
 * given the ES: the player's in its paint, the others in the AI's.  native/car.m
 * keeps the game drawing the car exactly as before with its lists emptied,
 * and queues a marker where the car sits in the display list; hcar_draw runs
 * when the list reaches it.
 *
 *   placement   the body by the car's own world matrix (car+0x00), each wheel
 *               by the game's matrix for that wheel (car+0x40.. +0x100: spin,
 *               steer and suspension), read when the list was built, like the
 *               game's own matrix slots.  The projection is the one current
 *               when the list runs, so the rear-view mirror works unchanged.
 *   depth       the Voodoo W-buffer word the Glide shader writes, from the same
 *               clip-space w, so the model and the game's scene share one
 *               depth buffer.
 *   shading     metal/roughness PBR from the model's maps, a clear coat over
 *               the paint, the baked occlusion and normal maps, the light rig
 *               host_fx.m uses for the weather, and
 *               the livery: separate projected decal textures laid over the
 *               painted surfaces only (the base colour's alpha marks paint),
 *               from the side, top, front and back (remaster_livery.py makes
 *               them from the original car's own ribbons).
 *               In the main view the result goes to the fx composite as a
 *               pre-lit surface (G-buffer class 4), which adds the sun's
 *               shadows, bloom and the tone map; in the mirror it is finished
 *               here.  The body casts sun shadows through a low-poly proxy.
 *   damage      the original dents a car by moving its model's vertices
 *               (BrRippleApply); each frame the displacement of every one of
 *               them is carried onto the body and the glass through
 *               remaster_dent.py's mapping, with a finer crumple, the paint
 *               scuffed through to primer and metal, and the glass crazed.
 *
 * Assets: <dir>/car.cfg, body.rcm, wheel.rcm, proxy.rcm, glass.rcm and their textures,
 * from ports/macos/tools/remaster_car.py.  <dir> is $BR_REMASTER_DIR, the
 * app's Resources/remaster, or ports/common/models/es/pack (the models are
 * kept out of git).  BR_CAR=0 turns it off.
 */
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
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
void hglide_map(int view, float T[4]);                 /* host_glide.m: the screen map */
MTLScissorRect hglide_scissor(int view);
int hrender_view(void);                                /* native/render.m */
void hfx_shadow_batch(id<MTLBuffer> buf, size_t off, int n, id<MTLTexture> tex, id<MTLSamplerState> smp,
                      int at_fn, int at_ref, int use_tex, float su, float sv);

#define STR(...) #__VA_ARGS__
static const char *CARSRC = "#include <metal_stdlib>\n" STR(
using namespace metal;
struct MV { packed_float3 p; packed_float3 n; float2 uv; float4 t; };
struct CU {
  float4x4 M, P, HP;
  float4 vpt, eye, sun, sunc, skyc, grnd, fogc, misc, liv, livs, paint, dbg, hvpt, hist, glass, jit, livf;
  float4 map, hmap;   /* host_glide.m's screen map of this view, and of the last frame's */
  float fogtab[64];
};
struct VO { float4 pos [[position]]; float3 wp; float3 lp; float3 wn; float3 wt; float tw; float2 uv; float oow; float dent; };
/* the dents (remaster_dent.py): each vertex follows eight of the original
   model's vertices, and D holds how far each of those has been knocked */
struct DM { ushort i[8]; float w[8]; };
float hash13(float3 p) { p = fract(p * 0.1031); p += dot(p, p.zyx + 31.32); return fract((p.x + p.y) * p.z); }
float vnoise(float3 p) {
  float3 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
  return mix(mix(mix(hash13(i), hash13(i + float3(1, 0, 0)), f.x), mix(hash13(i + float3(0, 1, 0)), hash13(i + float3(1, 1, 0)), f.x), f.y),
             mix(mix(hash13(i + float3(0, 0, 1)), hash13(i + float3(1, 0, 1)), f.x), mix(hash13(i + float3(0, 1, 1)), hash13(i + float3(1, 1, 1)), f.x), f.y), f.z); }
vertex VO cvs(uint vid [[vertex_id]], const device MV *v [[buffer(0)]], constant CU &u [[buffer(1)]],
               const device DM *dm [[buffer(2)]], const device float4 *D [[buffer(3)]]) {
  MV m = v[vid]; VO o;
  float3 p = m.p; float dent = 0;
  if (u.jit.z > 0.5) {                     /* the original's dent here, plus a crumple finer than its mesh */
    DM d = dm[vid]; float3 disp = 0;
    for (int k = 0; k < 8; k++) disp += d.w[k] * D[d.i[k]].xyz;
    dent = length(disp);
    p += disp + float3(m.n) * (vnoise(p * 14.0) - 0.5) * 2.0 * dent * 0.35;
  }
  float4 w = u.M * float4(p, 1);
  float4 c = u.P * w;
  float X = u.vpt.x * c.x + u.vpt.y * c.w, Y = u.vpt.z * c.y + u.vpt.w * c.w;
  float Yd = u.misc.x > 0.5 ? 480.0 * c.w - Y : Y;
  o.pos = float4(u.map.x * X + u.map.y * c.w, u.map.z * Yd + u.map.w * c.w, (c.z + c.w) * 0.5, c.w);
  o.pos.xy += u.jit.xy * c.w;              /* the scene's anti-aliasing jitter (host_fx.m's TAA) */
  o.wp = w.xyz; o.lp = p; o.dent = dent;
  o.wn = (u.M * float4(m.n, 0)).xyz; o.wt = (u.M * float4(m.t.xyz, 0)).xyz; o.tw = m.t.w;
  o.uv = m.uv; o.oow = 1.0 / c.w;
  return o; }
struct FO { float4 c [[color(0)]]; float4 n [[color(1)]]; float4 g [[color(2)]]; float4 a [[color(3)]]; float d [[depth(any)]]; };
uint wfloat(float oow) {
  if (oow >= 1.0) return 0;
  if (oow <= 0.0) return 0xFFFF;
  uint t = uint(min(oow * 4294967296.0, 4294967295.0));
  if (t == 0) return 0xFFFF;
  int e = int(clz(t));
  uint m = e <= 19 ? (~t >> uint(19 - e)) : (~t << uint(e - 19));
  uint w = (uint(e) << 12) | (m & 0xFFF);
  return w < 0xFFFF ? w + 1 : w; }
float fogof(constant CU &u, float w) {
  if (w <= 1.0) return u.fogtab[0]; float prev = 0, pw = 1;
  for (int i = 0; i < 64; i++) { float tw = exp2(3.0 + float(i >> 2)) / float(8 - (i & 3));
    if (w <= tw) return prev + (u.fogtab[i] - prev) * (w - pw) / (tw - pw);
    prev = u.fogtab[i]; pw = tw; }
  return u.fogtab[63]; }
float D_ggx(float nh, float a) { float a2 = a * a, d = nh * nh * (a2 - 1.0) + 1.0; return a2 / (3.14159 * d * d); }
float V_sg(float nl, float nv, float a) { float k = a * 0.5; return 0.25 / ((nl * (1 - k) + k) * (nv * (1 - k) + k)); }
float3 sky_at(constant CU &u, float3 d) {
  float h = d.z;
  float3 c = h > 0 ? mix(u.skyc.rgb * 1.25, u.skyc.rgb * 0.8, pow(saturate(h), 0.6)) : mix(u.skyc.rgb * 1.1, u.grnd.rgb * 0.7, saturate(-h * 4.0));
  float g = saturate(dot(d, u.sun.xyz));
  return c + u.sunc.rgb * (pow(g, 800.0) * 40.0 + pow(g, 16.0) * 0.25) * u.sun.w; }
/* what a reflection ray sees: the last finished frame where the ray lands
   on it (the ground for rays that point down, the distance for the rest),
   the sky model where it does not */
float3 env_at(constant CU &u, texture2d<float> h, float3 wp, float3 R, float rough) {
  constexpr sampler hs(filter::linear, mip_filter::linear, address::clamp_to_edge);
  float3 sky = sky_at(u, R);
  if (u.hist.x < 0.5 || u.dbg.z > 0.5 && u.dbg.z < 1.5) return sky;
  if (u.dbg.z > 1.5) rough = max(rough, 0.6);
  float t = R.z < -0.02 ? clamp((u.hist.w - wp.z) / R.z, 0.3, 400.0) : 400.0;
  float4 c = u.HP * float4(wp + R * t, 1);
  if (c.w <= 0.05) return sky;
  float sx = u.hvpt.x * c.x / c.w + u.hvpt.y, sy = u.hvpt.z * c.y / c.w + u.hvpt.w;
  float yd = u.hist.y > 0.5 ? 480.0 - sy : sy;
  float2 q = float2(u.hmap.x * sx + u.hmap.y, u.hmap.z * yd + u.hmap.w) * float2(0.5, -0.5) + 0.5;
  float2 e = smoothstep(0.0, 0.12, q) * smoothstep(0.0, 0.12, 1.0 - q);
  float3 f = pow(h.sample(hs, q, level(rough * 7.0)).rgb, 2.2) / u.hist.z;
  return mix(sky, f, e.x * e.y); }
float3 aces(float3 x) { return saturate(x * (2.51 * x + 0.03) / (x * (2.43 * x + 0.59) + 0.14)); }
fragment FO cfs(VO in [[stage_in]], constant CU &u [[buffer(0)]],
                texture2d<float> tb [[texture(0)]], texture2d<float> tm [[texture(1)]], texture2d<float> tn [[texture(2)]],
                texture2d<float> ls [[texture(3)]], texture2d<float> lt [[texture(4)]], texture2d<float> th [[texture(5)]],
                texture2d<float> lf [[texture(6)]], texture2d<float> lr [[texture(7)]]) {
  constexpr sampler s(filter::linear, mip_filter::linear, address::repeat, max_anisotropy(8));
  constexpr sampler sc(filter::linear, mip_filter::linear, address::clamp_to_zero, max_anisotropy(8));
  FO o; o.a = 0;   /* no baked-light surface colour: the car is lit here */
  float4 B = tb.sample(s, in.uv);
  float3 alb = pow(B.rgb, 2.2);
  float paint = u.paint.w > 0.5 ? B.a : 0.0;
  float4 MR = tm.sample(s, in.uv);                 /* occlusion, roughness, metal */
  float rough = clamp(MR.g, 0.04, 1.0), metal = MR.b;
  /* the baked normal map: tangent space, MikkTSpace tangents from the bake */
  float3 nm = tn.sample(s, in.uv).xyz * 2.0 - 1.0;
  float3 N0 = normalize(in.wn);
  float3 T = in.wt - N0 * dot(N0, in.wt);
  T = length(T) > 1e-6 ? normalize(T) : float3(1, 0, 0);
  float3 Bt = cross(N0, T) * (in.tw < 0 ? -1.0 : 1.0);
  float3 N = normalize(T * nm.x + Bt * nm.y + N0 * nm.z);
  float ao = MR.r, so = saturate(ao * 1.3 - 0.1);                   /* ambient and specular occlusion */
  /* the stored normals face out (the screen mapping mirrors y, so the
     rasteriser's facing says nothing); an inside surface seen through the
     glass is the only one that faces away from the eye */
  if (dot(N0, u.eye.xyz - in.wp) < 0) { N = -N; N0 = -N0; }
  /* a dent: the surface as it now is (the facets the knock left), and the
     paint scuffed through to primer and bare metal */
  float dk = saturate(in.dent * 8.0);
  if (dk > 0.0) {
    float3 ng = normalize(cross(dfdy(in.wp), dfdx(in.wp)));
    if (dot(ng, N0) < 0) ng = -ng;
    N0 = normalize(mix(N0, ng, dk)); N = normalize(mix(N, ng, dk));
  }
  /* paint: one colour and the livery over it; the livery is projected in the
     car's own frame, sides from +-y and the top from +z */
  if (paint > 0.0) {
    float3 base = u.paint.rgb;
    float3 ln = normalize((transpose(u.M) * float4(N0, 0)).xyz);
    float2 us = float2((in.lp.x - u.liv.x) / u.liv.y, 1.0 - (in.lp.z - u.liv.z) / u.liv.w);
    float2 ut = float2((in.lp.x - u.liv.x) / u.liv.y, (in.lp.y - u.livs.x) / u.livs.y);
    float2 uf = float2((u.livf.x + u.livf.y - in.lp.y) / u.livf.y, us.y);   /* the front, seen from ahead */
    float2 ur = float2((in.lp.y - u.livf.x) / u.livf.y, us.y);             /* the back, seen from behind */
    float ws = pow(abs(ln.y), 3.0), wt = pow(saturate(ln.z), 3.0) * smoothstep(u.livs.z, u.livs.w, in.lp.z);
    float wf = pow(saturate(ln.x), 3.0), wr = pow(saturate(-ln.x), 3.0);
    float sw = ws + wt + wf + wr + 1e-4;
    float4 L = (ls.sample(sc, us) * ws + lt.sample(sc, ut) * wt + lf.sample(sc, uf) * wf + lr.sample(sc, ur) * wr) / sw;
    L.rgb = pow(L.rgb, 2.2);
    base = mix(base, L.rgb, L.a * saturate((ws + wt + wf + wr) * 3.0));
    alb = mix(alb, base, paint);
    rough = mix(rough, 0.32, paint); metal = mix(metal, 0.0, paint);
    float sc2 = smoothstep(0.01, 0.08, in.dent) * paint;
    if (sc2 > 0.0) {
      float scr = smoothstep(0.55, 0.75, vnoise(in.lp * float3(60, 60, 25)) * 0.7 + vnoise(in.lp * 9.0) * 0.5);
      alb = mix(alb, mix(float3(0.22, 0.22, 0.21), float3(0.55, 0.55, 0.56), scr), sc2 * (0.35 + 0.65 * scr));
      metal = mix(metal, scr, sc2); rough = mix(rough, 0.55, sc2);
    }
  }
  float3 V = normalize(u.eye.xyz - in.wp), L = u.sun.xyz, H = normalize(L + V);
  float nl = saturate(dot(N, L)), nv = max(dot(N, V), 1e-3), nh = saturate(dot(N, H)), vh = saturate(dot(V, H));
  float a = rough * rough;
  float3 F0 = mix(float3(0.04), alb, metal);
  float3 F = F0 + (1.0 - F0) * pow(1.0 - vh, 5.0);
  float3 spec = D_ggx(nh, a) * V_sg(nl, nv, a) * F;
  float3 kd = (1.0 - F) * (1.0 - metal);
  float3 sunl = u.sunc.rgb * nl;
  float3 amb = mix(u.grnd.rgb, u.skyc.rgb, N.z * 0.5 + 0.5) * ao;
  float3 R = reflect(-V, N);
  float3 Fr = F0 + (max(float3(1.0 - rough), F0) - F0) * pow(1.0 - nv, 5.0);
  float3 env = mix(amb, env_at(u, th, in.wp, R, rough) * so, 1.0 - rough) * Fr;
  float3 col = alb * kd * (sunl + amb) + spec * sunl + env;
  /* clear coat on the paint */
  if (paint > 0.0) {
    float Fc = 0.04 + 0.96 * pow(1.0 - nv, 5.0);
    float3 Hc = H; float nhc = saturate(dot(N0, Hc));
    float3 cs = D_ggx(nhc, 0.03 * 0.03 + 0.002) * V_sg(saturate(dot(N0, L)), max(dot(N0, V), 1e-3), 0.03) * (0.04 + 0.96 * pow(1.0 - vh, 5.0)) * u.sunc.rgb * saturate(dot(N0, L));
    float3 Rc = reflect(-V, N0);
    col = col * (1.0 - Fc * paint) + (env_at(u, th, in.wp, Rc, 0.02) * Fc * so + cs) * paint * u.misc.w;
  }
  if (u.dbg.x > 0.5) {
    int m = int(u.dbg.x);
    col = m == 6 ? alb * kd * (sunl + amb) : m == 7 ? env : m == 8 ? spec * sunl : m == 9 ? float3(0.02, 0.2, 0.05) :
          m == 1 ? pow(B.rgb, 2.2) * 2.0 : m == 2 ? float3(B.a) * 2.0 : m == 3 ? float3(ao) * 2.0 : m == 4 ? (N0 * 0.5 + 0.5) * 2.0 : float3(MR.g, MR.b, 0) * 2.0;
  } else if (u.fogc.w > 0.5) col = mix(col, u.fogc.rgb, fogof(u, 1.0 / in.oow) / 255.0);
  o.d = float(wfloat(in.oow)) / 65536.0;
  if (u.misc.y > 0.5) {                        /* main view: pre-lit, to the fx composite */
    o.c = float4(pow(saturate(col / 4.0), 1.0 / 2.2), 1);
    o.n = float4(N0, 1); o.g = float4(in.wp, 4.0);
  } else {                                     /* another view: finished here */
    o.c = float4(pow(aces(col * u.misc.z), 1.0 / 2.2), 1);
    o.n = float4(0, 0, 0, 1); o.g = float4(0);
  }
  return o; }
/* the windows: tinted glass, the scene and the sun reflected by Fresnel,
   blended over what is behind (premultiplied); it writes no depth and none
   of the G-buffer, so the lighting still sees the cabin through it */
fragment FO gfs(VO in [[stage_in]], constant CU &u [[buffer(0)]], texture2d<float> th [[texture(5)]]) {
  FO o; o.a = 0;   /* no baked-light surface colour: the car is lit here */
  float3 N = normalize(in.wn), V = normalize(u.eye.xyz - in.wp);
  if (dot(N, V) < 0) N = -N;
  float nv = saturate(dot(N, V));
  float F = 0.04 + 0.96 * pow(1.0 - nv, 5.0);
  float3 R = reflect(-V, N), L = u.sun.xyz, H = normalize(L + V);
  float nl = saturate(dot(N, L));
  float3 spec = D_ggx(saturate(dot(N, H)), 0.03 * 0.03 + 0.001) * V_sg(nl, max(nv, 1e-3), 0.03) * F * u.sunc.rgb * nl;
  /* the sky and the horizon, as a real car's glass shows them; the last
     frame only near the horizon, where it holds the scenery */
  float3 scene = env_at(u, th, in.wp, R, 0.02), sky = sky_at(u, R);
  float3 refl = mix(sky, scene, smoothstep(0.35, 0.0, abs(R.z))) * F + spec;
  float a = mix(u.glass.x, 1.0, F);                     /* more mirror than window at a glance */
  float3 col = u.glass.yzw * (1.0 - F) * a + refl;
  /* where the car has been knocked, the glass is broken: crazed into cells
     whose edges catch the light, and frosted around them */
  float ck = smoothstep(0.012, 0.05, in.dent);
  if (ck > 0.0) {
    float3 q = in.lp * 11.0, c0 = floor(q); float f1 = 9.0, f2 = 9.0;
    for (int z = -1; z <= 1; z++) for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
      float3 c = c0 + float3(x, y, z), j = float3(hash13(c), hash13(c + 17.1), hash13(c + 41.7));
      float dd = length(q - c - j);
      if (dd < f1) { f2 = f1; f1 = dd; } else if (dd < f2) f2 = dd; }
    float line = (1.0 - smoothstep(0.0, 0.06, f2 - f1)) * ck;
    float3 lit = (u.skyc.rgb + u.sunc.rgb * 0.5) * 0.9;
    col = mix(col, lit, line * 0.8) + lit * 0.12 * ck;
    a = max(a, mix(a, 0.97, line)); a = min(1.0, a + 0.15 * ck);
  }
  if (u.fogc.w > 0.5) { float k = fogof(u, 1.0 / in.oow) / 255.0; col = mix(col, u.fogc.rgb * a, k); }
  o.d = float(wfloat(in.oow)) / 65536.0;
  o.c = u.misc.y > 0.5 ? float4(pow(saturate(col / 4.0), 1.0 / 2.2), a) : float4(pow(aces(col * u.misc.z), 1.0 / 2.2), a);
  o.n = 0; o.g = 0;
  return o; }
);

typedef struct {
    float M[16], P[16], HP[16];
    float vpt[4], eye[4], sun[4], sunc[4], skyc[4], grnd[4], fogc[4], misc[4], liv[4], livs[4], paint[4], dbg[4], hvpt[4], hist[4], glass[4], jit[4], livf[4];
    float map[4], hmap[4];
    float fogtab[64];
} cu;

typedef struct {
    id<MTLBuffer> vb, ib;
    int nv, ni;
    id<MTLTexture> base, mr, nrm;
    id<MTLBuffer> dm;                    /* <name>_dent.bin: which original vertices it follows */
    int dm_src;                          /* the dent walk's vertex count it was made for */
    float *pos;                          /* the proxy keeps its positions on the CPU */
    u32 *idx;
} mesh;

static id<MTLDevice> D;
static id<MTLRenderPipelineState> g_pipe, g_gpipe;
static id<MTLDepthStencilState> g_ds, g_gds;
/* three levels of detail each (remaster_car.py --lods); level 0 is the finest */
static mesh g_bodyl[3], g_wheell[3], g_glassl[3], g_proxy;
static int g_nlod = 1;
#define g_body g_bodyl[0]
#define g_wheel g_wheell[0]
#define g_glass g_glassl[0]
static id<MTLTexture> g_liv_side, g_liv_top, g_liv_front, g_liv_rear;
static int g_state;                      /* 0 not tried, 1 ready, -1 unavailable */
static float g_paint[3] = { 0.03f, 0.22f, 0.07f };
static float g_paint_ai[3] = { 0.79f, 0.40f, 0.02f };
static float g_liv[4], g_livs[4], g_livf[4];
static float g_wheel_flip = 1;
static float g_lamps[4][3];
static int g_have_lamps;

/* per list build: the car's transforms when its marker was queued */
typedef struct { float car[16], wheel[4][16]; u32 addr; int ai, dent; } rec;
static rec g_rec[16];
/* the original model's vertices, in BrRippleApply's walk order, and where
 * they were before the car's first dent; one per model record (car) */
#define DENT_MAX 512
typedef struct { u32 model; int n, have_rest; u32 addr[DENT_MAX]; float rest[DENT_MAX][3]; } dentsrc;
static dentsrc g_dsrc[16];
/* dent_rest.bin (remaster_dent.py): the walk's vertices before any dent, from
 * the source model, so a car already knocked when Remastered is switched on
 * is measured against its true shape, not against itself */
static float *g_rest;
static int g_nrest;
static id<MTLBuffer> g_dbuf[3], g_dummy;
static id<MTLTexture> g_flatn;          /* a flat normal map */
static float g_mainP[16];
static unsigned g_main_serial = ~0u;
static int g_nrec;
static unsigned g_rec_serial = ~0u;
static id<MTLBuffer> g_shadow[3];
/* the last finished frame, for reflections, and the main view it was seen through */
static id<MTLTexture> g_hist, g_black;
static float g_hP[16], g_hvpt[4], g_curP[16], g_curvpt[4], g_hmap[4], g_curmap[4];
static int g_hvalid, g_curvalid, g_horigin;
static int g_shadow_i;

static NSString *asset_dir(void)
{
    const char *e = getenv("BR_REMASTER_DIR");
    NSString *r;
    if (e) return [NSString stringWithUTF8String:e];
    r = [[NSBundle mainBundle].resourcePath stringByAppendingPathComponent:@"remaster"];
    if ([[NSFileManager defaultManager] fileExistsAtPath:[r stringByAppendingPathComponent:@"car.cfg"]]) return r;
    return @"ports/common/models/es/pack";
}

/* host_load.m: decode, parallel loops, batched mip chains */
u8 *hload_png(const char *path, int *w, int *h, int *spp);
void hload_for(int n, void (^f)(int i));
void hload_mips(NSMutableArray *batch, id<MTLTexture> t);
void hload_flush(NSMutableArray *batch);

/* any thread; the mip chain is left to `batch` (hload_flush) */
static id<MTLTexture> load_tex(NSString *path, NSMutableArray *batch)
{
    id<MTLTexture> t;
    MTLTextureDescriptor *td;
    int w, h, spp;
    u8 *px;
    if (![[NSFileManager defaultManager] fileExistsAtPath:path]) { fprintf(stderr, "car: missing %s\n", path.UTF8String); return nil; }
    /* remaster_car.py writes 8-bit RGBA, straight alpha (the body's alpha
     * is a mask, so it must not be premultiplied on the way in) */
    px = hload_png(path.UTF8String, &w, &h, &spp);
    if (!px || spp != 4) {
        fprintf(stderr, "car: %s is not 8-bit RGBA\n", path.UTF8String);
        free(px);
        return nil;
    }
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                            width:(NSUInteger)w height:(NSUInteger)h mipmapped:YES];
    td.usage = MTLTextureUsageShaderRead;
    t = [D newTextureWithDescriptor:td];
    [t replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)w, (NSUInteger)h) mipmapLevel:0 withBytes:px bytesPerRow:(NSUInteger)w * 4];
    free(px);
    hload_mips(batch, t);
    return t;
}

/* .rcm: "RCM1", nv, ni, then nv * {pos3 nrm3 uv2 tan4} floats, then ni u32 */
static int load_mesh(NSString *dir, NSString *name, mesh *m, int textured, int keep_cpu)
{
    NSData *d = [NSData dataWithContentsOfFile:[dir stringByAppendingPathComponent:[name stringByAppendingString:@".rcm"]]];
    const u8 *p;
    u32 nv, ni;
    if (!d || d.length < 12) { fprintf(stderr, "car: missing %s.rcm\n", name.UTF8String); return 0; }
    p = d.bytes;
    if (memcmp(p, "RCM1", 4)) return 0;
    memcpy(&nv, p + 4, 4); memcpy(&ni, p + 8, 4);
    if (d.length < 12 + (size_t)nv * 48 + (size_t)ni * 4) return 0;
    m->nv = (int)nv; m->ni = (int)ni;
    m->vb = [D newBufferWithBytes:p + 12 length:(size_t)nv * 48 options:MTLResourceStorageModeShared];
    m->ib = [D newBufferWithBytes:p + 12 + (size_t)nv * 48 length:(size_t)ni * 4 options:MTLResourceStorageModeShared];
    if (keep_cpu) {
        u32 i;
        m->pos = malloc((size_t)nv * 12); m->idx = malloc((size_t)ni * 4);
        for (i = 0; i < nv; i++) memcpy(m->pos + 3 * i, p + 12 + (size_t)i * 48, 12);
        memcpy(m->idx, p + 12 + (size_t)nv * 48, (size_t)ni * 4);
    }
    {
        NSData *dd = [NSData dataWithContentsOfFile:[dir stringByAppendingPathComponent:[name stringByAppendingString:@"_dent.bin"]]];
        u32 dn, ds;
        if (dd.length >= 12 && !memcmp(dd.bytes, "RDN1", 4)) {
            memcpy(&dn, (const u8 *)dd.bytes + 4, 4); memcpy(&ds, (const u8 *)dd.bytes + 8, 4);
            if (dn == nv && dd.length >= 12 + (size_t)dn * 48) {
                m->dm = [D newBufferWithBytes:(const u8 *)dd.bytes + 12 length:(size_t)dn * 48 options:MTLResourceStorageModeShared];
                m->dm_src = (int)ds;
            }
        }
    }
    if (textured) {
        m->nrm = g_flatn;                    /* no normal map: see setup() */
        if (!m->base || !m->mr) return 0;    /* loaded beforehand (setup) */
    }
    return 1;
}

static void setup(id<MTLDevice> dev)
{
    NSString *dir, *cfg;
    NSError *err = nil;
    id<MTLLibrary> lib;
    MTLRenderPipelineDescriptor *pd;
    if (g_state) return;
    g_state = -1;
    D = dev;
    dir = asset_dir();
    cfg = [NSString stringWithContentsOfFile:[dir stringByAppendingPathComponent:@"car.cfg"] encoding:NSUTF8StringEncoding error:nil];
    if (!cfg) { fprintf(stderr, "car: no model in %s; the original car stays\n", dir.UTF8String); return; }
    for (NSString *line in [cfg componentsSeparatedByString:@"\n"]) {
        const char *l = line.UTF8String;
        sscanf(l, "paint %f %f %f", &g_paint[0], &g_paint[1], &g_paint[2]);
        sscanf(l, "paint_ai %f %f %f", &g_paint_ai[0], &g_paint_ai[1], &g_paint_ai[2]);
        sscanf(l, "livery_side %f %f %f %f", &g_liv[0], &g_liv[1], &g_liv[2], &g_liv[3]);
        sscanf(l, "livery_top %f %f %f %f", &g_livs[0], &g_livs[1], &g_livs[2], &g_livs[3]);
        sscanf(l, "livery_front %f %f", &g_livf[0], &g_livf[1]);
        sscanf(l, "wheel_flip %f", &g_wheel_flip);
        if (sscanf(l, "lamps %f %f %f %f %f %f %f %f %f %f %f %f", &g_lamps[0][0], &g_lamps[0][1], &g_lamps[0][2],
                   &g_lamps[1][0], &g_lamps[1][1], &g_lamps[1][2], &g_lamps[2][0], &g_lamps[2][1], &g_lamps[2][2],
                   &g_lamps[3][0], &g_lamps[3][1], &g_lamps[3][2]) == 12)
            g_have_lamps = 1;
    }
    {
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
        u32 up = 0xFFFF8080u;                                  /* (0.5, 0.5, 1): straight out */
        g_flatn = [D newTextureWithDescriptor:td];
        [g_flatn replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&up bytesPerRow:4];
    }
    {   /* the maps first, all at once, across the cores */
        NSMutableArray *batch = [NSMutableArray new];
        NSArray *nm = @[ @"body_base", @"body_orm", @"wheel_base", @"wheel_orm",
                         @"livery_side", @"livery_top", @"livery_front", @"livery_rear" ];
        hload_for(8, ^(int i) { @autoreleasepool {
            id<MTLTexture> t = load_tex([dir stringByAppendingPathComponent:[nm[(NSUInteger)i] stringByAppendingString:@".png"]], batch);
            switch (i) {
            case 0: g_body.base = t; break;   case 1: g_body.mr = t; break;
            case 2: g_wheel.base = t; break;  case 3: g_wheel.mr = t; break;
            case 4: g_liv_side = t; break;    case 5: g_liv_top = t; break;
            case 6: g_liv_front = t; break;   default: g_liv_rear = t; break;
            }
        } });
        hload_flush(batch);
    }
    if (!load_mesh(dir, @"body", &g_body, 1, 0) || !load_mesh(dir, @"wheel", &g_wheel, 1, 0) ||
        !load_mesh(dir, @"proxy", &g_proxy, 0, 1))
        return;
    if (!load_mesh(dir, @"glass", &g_glass, 0, 0)) g_glass.ni = 0;   /* optional: no windows */
    {
        NSData *rd = [NSData dataWithContentsOfFile:[dir stringByAppendingPathComponent:@"dent_rest.bin"]];
        u32 rn;
        if (rd.length >= 8 && !memcmp(rd.bytes, "RDR1", 4)) {
            memcpy(&rn, (const u8 *)rd.bytes + 4, 4);
            if (rn <= DENT_MAX && rd.length >= 8 + (size_t)rn * 12) {
                g_rest = malloc((size_t)rn * 12);
                memcpy(g_rest, (const u8 *)rd.bytes + 8, (size_t)rn * 12);
                g_nrest = (int)rn;
            }
        }
    }
    for (g_nlod = 1; g_nlod < 3; g_nlod++) {          /* optional: the lower levels, sharing level 0's maps */
        NSString *sfx = [NSString stringWithFormat:@"_lod%d", g_nlod];
        mesh *b = &g_bodyl[g_nlod], *w = &g_wheell[g_nlod], *gl = &g_glassl[g_nlod];
        if (!load_mesh(dir, [@"body" stringByAppendingString:sfx], b, 0, 0) ||
            !load_mesh(dir, [@"wheel" stringByAppendingString:sfx], w, 0, 0))
            break;
        /* every level shades with its own crease normals (remaster_bake.py):
         * the model's normal map holds the full-density surface's detail and
         * on a decimated one it no longer lines up (measured: lumpy) */
        b->base = g_body.base; b->mr = g_body.mr; b->nrm = g_flatn;
        w->base = g_wheel.base; w->mr = g_wheel.mr; w->nrm = g_flatn;
        if (!load_mesh(dir, [@"glass" stringByAppendingString:sfx], gl, 0, 0)) *gl = g_glass;
    }
    if (!g_liv_side || !g_liv_top || !g_liv_front || !g_liv_rear) return;
    lib = [D newLibraryWithSource:[NSString stringWithUTF8String:CARSRC] options:nil error:&err];
    if (!lib) { fprintf(stderr, "car shader: %s\n", err.localizedDescription.UTF8String); return; }
    pd = [MTLRenderPipelineDescriptor new];
    pd.vertexFunction = [lib newFunctionWithName:@"cvs"];
    pd.fragmentFunction = [lib newFunctionWithName:@"cfs"];
    pd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
    pd.colorAttachments[1].pixelFormat = MTLPixelFormatRGBA16Float;
    pd.colorAttachments[2].pixelFormat = MTLPixelFormatRGBA32Float;
    pd.colorAttachments[3].pixelFormat = MTLPixelFormatRGBA8Unorm;
    pd.colorAttachments[4].pixelFormat = MTLPixelFormatR8Unorm;
    pd.colorAttachments[4].writeMask = MTLColorWriteMaskNone;
    pd.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    g_pipe = [D newRenderPipelineStateWithDescriptor:pd error:&err];
    if (!g_pipe) { fprintf(stderr, "car pipeline: %s\n", err.localizedDescription.UTF8String); return; }
    pd.fragmentFunction = [lib newFunctionWithName:@"gfs"];
    pd.colorAttachments[0].blendingEnabled = YES;
    pd.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
    pd.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pd.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorZero;
    pd.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
    pd.colorAttachments[1].writeMask = MTLColorWriteMaskNone;
    pd.colorAttachments[2].writeMask = MTLColorWriteMaskNone;
    pd.colorAttachments[3].writeMask = MTLColorWriteMaskNone;
    g_gpipe = [D newRenderPipelineStateWithDescriptor:pd error:&err];
    if (!g_gpipe) { fprintf(stderr, "car glass pipeline: %s\n", err.localizedDescription.UTF8String); return; }
    {
        MTLDepthStencilDescriptor *d = [MTLDepthStencilDescriptor new];
        d.depthCompareFunction = MTLCompareFunctionLess;
        d.depthWriteEnabled = YES;
        g_ds = [D newDepthStencilStateWithDescriptor:d];
        d.depthWriteEnabled = NO;
        g_gds = [D newDepthStencilStateWithDescriptor:d];
    }
    {
        int i;
        for (i = 0; i < 3; i++)
            g_shadow[i] = [D newBufferWithLength:(size_t)g_proxy.ni * 18 * 4 options:MTLResourceStorageModeShared];
    }
    {
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
        u32 z = 0;
        g_black = [D newTextureWithDescriptor:td];
        [g_black replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&z bytesPerRow:4];
    }
    {
        int k;
        for (k = 0; k < 3; k++) g_dbuf[k] = [D newBufferWithLength:16 * DENT_MAX * 16 options:MTLResourceStorageModeShared];
        g_dummy = [D newBufferWithLength:4096 options:MTLResourceStorageModeShared];
    }
    fprintf(stderr, "car: Remastered model loaded from %s (body %d tris, wheel %d, %d levels of detail)\n",
            dir.UTF8String, g_body.ni / 3, g_wheel.ni / 3, g_nlod);
    __atomic_store_n(&g_state, 1, __ATOMIC_RELEASE);   /* setup may run on a prefetch thread */
}

static int car_off(void)
{
    static int off = -1;
    if (off < 0) off = getenv("BR_CAR") && !atoi(getenv("BR_CAR"));
    return off;
}
static dispatch_once_t g_once;
static void setup_once(void) { dispatch_once(&g_once, ^{ setup(MTLCreateSystemDefaultDevice()); }); }

/* host_fx.m, while Remastered is on: load the model in the background, long
 * before the first race draws it */
void hcar_prefetch(void)
{
    static int done;
    if (done || car_off() || !hfx_on()) return;
    done = 1;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{ @autoreleasepool { setup_once(); } });
}

int hcar_enabled(void)
{
    if (car_off() || !hfx_on()) return 0;
    setup_once();                    /* waits for a prefetch still loading */
    return g_state == 1;
}

/* The car's transforms as the list is built: its world matrix and the four
 * wheel matrices (row vectors, metres). */
/* BrRippleApply's walk: the model record's level-0 lists at +0x8018 + i*4,
 * top level only, every G_VTX's vertices (floats, 32 bytes apart) */
static int dent_walk(u32 model, u32 *addr)
{
    int i, n = 0;
    for (i = 0; i < 9; i++) {
        u32 dl = W_LD(u32, model, 0x8018 + 4 * i), w0;
        int guard = 4096;
        if (!dl) continue;
        while (guard-- && (w0 = W_LD(u32, dl, 0)) >> 24 != 0xB8) {
            if (w0 >> 24 == 0x04) {
                u32 p = W_LD(u32, dl, 4), j, c = (w0 >> 10) & 0x3F;
                for (j = 0; j < c && n < DENT_MAX; j++) addr[n++] = p + 32 * j;
            }
            dl += 8;
        }
    }
    return n;
}

/* this car's displacement per original vertex, metres in the car frame,
 * into this frame's dent buffer at `slot`; 0 if it cannot be followed */
static int dent_update(u32 car, int slot)
{
    u32 model = W_LD(u32, car, 0x29C4);
    dentsrc *d = NULL;
    float *out;
    int i, fresh = 1;
    if (!model || !g_body.dm || !g_dbuf[0]) return 0;
    for (i = 0; i < 16 && !d; i++) if (g_dsrc[i].model == model) d = &g_dsrc[i];
    for (i = 0; i < 16 && !d; i++) if (!g_dsrc[i].model) { d = &g_dsrc[i]; d->model = model; d->n = dent_walk(model, d->addr); }
    if (!d || d->n != g_body.dm_src) return 0;
    for (i = 0; i < 8; i++) if (W_LD(s16, car, 0x29C8 + 2 * i)) fresh = 0;   /* the zones' running totals */
    /* without the source model's shape, only a car seen before its first
     * dent can be followed (its shape is learnt then) */
    if (!fresh && !d->have_rest && g_nrest != d->n) return 0;
    out = (float *)g_dbuf[g_rec_serial % 3].contents + (size_t)slot * DENT_MAX * 4;
    if (!d->have_rest && g_nrest == d->n) {
        memcpy(d->rest, g_rest, (size_t)d->n * 12);
        d->have_rest = 1;
    }
    if (fresh && getenv("BR_DENTLOG") && g_nrest == d->n) {
        static int once;
        float mx = 0;
        for (i = 0; i < d->n && !once; i++) {
            int k;
            for (k = 0; k < 3; k++) { float e = fabsf(W_LD(f32, d->addr[i], 4 * k) - g_rest[3 * i + k]); if (e > mx) mx = e; }
        }
        if (!once) fprintf(stderr, "dent: undented car vs dent_rest.bin, largest difference %g\n", mx);
        once = 1;
    }
    for (i = 0; i < d->n; i++) {
        float c[3] = { W_LD(f32, d->addr[i], 0), W_LD(f32, d->addr[i], 4), W_LD(f32, d->addr[i], 8) };
        if (fresh && g_nrest != d->n) { memcpy(d->rest[i], c, sizeof c); d->have_rest = 1; }   /* no dent yet: this is its shape */
        out[4 * i] = (c[0] - d->rest[i][0]) / 255.0f;
        out[4 * i + 1] = (c[1] - d->rest[i][1]) / 255.0f;
        out[4 * i + 2] = (c[2] - d->rest[i][2]) / 255.0f;
        out[4 * i + 3] = 0;
    }
    if (getenv("BR_DENTLOG") && slot == 0 && hglide_swaps() % 30 == 0) {
        float mx = 0;
        for (i = 0; i < d->n; i++) { float *q = out + 4 * i; float l = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]); if (l > mx) mx = l; }
        if (getenv("BR_DENTDUMP")) { FILE *f = fopen(getenv("BR_DENTDUMP"), "wb"); if (f) { fwrite(out, 16, (size_t)d->n, f); fclose(f); } }
        fprintf(stderr, "dent: car %u zones %d %d %d %d %d %d %d %d, largest move %.3f m\n", W_LD(u32, car, 0x140),
                W_LD(s16, car, 0x29C8), W_LD(s16, car, 0x29CA), W_LD(s16, car, 0x29CC), W_LD(s16, car, 0x29CE),
                W_LD(s16, car, 0x29D0), W_LD(s16, car, 0x29D2), W_LD(s16, car, 0x29D4), W_LD(s16, car, 0x29D6), mx);
    }
    return !fresh;
}

int hcar_record(u32 car)
{
    rec *r;
    int i;
    if (hglide_swaps() != g_rec_serial) { g_rec_serial = hglide_swaps(); g_nrec = 0; }
    if (g_nrec == 16) return -1;
    r = &g_rec[g_nrec];
    for (i = 0; i < 16; i++) r->car[i] = W_LD(f32, car, 4 * i);
    for (i = 0; i < 64; i++) r->wheel[i / 16][i % 16] = W_LD(f32, car, 0x40 + 4 * i);
    r->addr = car;
    r->ai = W_LD(u32, car, 0x140) != 0;     /* car 0 is the player's */
    r->dent = dent_update(car, g_nrec);
    return g_nrec++;
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

/* host_fx.m's light rig for the weather, the same numbers */
static void rig(cu *u)
{
    int w = (int)H32(0x104B15E8u);
    float k, sc[3], sk[3], gr[3];
    const char *ov = getenv("BR_FX_WEATHER"), *s = getenv("BR_FX_SUN");
    double sun[3] = { 1, 1, 1.1 }, l;
    if (ov) w = atoi(ov);
    u->misc[2] = w == 2 ? 1.12f : w == 4 ? 1.25f : w == 3 ? 1.05f : w == 1 ? 1.08f : 1.08f;   /* the fx exposure */
    u->misc[3] = w == 2 || w == 4 ? 1.0f : 0.85f;                                            /* coat strength */
    /* host_fx.m's rig for true-albedo models, so the two never drift apart */
    if (hfx_car_rig(u->sun, u->sunc, u->skyc, u->grnd)) return;
    switch (w) {
    case 4:  k = 0.28f; sc[0] = 0.85f; sc[1] = 0.9f; sc[2] = 1.0f; sk[0] = 0.62f; sk[1] = 0.68f; sk[2] = 0.78f; gr[0] = 0.32f; gr[1] = 0.32f; gr[2] = 0.33f; break;
    case 2:  k = 0.10f; sc[0] = 0.8f; sc[1] = 0.85f; sc[2] = 1.0f; sk[0] = 0.48f; sk[1] = 0.53f; sk[2] = 0.64f; gr[0] = 0.22f; gr[1] = 0.22f; gr[2] = 0.24f; break;
    case 3:  k = 0.75f; sc[0] = 1.0f; sc[1] = 0.97f; sc[2] = 0.95f; sk[0] = 0.62f; sk[1] = 0.68f; sk[2] = 0.8f; gr[0] = 0.62f; gr[1] = 0.64f; gr[2] = 0.7f; break;
    case 1:  k = 0.3f; sc[0] = 1.0f; sc[1] = 0.97f; sc[2] = 0.92f; sk[0] = 0.66f; sk[1] = 0.68f; sk[2] = 0.7f; gr[0] = 0.4f; gr[1] = 0.39f; gr[2] = 0.37f; break;
    default: k = 1.6f; sc[0] = 1.0f; sc[1] = 0.91f; sc[2] = 0.76f; sk[0] = 0.40f; sk[1] = 0.48f; sk[2] = 0.62f; gr[0] = 0.34f; gr[1] = 0.28f; gr[2] = 0.21f; break;
    }
    if (s) sscanf(s, "%lf,%lf,%lf", &sun[0], &sun[1], &sun[2]);
    l = sqrt(sun[0] * sun[0] + sun[1] * sun[1] + sun[2] * sun[2]);
    u->sun[0] = (float)(sun[0] / l); u->sun[1] = (float)(sun[1] / l); u->sun[2] = (float)(sun[2] / l);
    u->sun[3] = k > 0.2f ? 1.0f : 0.0f;
    u->sunc[0] = sc[0] * k; u->sunc[1] = sc[1] * k; u->sunc[2] = sc[2] * k;
    memcpy(u->skyc, sk, 12); memcpy(u->grnd, gr, 12);
    u->misc[2] = w == 2 ? 1.4f : w == 4 ? 1.35f : w == 3 ? 1.2f : w == 1 ? 1.3f : 1.35f;   /* the fx exposure */
    u->misc[3] = w == 2 || w == 4 ? 1.0f : 0.85f;                                            /* coat strength */
}

static void mul44(const float *a, const float *b, float *o)
{
    int i, j, k;
    for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) { float s = 0; for (k = 0; k < 4; k++) s += a[i * 4 + k] * b[k * 4 + j]; o[i * 4 + j] = s; }
}

/* Back faces are not drawn: the car's depth is written by its shader (the
 * game's W-buffer word), which turns the GPU's early depth test off, so
 * every layer drawn is fully shaded.  Which winding faces the camera follows
 * the model-to-screen map: its handedness, and the screen's y mirror. */
static void set_cull(id<MTLRenderCommandEncoder> e, const cu *u, int origin_ll)
{
    double a[3][3];
    int i, j, k;
    static const int col[3] = { 0, 1, 3 };               /* clip x, y, w */
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) {
            double s = 0;
            for (k = 0; k < 3; k++) s += u->M[i * 4 + k] * u->P[k * 4 + col[j]];
            a[i][j] = s;
        }
    double det = a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) +
                 a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
    if (!origin_ll) det = -det;
    if (getenv("BR_CAR_CULLFLIP")) det = -det;
    [e setFrontFacingWinding:det > 0 ? MTLWindingClockwise : MTLWindingCounterClockwise];
    [e setCullMode:MTLCullModeBack];
}

static void draw_mesh(id<MTLRenderCommandEncoder> e, mesh *m, cu *u, int paint, int dent_slot)
{
    u->paint[3] = (float)paint;
    [e setVertexBuffer:m->vb offset:0 atIndex:0];
    u->jit[2] = dent_slot >= 0 && m->dm ? 1.0f : 0.0f;
    [e setVertexBuffer:u->jit[2] > 0 ? m->dm : g_dummy offset:0 atIndex:2];
    [e setVertexBuffer:u->jit[2] > 0 ? g_dbuf[g_rec_serial % 3] : g_dummy
                offset:u->jit[2] > 0 ? (NSUInteger)dent_slot * DENT_MAX * 16 : 0 atIndex:3];
    [e setVertexBytes:u length:sizeof *u atIndex:1];
    [e setFragmentBytes:u length:sizeof *u atIndex:0];
    [e setFragmentTexture:m->base atIndex:0];
    [e setFragmentTexture:m->mr atIndex:1];
    [e setFragmentTexture:m->nrm atIndex:2];
    [e setFragmentTexture:g_liv_side atIndex:3];
    [e setFragmentTexture:g_liv_top atIndex:4];
    [e setFragmentTexture:g_liv_front atIndex:6];
    [e setFragmentTexture:g_liv_rear atIndex:7];
    [e setFragmentTexture:g_hist ? g_hist : g_black atIndex:5];
    [e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:(NSUInteger)m->ni indexType:MTLIndexTypeUInt32
                 indexBuffer:m->ib indexBufferOffset:0];
}

/* the proxy in world space, as host_fx.m's shadow pass takes geometry */
static void cast_shadow(const float *M)
{
    id<MTLBuffer> b = g_shadow[g_shadow_i = (g_shadow_i + 1) % 3];
    float *o = b.contents;
    int i;
    for (i = 0; i < g_proxy.ni; i++) {
        const float *p = g_proxy.pos + 3 * g_proxy.idx[i];
        float *v = o + 18 * i;           /* host_glide.m's clip-space corner (CVN floats) */
        memset(v, 0, 18 * 4);
        v[10] = p[0] * M[0] + p[1] * M[4] + p[2] * M[8] + M[12];
        v[11] = p[0] * M[1] + p[1] * M[5] + p[2] * M[9] + M[13];
        v[12] = p[0] * M[2] + p[1] * M[6] + p[2] * M[10] + M[14];
        v[13] = 1;
    }
    hfx_shadow_batch(b, 0, g_proxy.ni, nil, nil, 7, 0, 0, 1, 1);
}

void hcar_draw(int slot)
{
    id<MTLDevice> dev;
    id<MTLRenderCommandEncoder> e;
    MTLScissorRect sc;
    int origin_ll, fogmode, i, main, rw, rh, lod = 0;
    float fogc[4];
    cu u;
    rec *r;
    double P[16], IP[16];
    if (__atomic_load_n(&g_state, __ATOMIC_ACQUIRE) != 1 || slot < 0 || slot >= g_nrec || hglide_swaps() != g_rec_serial) return;
    r = &g_rec[slot];
    memset(&u, 0, sizeof u);
    e = hglide_native_pass(&dev, &sc, &origin_ll, &fogmode, fogc, u.fogtab, &rw, &rh);
    hfx_jitter(&u.jit[0], &u.jit[1], rw, rh);
    {   /* where this view sits on the target: the camera's or the mirror's */
        int view = hrender_view();
        hglide_map(view, u.map);
        sc = hglide_scissor(view);
    }
    for (i = 0; i < 16; i++) { u.P[i] = W_LD(f32, 0x105CCD00u, 4 * i); P[i] = u.P[i]; }
    u.vpt[0] = W_LD(f32, 0x105CCD48u, 0); u.vpt[1] = W_LD(f32, 0x105CD9F8u, 0);
    u.vpt[2] = W_LD(f32, 0x105CCFDCu, 0); u.vpt[3] = W_LD(f32, 0x105CD9FCu, 0);
    inv4(P, IP);
    {   /* the eye: clip-space (0,0,1,0) back into the world */
        double h[4]; int j;
        for (j = 0; j < 4; j++) h[j] = IP[8 + j];
        for (j = 0; j < 3; j++) u.eye[j] = (float)(h[j] / h[3]);
    }
    rig(&u);
    /* the main view's list runs first in a frame, the rear-view mirror's
     * after it: the first marker names the main view's projection */
    if (g_main_serial != g_rec_serial) { memcpy(g_mainP, u.P, sizeof g_mainP); g_main_serial = g_rec_serial; }
    main = !memcmp(g_mainP, u.P, sizeof g_mainP);
    u.misc[0] = (float)origin_ll;
    u.misc[1] = (float)main;
    u.fogc[0] = fogc[0] / 255.0f; u.fogc[1] = fogc[1] / 255.0f; u.fogc[2] = fogc[2] / 255.0f;
    u.fogc[3] = (float)(fogmode & 1);
    {
        int k;
        for (k = 0; k < 3; k++) u.fogc[k] = powf(u.fogc[k], 2.2f);
    }
    memcpy(u.liv, g_liv, sizeof g_liv);
    memcpy(u.livs, g_livs, sizeof g_livs);
    memcpy(u.livf, g_livf, sizeof g_livf);
    memcpy(u.paint, r->ai ? g_paint_ai : g_paint, sizeof g_paint);   /* linear */
    if (getenv("BR_CAR_DEBUG")) u.dbg[0] = (float)atoi(getenv("BR_CAR_DEBUG"));
    if (getenv("BR_CAR_ENV")) u.dbg[2] = (float)atoi(getenv("BR_CAR_ENV"));

    if (main) { memcpy(g_curP, u.P, sizeof g_curP); memcpy(g_curvpt, u.vpt, sizeof g_curvpt); memcpy(g_curmap, u.map, sizeof g_curmap); g_curvalid = 1; g_horigin = origin_ll; }
    if (g_hvalid && g_hist) {
        memcpy(u.HP, g_hP, sizeof u.HP); memcpy(u.hvpt, g_hvpt, sizeof u.hvpt); memcpy(u.hmap, g_hmap, sizeof u.hmap);
        u.hist[0] = 1; u.hist[1] = (float)g_horigin; u.hist[2] = u.misc[2];
        u.hist[3] = r->car[14] - 0.28f;          /* the ground under the car */
    }
    [e setRenderPipelineState:g_pipe];
    [e setDepthStencilState:g_ds];
    [e setScissorRect:sc];
    [e setCullMode:MTLCullModeNone];
    memcpy(u.M, r->car, sizeof u.M);
    if (main) cast_shadow(r->car);      /* even when the car itself is off screen: its shadow may not be */
    {   /* off screen: skip it (a 3 m sphere around the car against the view) */
        const float *C = r->car, R0 = 3.0f;
        float c[4]; int j;
        for (j = 0; j < 4; j++) c[j] = C[12] * u.P[j] + C[13] * u.P[4 + j] + C[14] * u.P[8 + j] + u.P[12 + j];
        float kx = sqrtf(u.P[0] * u.P[0] + u.P[4] * u.P[4] + u.P[8] * u.P[8]) + sqrtf(u.P[3] * u.P[3] + u.P[7] * u.P[7] + u.P[11] * u.P[11]);
        float ky = sqrtf(u.P[1] * u.P[1] + u.P[5] * u.P[5] + u.P[9] * u.P[9]) + sqrtf(u.P[3] * u.P[3] + u.P[7] * u.P[7] + u.P[11] * u.P[11]);
        float kw = sqrtf(u.P[3] * u.P[3] + u.P[7] * u.P[7] + u.P[11] * u.P[11]);
        if (c[3] < -R0 * kw || c[0] > c[3] + R0 * kx || -c[0] > c[3] + R0 * kx || c[1] > c[3] + R0 * ky || -c[1] > c[3] + R0 * ky)
            return;
        /* the level of detail by distance; the mirror, a small picture, the coarsest */
        {
            float dx = C[12] - u.eye[0], dy = C[13] - u.eye[1], dz = C[14] - u.eye[2], dist = sqrtf(dx * dx + dy * dy + dz * dz);
            const char *f = getenv("BR_CAR_LOD");
            /* subdivided and decimated (remaster_bake.py), 300k reflects as cleanly as 1M */
            lod = !main ? 2 : dist < 5.0f ? 0 : dist < 28.0f ? 1 : 2;
            if (f) lod = atoi(f);
            if (lod >= g_nlod) lod = g_nlod - 1;
            if (lod < 0) lod = 0;
        }
    }
    set_cull(e, &u, origin_ll);
    draw_mesh(e, &g_bodyl[lod], &u, 1, r->dent ? slot : -1);
    for (i = 0; i < 4; i++) {
        /* the wheel's face points out of the car on both sides: the model's
         * outer face is its +y; on the car's -y side turn it half round about
         * its vertical axis (a rotation, so the winding and the spin stay) */
        const float *W = r->wheel[i];
        float side = (W[12] - r->car[12]) * r->car[4] + (W[13] - r->car[13]) * r->car[5] + (W[14] - r->car[14]) * r->car[6];
        float flip[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
        if ((side < 0) == (g_wheel_flip > 0)) { flip[0] = -1; flip[5] = -1; }
        mul44(flip, W, u.M);
        set_cull(e, &u, origin_ll);
        draw_mesh(e, &g_wheell[lod], &u, 0, -1);
    }
    [e setCullMode:MTLCullModeNone];      /* the glass: thin, blended, either side */
    mesh *gm = &g_glassl[lod];
    if (gm->ni) {                       /* the windows, over the body and the cabin */
        u.glass[0] = 0.78f;                 /* opacity face-on: dark rally tint */
        u.glass[1] = 0.010f; u.glass[2] = 0.013f; u.glass[3] = 0.016f;   /* the tint, linear */
        memcpy(u.M, r->car, sizeof u.M);
        [e setRenderPipelineState:g_gpipe];
        [e setDepthStencilState:g_gds];
        [e setVertexBuffer:gm->vb offset:0 atIndex:0];
        u.jit[2] = r->dent && gm->dm ? 1.0f : 0.0f;
        [e setVertexBuffer:u.jit[2] > 0 ? gm->dm : g_dummy offset:0 atIndex:2];
        [e setVertexBuffer:u.jit[2] > 0 ? g_dbuf[g_rec_serial % 3] : g_dummy
                    offset:u.jit[2] > 0 ? (NSUInteger)slot * DENT_MAX * 16 : 0 atIndex:3];
        [e setVertexBytes:&u length:sizeof u atIndex:1];
        [e setFragmentBytes:&u length:sizeof u atIndex:0];
        [e setFragmentTexture:g_hist ? g_hist : g_black atIndex:5];
        [e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:(NSUInteger)gm->ni indexType:MTLIndexTypeUInt32
                     indexBuffer:gm->ib indexBufferOffset:0];
    }
    if (getenv("BR_CARLOG") && hglide_swaps() % 120 == 0) {
        const float *C = r->car;
        float c[4]; int j;
        for (j = 0; j < 4; j++) c[j] = C[12] * u.P[j] + C[13] * u.P[4 + j] + C[14] * u.P[8 + j] + u.P[12 + j];
        fprintf(stderr, "car: main %d P %g %g %g %g / %g %g %g %g / %g %g %g %g / %g %g %g %g  clip(pos) %g %g %g %g  vpt %g %g %g %g eye %g %g %g\n", main,
                u.P[0], u.P[1], u.P[2], u.P[3], u.P[4], u.P[5], u.P[6], u.P[7], u.P[8], u.P[9], u.P[10], u.P[11], u.P[12], u.P[13], u.P[14], u.P[15],
                c[0], c[1], c[2], c[3], u.vpt[0], u.vpt[1], u.vpt[2], u.vpt[3], u.eye[0], u.eye[1], u.eye[2]);
        fprintf(stderr, "car: pos %.2f %.2f %.2f  x %.2f %.2f %.2f  y %.2f %.2f %.2f  z %.2f %.2f %.2f\n",
                C[12], C[13], C[14], C[0], C[1], C[2], C[4], C[5], C[6], C[8], C[9], C[10]);
        for (i = 0; i < 4; i++) {
            const float *W = r->wheel[i];
            float d[3] = { W[12] - C[12], W[13] - C[13], W[14] - C[14] };
            fprintf(stderr, "car:   wheel %d local %.3f %.3f %.3f  axle %.2f %.2f %.2f\n", i,
                    d[0] * C[0] + d[1] * C[1] + d[2] * C[2], d[0] * C[4] + d[1] * C[5] + d[2] * C[6],
                    d[0] * C[8] + d[1] * C[9] + d[2] * C[10], W[4], W[5], W[6]);
        }
    }
}

/* host_glide.m, at every swap with the finished picture: keep it (mip-mapped)
 * as the next frame's reflection source, with the main view it was seen
 * through.  A frame with no car keeps nothing. */
void hcar_frame_end(id<MTLCommandBuffer> cb, id<MTLTexture> pic)
{
    id<MTLBlitCommandEncoder> b;
    if (__atomic_load_n(&g_state, __ATOMIC_ACQUIRE) != 1 || !g_curvalid || !pic) { g_hvalid = 0; g_curvalid = 0; return; }
    if (!g_hist || g_hist.width != pic.width || g_hist.height != pic.height) {
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                        width:pic.width height:pic.height mipmapped:YES];
        td.usage = MTLTextureUsageShaderRead;
        td.storageMode = MTLStorageModePrivate;
        g_hist = [D newTextureWithDescriptor:td];
    }
    b = [cb blitCommandEncoder];
    [b copyFromTexture:pic sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(pic.width, pic.height, 1)
             toTexture:g_hist destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
    [b generateMipmapsForTexture:g_hist];
    [b endEncoding];
    memcpy(g_hP, g_curP, sizeof g_hP); memcpy(g_hvpt, g_curvpt, sizeof g_hvpt); memcpy(g_hmap, g_curmap, sizeof g_hmap);
    g_hvalid = 1; g_curvalid = 0;
}

/* host_fx.m's headlights: where this model's lamps are, when `car` is drawn
 * as it (this frame's list, or the last one), in the car frame (metres from
 * the car matrix's origin: x forward, y left, z up).  Front-left,
 * front-right, rear-left, rear-right, measured from the model by
 * remaster_car.py. */
int hcar_lamps(u32 car, float out[4][3])
{
    int i;
    if (__atomic_load_n(&g_state, __ATOMIC_ACQUIRE) != 1 || !g_have_lamps || !hcar_enabled()) return 0;
    for (i = 0; i < g_nrec; i++)
        if (g_rec[i].addr == car) {
            memcpy(out, g_lamps, sizeof g_lamps);
            return 1;
        }
    return 0;
}
