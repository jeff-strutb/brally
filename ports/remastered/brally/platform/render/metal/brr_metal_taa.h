/* brr_metal_taa.h: temporal anti-aliasing for the lit frame (DESIGN.md 10).
 *
 *   jitter      the lit 3D (G-buffer, backdrop, forward) drawn a sub-pixel
 *               off the pixel grid each frame, Halton (2,3) over 16 phases,
 *               through a jittered copy of the frame's screen map table (an
 *               ndc offset added to xft.y and xft.w moves the clip-space
 *               output by offset x w); the 2D replay is not jittered
 *   motion      a pixel's world position (the G-buffer's) through last
 *               frame's world-to-pixel matrix (brr_screen_matrix,
 *               unjittered); the sky through the same matrix at infinity.  A
 *               car's pixel (its material: paint, lens, tyre) is first
 *               carried back by its own car's rigid motion, the point's place
 *               in the car's frame now put in its frame last frame
 *               (brr_taa_carprev; Unreal's and Frostbite's per-object
 *               velocity).  Without it the car came out as camera motion: its
 *               history was the road behind it, the disocclusion test threw
 *               it away every frame and the car was never anti-aliased
 *   resolve     after the lighting, before bloom and post, in pre-exposed HDR:
 *               history by Catmull-Rom (16 texel reads), clipped to the
 *               current 3x3 neighbourhood's variance box in YCoCg (Salvi
 *               2016) intersected with its min/max, blended by Karis's
 *               luminance weights, alpha = max(1 / (n + 1), TAA_ALPHA) over n
 *               frames of history; reset on a camera cut and on disocclusion
 *               (the history's view distance against this pixel's)
 *   MetalFX     brr_metal_taa.m: MTLFXTemporalScaler where the device has it,
 *               and the dynamic resolution controller (brr_taa_dynres)
 *
 * The C here is the reference the tests check the shader against
 * (ports/brally/tools/lighttest/taa.sh). */
#ifndef BRR_METAL_TAA_H
#define BRR_METAL_TAA_H

#include <math.h>

#define TAA_PHASES 16
#define TAA_ALPHA 0.0625f       /* the floor weight of this frame, 1/16 (one Halton cycle; 1 / (n + 1) before it): 0.1 leaves a 6.5% edge error after 16 frames */
#define TAA_GAMMA 1.25f         /* the variance box's half width in standard deviations */
#define TAA_DISOCC 0.05f        /* a history view distance this far off (relative) is another surface */
#define TAA_CUT_PX 64.0f        /* a camera cut: the screen centre's world point moves this far */

/* the radical inverse of i in base b: Halton's sequence, in [0, 1) */
static float brr_halton(int i, int b)
{
    float f = 1.0f, r = 0.0f;
    while (i > 0) {
        f /= (float)b;
        r += f * (float)(i % b);
        i /= b;
    }
    return r;
}

/* frame n's jitter: px (pixels, inside the pixel, mean 0 over the phases;
 * +x right, +y down the target) and the ndc offset that draws it (y up).
 * Halton indices 0..15, less the 16-sample set's mean: base 2 then lands on
 * the centres of 16 strata ((k - 7.5) / 16) */
static void brr_taa_jitter(unsigned n, float tw, float th, float px[2], float ndc[2])
{
    int i = (int)(n % TAA_PHASES), k;
    double m2 = 0.0, m3 = 0.0;
    for (k = 0; k < TAA_PHASES; k++) {
        m2 += brr_halton(k, 2);
        m3 += brr_halton(k, 3);
    }
    px[0] = (float)((double)brr_halton(i, 2) - m2 / TAA_PHASES);
    px[1] = (float)((double)brr_halton(i, 3) - m3 / TAA_PHASES);
    ndc[0] = 2.0f * px[0] / tw;
    ndc[1] = -2.0f * px[1] / th;
}

/* a world point through a world-to-pixel matrix S (brr_screen_matrix's,
 * column-major, h = S [p w]); w = 0 for a direction (the sky).  0 when it
 * is behind the camera */
static int brr_taa_reproject(const float S[16], const float p[3], float w, float out[2])
{
    float h[4];
    int r;
    for (r = 0; r < 4; r++)
        h[r] = S[r] * p[0] + S[4 + r] * p[1] + S[8 + r] * p[2] + S[12 + r] * w;
    if (h[3] <= 1e-6f)
        return 0;
    out[0] = h[0] / h[3];
    out[1] = h[1] / h[3];
    return 1;
}

/* the world-to-pixel matrix with clip z in its third row: invertible, so a
 * pixel's view ray comes back (brr_taa_ray) */
static void brr_taa_full(const float S[16], const float vp[16], float F[16])
{
    int i;
    for (i = 0; i < 16; i++)
        F[i] = S[i];
    for (i = 0; i < 4; i++)
        F[4 * i + 2] = vp[4 * i + 2];
}

/* in double: the sky's rays come from the difference of two nearby points */
static int brr_inv4(const float mf[16], float o[16])
{
    double m[16], inv[16], det;
    int i;
    for (i = 0; i < 16; i++)
        m[i] = mf[i];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (fabs(det) < 1e-300)
        return 0;
    for (i = 0; i < 16; i++)
        o[i] = (float)(inv[i] / det);
    return 1;
}

/* a pixel's view direction (world) from the inverse of brr_taa_full's matrix */
static void brr_taa_ray(const float Fi[16], float x, float y, float d[3])
{
    float a[4], b[4];
    int r;
    for (r = 0; r < 4; r++) {
        a[r] = Fi[r] * x + Fi[4 + r] * y + Fi[8 + r] * 0.25f + Fi[12 + r];
        b[r] = Fi[r] * x + Fi[4 + r] * y + Fi[8 + r] * 0.75f + Fi[12 + r];
    }
    for (r = 0; r < 3; r++)
        d[r] = b[r] / b[3] - a[r] / a[3];
}

/* a car's point p (world, this frame) where it was last frame: in car i's
 * box (its frame now, grown by TAA_CAR_MARGIN) its place there put in its
 * last frame.  c: brr_metal_taa_api.h BrrTaaCars' rows, 8 float4 a car.
 * Returns 1 moved, 0 in no car's box (left as static world), -1 in a car
 * with no last frame (no history) */
#define TAA_CAR_MARGIN 0.3f
#ifndef BRR_TAA_CARS
#define BRR_TAA_CARS 20         /* the cars the resolve moves (brr_metal_taa_api.h BrrTaaCars) */
#endif
static int brr_taa_carprev(const float (*c)[8][4], int n, const float p[3], float o[3])
{
    int i, a;
    for (i = 0; i < n; i++) {
        const float (*r)[4] = c[i];
        float d[3] = { p[0] - r[0][3], p[1] - r[1][3], p[2] - r[2][3] }, l[3];
        int in = 1;
        for (a = 0; a < 3; a++) {
            l[a] = d[0] * r[a][0] + d[1] * r[a][1] + d[2] * r[a][2];
            in &= l[a] >= r[6][a] - TAA_CAR_MARGIN && l[a] <= r[7][a] + TAA_CAR_MARGIN;
        }
        if (!in)
            continue;
        if (r[6][3] < 0.5f)
            return -1;
        for (a = 0; a < 3; a++)
            o[a] = r[a + 3][3] + r[3][a] * l[0] + r[4][a] * l[1] + r[5][a] * l[2];
        return 1;
    }
    return 0;
}

/* ---- the resolve's colour maths (the shader's, line for line) ---- */
static void brr_ycocg(const float c[3], float o[3])
{
    o[0] = 0.25f * c[0] + 0.5f * c[1] + 0.25f * c[2];
    o[1] = 0.5f * c[0] - 0.5f * c[2];
    o[2] = -0.25f * c[0] + 0.5f * c[1] - 0.25f * c[2];
}
static void brr_rgb(const float y[3], float o[3])
{
    o[0] = y[0] + y[1] - y[2];
    o[1] = y[0] + y[2];
    o[2] = y[0] - y[1] - y[2];
}
/* Salvi's clip: h (YCoCg) toward the box's centre until it is inside */
static void brr_taa_clip(const float h[3], const float lo[3], const float hi[3], float o[3])
{
    float c[3], e[3], v[3], m = 1.0f;
    int k;
    for (k = 0; k < 3; k++) {
        c[k] = 0.5f * (hi[k] + lo[k]);
        e[k] = 0.5f * (hi[k] - lo[k]) + 1e-5f;
        v[k] = h[k] - c[k];
        if (fabsf(v[k]) / e[k] > m)
            m = fabsf(v[k]) / e[k];
    }
    for (k = 0; k < 3; k++)
        o[k] = c[k] + v[k] / m;
}
static float brr_taa_luma(const float c[3]) { return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2]; }

/* the blend: Karis's weights 1 / (1 + luma) (k = 0: plain) */
static void brr_taa_blend(const float c[3], const float h[3], float a, int karis, float o[3])
{
    float wc = a, wh = 1.0f - a;
    int k;
    if (karis) {
        wc /= 1.0f + brr_taa_luma(c);
        wh /= 1.0f + brr_taa_luma(h);
    }
    for (k = 0; k < 3; k++)
        o[k] = (c[k] * wc + h[k] * wh) / (wc + wh);
}

/* the dynamic resolution controller: the internal scale from the last
 * frames' GPU time against the target, with hysteresis (a band of +-8%
 * around the target where nothing changes, steps of 5%, at most one change
 * every 8 frames), clamped to [lo, 1] */
typedef struct BrrDynRes { float scale, ms; int hold; } BrrDynRes;
static float brr_taa_dynres(BrrDynRes *d, float gpu_ms, float target_ms, float lo)
{
    if (d->scale <= 0.0f) {
        d->scale = 1.0f;
        d->ms = gpu_ms;
    }
    d->ms = d->ms * 0.8f + gpu_ms * 0.2f;      /* smoothed over ~5 frames */
    if (d->hold > 0) {
        d->hold--;
        return d->scale;
    }
    if (d->ms > target_ms * 1.08f && d->scale > lo) {
        d->scale = fmaxf(lo, d->scale - 0.05f);
        d->hold = 8;
    } else if (d->ms < target_ms * 0.92f * (d->scale * d->scale) / ((d->scale + 0.05f) * (d->scale + 0.05f)) && d->scale < 1.0f) {
        d->scale = fminf(1.0f, d->scale + 0.05f);   /* up only when the larger size's estimated time still fits */
        d->hold = 8;
    }
    return d->scale;
}

/* ---- the shaders (a library of their own, brr_metal_taa.m) ---- */
#define BRR_TAA_STR_(x) #x
#define BRR_TAA_STR(x) BRR_TAA_STR_(x)
#define BRR_TAA_SRC \
"#include <metal_stdlib>\n" \
"using namespace metal;\n" \
"struct TU { float4 S[4]; float4 Fi[4]; float4 eye; float4 jit; float4 k; };\n" \
"/* jit: this frame's jitter px .xy; k: alpha floor, reset, karis, gamma */\n" \
"struct TV { float4 pos [[position]]; };\n" \
"vertex TV taa_vs(uint i [[vertex_id]]) { TV o; float2 p = float2((i << 1) & 2, i & 2); o.pos = float4(p * 2.0 - 1.0, 0, 1); return o; }\n" \
"static float3 ycocg(float3 c) { return float3(0.25 * c.r + 0.5 * c.g + 0.25 * c.b, 0.5 * c.r - 0.5 * c.b, -0.25 * c.r + 0.5 * c.g - 0.25 * c.b); }\n" \
"static float3 rgbc(float3 y) { return float3(y.x + y.y - y.z, y.x + y.z, y.x - y.y - y.z); }\n" \
"static float luma(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }\n" \
"static float4 texl(texture2d<float> t, int2 p) { int2 m = int2(t.get_width(), t.get_height()) - 1; return t.read(uint2(clamp(p, int2(0), m))); }\n" \
"static float cr(float x) { x = abs(x); return x < 1.0 ? 1.5 * x * x * x - 2.5 * x * x + 1.0 : x < 2.0 ? -0.5 * x * x * x + 2.5 * x * x - 4.0 * x + 2.0 : 0.0; }\n" \
"/* Catmull-Rom at a continuous pixel position q (a texel's centre at +0.5) */\n" \
"static float4 catrom(texture2d<float> t, float2 q) {\n" \
"  float2 b = floor(q - 0.5), f = q - 0.5 - b; float4 s = 0; float ws = 0;\n" \
"  for (int j = -1; j <= 2; j++) for (int i = -1; i <= 2; i++) {\n" \
"    float w = cr(float(i) - f.x) * cr(float(j) - f.y); s += w * texl(t, int2(b) + int2(i, j)); ws += w; }\n" \
"  return s / ws; }\n" \
"static float4 mulS(constant float4 *S, float4 p) { return S[0] * p.x + S[1] * p.y + S[2] * p.z + S[3] * p.w; }\n" \
"struct TC { int4 n; float4 c[" BRR_TAA_STR(BRR_TAA_CARS) " * 8]; };\n" \
"/* a car's material (brr_metal_refl.h: tyre 84, paint 88..104, lens 112..120) */\n" \
"static bool carmat(float a) { a *= 255.0; return (a > 82.5 && a < 104.5) || (a > 110.5 && a < 121.5); }\n" \
"/* brr_taa_carprev, line for line: 1 moved, 0 in no car, -1 a car with no last frame */\n" \
"static int carprev(constant TC &tc, float3 p, thread float3 &o) {\n" \
"  for (int i = 0; i < tc.n.x; i++) { int b = 8 * i; float3 d = p - float3(tc.c[b].w, tc.c[b + 1].w, tc.c[b + 2].w);\n" \
"    float3 l = float3(dot(d, tc.c[b].xyz), dot(d, tc.c[b + 1].xyz), dot(d, tc.c[b + 2].xyz));\n" \
"    if (any(l < tc.c[b + 6].xyz - " BRR_TAA_STR(TAA_CAR_MARGIN) ") || any(l > tc.c[b + 7].xyz + " BRR_TAA_STR(TAA_CAR_MARGIN) ")) continue;\n" \
"    if (tc.c[b + 6].w < 0.5) return -1;\n" \
"    o = float3(tc.c[b + 3].w, tc.c[b + 4].w, tc.c[b + 5].w) + tc.c[b + 3].xyz * l.x + tc.c[b + 4].xyz * l.y + tc.c[b + 5].xyz * l.z; return 1; }\n" \
"  return 0; }\n" \
"/* where this pixel was last frame (pixels), and its view distance now; .w 0: off the history */\n" \
"static float4 prevpos(constant TU &u, constant TC &tc, texture2d<float> gpos, texture2d<float> galb, uint2 px) {\n" \
"  float3 P = gpos.read(px).xyz; float4 h; float dist;\n" \
"  if (all(P == 0.0)) {\n" \
"    float2 q = float2(px) + 0.5 - u.jit.xy;\n" \
"    float4 a = mulS(u.Fi, float4(q, 0.25, 1)), b = mulS(u.Fi, float4(q, 0.75, 1));\n" \
"    h = mulS(u.S, float4(b.xyz / b.w - a.xyz / a.w, 0)); dist = 65504.0;\n" \
"  } else {\n" \
"    dist = distance(P, u.eye.xyz); float3 Q = P;\n" \
"    if (tc.n.x > 0 && carmat(galb.read(px).a) && carprev(tc, P, Q) < 0) return float4(0, 0, dist, 0);\n" \
"    h = mulS(u.S, float4(Q, 1)); }\n" \
"  if (h.w <= 1e-6) return float4(0, 0, dist, 0);\n" \
"  return float4(h.xy / h.w, dist, 1); }\n" \
"struct TO { float4 c [[color(0)]]; float d [[color(1)]]; };\n" \
"fragment TO taa_resolve(TV in [[stage_in]], constant TU &u [[buffer(0)]], texture2d<float> cur [[texture(0)]],\n" \
"                        texture2d<float> hist [[texture(1)]], texture2d<float> hdist [[texture(2)]], texture2d<float> gpos [[texture(3)]],\n" \
"                        texture2d<float> galb [[texture(4)]], constant TC &tc [[buffer(1)]]) {\n" \
"  uint2 px = uint2(in.pos.xy); int2 ip = int2(px); TO o;\n" \
"  float3 c = min(cur.read(px).rgb, 65504.0), m1 = 0, m2 = 0, lo = 1e30, hi = -1e30;\n" \
"  for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) {\n" \
"    float3 y = ycocg(min(texl(cur, ip + int2(i, j)).rgb, 65504.0)); m1 += y; m2 += y * y; lo = min(lo, y); hi = max(hi, y); }\n" \
"  m1 /= 9.0; m2 /= 9.0; float3 sd = sqrt(max(m2 - m1 * m1, 0.0));\n" \
"  float4 pp = prevpos(u, tc, gpos, galb, px); o.d = pp.z;\n" \
"  float2 tsz = float2(hist.get_width(), hist.get_height());\n" \
"  if (u.k.y > 0.5 || pp.w < 0.5 || any(pp.xy < 0.0) || any(pp.xy >= tsz)) { o.c = float4(c, 1); return o; }\n" \
"  float hd = hdist.read(uint2(pp.xy)).r;\n" \
"  if (abs(hd - pp.z) > " "0.05" " * pp.z) { o.c = float4(c, 1); return o; }\n" \
"  float n = hist.read(uint2(pp.xy)).a;   /* the variance box once the history has a whole Halton cycle: before, min/max only */\n" \
"  if (n >= 16.0) { lo = max(lo, m1 - u.k.w * sd); hi = min(hi, m1 + u.k.w * sd); }\n" \
"  float4 h4 = catrom(hist, pp.xy); float3 hy = max(ycocg(max(h4.rgb, 0.0)), float3(0, -65504, -65504));\n" \
"  float3 v = hy - 0.5 * (hi + lo), e = 0.5 * (hi - lo) + 1e-5, r = abs(v) / e;\n" \
"  float mx = max(1.0, max(r.x, max(r.y, r.z))); float3 h = rgbc(0.5 * (hi + lo) + v / mx);\n" \
"  float a = max(1.0 / (n + 1.0), u.k.x);\n" \
"  float wc = a, wh = 1.0 - a; if (u.k.z > 0.5) { wc /= 1.0 + luma(c); wh /= 1.0 + luma(h); }\n" \
"  o.c = float4((c * wc + h * wh) / (wc + wh), min(n + 1.0, 255.0)); return o; }\n" \
"/* MetalFX's motion: this pixel's offset to where it was, in pixels of the input */\n" \
"fragment float2 taa_motion(TV in [[stage_in]], constant TU &u [[buffer(0)]], texture2d<float> gpos [[texture(0)]],\n" \
"                          texture2d<float> galb [[texture(1)]], constant TC &tc [[buffer(1)]]) {\n" \
"  uint2 px = uint2(in.pos.xy); float4 pp = prevpos(u, tc, gpos, galb, px);\n" \
"  return pp.w > 0.5 ? pp.xy - (float2(px) + 0.5 - u.jit.xy) : float2(0); }\n"

#endif
