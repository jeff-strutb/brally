/* host_env.m -- the Remastered environment models (port code).
 *
 * While the Remastered renderer is on (host_fx.m, the ~ key), the track's
 * flat cut-out cards that remaster_env_place.py identified (a picture of a
 * pine, a strip of forest, a clump of bushes) are replaced by real models
 * standing where the cards stood.  ~ switches back to the original, cards and
 * all.  Only what is drawn changes: the cards have no collision (the track's
 * collision mesh is a separate array the game never draws), so nothing the
 * game simulates or reads is touched.
 *
 *   hiding      the original instance list is never modified.  For an
 *               instance holding replaced cards, a copy of its list is made in
 *               game memory with those cards' triangle commands blanked (all
 *               three corners the same vertex: nothing is drawn), and
 *               native/env.m hands the game that copy while Remastered is on.
 *   placement   ports/common/models/placements/<track>.env: which instance
 *               each model stands on (its "anchor") and its world matrix.
 *               native/env.m queues a marker after every anchor the game
 *               draws, so a model is drawn exactly when the game would have
 *               drawn its card: the game's own visibility and range, in the
 *               list's order and through the current view's projection
 *               (main view or rear-view mirror).
 *   shading     metal/roughness PBR from the model's own maps, cut-out
 *               alpha for leaves, light through leaves from behind, a light
 *               wind sway, the weather's light rig (host_fx.m's, as the car
 *               uses), the game's fog.  In the main view the result goes to
 *               the fx composite as a pre-lit surface (G-buffer class 4, like
 *               the Remastered car), which adds the sun's shadows, bloom and
 *               the tone map.  Every model also casts into the sun's shadow
 *               map (henv_shadow, called from the fx shadow pass).
 *   depth       the Voodoo W-buffer word, from the same clip-space w as the
 *               game's own triangles, so models and scene share one buffer.
 *
 * Assets: ports/common/models/<asset>/ (bake.json and the .rcm meshes that
 * remaster_env_bake.py writes) and ports/common/textures/<asset>/, or the
 * app's Resources/env.  BR_ENV=0 turns it off.
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
void hglide_map(int view, float T[4]);
MTLScissorRect hglide_scissor(int view);
int hrender_view(void);
u32 hmem_alloc(u32 n, int zero);
double hframe_game_ms(void);

#define TRK_HDR    0x106EECD8u        /* the loaded .TRK header (host order) */
#define OBJ_TABLE  0x106EED38u        /* its instance array, 0x54-byte records */
#define PROJ       0x105CCD00u        /* the list machine's view x projection */
#define MAX_INST   0x800
#define MAX_SUB    8
#define NLOD       3

#define STR(...) #__VA_ARGS__
static const char *ENVSRC = "#include <metal_stdlib>\n" STR(
using namespace metal;
struct MV { packed_float3 p; packed_float3 n; float2 uv; float4 t; };
struct EU {
  float4x4 M, P;
  float4 vpt, eye, sun, sunc, skyc, grnd, fogc, misc, map, jit, mat, geo;
  float fogtab[64];
};
struct VO { float4 pos [[position]]; float3 wp; float3 wn; float3 wt; float tw; float2 uv; float oow; float3 lp; };
/* a light sway for leaves: stronger toward the top of the model, phased by
   where it stands so a row of trees does not move as one */
float3 sway(constant EU &u, float4x4 M, float3 lp) {
  if (u.mat.z <= 0.0) return float3(0);
  float h = saturate(lp.z / max(u.mat.w, 0.1));
  float ph = dot(M[3].xy, float2(0.37, 0.61));
  float t = u.misc.w * 0.001;
  float a = sin(t * 1.3 + ph) * 0.6 + sin(t * 2.9 + ph * 1.7 + lp.x * 0.8) * 0.25;
  return float3(a, a * 0.6, 0) * u.mat.z * length(M[0].xyz) * h * h;
}
vertex VO evs(uint vid [[vertex_id]], uint iid [[instance_id]], const device MV *v [[buffer(0)]],
              constant EU &u [[buffer(1)]], const device float4x4 *inst [[buffer(2)]]) {
  MV m = v[vid]; VO o;
  float4x4 M = inst[iid];
  float3 p = m.p;
  float4 w = M * float4(p, 1);
  w.xyz += sway(u, M, p);
  float4 c = u.P * w;
  float X = u.vpt.x * c.x + u.vpt.y * c.w, Y = u.vpt.z * c.y + u.vpt.w * c.w;
  float Yd = u.misc.x > 0.5 ? 480.0 * c.w - Y : Y;
  o.pos = float4(u.map.x * X + u.map.y * c.w, u.map.z * Yd + u.map.w * c.w, (c.z + c.w) * 0.5, c.w);
  o.pos.xy += u.jit.xy * c.w;
  o.wp = w.xyz;
  o.wn = (M * float4(float3(m.n), 0)).xyz; o.wt = (M * float4(m.t.xyz, 0)).xyz; o.tw = m.t.w;
  o.uv = m.uv; o.oow = 1.0 / c.w; o.lp = p;
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
float fogof(constant EU &u, float w) {
  if (w <= 1.0) return u.fogtab[0]; float prev = 0, pw = 1;
  for (int i = 0; i < 64; i++) { float tw = exp2(3.0 + float(i >> 2)) / float(8 - (i & 3));
    if (w <= tw) return prev + (u.fogtab[i] - prev) * (w - pw) / (tw - pw);
    prev = u.fogtab[i]; pw = tw; }
  return u.fogtab[63]; }
float D_ggx(float nh, float a) { float a2 = a * a, d = nh * nh * (a2 - 1.0) + 1.0; return a2 / (3.14159 * d * d); }
float V_sg(float nl, float nv, float a) { float k = a * 0.5; return 0.25 / ((nl * (1 - k) + k) * (nv * (1 - k) + k)); }
/* foliage occlusion: leaves deep in the crown and low on it see less sky
   (geo: x half width, y height, model units; lp the model-space point) */
float canopy_ao(constant EU &u, float3 lp) {
  if (u.geo.x <= 0.0) return 1.0;
  float r = saturate(length(lp.xy) / u.geo.x), z = saturate(lp.z / max(u.geo.y, 1e-3));
  return mix(0.28, 1.0, saturate(r * r * 0.85 + z * 0.35)); }
float3 aces(float3 x) { return saturate(x * (2.51 * x + 0.03) / (x * (2.43 * x + 0.59) + 0.14)); }
fragment FO efs(VO in [[stage_in]], bool front [[front_facing]], constant EU &u [[buffer(0)]],
                texture2d<float> tb [[texture(0)]], texture2d<float> tm [[texture(1)]], texture2d<float> tn [[texture(2)]]) {
  constexpr sampler s(filter::linear, mip_filter::linear, address::repeat, max_anisotropy(8));
  FO o; o.a = 0;
  float4 B = tb.sample(s, in.uv);
  if (u.jit.w > 0.5 && u.jit.w < 1.5) {  /* BR_ENV_DEBUG=1: every model flat magenta, wherever it lands */
    o.c = float4(1, 0, 1, 1); o.n = float4(0, 0, 0, 1); o.g = float4(0); o.d = 0.0; return o; }
  if (u.mat.x > 0.5 && B.a < 0.5) discard_fragment();
  float3 alb = pow(B.rgb, 2.2);
  float4 MR = tm.sample(s, in.uv);                 /* occlusion, roughness, metal */
  float ao = MR.r * (u.mat.x > 0.5 ? canopy_ao(u, in.lp) : 1.0), rough = clamp(MR.g, 0.05, 1.0), metal = MR.b;
  float3 nm = tn.sample(s, in.uv).xyz * 2.0 - 1.0;
  float3 N0 = normalize(in.wn);
  float3 V = normalize(u.eye.xyz - in.wp);
  /* a leaf card or a thin surface is seen from both sides */
  if (dot(N0, V) < 0) { N0 = -N0; nm.xy = -nm.xy; }
  float3 T = in.wt - N0 * dot(N0, in.wt);
  T = length(T) > 1e-6 ? normalize(T) : float3(1, 0, 0);
  float3 Bt = cross(N0, T) * (in.tw < 0 ? -1.0 : 1.0);
  float3 N = normalize(T * nm.x + Bt * nm.y + N0 * max(nm.z, 0.05));
  float3 L = u.sun.xyz, H = normalize(L + V);
  float nl = saturate(dot(N, L)), nv = max(dot(N, V), 1e-3), nh = saturate(dot(N, H)), vh = saturate(dot(V, H));
  float a = rough * rough;
  float3 F0 = mix(float3(0.04), alb, metal);
  float3 F = F0 + (1.0 - F0) * pow(1.0 - vh, 5.0);
  float3 spec = D_ggx(nh, a) * V_sg(nl, nv, a) * F;
  float3 kd = (1.0 - F) * (1.0 - metal);
  float3 amb = mix(u.grnd.rgb, u.skyc.rgb, N.z * 0.5 + 0.5) * ao;
  float sunv = u.mat.x > 0.5 ? mix(0.2, 1.0, ao * ao) : 1.0;   /* inner leaves are mostly shaded by outer ones */
  float3 col = alb * kd * (u.sunc.rgb * nl * sunv + amb) + spec * u.sunc.rgb * nl * sunv;
  /* leaves: the sun through them from behind */
  if (u.mat.x > 0.5) {
    float back = saturate(dot(-N0, L)) * pow(saturate(dot(-V, L)) * 0.5 + 0.5, 2.0);
    col += alb * u.sunc.rgb * back * 0.2 * ao * ao;
  }
  /* BR_ENV_DEBUG=2: the surface colour unlit; 3: the shading normal; 4: the light (white albedo) */
  if (u.jit.w > 1.5) {
    int dm = int(u.jit.w + 0.5);
    col = dm == 2 ? alb * 2.0 : dm == 3 ? (N * 0.5 + 0.5) * 2.0 : dm == 4 ? kd * (u.sunc.rgb * nl + amb)
        : dm == 5 ? float3(ao) * 2.0 : dm == 6 ? kd * 2.0 : dm == 7 ? float3(nl) * 2.0 : dm == 8 ? amb * 2.0
        : float3(MR.rgb) * 2.0; }
  if (u.fogc.w > 0.5 && u.jit.w < 1.5) col = mix(col, u.fogc.rgb, fogof(u, 1.0 / in.oow) / 255.0);
  o.d = float(wfloat(in.oow)) / 65536.0;
  if (u.jit.w > 1.5) { o.c = float4(pow(saturate(col * 0.5), 1.0 / 2.2), 1); o.n = float4(0, 0, 0, 1); o.g = float4(0); return o; }
  if (u.misc.y > 0.5) {                        /* main view: pre-lit, to the fx composite */
    o.c = float4(pow(saturate(col / 4.0), 1.0 / 2.2), 1);
    o.n = float4(N0, 1); o.g = float4(in.wp, 4.0);
    o.a = float4(pow(saturate(alb), 1.0 / 2.2), 1);   /* its colour, for host_fx.m's lamps */
  } else {                                     /* another view: finished here */
    o.c = float4(pow(aces(col * u.misc.z), 1.0 / 2.2), 1);
    o.n = float4(0, 0, 0, 1); o.g = float4(0);
  }
  return o; }
/* the sun's shadow map: the same meshes, placed, through the sun's view */
struct SU { float4x4 M, svp; float4 mat; float4 misc; };
struct SO { float4 pos [[position]]; float2 uv; };
vertex SO esvs(uint vid [[vertex_id]], uint iid [[instance_id]], const device MV *v [[buffer(0)]],
               constant SU &u [[buffer(1)]], const device float4x4 *inst [[buffer(2)]]) {
  MV m = v[vid]; SO o;
  o.pos = u.svp * (inst[iid] * float4(float3(m.p), 1));
  o.uv = m.uv; return o; }
fragment void esfs(SO in [[stage_in]], constant SU &u [[buffer(0)]], texture2d<float> tb [[texture(0)]]) {
  constexpr sampler s(filter::linear, mip_filter::linear, address::repeat);
  if (u.mat.x > 0.5 && tb.sample(s, in.uv).a < 0.5) discard_fragment(); }

/* the far level: the tree's picture from the nearest of its rendered
   directions (remaster_env_impostor.py), on a card that turns about the
   tree's own up axis to face the camera.  mat: y frames, z half width,
   w height (model units).  The frame's normals light it like the model. */
struct IO { float4 pos [[position]]; float3 wp; float2 uv; float oow; float3 rt; float3 up; float3 fw; float2 cq; };
float3 card_basis(float4x4 M, float3 toward, thread float3 &rt, thread float3 &up) {
  up = normalize(M[2].xyz);
  float3 h = toward - up * dot(toward, up);
  h = length(h) > 1e-5 ? normalize(h) : normalize(M[0].xyz);
  rt = normalize(cross(up, h));
  return h; }
float2 card_uv(float4x4 M, float3 h, float2 c, float views) {
  float a = atan2(dot(h, normalize(M[1].xyz)), dot(h, normalize(M[0].xyz)));
  float k = fmod(round(a / (6.2831853 / views)) + views, views);
  return float2((k + c.x * 0.5 + 0.5) / views, 1.0 - c.y); }
vertex IO ivs(uint vid [[vertex_id]], uint iid [[instance_id]], constant EU &u [[buffer(1)]],
              const device float4x4 *inst [[buffer(2)]]) {
  const float2 C[6] = { float2(-1, 0), float2(1, 0), float2(1, 1), float2(-1, 0), float2(1, 1), float2(-1, 1) };
  float2 c = C[vid]; IO o;
  float4x4 M = inst[iid];
  float s = length(M[0].xyz);
  float3 base = M[3].xyz, rt, up;
  float3 h = card_basis(M, u.eye.xyz - base, rt, up);
  float3 w = base + rt * c.x * u.mat.z * s + up * c.y * u.mat.w * s;
  float4 cl = u.P * float4(w, 1);
  float X = u.vpt.x * cl.x + u.vpt.y * cl.w, Y = u.vpt.z * cl.y + u.vpt.w * cl.w;
  float Yd = u.misc.x > 0.5 ? 480.0 * cl.w - Y : Y;
  o.pos = float4(u.map.x * X + u.map.y * cl.w, u.map.z * Yd + u.map.w * cl.w, (cl.z + cl.w) * 0.5, cl.w);
  o.pos.xy += u.jit.xy * cl.w;
  o.wp = w; o.uv = card_uv(M, h, c, u.mat.y); o.oow = 1.0 / cl.w; o.rt = rt; o.up = up; o.fw = h; o.cq = c;
  return o; }
fragment FO ifs(IO in [[stage_in]], constant EU &u [[buffer(0)]],
                texture2d<float> tb [[texture(0)]], texture2d<float> tn [[texture(2)]]) {
  constexpr sampler s(filter::linear, mip_filter::linear, address::clamp_to_edge);
  FO o; o.a = 0;
  float4 B = tb.sample(s, in.uv);
  if (B.a < 0.5) discard_fragment();
  float3 alb = pow(B.rgb, 2.2);
  float3 nc = tn.sample(s, in.uv).xyz * 2.0 - 1.0;
  float3 N = normalize(nc.x * in.rt + nc.y * in.up + nc.z * in.fw);
  float3 L = u.sun.xyz, V = normalize(u.eye.xyz - in.wp);
  float nl = saturate(dot(N, L));
  /* the crown's occlusion as the model has it: across the card is the
     distance from the trunk, up the card the height */
  float ao = mix(0.28, 1.0, saturate(in.cq.x * in.cq.x * 0.85 + in.cq.y * 0.35));
  float3 amb = mix(u.grnd.rgb, u.skyc.rgb, N.z * 0.5 + 0.5) * ao;
  float3 col = alb * 0.96 * (u.sunc.rgb * nl * mix(0.2, 1.0, ao * ao) + amb);
  float back = saturate(dot(-N, L)) * pow(saturate(dot(-V, L)) * 0.5 + 0.5, 2.0);
  col += alb * u.sunc.rgb * back * 0.2 * ao * ao;
  if (u.jit.w > 1.5 && u.jit.w < 2.5) col = alb * 2.0;
  if (u.fogc.w > 0.5) col = mix(col, u.fogc.rgb, fogof(u, 1.0 / in.oow) / 255.0);
  o.d = float(wfloat(in.oow)) / 65536.0;
  if (u.misc.y > 0.5) { o.c = float4(pow(saturate(col / 4.0), 1.0 / 2.2), 1); o.n = float4(N, 1); o.g = float4(in.wp, 4.0);
                        o.a = float4(pow(saturate(alb), 1.0 / 2.2), 1); }
  else { o.c = float4(pow(aces(col * u.misc.z), 1.0 / 2.2), 1); o.n = float4(0, 0, 0, 1); o.g = float4(0); }
  return o; }
/* its shadow: the frame facing the sun, on a card across the sun's direction */
vertex SO isvs(uint vid [[vertex_id]], uint iid [[instance_id]], constant SU &u [[buffer(1)]],
               const device float4x4 *inst [[buffer(2)]]) {
  const float2 C[6] = { float2(-1, 0), float2(1, 0), float2(1, 1), float2(-1, 0), float2(1, 1), float2(-1, 1) };
  float2 c = C[vid]; SO o;
  float4x4 M = inst[iid];
  float s = length(M[0].xyz);
  float3 rt, up;
  float3 h = card_basis(M, u.misc.xyz, rt, up);
  float3 w = M[3].xyz + rt * c.x * u.mat.z * s + up * c.y * u.mat.w * s;
  o.pos = u.svp * float4(w, 1);
  o.uv = card_uv(M, h, c, u.mat.y); return o; }
);

typedef struct {
    float M[16], P[16];
    float vpt[4], eye[4], sun[4], sunc[4], skyc[4], grnd[4], fogc[4], misc[4], map[4], jit[4], mat[4], geo[4];
    float fogtab[64];
} eu;
typedef struct { float M[16], svp[16]; float mat[4]; float misc[4]; } su;

typedef struct { id<MTLTexture> base, orm, nrm; int clip; } emat;
typedef struct { char name[64]; emat m[16]; int nm; int loaded; } easset;
typedef struct { id<MTLBuffer> vb, ib; int ni, mat; } esub;
typedef struct { int asset; char var[32]; esub lod[NLOD][MAX_SUB]; int nsub[NLOD]; float h, r; int ok;
                 id<MTLTexture> ibase, inrm; float iviews, ihalf, iheight; } emodel;
typedef struct { int inst, model; float M[16]; float c[3], r, maxd; } eput;

static id<MTLDevice> D;
static id<MTLRenderPipelineState> g_pipe, g_spipe, g_ipipe, g_ispipe;
static id<MTLDepthStencilState> g_ds, g_dsall;
static id<MTLTexture> g_flatn, g_white;
static int g_state;                       /* 0 not tried, 1 ready, -1 unavailable */
#define MAX_ASSETS 64
#define MAX_MODELS 256
/* fixed tables: they hold Metal objects, which ARC does not let realloc move */
static easset g_assets[MAX_ASSETS]; static int g_nassets;
static emodel g_models[MAX_MODELS]; static int g_nmodels;
static eput *g_puts; static int g_nputs;
/* placements by model, for the shadow pass: g_mput[g_mfirst[m] ..] (scatter left out) */
static int *g_mput, g_mfirst[MAX_MODELS + 1];
static int g_first[MAX_INST], g_count[MAX_INST];
static int *g_hide[MAX_INST]; static int g_nhide[MAX_INST];
static u32 g_alt[MAX_INST];               /* the Remastered copy of an instance's list */
/* list address (original or copy) -> instance, for the instances that have
 * cards hidden or models standing on them; open addressing */
#define OWN_N 8192
static u32 g_own_key[OWN_N]; static int g_own_val[OWN_N];
static void own_put(u32 a, int idx)
{
    u32 h = (a * 2654435761u) >> 19;
    if (!a) return;
    while (g_own_key[h & (OWN_N - 1)] && g_own_key[h & (OWN_N - 1)] != a) h++;
    g_own_key[h & (OWN_N - 1)] = a; g_own_val[h & (OWN_N - 1)] = idx;
}
/* native/env.m: the instance whose list starts at `a`, or -1 */
int henv_owner(u32 a)
{
    u32 h = (a * 2654435761u) >> 19;
    if (!a) return -1;
    while (g_own_key[h & (OWN_N - 1)]) {
        if (g_own_key[h & (OWN_N - 1)] == a) return g_own_val[h & (OWN_N - 1)];
        h++;
    }
    return -1;
}
static u32 g_track_sig[3], g_track_tab;   /* which track the placements belong to */
static int g_track_ok;
static unsigned char *g_canopy;            /* the forest's canopy (canopy_load) */
static int g_canopy_w, g_canopy_h, g_canopy_gen;
static float g_canopy_at[3];
static char g_track_name[32];              /* the loaded track, by its placements file */
static id<MTLBuffer> g_inst[6];           /* instance matrices for the shadow pass: 2 cascades x 3 frames */
static int g_inst_i;
/* this frame's instance matrices for the scene draws, a ring of three frames */
#define DRAW_INST (1 << 18)
static id<MTLBuffer> g_dinst[3]; static size_t g_dinst_used; static unsigned g_dinst_serial = ~0u;
static float g_mainP[16]; static unsigned g_main_serial = ~0u;
/* BR_ENV_STAT=1: the seam's counts (native/env.m reports through henv_note) */
static long g_note[8];
void henv_note(int k) { if (k >= 0 && k < 8) g_note[k]++; }
/* BR_ENV_STAT=1: per 120 frames, markers run, models drawn and culled */
static unsigned g_st_frame = ~0u; static long g_st_mark, g_st_draw, g_st_cull, g_st_alt, g_st_card, g_st_tris;
void henv_stat_tick(void);
void henv_stat_tick(void)
{
    static int on = -1;
    if (on < 0) on = getenv("BR_ENV_STAT") != NULL;
    if (!on || hglide_swaps() == g_st_frame) return;
    if (hglide_swaps() % 120 == 0)
        fprintf(stderr, "env: seam calls %ld active %ld anchored %ld marked %ld handled %ld | fx %d state %d track_ok %d puts %d\n",
                g_note[0], g_note[1], g_note[2], g_note[3], g_note[4], hfx_on(), g_state, g_track_ok, g_nputs);
    g_st_frame = hglide_swaps();
    if (g_st_frame % 120 == 0) {
        fprintf(stderr, "env: frame %u markers %ld drawn %ld (as cards %ld) culled %ld tris/frame %ld (per 120 frames), card-less lists %ld\n",
                g_st_frame, g_st_mark, g_st_draw, g_st_card, g_st_cull, g_st_tris / 120, g_st_alt);
        g_st_mark = g_st_draw = g_st_cull = g_st_card = g_st_tris = 0;
    }
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
static NSString *textures_dir(void)
{
    NSString *m = models_dir();
    return [[m stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"textures"];
}

/* host_load.m: decode, parallel loops, batched mip chains */
u8 *hload_png(const char *path, int *w, int *h, int *spp);
void hload_for(int n, void (^f)(int i));
void hload_rows(int h, void (^f)(int y0, int y1));
void hload_mips(NSMutableArray *batch, id<MTLTexture> t);
void hload_flush(NSMutableArray *batch);

/* the share of texels whose alpha, times `k`, passes the cut-out test, from
 * the texels' alpha histogram (the same count as testing every texel) */
static double coverage(const long *hist, long n, double k)
{
    long c = 0;
    int a;
    for (a = 0; a < 256; a++) if (a * k >= 127.5) c += hist[a];
    return (double)c / n;
}
static void alpha_hist(const u8 *px, long n, long *hist)
{
    long i;
    memset(hist, 0, sizeof(long) * 256);
    for (i = 0; i < n; i++) hist[px[i * 4 + 3]]++;
}

/* 8-bit RGBA PNG, straight alpha, halved until it fits `maxdim`, mip-mapped.
 * A cut-out texture (`clip`) gets coverage-preserving mip levels: each level's
 * alpha is scaled so the same share of it passes the test as at full size.
 * Plain averaging thins a leaf atlas (a quarter opaque) below the cut-out
 * threshold within a few levels, and the leaves vanish with distance.
 * Any thread; the CPU passes split by rows.  A plain texture's mip chain is
 * left to `batch` (hload_flush). */
static id<MTLTexture> load_tex_clip(NSString *path, int maxdim, int clip, NSMutableArray *batch);
static id<MTLTexture> load_tex(NSString *path, int maxdim, NSMutableArray *batch) { return load_tex_clip(path, maxdim, 0, batch); }
static id<MTLTexture> load_tex_clip(NSString *path, int maxdim, int clip, NSMutableArray *batch)
{
    MTLTextureDescriptor *td;
    id<MTLTexture> t;
    int w, h, spp;
    u8 *px, *nx;
    if (![[NSFileManager defaultManager] fileExistsAtPath:path]) { fprintf(stderr, "env: missing %s\n", path.UTF8String); return nil; }
    if (!(px = hload_png(path.UTF8String, &w, &h, &spp))) { fprintf(stderr, "env: %s is not 8-bit RGB(A)\n", path.UTF8String); return nil; }
    while ((w > maxdim || h > maxdim) && w > 1 && h > 1) {
        int w2 = w / 2, h2 = h / 2, sw = w;
        const u8 *src = px;
        nx = malloc((size_t)w2 * h2 * 4);
        hload_rows(h2, ^(int y0, int y1) {
            int y, x, c;
            for (y = y0; y < y1; y++)
                for (x = 0; x < w2; x++)
                    for (c = 0; c < 4; c++) {
                        const u8 *s = src + ((size_t)(2 * y) * sw + 2 * x) * 4 + c;
                        nx[((size_t)y * w2 + x) * 4 + c] = (u8)((s[0] + s[4] + s[(size_t)sw * 4] + s[(size_t)sw * 4 + 4] + 2) / 4);
                    }
        });
        free(px); px = nx; w = w2; h = h2;
    }
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                            width:(NSUInteger)w height:(NSUInteger)h mipmapped:YES];
    td.usage = MTLTextureUsageShaderRead;
    t = [D newTextureWithDescriptor:td];
    if (clip) {
        /* level 0: a transparent texel takes its opaque neighbours' colour,
           grown outward a few texels, so filtering at a leaf's edge never
           pulls in what the transparent texels happen to hold (black in the
           leaf atlases, white in the rendered far cards).  A pass writes only
           texels that were transparent and reads only ones that were opaque,
           so its rows run in parallel. */
        {
            u8 *ok = malloc((size_t)w * h), *ok2 = malloc((size_t)w * h);
            const u8 *okr = ok;
            u8 *pxw = px;
            int pass, i2;
            for (i2 = 0; i2 < w * h; i2++) ok[i2] = px[i2 * 4 + 3] >= 128;
            for (pass = 0; pass < 6; pass++) {
                memcpy(ok2, ok, (size_t)w * h);
                hload_rows(h, ^(int y0, int y1) {
                    int y, x;
                    for (y = y0; y < y1; y++)
                        for (x = 0; x < w; x++) {
                            int r = 0, g = 0, b2 = 0, n = 0, dy, dx;
                            if (okr[(size_t)y * w + x]) continue;
                            for (dy = -1; dy <= 1; dy++)
                                for (dx = -1; dx <= 1; dx++) {
                                    int yy = y + dy, xx = x + dx;
                                    const u8 *s2;
                                    if (yy < 0 || yy >= h || xx < 0 || xx >= w || !okr[(size_t)yy * w + xx]) continue;
                                    s2 = pxw + ((size_t)yy * w + xx) * 4;
                                    r += s2[0]; g += s2[1]; b2 += s2[2]; n++;
                                }
                            if (n) {
                                u8 *d = pxw + ((size_t)y * w + x) * 4;
                                d[0] = (u8)(r / n); d[1] = (u8)(g / n); d[2] = (u8)(b2 / n);
                                ok2[(size_t)y * w + x] = 1;
                            }
                        }
                });
                memcpy(ok, ok2, (size_t)w * h);
            }
            free(ok); free(ok2);
            [t replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)w, (NSUInteger)h) mipmapLevel:0 withBytes:px bytesPerRow:(NSUInteger)w * 4];
        }
        /* every level on the CPU, box-filtered, alpha rescaled to the full
           size's coverage (a binary search on the scale) */
        long hist[256];
        alpha_hist(px, (long)w * h, hist);
        double cov0 = coverage(hist, (long)w * h, 1.0);
        int lw = w, lh = h, lev = 0;
        u8 *cur = px;
        while (lw > 1 || lh > 1) {
            int w2 = lw > 1 ? lw / 2 : 1, h2 = lh > 1 ? lh / 2 : 1, slw = lw, slh = lh;
            u8 *nx2 = malloc((size_t)w2 * h2 * 4);
            const u8 *src = cur;
            double lo = 0.0, hi = 8.0, k = 1.0;
            /* colour weighted by alpha: the atlas is black where it is
               transparent, and a plain average darkens every leaf edge and
               then every leaf as the levels shrink */
            hload_rows(h2, ^(int y0, int y1) {
                int x2, y2, c;
                for (y2 = y0; y2 < y1; y2++)
                    for (x2 = 0; x2 < w2; x2++) {
                        int xa = x2 * 2 < slw ? x2 * 2 : slw - 1, xb = x2 * 2 + 1 < slw ? x2 * 2 + 1 : slw - 1;
                        int ya = y2 * 2 < slh ? y2 * 2 : slh - 1, yb = y2 * 2 + 1 < slh ? y2 * 2 + 1 : slh - 1;
                        const u8 *q[4] = { src + ((size_t)ya * slw + xa) * 4, src + ((size_t)ya * slw + xb) * 4,
                                           src + ((size_t)yb * slw + xa) * 4, src + ((size_t)yb * slw + xb) * 4 };
                        u8 *d = nx2 + ((size_t)y2 * w2 + x2) * 4;
                        int asum = q[0][3] + q[1][3] + q[2][3] + q[3][3];
                        for (c = 0; c < 3; c++) {
                            int ws = q[0][c] * q[0][3] + q[1][c] * q[1][3] + q[2][c] * q[2][3] + q[3][c] * q[3][3];
                            d[c] = asum ? (u8)((ws + asum / 2) / asum) : (u8)((q[0][c] + q[1][c] + q[2][c] + q[3][c] + 2) / 4);
                        }
                        d[3] = (u8)((asum + 2) / 4);
                    }
            });
            /* the scale whose coverage matches the full size's */
            alpha_hist(nx2, (long)w2 * h2, hist);
            for (int it = 0; it < 16; it++) {
                k = (lo + hi) * 0.5;
                if (coverage(hist, (long)w2 * h2, k) < cov0) lo = k; else hi = k;
            }
            {
                u8 *out = malloc((size_t)w2 * h2 * 4);
                memcpy(out, nx2, (size_t)w2 * h2 * 4);
                for (int i = 0; i < w2 * h2; i++) { double a = out[i * 4 + 3] * k; out[i * 4 + 3] = (u8)(a > 255 ? 255 : a); }
                [t replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)w2, (NSUInteger)h2) mipmapLevel:(NSUInteger)(++lev)
                       withBytes:out bytesPerRow:(NSUInteger)w2 * 4];
                free(out);
            }
            if (cur != px) free(cur);
            cur = nx2; lw = w2; lh = h2;
        }
        if (cur != px) free(cur);
        free(px);
        return t;
    }
    [t replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)w, (NSUInteger)h) mipmapLevel:0 withBytes:px bytesPerRow:(NSUInteger)w * 4];
    free(px);
    hload_mips(batch, t);
    return t;
}

static int load_rcm(NSString *path, esub *s)
{
    NSData *d = [NSData dataWithContentsOfFile:path options:NSDataReadingMappedIfSafe error:nil];
    const u8 *p;
    u32 nv, ni;
    if (!d || d.length < 12) { fprintf(stderr, "env: missing %s\n", path.UTF8String); return 0; }
    p = d.bytes;
    if (memcmp(p, "RCM1", 4)) return 0;
    memcpy(&nv, p + 4, 4); memcpy(&ni, p + 8, 4);
    if (d.length < 12 + (size_t)nv * 48 + (size_t)ni * 4 || !nv || !ni) return 0;
    s->vb = [D newBufferWithBytes:p + 12 length:(size_t)nv * 48 options:MTLResourceStorageModeShared];
    s->ib = [D newBufferWithBytes:p + 12 + (size_t)nv * 48 length:(size_t)ni * 4 options:MTLResourceStorageModeShared];
    s->ni = (int)ni;
    return 1;
}

static int asset_index(const char *name)
{
    int i;
    for (i = 0; i < g_nassets; i++) if (!strcmp(g_assets[i].name, name)) return i;
    if (g_nassets == MAX_ASSETS) return MAX_ASSETS - 1;
    snprintf(g_assets[g_nassets].name, sizeof g_assets[0].name, "%s", name);
    return g_nassets++;
}

/* Models [m0, m1) of g_models, new: each variant's meshes at every level,
 * and its asset's materials the first time the asset is used.  Every file is
 * one job and the jobs run across the cores, largest (the textures) first;
 * then one GPU pass builds the plain textures' mip chains. */
static void load_models(int m0, int m1)
{
    NSMutableArray *batch = [NSMutableArray new], *tjobs = [NSMutableArray new], *mjobs = [NSMutableArray new];
    NSMutableDictionary *json = [NSMutableDictionary new];
    NSMutableArray *found = [NSMutableArray new];        /* per model: @[variant, per level: per file result] or NSNull */
    int newa[MAX_ASSETS], nnewa = 0, mi, k;
    for (mi = m0; mi < m1; mi++) {
        emodel *m = &g_models[mi];
        easset *a = &g_assets[m->asset];
        NSString *an = @(a->name);
        NSString *dir = [models_dir() stringByAppendingPathComponent:an];
        NSString *tdir = [textures_dir() stringByAppendingPathComponent:an];
        NSDictionary *j = json[an], *vv = nil;
        NSMutableArray *levels = [NSMutableArray new];
        if (!j) {
            NSData *jd = [NSData dataWithContentsOfFile:[dir stringByAppendingPathComponent:@"bake.json"]];
            j = jd ? [NSJSONSerialization JSONObjectWithData:jd options:0 error:nil] : nil;
            if (!j) { fprintf(stderr, "env: no bake.json for %s\n", a->name); j = (id)[NSNull null]; }
            json[an] = j;
        }
        if ((id)j == [NSNull null]) { [found addObject:[NSNull null]]; continue; }
        if (!a->loaded) {
            NSArray *mats = j[@"materials"];
            a->loaded = 1;
            a->nm = (int)MIN(mats.count, 16u);
            newa[nnewa++] = m->asset;
            for (k = 0; k < a->nm; k++) {
                NSDictionary *mt = mats[k], *tx = mt[@"textures"];
                int clip = [mt[@"alpha_clip"] boolValue];
                /* leaves at full size; bark and the rest at half: they are seen smaller */
                int dim = clip ? 2048 : 1024;
                emat *e = &a->m[k];
                e->clip = clip;
                if (tx[@"base"]) { NSString *p = [tdir stringByAppendingPathComponent:tx[@"base"]];
                                   [tjobs addObject:^{ e->base = load_tex_clip(p, dim, clip, batch); }]; }
                if (tx[@"orm"]) { NSString *p = [tdir stringByAppendingPathComponent:tx[@"orm"]];
                                  [tjobs addObject:^{ e->orm = load_tex(p, 1024, batch); }]; }
                if (tx[@"nrm"]) { NSString *p = [tdir stringByAppendingPathComponent:tx[@"nrm"]];
                                  [tjobs addObject:^{ e->nrm = load_tex(p, dim, batch); }]; }
            }
        }
        for (NSDictionary *v in j[@"variants"]) if (!strcmp([v[@"name"] UTF8String], m->var)) { vv = v; break; }
        if (!vv) { fprintf(stderr, "env: %s has no variant %s\n", a->name, m->var); [found addObject:[NSNull null]]; continue; }
        {
            NSArray *lods = vv[@"lods"];
            for (k = 0; k < NLOD && k < (int)lods.count; k++) {
                NSArray *files = lods[k][@"files"];
                NSMutableArray *res = [NSMutableArray new];
                NSUInteger fi;
                for (fi = 0; fi < files.count; fi++) [res addObject:[NSNull null]];
                for (fi = 0; fi < files.count; fi++) {
                    NSString *p = [dir stringByAppendingPathComponent:files[fi][@"file"]];
                    [mjobs addObject:^{
                        esub s = { 0 };
                        if (load_rcm(p, &s)) @synchronized (res) { res[fi] = @[ s.vb, s.ib, @(s.ni) ]; }
                    }];
                }
                [levels addObject:res];
            }
        }
        {
            NSDictionary *im = vv[@"impostor"];
            if (im) {
                NSString *pb = [tdir stringByAppendingPathComponent:im[@"base"]], *pn = [tdir stringByAppendingPathComponent:im[@"nrm"]];
                [tjobs insertObject:^{ m->ibase = load_tex_clip(pb, 4096, 1, batch); } atIndex:0];
                [tjobs addObject:^{ m->inrm = load_tex(pn, 4096, batch); }];
            }
        }
        [found addObject:@[ vv, levels ]];
    }
    [tjobs addObjectsFromArray:mjobs];
    hload_for((int)tjobs.count, ^(int i) { @autoreleasepool { ((void (^)(void))tjobs[(NSUInteger)i])(); } });
    hload_flush(batch);
    /* what failed or was never given stands in as white / flat */
    for (k = 0; k < nnewa; k++) {
        easset *a = &g_assets[newa[k]];
        int q;
        for (q = 0; q < a->nm; q++) {
            if (!a->m[q].base) a->m[q].base = g_white;
            if (!a->m[q].orm) a->m[q].orm = g_white;
            if (!a->m[q].nrm) a->m[q].nrm = g_flatn;
        }
    }
    for (mi = m0; mi < m1; mi++) {
        emodel *m = &g_models[mi];
        id fd = found[(NSUInteger)(mi - m0)];
        NSDictionary *vv;
        NSArray *levels, *bb, *lods;
        if (fd == [NSNull null]) continue;
        vv = fd[0]; levels = fd[1];
        lods = vv[@"lods"]; bb = vv[@"bounds"];
        m->h = [bb[1][2] floatValue] - [bb[0][2] floatValue];
        m->r = 0;
        for (k = 0; k < 3; k++) {
            float e = fmaxf(fabsf([bb[0][k] floatValue]), fabsf([bb[1][k] floatValue]));
            m->r += e * e;
        }
        m->r = sqrtf(m->r);
        /* each level: its files in order, the ones that loaded, up to MAX_SUB */
        for (k = 0; k < (int)levels.count; k++) {
            NSArray *res = levels[(NSUInteger)k], *files = lods[(NSUInteger)k][@"files"];
            int n = 0;
            NSUInteger fi;
            for (fi = 0; fi < res.count && n < MAX_SUB; fi++) {
                NSArray *r = res[fi];
                if ((id)r == [NSNull null]) continue;
                m->lod[k][n].vb = r[0]; m->lod[k][n].ib = r[1]; m->lod[k][n].ni = [r[2] intValue];
                m->lod[k][n].mat = [files[fi][@"mat"] intValue];
                n++;
            }
            m->nsub[k] = n;
        }
        for (; k < NLOD; k++) {
            int s2;
            for (s2 = 0; s2 < MAX_SUB; s2++) m->lod[k][s2] = m->lod[k - 1][s2];
            m->nsub[k] = m->nsub[k - 1];
        }
        m->ok = m->nsub[0] > 0;
        {
            NSDictionary *im = vv[@"impostor"];
            if (im) {
                m->iviews = [im[@"views"] floatValue];
                m->ihalf = [im[@"half"] floatValue];
                m->iheight = [im[@"height"] floatValue];
                if (!m->ibase || !m->inrm) m->ibase = nil;
            }
        }
    }
}

static void clear_track(void)
{
    int i;
    for (i = 0; i < MAX_INST; i++) { free(g_hide[i]); g_hide[i] = NULL; g_nhide[i] = 0; g_alt[i] = 0; g_count[i] = 0; }
    free(g_puts); g_puts = NULL; g_nputs = 0;
    memset(g_own_key, 0, sizeof g_own_key);
    free(g_canopy); g_canopy = NULL; g_canopy_gen++;
    g_track_name[0] = 0;
    g_track_ok = 0;
}

/* One placements file, read: its "put" lines as written, its "hide" lines,
 * and its assets as models (loaded).  ports/common/models/placements/<track>.env:
 *   track <name> <faces> <vertices> <instances>
 *   asset <k> <asset> <variant>
 *   hide <instance> <offset>...
 *   put <instance> <k> <16 matrix floats> [<draw distance>]  */
typedef struct { int inst, k; float M[16], maxd; } rput;
typedef struct { int inst, n; int *o; } rhide;
typedef struct {
    char name[64]; u32 sig[3];
    int nmap, *map;                 /* asset k -> model, or -1 */
    rput *puts; long nputs;
    rhide *hides; int nhides;
} tpack;

static void pack_free(tpack *p)
{
    int i;
    if (!p) return;
    for (i = 0; i < p->nhides; i++) free(p->hides[i].o);
    free(p->hides); free(p->puts); free(p->map); free(p);
}

/* the "track" line of a placements file: 1 if it reads */
static int pack_header(const char *path, char *name, u32 *sig)
{
    char ln[256];
    unsigned f, v, n;
    FILE *fp = fopen(path, "r");
    int ok = 0;
    if (!fp) return 0;
    if (fgets(ln, sizeof ln, fp) && sscanf(ln, "track %63s %u %u %u", name, &f, &v, &n) == 4) {
        sig[0] = f; sig[1] = v; sig[2] = n; ok = 1;
    }
    fclose(fp);
    return ok;
}

/* Read a placements file: the lines split across the cores (each band of
 * the file its own "put" list, joined in file order), then the assets it
 * names loaded as models.  Any thread; one at a time. */
static tpack *pack_read(const char *path)
{
    enum { NB = 64 };
    tpack *p;
    char *buf;
    size_t len;
    long *cut = malloc(sizeof(long) * (NB + 1));
    rput **bp = calloc(NB, sizeof *bp);
    long *bn = calloc(NB, sizeof *bn);
    NSMutableArray *other = [NSMutableArray new];   /* per band: the asset and hide lines' offsets */
    int i, m0;
    {
        FILE *fp = fopen(path, "rb");
        long sz;
        if (!fp) return NULL;
        fseek(fp, 0, SEEK_END); sz = ftell(fp); fseek(fp, 0, SEEK_SET);
        buf = malloc((size_t)sz + 1);
        len = fread(buf, 1, (size_t)sz, fp);
        fclose(fp);
        buf[len] = 0;
    }
    p = calloc(1, sizeof *p);
    {
        char *e = strchr(buf, '\n');
        unsigned f, v, n;
        if (e) *e = 0;
        if (sscanf(buf, "track %63s %u %u %u", p->name, &f, &v, &n) != 4) { free(buf); free(p); free(cut); free(bp); free(bn); return NULL; }
        p->sig[0] = f; p->sig[1] = v; p->sig[2] = n;
        cut[0] = e ? e + 1 - buf : (long)len;
    }
    /* bands that start on a line */
    for (i = 1; i < NB; i++) {
        long c = cut[0] + (long)((len - (size_t)cut[0]) * (size_t)i / NB);
        if (c < cut[i - 1]) c = cut[i - 1];
        while (c < (long)len && buf[c - 1] != '\n') c++;
        cut[i] = c;
    }
    cut[NB] = (long)len;
    for (i = 0; i < NB; i++) [other addObject:[NSMutableArray new]];
    hload_for(NB, ^(int b) {
        char *l = buf + cut[b], *end = buf + cut[b + 1];
        long cap = 1024, n = 0;
        rput *out = malloc(sizeof *out * (size_t)cap);
        NSMutableArray *oth = other[(NSUInteger)b];
        while (l < end) {
            char *nl = memchr(l, '\n', (size_t)(end - l));
            if (nl) *nl = 0;
            if (!strncmp(l, "put ", 4)) {
                /* as sscanf("put %d %d" + 17 x " %f") would, 18 fields at least */
                rput q;
                char *s = l + 4, *e;
                float *f = q.M;
                int got = 0;
                q.maxd = 0;
                q.inst = (int)strtol(s, &e, 10); if (e != s) { got++; s = e;
                q.k = (int)strtol(s, &e, 10); if (e != s) { got++; s = e;
                for (; got < 19; got++) {
                    float x = strtof(s, &e);
                    if (e == s) break;
                    s = e;
                    if (got < 18) f[got - 2] = x; else q.maxd = x;
                } } }
                if (got >= 18 && q.inst >= 0 && q.inst < MAX_INST) {
                    if (n == cap) out = realloc(out, sizeof *out * (size_t)(cap *= 2));
                    out[n++] = q;
                }
            } else if (!strncmp(l, "asset ", 6) || !strncmp(l, "hide ", 5)) {
                [oth addObject:@(l - buf)];
            }
            if (!nl) break;
            l = nl + 1;
        }
        bp[b] = out; bn[b] = n;
    });
    for (i = 0; i < NB; i++) p->nputs += bn[i];
    p->puts = malloc(sizeof *p->puts * (size_t)(p->nputs + 1));
    {
        long o = 0;
        for (i = 0; i < NB; i++) { memcpy(p->puts + o, bp[i], sizeof *p->puts * (size_t)bn[i]); o += bn[i]; free(bp[i]); }
    }
    free(cut); free(bp); free(bn);
    /* the asset and hide lines, in file order */
    m0 = g_nmodels;
    for (NSArray *oth in other)
        for (NSNumber *off in oth) {
            const char *l = buf + off.longValue;
            if (l[0] == 'a') {
                int k; char an[64], vn[32];
                if (sscanf(l, "asset %d %63s %31s", &k, an, vn) != 3 || k < 0) continue;
                if (k >= p->nmap) { p->map = realloc(p->map, sizeof *p->map * (size_t)(k + 1)); while (p->nmap <= k) p->map[p->nmap++] = -1; }
                {   /* one model per (asset, variant) across tracks */
                    int ai = asset_index(an), mi;
                    for (mi = 0; mi < g_nmodels; mi++) if (g_models[mi].asset == ai && !strcmp(g_models[mi].var, vn)) break;
                    if (mi == g_nmodels && g_nmodels < MAX_MODELS) {
                        g_models[g_nmodels].asset = ai;
                        snprintf(g_models[g_nmodels].var, sizeof g_models[0].var, "%s", vn);
                        g_nmodels++;
                    }
                    p->map[k] = mi < g_nmodels ? mi : -1;
                }
            } else {
                int inst, n = 0, cap = 16, o; const char *s = l + 5; char *e;
                int *lst;
                inst = (int)strtol(s, &e, 10); s = e;
                if (inst < 0 || inst >= MAX_INST) continue;
                lst = malloc(sizeof(int) * (size_t)cap);
                for (;;) {
                    o = (int)strtol(s, &e, 10);
                    if (e == s) break;
                    s = e;
                    if (n == cap) lst = realloc(lst, sizeof(int) * (size_t)(cap *= 2));
                    lst[n++] = o;
                }
                p->hides = realloc(p->hides, sizeof *p->hides * (size_t)(p->nhides + 1));
                p->hides[p->nhides++] = (rhide){ inst, n, lst };
            }
        }
    free(buf);
    load_models(m0, g_nmodels);
    return p;
}

/* The forest's canopy over the ground (placements/<track>.canopy, written
 * with the placements): crown cover on a grid, 0..255, for the lighting
 * pass's forest floor and the light under the trees (host_fx.m).  Header
 * "CANOPY1 x0 y0 cell w h", then w*h bytes, row y from y0 up. */
static void canopy_load(const char *name)
{
    NSString *path = [[models_dir() stringByAppendingPathComponent:@"placements"]
                      stringByAppendingPathComponent:[NSString stringWithFormat:@"%s.canopy", name]];
    FILE *f = fopen(path.UTF8String, "rb");
    float x0, y0, cell;
    int w, h;
    free(g_canopy); g_canopy = NULL; g_canopy_gen++;
    if (!f) return;
    if (fscanf(f, "CANOPY1 %f %f %f %d %d", &x0, &y0, &cell, &w, &h) == 5 && fgetc(f) == '\n'
        && w > 0 && h > 0 && w <= 8192 && h <= 8192 && cell > 0) {
        g_canopy = malloc((size_t)w * (size_t)h);
        if (fread(g_canopy, 1, (size_t)w * (size_t)h, f) != (size_t)w * (size_t)h) { free(g_canopy); g_canopy = NULL; }
        else { g_canopy_w = w; g_canopy_h = h; g_canopy_at[0] = x0; g_canopy_at[1] = y0; g_canopy_at[2] = cell; }
    }
    fclose(f);
}
/* host_sky.m: the track the game has loaded, known by its header's counts
 * (the chosen-track setting does not always say: a scripted race on Mountain
 * reads 5); "" until its placements are applied */
const char *henv_track_name(void) { return g_track_name; }

/* host_fx.m: the canopy grid, or 0; *gen changes whenever it does */
int henv_canopy(const unsigned char **px, int *w, int *h, float *x0, float *y0, float *cell, int *gen)
{
    *gen = g_canopy_gen;
    if (!g_canopy || !g_track_ok) return 0;
    *px = g_canopy; *w = g_canopy_w; *h = g_canopy_h;
    *x0 = g_canopy_at[0]; *y0 = g_canopy_at[1]; *cell = g_canopy_at[2];
    return 1;
}

/* the placements the game's track takes, from a read file */
static void pack_apply(const tpack *p, const u32 *sig, u32 tab)
{
    long j;
    int i;
    fprintf(stderr, "env: track %s: placements %s.env\n", p->name, p->name);
    snprintf(g_track_name, sizeof g_track_name, "%s", p->name);
    canopy_load(p->name);
    for (i = 0; i < p->nhides; i++) {
        const rhide *h = &p->hides[i];
        free(g_hide[h->inst]);
        g_hide[h->inst] = malloc(sizeof(int) * (size_t)(h->n ? h->n : 1));
        memcpy(g_hide[h->inst], h->o, sizeof(int) * (size_t)h->n);
        g_nhide[h->inst] = h->n;
    }
    g_puts = malloc(sizeof *g_puts * (size_t)(p->nputs + 1));
    for (j = 0; j < p->nputs; j++) {
        const rput *r = &p->puts[j];
        eput q;
        const float *M;
        if (r->k < 0 || r->k >= p->nmap || p->map[r->k] < 0 || !g_models[p->map[r->k]].ok) continue;
        q.inst = r->inst; q.model = p->map[r->k]; q.maxd = r->maxd;
        memcpy(q.M, r->M, sizeof q.M);
        M = q.M;
        {
            emodel *m = &g_models[q.model];
            float s = sqrtf(M[0] * M[0] + M[1] * M[1] + M[2] * M[2]);
            q.c[0] = M[12] + M[8] * m->h * 0.5f; q.c[1] = M[13] + M[9] * m->h * 0.5f; q.c[2] = M[14] + M[10] * m->h * 0.5f;
            q.r = m->r * s;
        }
        g_puts[g_nputs++] = q;
    }
    (void)sig; (void)tab;
}

/* Prefetch: the placements and models for the track the game has chosen
 * (g_brCfgChosenTrack, as host_sky.m maps it), read on a background queue
 * from the moment the track is picked, so by the time the game has loaded it
 * and draws its first frame they are ready.  load_track takes them if the
 * loaded track's counts match, else reads the right file itself. */
static const char *const PACKENV[16] = {
    "desert", "mountain", "coast", "mine", "amazon", "race",
    "desert", "mountain", "coast", "mine", "amazon", "race",
    "desert", "bonus", "bonus", "desert"
};
static dispatch_queue_t g_pq;
static dispatch_group_t g_pg;
static tpack *g_pack;                     /* the last file read; the queue writes it, load_track reads it after a wait */
static volatile u32 g_pwant = ~0u;        /* the chosen track the newest prefetch is for */
static void setup_once(void);
static int env_off(void)
{
    static int off = -1;
    if (off < 0) off = getenv("BR_ENV") && !atoi(getenv("BR_ENV"));
    return off;
}
void henv_prefetch(void)
{
    u32 t = H32(0x100B3014u);
    NSString *path;
    if (env_off() || !hfx_on() || t == g_pwant) return;
    if (!g_pq) { g_pq = dispatch_queue_create("env.load", DISPATCH_QUEUE_SERIAL); g_pg = dispatch_group_create(); }
    g_pwant = t;
    path = [[models_dir() stringByAppendingPathComponent:@"placements"]
            stringByAppendingPathComponent:[NSString stringWithFormat:@"%s.env", PACKENV[t & 15]]];
    dispatch_group_async(g_pg, g_pq, ^{
        @autoreleasepool {
            char name[64]; u32 sig[3];
            tpack *p;
            if (g_pwant != t) return;            /* a newer pick is queued behind */
            setup_once();
            if (g_state != 1 || !pack_header(path.UTF8String, name, sig)) return;
            if (g_pack && !memcmp(g_pack->sig, sig, sizeof sig)) return;
            if ((p = pack_read(path.UTF8String))) { pack_free(g_pack); g_pack = p; }
        }
    });
}

/* the placements for the track the game has loaded, found by its counts */
static void load_track(void)
{
    u32 sig[3] = { W_LD(u32, TRK_HDR, 0x08), W_LD(u32, TRK_HDR, 0x10), W_LD(u32, TRK_HDR, 0x64) };
    u32 tab = W_LD(u32, OBJ_TABLE, 0);
    int i;
    if (!memcmp(sig, g_track_sig, sizeof sig) && tab == g_track_tab) return;
    if (g_pg) dispatch_group_wait(g_pg, DISPATCH_TIME_FOREVER);
    memcpy(g_track_sig, sig, sizeof sig); g_track_tab = tab;
    /* BR_TRKDUMP=dir: the loaded track image as the game holds it once
     * loaded (palettes filled in, pointers relocated), for the offline
     * placement tools.  The header's +0x84 is the payload, file 0x230. */
    if (getenv("BR_TRKDUMP")) {
        u32 base = W_LD(u32, TRK_HDR, 0x84) - 0x230u;
        char path[1024]; FILE *f;
        snprintf(path, sizeof path, "%s/trk_%u_%u_%u.bin", getenv("BR_TRKDUMP"), sig[0], sig[1], sig[2]);
        if ((f = fopen(path, "wb"))) {
            fwrite(&base, 4, 1, f);
            fwrite(W_P(base), 1, 4000000, f);
            fclose(f);
            fprintf(stderr, "env: dumped track image at 0x%08X to %s\n", base, path);
        }
    }
    clear_track();
    if (!g_pack || memcmp(g_pack->sig, sig, sizeof sig)) {
        NSString *pdir = [models_dir() stringByAppendingPathComponent:@"placements"];
        for (NSString *fn in [[NSFileManager defaultManager] contentsOfDirectoryAtPath:pdir error:nil]) {
            const char *path = [pdir stringByAppendingPathComponent:fn].UTF8String;
            char name[64]; u32 fs[3];
            tpack *p;
            if (![fn hasSuffix:@".env"] || !pack_header(path, name, fs) || memcmp(fs, sig, sizeof sig)) continue;
            if ((p = pack_read(path))) { pack_free(g_pack); g_pack = p; }
            break;
        }
    }
    if (g_pack && !memcmp(g_pack->sig, sig, sizeof sig)) pack_apply(g_pack, sig, tab);
    if (!g_nputs) fprintf(stderr, "env: track %u faces %u vertices %u instances: no placements\n", sig[0], sig[1], sig[2]);
    {   /* the shadow casters, by model: small scatter (a draw distance) casts nothing */
        int cnt[MAX_MODELS + 1], mm;
        memset(cnt, 0, sizeof cnt);
        for (i = 0; i < g_nputs; i++) if (g_puts[i].maxd <= 0) cnt[g_puts[i].model]++;
        g_mfirst[0] = 0;
        for (mm = 0; mm < MAX_MODELS; mm++) g_mfirst[mm + 1] = g_mfirst[mm] + cnt[mm];
        free(g_mput); g_mput = malloc(sizeof(int) * (size_t)(g_mfirst[MAX_MODELS] + 1));
        memset(cnt, 0, sizeof cnt);
        for (i = 0; i < g_nputs; i++) if (g_puts[i].maxd <= 0) { int m2 = g_puts[i].model; g_mput[g_mfirst[m2] + cnt[m2]++] = i; }
    }
    /* by anchor: the list is written instance by instance, keep it so */
    for (i = 0; i < g_nputs; i++) {
        int a = g_puts[i].inst;
        if (!g_count[a]) g_first[a] = i;
        g_count[a]++;
    }
    for (i = 0; i < MAX_INST && i < (int)sig[2]; i++)
        if (g_nhide[i] || g_count[i]) own_put(W_LD(u32, tab + (u32)i * 0x54u, 0x44), i);
    g_track_ok = 1;
    if (g_nputs) fprintf(stderr, "env: %d models placed, %d model variants loaded\n", g_nputs, g_nmodels);
}

static void setup(void)
{
    NSError *err = nil;
    id<MTLLibrary> lib;
    MTLRenderPipelineDescriptor *pd;
    if (g_state) return;
    g_state = -1;
    D = MTLCreateSystemDefaultDevice();
    {
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
        u32 up = 0xFFFF8080u, w = 0xFFFFFFFFu;
        g_flatn = [D newTextureWithDescriptor:td];
        [g_flatn replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&up bytesPerRow:4];
        g_white = [D newTextureWithDescriptor:td];
        [g_white replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&w bytesPerRow:4];
    }
    lib = [D newLibraryWithSource:[NSString stringWithUTF8String:ENVSRC] options:nil error:&err];
    if (!lib) { fprintf(stderr, "env shader: %s\n", err.localizedDescription.UTF8String); return; }
    pd = [MTLRenderPipelineDescriptor new];
    pd.vertexFunction = [lib newFunctionWithName:@"evs"];
    pd.fragmentFunction = [lib newFunctionWithName:@"efs"];
    pd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
    pd.colorAttachments[1].pixelFormat = MTLPixelFormatRGBA16Float;
    pd.colorAttachments[2].pixelFormat = MTLPixelFormatRGBA32Float;
    pd.colorAttachments[3].pixelFormat = MTLPixelFormatRGBA8Unorm;
    pd.colorAttachments[4].pixelFormat = MTLPixelFormatR8Unorm;
    pd.colorAttachments[4].writeMask = MTLColorWriteMaskNone;
    pd.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    g_pipe = [D newRenderPipelineStateWithDescriptor:pd error:&err];
    if (!g_pipe) { fprintf(stderr, "env pipeline: %s\n", err.localizedDescription.UTF8String); return; }
    pd.vertexFunction = [lib newFunctionWithName:@"ivs"];
    pd.fragmentFunction = [lib newFunctionWithName:@"ifs"];
    g_ipipe = [D newRenderPipelineStateWithDescriptor:pd error:&err];
    if (!g_ipipe) { fprintf(stderr, "env card pipeline: %s\n", err.localizedDescription.UTF8String); return; }
    pd = [MTLRenderPipelineDescriptor new];
    pd.vertexFunction = [lib newFunctionWithName:@"esvs"];
    pd.fragmentFunction = [lib newFunctionWithName:@"esfs"];
    pd.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    g_spipe = [D newRenderPipelineStateWithDescriptor:pd error:&err];
    pd.vertexFunction = [lib newFunctionWithName:@"isvs"];
    g_ispipe = [D newRenderPipelineStateWithDescriptor:pd error:&err];
    if (!g_ispipe) { fprintf(stderr, "env card shadow pipeline: %s\n", err.localizedDescription.UTF8String); return; }
    if (!g_spipe) { fprintf(stderr, "env shadow pipeline: %s\n", err.localizedDescription.UTF8String); return; }
    {
        MTLDepthStencilDescriptor *d = [MTLDepthStencilDescriptor new];
        d.depthCompareFunction = MTLCompareFunctionLess;
        d.depthWriteEnabled = YES;
        g_ds = [D newDepthStencilStateWithDescriptor:d];
        d.depthCompareFunction = MTLCompareFunctionAlways;
        g_dsall = [D newDepthStencilStateWithDescriptor:d];
    }
    for (int i = 0; i < 6; i++) g_inst[i] = [D newBufferWithLength:64 * 65536 options:MTLResourceStorageModeShared];
    for (int i = 0; i < 3; i++) g_dinst[i] = [D newBufferWithLength:64 * (size_t)DRAW_INST options:MTLResourceStorageModeShared];
    g_state = 1;
}
static void setup_once(void)
{
    static dispatch_once_t once;
    dispatch_once(&once, ^{ setup(); });
}

/* native/env.m: is Remastered drawing the environment, for this track? */
int henv_active(void)
{
    if (env_off() || !hfx_on()) return 0;
    setup_once();
    if (g_state != 1) return 0;
    load_track();
    return g_track_ok;
}

/* native/env.m: the list to give the game for instance `idx` (its own list
 * `orig`), or 0 for the original.  The copy is made once per track: the
 * original's commands up to its G_ENDDL, with the replaced cards' triangle
 * commands turned into triangles whose corners are all vertex 0. */
u32 henv_list(u32 idx, u32 orig)
{
    u32 n, i, alt;
    if (idx >= MAX_INST || !g_nhide[idx] || !orig) return 0;
    if (g_alt[idx]) return g_alt[idx];
    for (n = 0; n < 0x10000; n++)
        if ((W_LD(u32, orig, n * 8) >> 24) == 0xB8) break;
    if (n == 0x10000) return 0;
    alt = hmem_alloc((n + 1) * 8, 0);
    for (i = 0; i <= n; i++) {
        W_ST(u32, alt, i * 8, W_LD(u32, orig, i * 8));
        W_ST(u32, alt, i * 8 + 4, W_LD(u32, orig, i * 8 + 4));
    }
    for (i = 0; i < (u32)g_nhide[idx]; i++) {
        u32 o = (u32)g_hide[idx][i], w0;
        if (o / 8 >= n) continue;
        w0 = W_LD(u32, alt, o);
        if ((w0 >> 24) == 0xB1) {
            W_ST(u32, alt, o, w0 & 0xFF000000u);
            W_ST(u32, alt, o + 4, W_LD(u32, alt, o + 4) & 0xFF000000u);
        } else if ((w0 >> 24) == 0xBF) {
            W_ST(u32, alt, o + 4, W_LD(u32, alt, o + 4) & 0xFF000000u);
        }
    }
    g_alt[idx] = alt;
    own_put(alt, (int)idx);
    g_st_alt++;
    return alt;
}

int henv_anchor(u32 idx) { return idx < MAX_INST && g_count[idx] > 0; }

static void rig(eu *u)
{
    int w = (int)H32(0x104B15E8u);
    const char *ov = getenv("BR_FX_WEATHER");
    if (ov) w = atoi(ov);
    u->misc[2] = w == 2 ? 1.4f : w == 4 ? 1.35f : w == 3 ? 1.2f : w == 1 ? 1.3f : 1.35f;
    if (!hfx_car_rig(u->sun, u->sunc, u->skyc, u->grnd)) {
        u->sun[0] = 0.57f; u->sun[1] = 0.57f; u->sun[2] = 0.6f; u->sun[3] = 1;
        u->sunc[0] = 1.6f; u->sunc[1] = 1.45f; u->sunc[2] = 1.2f;
        u->skyc[0] = 0.4f; u->skyc[1] = 0.48f; u->skyc[2] = 0.62f;
        u->grnd[0] = 0.34f; u->grnd[1] = 0.28f; u->grnd[2] = 0.21f;
    }
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
    /* clip-space (0,0,1,0) back into the world: row 2 of the inverse */
    for (j = 0; j < 3; j++) eye[j] = (float)(a[2][j + 4] / a[2][7]);
}

static int lod_for(const eput *q, const float *eye, int main)
{
    const char *f = getenv("BR_ENV_LOD");
    float dx = q->c[0] - eye[0], dy = q->c[1] - eye[1], dz = q->c[2] - eye[2];
    float d = sqrtf(dx * dx + dy * dy + dz * dz) / fmaxf(q->r, 1.0f);
    if (f) return atoi(f);
    if (!main) return NLOD;              /* the mirror: the card */
    /* measured 2026-09-30: full models out to 9 radii drew up to 17.8M triangles a
     * frame on Mountain; the card (512 px frames) holds up from 5 */
    return d < 2.0f ? 0 : d < 3.5f ? 1 : d < 5.0f ? 2 : NLOD;   /* NLOD: the card */
}

/* native/env.m, as the list jumps into instance `idx`'s list: draw the
 * models that stand on it, through the view the list is drawing now. */
void henv_draw(int idx)
{
    id<MTLDevice> dev;
    id<MTLRenderCommandEncoder> e;
    MTLScissorRect sc;
    int origin_ll, fogmode, rw, rh, i, j, main, view;
    float fogc[4];
    eu u;
    if (g_state != 1 || idx < 0 || idx >= MAX_INST || !g_count[idx]) return;
    g_st_mark++;
    if (getenv("BR_ENV_TRACE")) { static u8 seen[MAX_INST]; if (!seen[idx]) { seen[idx] = 1; fprintf(stderr, "envtrace: draw %d\n", idx); } }
    memset(&u, 0, sizeof u);
    e = hglide_native_pass(&dev, &sc, &origin_ll, &fogmode, fogc, u.fogtab, &rw, &rh);
    if (!e) return;
    hfx_jitter(&u.jit[0], &u.jit[1], rw, rh);
    view = hrender_view();
    hglide_map(view, u.map);
    sc = hglide_scissor(view);
    for (i = 0; i < 16; i++) u.P[i] = W_LD(f32, PROJ, 4 * i);
    u.vpt[0] = W_LD(f32, 0x105CCD48u, 0); u.vpt[1] = W_LD(f32, 0x105CD9F8u, 0);
    u.vpt[2] = W_LD(f32, 0x105CCFDCu, 0); u.vpt[3] = W_LD(f32, 0x105CD9FCu, 0);
    inv_eye(u.P, u.eye);
    rig(&u);
    static int rig_logged; if (getenv("BR_ENV_STAT") && rig_logged++ < 3)
        fprintf(stderr, "env: rig sun %.2f %.2f %.2f (%.2f) sunc %.2f %.2f %.2f sky %.2f %.2f %.2f grnd %.2f %.2f %.2f exp %.2f eye %.1f %.1f %.1f\n",
                u.sun[0], u.sun[1], u.sun[2], u.sun[3], u.sunc[0], u.sunc[1], u.sunc[2], u.skyc[0], u.skyc[1], u.skyc[2],
                u.grnd[0], u.grnd[1], u.grnd[2], u.misc[2], u.eye[0], u.eye[1], u.eye[2]);
    if (g_main_serial != hglide_swaps()) { memcpy(g_mainP, u.P, sizeof g_mainP); g_main_serial = hglide_swaps(); }
    main = view == 0 && !memcmp(g_mainP, u.P, sizeof g_mainP);
    u.misc[0] = (float)origin_ll;
    u.misc[1] = (float)main;
    u.misc[3] = (float)fmod(hframe_game_ms(), 1e6);
    u.fogc[3] = (float)(fogmode & 1);
    for (i = 0; i < 3; i++) u.fogc[i] = powf(fogc[i] / 255.0f, 2.2f);
    if (getenv("BR_ENV_DEBUG")) u.jit[3] = (float)atoi(getenv("BR_ENV_DEBUG"));
    [e setRenderPipelineState:g_pipe];
    [e setDepthStencilState:u.jit[3] > 0.5 && u.jit[3] < 1.5 ? g_dsall : g_ds];
    [e setScissorRect:sc];
    [e setCullMode:MTLCullModeNone];
    if (g_dinst_serial != hglide_swaps()) { g_dinst_serial = hglide_swaps(); g_dinst_used = 0; }
    {
        /* the visible models standing on this instance, bucketed by (model,
         * level); a bucket is one instanced draw per material */
        enum { MAXB = 64 };
        int bm[MAXB], bl[MAXB], bn[MAXB], nb = 0, b2;
        size_t bo[MAXB];
        float *ring = g_dinst[g_dinst_serial % 3].contents;
        int *pick = malloc(sizeof(int) * (size_t)g_count[idx] * 2);
        int np = 0;
        for (i = g_first[idx]; i < g_first[idx] + g_count[idx]; i++) {
            const eput *q = &g_puts[i];
            emodel *m = &g_models[q->model];
            float c[4], dx, dy, dz; int lod;
            for (j = 0; j < 4; j++) c[j] = q->c[0] * u.P[j] + q->c[1] * u.P[4 + j] + q->c[2] * u.P[8 + j] + u.P[12 + j];
            if (getenv("BR_ENV_TRACE") && idx == atoi(getenv("BR_ENV_TRACE"))) {
                static int nl2; if (nl2++ < 3) fprintf(stderr, "envtrace: pre-frustum idx %d clip %.2f %.2f %.2f %.2f\n", idx, c[0], c[1], c[2], c[3]);
            }
            {
                float kx = sqrtf(u.P[0] * u.P[0] + u.P[4] * u.P[4] + u.P[8] * u.P[8]);
                float ky = sqrtf(u.P[1] * u.P[1] + u.P[5] * u.P[5] + u.P[9] * u.P[9]);
                float kw = sqrtf(u.P[3] * u.P[3] + u.P[7] * u.P[7] + u.P[11] * u.P[11]);
                float R = q->r;
                if (c[3] < -R * kw || c[0] > c[3] + R * (kx + kw) || -c[0] > c[3] + R * (kx + kw) ||
                    c[1] > c[3] + R * (ky + kw) || -c[1] > c[3] + R * (ky + kw)) {
                    g_st_cull++;
                    continue;
                }
            }
            dx = q->c[0] - u.eye[0]; dy = q->c[1] - u.eye[1]; dz = q->c[2] - u.eye[2];
            if (getenv("BR_ENV_TRACE") && idx == atoi(getenv("BR_ENV_TRACE"))) {
                static int nlog; if (nlog++ < 6)
                    fprintf(stderr, "envtrace: idx %d c %.1f %.1f %.1f eye %.1f %.1f %.1f d %.1f maxd %.1f r %.2f clip %.2f %.2f %.2f %.2f main %d\n",
                            idx, q->c[0], q->c[1], q->c[2], u.eye[0], u.eye[1], u.eye[2], sqrtf(dx*dx+dy*dy+dz*dz), q->maxd, q->r, c[0], c[1], c[2], c[3], main);
            }
            if (q->maxd > 0 && dx * dx + dy * dy + dz * dz > q->maxd * q->maxd) { g_st_cull++; continue; }
            g_st_draw++;
            lod = lod_for(q, u.eye, main);
            if (lod < 0) lod = 0;
            if (lod >= NLOD && !m->ibase) lod = NLOD - 1;
            pick[np * 2] = i; pick[np * 2 + 1] = lod; np++;
        }
        /* buckets */
        for (j = 0; j < np; j++) {
            const eput *q = &g_puts[pick[j * 2]];
            int lod = pick[j * 2 + 1];
            for (b2 = 0; b2 < nb; b2++) if (bm[b2] == q->model && bl[b2] == lod) break;
            if (b2 == nb) { if (nb == MAXB) continue; bm[nb] = q->model; bl[nb] = lod; bn[nb] = 0; nb++; }
            bn[b2]++;
        }
        {
            size_t off = g_dinst_used;
            for (b2 = 0; b2 < nb; b2++) { bo[b2] = off; off += (size_t)bn[b2]; bn[b2] = 0; }
            if (off > DRAW_INST) { free(pick); return; }
            for (j = 0; j < np; j++) {
                const eput *q = &g_puts[pick[j * 2]];
                int lod = pick[j * 2 + 1];
                for (b2 = 0; b2 < nb; b2++) if (bm[b2] == q->model && bl[b2] == lod) break;
                if (b2 == nb) continue;
                memcpy(ring + (bo[b2] + (size_t)bn[b2]) * 16, q->M, 64);
                bn[b2]++;
            }
            g_dinst_used = off;
        }
        free(pick);
        for (b2 = 0; b2 < nb; b2++) {
            emodel *m = &g_models[bm[b2]];
            easset *a = &g_assets[m->asset];
            int lod = bl[b2], s2;
            float sc = 1;
            if (!bn[b2]) continue;
            {   const float *M0 = ring + bo[b2] * 16; sc = sqrtf(M0[0] * M0[0] + M0[1] * M0[1] + M0[2] * M0[2]); }
            if (lod >= NLOD) {
                /* the far card, one per model, instanced */
                u.mat[0] = 1; u.mat[1] = m->iviews; u.mat[2] = m->ihalf; u.mat[3] = m->iheight;
                [e setRenderPipelineState:g_ipipe];
                [e setVertexBytes:&u length:sizeof u atIndex:1];
                [e setVertexBuffer:g_dinst[g_dinst_serial % 3] offset:bo[b2] * 64 atIndex:2];
                [e setFragmentBytes:&u length:sizeof u atIndex:0];
                [e setFragmentTexture:m->ibase atIndex:0];
                [e setFragmentTexture:m->inrm atIndex:2];
                [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6 instanceCount:(NSUInteger)bn[b2]];
                [e setRenderPipelineState:g_pipe];
                g_st_card += bn[b2];
                continue;
            }
            for (s2 = 0; s2 < m->nsub[lod]; s2++) {
                esub *sb = &m->lod[lod][s2];
                emat *mt = &a->m[sb->mat < a->nm ? sb->mat : 0];
                u.mat[0] = (float)mt->clip;
                u.mat[2] = mt->clip ? 0.005f * m->h : 0.0f;    /* sway, model units; scaled by the instance */
                u.mat[3] = m->h;
                u.geo[0] = m->ihalf > 0 ? m->ihalf : m->r * 0.6f; u.geo[1] = m->h;
                (void)sc;
                [e setVertexBuffer:sb->vb offset:0 atIndex:0];
                [e setVertexBytes:&u length:sizeof u atIndex:1];
                [e setVertexBuffer:g_dinst[g_dinst_serial % 3] offset:bo[b2] * 64 atIndex:2];
                [e setFragmentBytes:&u length:sizeof u atIndex:0];
                [e setFragmentTexture:mt->base atIndex:0];
                [e setFragmentTexture:mt->orm atIndex:1];
                [e setFragmentTexture:mt->nrm atIndex:2];
                [e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:(NSUInteger)sb->ni indexType:MTLIndexTypeUInt32
                             indexBuffer:sb->ib indexBufferOffset:0 instanceCount:(NSUInteger)bn[b2]];
                g_st_tris += (long)sb->ni / 3 * bn[b2];
            }
        }
    }
}

/* host_fx.m's shadow pass, per cascade: every placed model near the camera,
 * its coarsest level, instanced by model through the sun's view `svp`
 * (row-vector, like the fx pass's own).  `eye` and `range` pick which. */
void henv_shadow(id<MTLRenderCommandEncoder> e, const float *svp, const float *eye, float range)
{
    int mi, i, s, n, pass;
    float *out;
    size_t used = 0;
    /* within NEAR the model's own coarsest mesh casts, beyond it the card.  The
     * coarsest artist level of a pine is still 270-420k triangles, drawn twice
     * (two cascades): measured 2026-09-30 as the frame's GPU cost, so every
     * tree with a card casts with the card (BR_ENV_SHNEAR=m for meshes) */
    static float NEAR = -1;
    if (NEAR < 0) NEAR = getenv("BR_ENV_SHNEAR") ? (float)atof(getenv("BR_ENV_SHNEAR")) : 0.0f;
    if (g_state != 1 || !hfx_on() || !g_nputs) return;
    g_inst_i = (g_inst_i + 1) % 6;
    out = g_inst[g_inst_i].contents;
    for (pass = 0; pass < 2; pass++)
    for (mi = 0; mi < g_nmodels; mi++) {
        /* only this track's models: a prefetch may be loading others */
        if (g_mfirst[mi] == g_mfirst[mi + 1]) continue;
        emodel *m = &g_models[mi];
        easset *a = &g_assets[m->asset];
        size_t first = used;
        int card = pass == 1;
        if (!m->ok || (card && !m->ibase)) continue;
        for (int k2 = g_mfirst[mi]; k2 < g_mfirst[mi + 1] && used < 65536; k2++) {
            const eput *q = &g_puts[g_mput[k2]];
            float dx = q->c[0] - eye[0], dy = q->c[1] - eye[1], d2 = dx * dx + dy * dy;
            int near = d2 < NEAR * NEAR || !m->ibase;
            if (d2 > (range + q->r) * (range + q->r) || near == card) continue;
            memcpy(out + used * 16, q->M, 64);
            used++;
        }
        n = (int)(used - first);
        if (!n) continue;
        if (card) {
            su u;
            memset(&u, 0, sizeof u);
            memcpy(u.svp, svp, sizeof u.svp);
            u.mat[0] = 1; u.mat[1] = m->iviews; u.mat[2] = m->ihalf; u.mat[3] = m->iheight;
            {   /* toward the sun: host_fx.m's svp row 2 is the sun's view direction */
                float fx = svp[2], fy = svp[6], fz = svp[10], l = sqrtf(fx * fx + fy * fy + fz * fz);
                if (l > 0) { u.misc[0] = -fx / l; u.misc[1] = -fy / l; u.misc[2] = -fz / l; }
            }
            [e setRenderPipelineState:g_ispipe];
            [e setVertexBytes:&u length:sizeof u atIndex:1];
            [e setVertexBuffer:g_inst[g_inst_i] offset:first * 64 atIndex:2];
            [e setFragmentBytes:&u length:sizeof u atIndex:0];
            [e setFragmentTexture:m->ibase atIndex:0];
            [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6 instanceCount:(NSUInteger)n];
            continue;
        }
        [e setRenderPipelineState:g_spipe];
        for (s = 0; s < m->nsub[NLOD - 1]; s++) {
            esub *sb = &m->lod[NLOD - 1][s];
            emat *mt = &a->m[sb->mat < a->nm ? sb->mat : 0];
            su u;
            memset(&u, 0, sizeof u);
            memcpy(u.svp, svp, sizeof u.svp);
            u.mat[0] = (float)mt->clip;
            [e setVertexBuffer:sb->vb offset:0 atIndex:0];
            [e setVertexBytes:&u length:sizeof u atIndex:1];
            [e setVertexBuffer:g_inst[g_inst_i] offset:first * 64 atIndex:2];
            [e setFragmentBytes:&u length:sizeof u atIndex:0];
            [e setFragmentTexture:mt->base atIndex:0];
            [e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:(NSUInteger)sb->ni indexType:MTLIndexTypeUInt32
                         indexBuffer:sb->ib indexBufferOffset:0 instanceCount:(NSUInteger)n];
        }
    }
}
