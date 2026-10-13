/* refl_test.m: the reflection pass's maths (brr_metal_refl.h) against
 * references, on synthetic data; no game.
 *
 *   1. the screen-space march (ssr), a CPU transcription in float, on a
 *      synthetic G-buffer of a ground plane and a box, against the analytic
 *      mirror ray's hit: within one pixel wherever the hit is in view
 *   2. the GPU's ssr on the same G-buffer against the CPU transcription
 *   3. wet(): Lagarde 2013's porosity darkening and the roughness, against
 *      the formula in double, and the energy bound: the lobe's albedo E plus
 *      the coupled diffuse's albedo (1 - E) times the wet albedo stays <= 1
 *      at every wetness, roughness and angle (E by quadrature in double)
 *   4. LU's size, the shader's against the C struct's
 *   5. the bilateral upsample (rf_up): a half-resolution image of a smooth
 *      function of position, sampled as reflk samples (pixel 2 h), brought
 *      up against the function at full resolution: within tolerance away
 *      from depth edges, and nothing of one surface across an edge into
 *      another (the box and the plane carry values 100 apart)
 * (the DFG table against the GGX integral is maths.sh's, lighttest.m)
 *
 *   refl_test <refl.metal>   (refl.sh builds and runs it) */
#import <Metal/Metal.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct LU { float sunvp[4][16]; float csplit[4], cfwd[4], ctexel[4], cbias[4]; float eye[4], sun[4], sunc[4], skyc[4], gndc[4], misc[4], wb[4], bg[4], fogc[4];
                   float cdepth[4], csoft[4], scr[16], vol[4], gi[4], wet[4], lexp[4]; } LU;

static int g_fail;
static void report(int ok, const char *name, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
#include <stdarg.h>
static void report(int ok, const char *name, const char *fmt, ...)
{
    va_list a;
    printf("%s  %-48s ", ok ? "PASS" : "FAIL", name);
    va_start(a, fmt);
    vprintf(fmt, a);
    va_end(a);
    printf("\n");
    if (!ok)
        g_fail++;
}

enum { W = 320, H = 180 };
static const float FX = 300.0f;
static const float BX0 = -1.5f, BX1 = 1.5f, BY0 = 2.0f, BY1 = 4.0f, BZ1 = 2.0f;
static float g_eye[3] = { 0.3f, -10.0f, 2.0f }, g_f[3], g_r[3], g_u[3];
static float g_pos[H][W][4];
static LU g_lu;

static float d3(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
static void nrm3(float *a) { float l = sqrtf(d3(a, a)); a[0] /= l; a[1] /= l; a[2] /= l; }

/* the box's slab test (double) */
static double box_hit(const double *o, const double *d, double tmax)
{
    double lo[3] = { BX0, BY0, 0.0 }, hi[3] = { BX1, BY1, BZ1 }, t0 = 1e-4, t1 = tmax;
    int k;
    for (k = 0; k < 3; k++) {
        double a, b, c;
        if (fabs(d[k]) < 1e-12) {
            if (o[k] < lo[k] || o[k] > hi[k])
                return -1;
            continue;
        }
        a = (lo[k] - o[k]) / d[k]; b = (hi[k] - o[k]) / d[k];
        if (a > b) c = a, a = b, b = c;
        t0 = fmax(t0, a); t1 = fmin(t1, b);
        if (t0 > t1)
            return -1;
    }
    return t0;
}
static double scene_hit(const double *o, const double *d, double tmax)
{
    double tb = box_hit(o, d, tmax), tp = d[2] < 0 ? -o[2] / d[2] : -1;
    if (tp > 500.0 || tp < 1e-4) tp = -1;
    if (tb > 0 && (tp < 0 || tb < tp)) return tb;
    return tp;
}
static void project(const float *q, float s[2], float *w)
{
    const float *m = g_lu.scr;
    float h[4];
    int r;
    for (r = 0; r < 4; r++)
        h[r] = m[r] * q[0] + m[4 + r] * q[1] + m[8 + r] * q[2] + m[12 + r];
    s[0] = h[0] / h[3]; s[1] = h[1] / h[3];
    if (w) *w = h[3];
}

static void setup(void)
{
    float tgt[3] = { 0.0f, 3.0f, 0.4f }, up[3] = { 0, 0, 1 }, row[4][3];
    int i, j, k;
    for (k = 0; k < 3; k++) g_f[k] = tgt[k] - g_eye[k];
    nrm3(g_f);
    g_r[0] = g_f[1] * up[2] - g_f[2] * up[1]; g_r[1] = g_f[2] * up[0] - g_f[0] * up[2]; g_r[2] = g_f[0] * up[1] - g_f[1] * up[0];
    nrm3(g_r);
    g_u[0] = g_r[1] * g_f[2] - g_r[2] * g_f[1]; g_u[1] = g_r[2] * g_f[0] - g_r[0] * g_f[2]; g_u[2] = g_r[0] * g_f[1] - g_r[1] * g_f[0];
    memset(&g_lu, 0, sizeof g_lu);
    for (k = 0; k < 3; k++) {
        row[0][k] = 0.5f * W * g_f[k] + FX * g_r[k];
        row[1][k] = 0.5f * H * g_f[k] - FX * g_u[k];
        row[2][k] = 0.0f;
        row[3][k] = g_f[k];
        g_lu.eye[k] = g_eye[k];
        g_lu.cfwd[k] = g_f[k];
    }
    for (i = 0; i < 4; i++) {
        for (k = 0; k < 3; k++) g_lu.scr[4 * k + i] = row[i][k];
        g_lu.scr[12 + i] = -d3(row[i], g_eye);
    }
    for (j = 0; j < H; j++)
        for (i = 0; i < W; i++) {
            double d[3], o[3] = { g_eye[0], g_eye[1], g_eye[2] }, t, l;
            double x = (i + 0.5 - 0.5 * W) / FX, y = (j + 0.5 - 0.5 * H) / FX;
            for (k = 0; k < 3; k++) d[k] = g_f[k] + x * g_r[k] - y * g_u[k];
            l = sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            for (k = 0; k < 3; k++) d[k] /= l;
            t = scene_hit(o, d, 1e9);
            for (k = 0; k < 4; k++) g_pos[j][i][k] = 0.0f;
            if (t > 0)
                for (k = 0; k < 3; k++) g_pos[j][i][k] = (float)(o[k] + d[k] * t);
        }
}

/* ssr as brr_metal_refl.h writes it, in float */
static float vd(const float *q) { float e[3] = { q[0] - g_eye[0], q[1] - g_eye[1], q[2] - g_eye[2] }; return d3(e, g_f); }
static const float *rdpos(float sx, float sy) { return g_pos[(int)sy][(int)sx]; }
static int zero3(const float *P) { return P[0] == 0.0f && P[1] == 0.0f && P[2] == 0.0f; }
static void lerpq(const float *p, const float *e, float k0, float k1, float l, float q[3])
{
    int k;
    float den = k0 + (k1 - k0) * l;
    for (k = 0; k < 3; k++) q[k] = (p[k] * k0 + (e[k] * k1 - p[k] * k0) * l) / den;
}
static void ssr_cpu(const float *p, const float *r, float maxd, float jit, float out[4])
{
    float dp = vd(p), dr = d3(r, g_f), len = maxd, e[3], s0[2], s1[2], w0, w1, k0, k1, lp = 0, dqp;
    int n, i, k;
    out[0] = out[1] = out[2] = out[3] = 0;
    if (dp + dr * len < 0.1f) len = (0.1f - dp) / dr;
    if (len <= 0) return;
    for (k = 0; k < 3; k++) e[k] = p[k] + r[k] * len;
    project(p, s0, &w0); project(e, s1, &w1);
    if (w0 <= 1e-6f || w1 <= 1e-6f) return;
    k0 = 1.0f / w0; k1 = 1.0f / w1;
    n = (int)fminf(fmaxf(ceilf(hypotf(s1[0] - s0[0], s1[1] - s0[1]) / 2.0f), 1.0f), 32.0f);   /* REFL_STRIDE, REFL_STEPS */
    dqp = dp;
    for (i = 0; i < n; i++) {
        float l = ((float)i + jit) / (float)n, s[2] = { s0[0] + (s1[0] - s0[0]) * l, s0[1] + (s1[1] - s0[1]) * l }, q[3], dq;
        const float *P;
        if (s[0] < 0 || s[1] < 0 || s[0] >= W || s[1] >= H) return;
        lerpq(p, e, k0, k1, l, q); dq = vd(q);
        P = rdpos(s[0], s[1]);
        if (!zero3(P)) {
            float ds = vd(P), th = 0.05f + 0.01f * dq + 2.0f * fabsf(dq - dqp), pp[3] = { P[0] - p[0], P[1] - p[1], P[2] - p[2] };
            if (dq > ds && sqrtf(d3(pp, pp)) > 0.05f) {
                float lo = lp, hi = l, sh[2], qh[3], dh;
                const float *Ph;
                int b;
                for (b = 0; b < 6; b++) {
                    float m = 0.5f * (lo + hi), sm[2] = { s0[0] + (s1[0] - s0[0]) * m, s0[1] + (s1[1] - s0[1]) * m }, qm[3];
                    const float *Pm = rdpos(sm[0], sm[1]);
                    lerpq(p, e, k0, k1, m, qm);
                    if (!zero3(Pm) && vd(qm) > vd(Pm)) hi = m; else lo = m;
                }
                sh[0] = s0[0] + (s1[0] - s0[0]) * hi; sh[1] = s0[1] + (s1[1] - s0[1]) * hi;
                lerpq(p, e, k0, k1, hi, qh);
                Ph = rdpos(sh[0], sh[1]); dh = vd(qh) - vd(Ph);
                if (!(dq - ds < th || (!zero3(Ph) && dh >= 0.0f && dh < 0.05f + 0.01f * vd(qh)))) { lp = l; dqp = dq; continue; }
                out[0] = sh[0]; out[1] = sh[1];
                out[2] = fminf(fmaxf(fminf(fminf(sh[0], W - sh[0]), fminf(sh[1], H - sh[1])) / (0.08f * H), 0.0f), 1.0f);
                out[3] = sqrtf((qh[0] - p[0]) * (qh[0] - p[0]) + (qh[1] - p[1]) * (qh[1] - p[1]) + (qh[2] - p[2]) * (qh[2] - p[2]));
                return;
            }
        }
        lp = l; dqp = dq;
    }
}

/* the lobe's albedo in double (GGX, height-correlated Smith, Schlick F0 0.04),
 * sampling D(h) n.h: as lighttest.m's furnace_ref, 256 x 256 */
static double E_ref(double rough, double mu)
{
    double a = fmax(rough * rough, 2e-3), a2 = a * a, v[3] = { sqrt(fmax(1.0 - mu * mu, 0.0)), 0.0, mu }, es = 0;
    int i, j, N = 256;
    for (j = 0; j < N; j++) {
        double u1 = (j + 0.5) / N, den = 1.0 + (a2 - 1.0) * u1, ch = sqrt((1.0 - u1) / den), sh = sqrt(a2 * u1 / den);
        for (i = 0; i < N; i++) {
            double ph = 2.0 * M_PI * (i + 0.5) / N, h[3] = { sh * cos(ph), sh * sin(ph), ch };
            double vh = v[0] * h[0] + v[1] * h[1] + v[2] * h[2], nl = 2.0 * vh * h[2] - v[2];
            if (nl > 0.0 && vh > 0.0) {
                double V = 0.5 / (nl * sqrt(mu * mu * (1.0 - a2) + a2) + mu * sqrt(nl * nl * (1.0 - a2) + a2));
                es += 4.0 * V * nl * vh / ch * (0.04 + 0.96 * pow(1.0 - vh, 5.0));
            }
        }
    }
    return es / (N * N);
}

static const char *k_kern =
"kernel void t_size(device uint *o [[buffer(0)]]) { o[0] = sizeof(LU); }\n"
"kernel void t_ssr(uint2 px [[thread_position_in_grid]], constant LU &u [[buffer(0)]], texture2d<float> pos [[texture(0)]],\n"
"                  device float4 *o [[buffer(1)]]) {\n"
"  if (px.x >= pos.get_width() || px.y >= pos.get_height()) return;\n"
"  float3 p = pos.read(px).xyz; uint k = px.y * pos.get_width() + px.x; o[k] = 0.0;\n"
"  if (all(p == 0.0) || abs(p.z) > 1e-4) return;\n"
"  float3 v = normalize(u.eye.xyz - p), r = reflect(-v, float3(0, 0, 1));\n"
"  o[k] = ssr(u, pos, p, r, 80.0, 1.0); }\n"
"static float t_f(float3 P) { return (P.z > 1e-3 ? 100.0 : 0.0) + 1.0 + 0.3 * sin(P.x * 0.15) + 0.2 * cos(P.y * 0.1) + 0.1 * P.z; }\n"
"kernel void t_half(uint2 h [[thread_position_in_grid]], texture2d<float> pos [[texture(0)]], texture2d<float, access::write> hr [[texture(1)]]) {\n"
"  if (h.x >= hr.get_width() || h.y >= hr.get_height()) return;\n"
"  float3 P = pos.read(min(h * 2, uint2(pos.get_width(), pos.get_height()) - 1)).xyz;\n"
"  hr.write(all(P == 0.0) ? float4(0.0) : float4(t_f(P), 0, 0, 1), h); }\n"
"kernel void t_up(uint2 px [[thread_position_in_grid]], constant LU &u [[buffer(0)]], texture2d<float> pos [[texture(0)]], texture2d<float> nrm [[texture(1)]],\n"
"                 texture2d<float> hr [[texture(2)]], device float4 *o [[buffer(1)]]) {\n"
"  if (px.x >= pos.get_width() || px.y >= pos.get_height()) return;\n"
"  uint k = px.y * pos.get_width() + px.x; float3 p = pos.read(px).xyz, n = nrm.read(px).xyz; o[k] = 0.0;\n"
"  if (all(p == 0.0)) return;\n"
"  if (dot(n, u.eye.xyz - p) < 0.0) n = -n;\n"
"  float4 r = rf_up(u, hr, pos, nrm, px, p, n, 0.0); o[k] = float4(r.x, t_f(p), r.a, 1); }\n"
/* the denoiser: one frame's noisy samples of a constant environment (1, \
   +-35% uniform, as one GGX sample a pixel gives), on the plane and the box's \
   top (100: what must not bleed); accumulated as reflt does (static camera: \
   the same pixel) and filtered as reflc does at roughness 0.3 */ \
"static float t_n(uint2 h, uint f) { uint x = h.x * 1973u + h.y * 9277u + f * 26699u; x ^= x >> 13; x *= 0x5bd1e995u; x ^= x >> 15; return float(x & 0xffffu) / 65535.0; }\n"
"kernel void t_acc(uint2 h [[thread_position_in_grid]], constant uint &f [[buffer(0)]], texture2d<float> pos [[texture(0)]],\n"
"                  texture2d<float> hs [[texture(1)]], texture2d<float, access::write> ho [[texture(2)]], texture2d<float, access::write> cur [[texture(3)]]) {\n"
"  int2 hw = int2(ho.get_width(), ho.get_height()); if (int(h.x) >= hw.x || int(h.y) >= hw.y) return;\n"
"  float3 P = pos.read(min(h * 2, uint2(pos.get_width(), pos.get_height()) - 1)).xyz; if (all(P == 0.0)) { ho.write(0.0, h); return; }\n"
"  float base = P.z > 1e-3 ? 100.0 : 1.0, c = base * (1.0 + 0.7 * (t_n(h, f) - 0.5)), lo = c, hi = c;\n"
"  for (int k = 0; k < 9; k++) { int2 q = clamp(int2(h) + int2(k % 3 - 1, k / 3 - 1), int2(0), hw - 1);\n"
"    float3 Q = pos.read(min(uint2(q) * 2, uint2(pos.get_width(), pos.get_height()) - 1)).xyz; if (all(Q == 0.0)) continue;\n"
"    float C = (Q.z > 1e-3 ? 100.0 : 1.0) * (1.0 + 0.7 * (t_n(uint2(q), f) - 0.5)); lo = min(lo, C); hi = max(hi, C); }\n"
"  float4 H = hs.read(h); float cnt = f == 0u ? 0.0 : H.a; float3 L = cnt > 0.0 ? rf_hist(float3(c), H.rgb, float3(lo), float3(hi), cnt) : float3(c);\n"
"  ho.write(float4(L, max(cnt, 1.0)), h); }\n"
"kernel void t_filt(uint2 px [[thread_position_in_grid]], constant LU &u [[buffer(0)]], texture2d<float> pos [[texture(0)]], texture2d<float> nrm [[texture(1)]],\n"
"                 texture2d<float> hr [[texture(2)]], device float4 *o [[buffer(1)]]) {\n"
"  if (px.x >= pos.get_width() || px.y >= pos.get_height()) return;\n"
"  uint k = px.y * pos.get_width() + px.x; float3 p = pos.read(px).xyz, n = nrm.read(px).xyz; o[k] = 0.0;\n"
"  if (all(p == 0.0)) return;\n"
"  if (dot(n, u.eye.xyz - p) < 0.0) n = -n;\n"
"  float4 r = rf_up(u, hr, pos, nrm, px, p, n, rf_rad(0.3)); o[k] = float4(r.x, p.z > 1e-3 ? 100.0 : 1.0, r.a, 1); }\n"
"kernel void t_wet(uint i [[thread_position_in_grid]], constant LU &u [[buffer(0)]], device const float4 *in [[buffer(1)]], device float4 *o [[buffer(2)]]) {\n"
"  float3 p = float3(in[i].xy, 0.0), n = float3(0, 0, 1), a = float3(in[i].z); float r = in[i].w;\n"
"  wet(u, p, n, a, r); float2 w = wetness(u, p, n); o[i] = float4(a.x, r, w.x, w.y); }\n";

int main(int argc, char **argv)
{
    @autoreleasepool {
        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        NSError *err = nil;
        NSString *src;
        id<MTLLibrary> lib;
        id<MTLCommandQueue> q;
        MTLCompileOptions *opt = [MTLCompileOptions new];
        if (argc < 2 || !dev) {
            fprintf(stderr, "usage: refl_test <refl.metal> (and a Metal device)\n");
            return 2;
        }
        src = [NSString stringWithContentsOfFile:[NSString stringWithUTF8String:argv[1]] encoding:NSUTF8StringEncoding error:&err];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        opt.fastMathEnabled = NO;
#pragma clang diagnostic pop
        lib = src ? [dev newLibraryWithSource:[src stringByAppendingString:@(k_kern)] options:opt error:&err] : nil;
        if (!lib) {
            fprintf(stderr, "refl_test: %s\n", [[err description] UTF8String]);
            return 2;
        }
        q = [dev newCommandQueue];
        setup();

        /* 4: LU */
        {
            id<MTLBuffer> o = [dev newBufferWithLength:16 options:MTLResourceStorageModeShared];
            id<MTLComputePipelineState> ps = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"t_size"] error:&err];
            id<MTLCommandBuffer> cb = [q commandBuffer];
            id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
            [e setComputePipelineState:ps]; [e setBuffer:o offset:0 atIndex:0];
            [e dispatchThreads:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
            [e endEncoding]; [cb commit]; [cb waitUntilCompleted];
            report(*(uint32_t *)o.contents == sizeof(LU), "LU: the shader's size = the C struct's", "%u and %zu bytes", *(uint32_t *)o.contents, sizeof(LU));
        }

        /* 1 and 2: the march */
        {
            static float gpu[H][W][4];
            MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:W height:H mipmapped:NO];
            id<MTLTexture> t;
            id<MTLBuffer> o = [dev newBufferWithLength:sizeof gpu options:MTLResourceStorageModeShared];
            id<MTLComputePipelineState> ps = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"t_ssr"] error:&err];
            id<MTLCommandBuffer> cb;
            id<MTLComputeCommandEncoder> e;
            int i, j, k, nvis = 0, nfound = 0, nfalse = 0, nplane = 0, nagree = 0, ncmp = 0;
            double emax = 0, gmax = 0, t0;
            td.usage = MTLTextureUsageShaderRead;
            td.storageMode = MTLStorageModeShared;
            t = [dev newTextureWithDescriptor:td];
            [t replaceRegion:MTLRegionMake2D(0, 0, W, H) mipmapLevel:0 withBytes:g_pos bytesPerRow:W * 16];
            cb = [q commandBuffer];
            e = [cb computeCommandEncoder];
            [e setComputePipelineState:ps];
            [e setBytes:&g_lu length:sizeof g_lu atIndex:0];
            [e setTexture:t atIndex:0];
            [e setBuffer:o offset:0 atIndex:1];
            [e dispatchThreads:MTLSizeMake(W, H, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
            [e endEncoding];
            t0 = CFAbsoluteTimeGetCurrent();
            [cb commit]; [cb waitUntilCompleted];
            (void)t0;
            memcpy(gpu, o.contents, sizeof gpu);
            for (j = 0; j < H; j++)
                for (i = 0; i < W; i++) {
                    const float *p = g_pos[j][i];
                    float v[3], r[3], c[4];
                    double o3[3], d3d[3], th, hp[3];
                    if (zero3(p) || fabsf(p[2]) > 1e-4f)
                        continue;
                    nplane++;
                    for (k = 0; k < 3; k++) v[k] = g_eye[k] - p[k];
                    nrm3(v);
                    r[0] = -v[0]; r[1] = -v[1]; r[2] = v[2];
                    ssr_cpu(p, r, 80.0f, 1.0f, c);
                    /* 2: the GPU against the CPU */
                    ncmp++;
                    if ((c[2] > 0) == (gpu[j][i][2] > 0) && (c[2] <= 0 || hypot(c[0] - gpu[j][i][0], c[1] - gpu[j][i][1]) <= 0.01))
                        nagree++;
                    if (c[2] > 0 && gpu[j][i][2] > 0)
                        gmax = fmax(gmax, hypot(c[0] - gpu[j][i][0], c[1] - gpu[j][i][1]));
                    /* 1: the CPU against the analytic hit */
                    for (k = 0; k < 3; k++) o3[k] = p[k], d3d[k] = r[k];
                    th = box_hit(o3, d3d, 80.0);
                    if (th > 0) {
                        float hf[3], s[2], w;
                        const float *P;
                        for (k = 0; k < 3; k++) hp[k] = o3[k] + d3d[k] * th, hf[k] = (float)hp[k];
                        project(hf, s, &w);
                        if (w > 0 && s[0] >= 0 && s[1] >= 0 && s[0] < W && s[1] < H) {
                            P = rdpos(s[0], s[1]);
                            if (fabs(P[0] - hp[0]) + fabs(P[1] - hp[1]) + fabs(P[2] - hp[2]) < 0.02) {   /* in view: the G-buffer holds it */
                                nvis++;
                                if (c[2] > 0) {
                                    nfound++;
                                    emax = fmax(emax, hypot(c[0] - s[0], c[1] - s[1]));
                                }
                            }
                        }
                    } else if (c[2] > 0)
                        nfalse++;
                }
            /* 2 px strides (half resolution) step over the box's front face where it is 1 to 2 px tall under its top edge */
            report(nvis > 1000 && nfound >= 0.88 * nvis, "SSR: visible mirror hits found (plane and box)", "%d of %d (%.2f%%), %d plane pixels", nfound, nvis,
                   100.0 * nfound / nvis, nplane);
            report(emax <= 1.0, "SSR: hit pixel against the analytic hit", "max %.3f px", emax);
            report(nfalse <= 0.005 * nplane, "SSR: hits where the ray meets nothing", "%d of %d plane pixels", nfalse, nplane);
            report(nagree >= ncmp - ncmp / 1000, "SSR: GPU kernel = CPU reference", "%d of %d pixels agree (hit or miss, within 0.01 px); max %.4f px where both hit",
                   nagree, ncmp, gmax);
        }

        /* 5: the bilateral upsample */
        {
            static float nrm[H][W][4], up[H][W][4];
            MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:W height:H mipmapped:NO];
            MTLTextureDescriptor *hd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float width:(W + 1) / 2 height:(H + 1) / 2 mipmapped:NO];
            id<MTLTexture> tp, tn, th;
            id<MTLBuffer> o = [dev newBufferWithLength:sizeof up options:MTLResourceStorageModeShared];
            id<MTLComputePipelineState> ph = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"t_half"] error:&err];
            id<MTLComputePipelineState> pu = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"t_up"] error:&err];
            id<MTLCommandBuffer> cb;
            id<MTLComputeCommandEncoder> e;
            int i, j, di, dj, nin = 0, nedge = 0, nbleed = 0, nnone = 0;
            double emax = 0;
            for (j = 0; j < H; j++)
                for (i = 0; i < W; i++) {   /* the face normals: the plane up, the box's faces by the nearest slab */
                    const float *P = g_pos[j][i];
                    float *N = nrm[j][i];
                    N[0] = N[1] = N[2] = N[3] = 0.0f;
                    if (zero3(P)) continue;
                    if (P[2] < 1e-4f || fabsf(P[2] - BZ1) < 1e-4f) N[2] = 1.0f;
                    else if (fabsf(P[0] - BX0) < 1e-4f) N[0] = -1.0f;
                    else if (fabsf(P[0] - BX1) < 1e-4f) N[0] = 1.0f;
                    else if (fabsf(P[1] - BY0) < 1e-4f) N[1] = -1.0f;
                    else N[1] = 1.0f;
                }
            td.usage = MTLTextureUsageShaderRead; td.storageMode = MTLStorageModeShared;
            tp = [dev newTextureWithDescriptor:td]; tn = [dev newTextureWithDescriptor:td];
            [tp replaceRegion:MTLRegionMake2D(0, 0, W, H) mipmapLevel:0 withBytes:g_pos bytesPerRow:W * 16];
            [tn replaceRegion:MTLRegionMake2D(0, 0, W, H) mipmapLevel:0 withBytes:nrm bytesPerRow:W * 16];
            hd.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite; hd.storageMode = MTLStorageModePrivate;
            th = [dev newTextureWithDescriptor:hd];
            cb = [q commandBuffer];
            e = [cb computeCommandEncoder];
            [e setComputePipelineState:ph]; [e setTexture:tp atIndex:0]; [e setTexture:th atIndex:1];
            [e dispatchThreads:MTLSizeMake(hd.width, hd.height, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
            [e setComputePipelineState:pu]; [e setBytes:&g_lu length:sizeof g_lu atIndex:0];
            [e setTexture:tp atIndex:0]; [e setTexture:tn atIndex:1]; [e setTexture:th atIndex:2]; [e setBuffer:o offset:0 atIndex:1];
            [e dispatchThreads:MTLSizeMake(W, H, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
            [e endEncoding]; [cb commit]; [cb waitUntilCompleted];
            memcpy(up, o.contents, sizeof up);
            for (j = 0; j < H; j++)
                for (i = 0; i < W; i++) {
                    int edge = 0;
                    if (up[j][i][3] <= 0) continue;
                    for (dj = -3; dj <= 3; dj++)   /* an edge: a neighbour within 3 px on another face or nothing */
                        for (di = -3; di <= 3; di++) {
                            int x = i + di, y = j + dj;
                            if (x < 0 || y < 0 || x >= W || y >= H || zero3(g_pos[y][x]) ||
                                memcmp(nrm[y][x], nrm[j][i], 12) || (nrm[j][i][2] > 0.5f && (g_pos[y][x][2] > 1e-3f) != (g_pos[j][i][2] > 1e-3f)))
                                edge = 1;
                        }
                    if (up[j][i][2] <= 0) { nnone++; continue; }   /* no sample on its surface: the sky's specular instead */
                    if (fabsf(up[j][i][0] - up[j][i][1]) > 50.0f) nbleed++;
                    if (edge) nedge++;
                    else if (vd(g_pos[j][i]) < 40.0f) { nin++; emax = fmax(emax, fabs(up[j][i][0] - up[j][i][1]) / up[j][i][1]); }   /* nearer than 40 m: farther, 2 px span metres and the function itself aliases */
                }
            report(nin > 10000 && emax < 0.015, "upsample: half-res = full-res away from edges", "max relative error %.5f (half float 0.001, the function itself across 2 px) over %d pixels nearer than 40 m (values 0.4..101.6)", emax, nin);
            report(nbleed == 0, "upsample: no bleed across a depth edge", "%d of %d edge pixels take the other surface's value; %d without a sample",
                   nbleed, nedge, nnone);

            /* 6: the denoiser, 16 frames of noisy samples accumulated and filtered */
            {
                id<MTLTexture> hh[2];
                id<MTLComputePipelineState> pa = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"t_acc"] error:&err];
                id<MTLComputePipelineState> pf = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"t_filt"] error:&err];
                uint32_t fr;
                double sum = 0, sq = 0, emean;
                int npl = 0, nb2 = 0;
                hh[0] = [dev newTextureWithDescriptor:hd]; hh[1] = [dev newTextureWithDescriptor:hd];
                for (fr = 0; fr < 16; fr++) {
                    cb = [q commandBuffer];
                    e = [cb computeCommandEncoder];
                    [e setComputePipelineState:pa]; [e setBytes:&fr length:4 atIndex:0];
                    [e setTexture:tp atIndex:0]; [e setTexture:hh[fr & 1] atIndex:1]; [e setTexture:hh[(fr & 1) ^ 1] atIndex:2]; [e setTexture:th atIndex:3];
                    [e dispatchThreads:MTLSizeMake(hd.width, hd.height, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
                    [e endEncoding]; [cb commit]; [cb waitUntilCompleted];
                }
                cb = [q commandBuffer];
                e = [cb computeCommandEncoder];
                [e setComputePipelineState:pf]; [e setBytes:&g_lu length:sizeof g_lu atIndex:0];
                [e setTexture:tp atIndex:0]; [e setTexture:tn atIndex:1]; [e setTexture:hh[0] atIndex:2]; [e setBuffer:o offset:0 atIndex:1];
                [e dispatchThreads:MTLSizeMake(W, H, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
                [e endEncoding]; [cb commit]; [cb waitUntilCompleted];
                memcpy(up, o.contents, sizeof up);
                for (j = 0; j < H; j++)
                    for (i = 0; i < W; i++) {
                        if (up[j][i][3] <= 0 || up[j][i][2] <= 0) continue;
                        if (fabsf(up[j][i][0] - up[j][i][1]) > 50.0f) nb2++;
                        if (up[j][i][1] == 1.0f && g_pos[j][i][2] < 1e-4f && vd(g_pos[j][i]) < 40.0f) { sum += up[j][i][0]; sq += (double)up[j][i][0] * up[j][i][0]; npl++; }
                    }
                emean = npl ? sum / npl : 0;
                report(npl > 10000 && fabs(emean - 1.0) < 0.02, "denoise: 16 frames converge to the split-sum value", "mean %.4f of 1 (constant environment, +-35%% samples) over %d plane pixels", emean, npl);
                report(npl > 10000 && sqrt(fmax(sq / npl - emean * emean, 0)) / emean < 0.01, "denoise: per-pixel deviation below 1%", "%.4f relative standard deviation after accumulation and the roughness 0.3 filter", sqrt(fmax(sq / npl - emean * emean, 0)) / emean);
                report(nb2 == 0, "denoise: the spatial filter does not bleed across a depth edge", "%d pixels take the other surface's value", nb2);
            }
        }

        /* 3: wetness */
        {
            enum { N = 4096 };
            static float in[N][4], out[N][4];
            id<MTLBuffer> bi, bo;
            id<MTLComputePipelineState> ps = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"t_wet"] error:&err];
            id<MTLCommandBuffer> cb;
            id<MTLComputeCommandEncoder> e;
            double am = 0, rm = 0, en = 0, enr = 0, enw = 0;
            int i, k, wl;
            LU u = g_lu;
            for (wl = 0; wl <= 4; wl++) {
                double wv = wl / 4.0;
                u.wet[0] = (float)wv; u.wet[1] = 0.6f;
                for (i = 0; i < N; i++) {
                    in[i][0] = (float)(i % 64) * 0.37f - 11.0f; in[i][1] = (float)(i / 64) * 0.41f - 13.0f;
                    in[i][2] = 0.05f + 0.85f * (float)((i * 7) % 64) / 63.0f; in[i][3] = 0.05f + 0.95f * (float)((i * 13) % 64) / 63.0f;
                }
                bi = [dev newBufferWithBytes:in length:sizeof in options:MTLResourceStorageModeShared];
                bo = [dev newBufferWithLength:sizeof out options:MTLResourceStorageModeShared];
                cb = [q commandBuffer];
                e = [cb computeCommandEncoder];
                [e setComputePipelineState:ps];
                [e setBytes:&u length:sizeof u atIndex:0];
                [e setBuffer:bi offset:0 atIndex:1];
                [e setBuffer:bo offset:0 atIndex:2];
                [e dispatchThreads:MTLSizeMake(N, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
                [e endEncoding]; [cb commit]; [cb waitUntilCompleted];
                memcpy(out, bo.contents, sizeof out);
                for (i = 0; i < N; i++) {
                    /* Lagarde 2013 in double, with the GPU's wetness and puddle */
                    double w = out[i][2], pd = out[i][3], r0 = in[i][3], por = fmin(fmax((r0 - 0.5) / 0.4, 0.0), 1.0);
                    double f = 1.0 + (0.2 - 1.0) * por, a = w > 0 ? in[i][2] * (1.0 + (f - 1.0) * w) : in[i][2];
                    double r = w > 0 ? fmin(r0, (r0 + (0.1 - r0) * w) + (0.03 - (r0 + (0.1 - r0) * w)) * pd) : r0;
                    am = fmax(am, fabs(out[i][0] - a)); rm = fmax(rm, fabs(out[i][1] - r));
                    if (fabs(wv - 1.0) < 1e-9 && fabs(w - 1.0) > 1e-6)
                        am = fmax(am, 1.0);   /* full wetness everywhere on the road */
                }
            }
            report(am < 1e-5 && rm < 1e-5, "wet: albedo and roughness = Lagarde's formula", "max |albedo error| %.2e, |roughness error| %.2e over 5 wetness levels", am, rm);
            /* the energy bound: E(mu, r') + a' (1 - E(mu, r')) <= 1 for white (albedo 0.9, delit's cap) */
            for (k = 0; k <= 10; k++) {
                double w = k / 10.0;
                int ir, im;
                for (ir = 0; ir < 6; ir++)
                    for (im = 0; im < 5; im++) {
                        double r0 = 0.1 + 0.18 * ir, mu = (im + 0.5) / 5.0, por = fmin(fmax((r0 - 0.5) / 0.4, 0.0), 1.0);
                        double a = 0.9 * (1.0 + (0.2 + 0.8 * (1.0 - por) - 1.0) * w), r = fmin(r0, r0 + (0.1 - r0) * w);
                        double Ev = E_ref(r, mu), tot = Ev + a * (1.0 - Ev);
                        if (tot > en) en = tot, enr = r, enw = w;
                    }
            }
            report(en <= 1.0, "wet: specular + diffuse albedo <= 1", "max %.4f (roughness %.2f, wetness %.1f; albedo 0.9)", en, enr, enw);
        }
        printf("%s\n", g_fail ? "refl_test: FAILED" : "refl_test: all passed");
        return g_fail ? 1 : 0;
    }
}
