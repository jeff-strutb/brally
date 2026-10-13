/* lighttest.m: the Remastered lighting's verification suite (docs/brally/
 * remaster/lighting/MEASUREMENTS.md, "Verification suite").  A standalone
 * Metal program; the game is not built or run.
 *
 * It includes the renderer itself (brr_metal.m): the shader source it
 * compiles is the renderer's own string expression, the pipelines are the
 * renderer's own (lit_init), and the C it calls (lit_exposure, the weather
 * and sky tables) is the renderer's own code.  The test kernels
 * (kernels.metal) are appended to that source and call its functions.  Every
 * result is checked against a closed form or a CPU double-precision
 * reference (ref.h), and printed PASS or FAIL with its numbers.
 *
 *   lighttest <kernels.metal> [--dump <file.metal>]
 *
 * --dump writes the renderer's whole shader source (as brr_open compiles it)
 * for the offline compiles in run_all.sh. */
#include "brr_metal.m"
#include "brr_metal_taa.m"

#include <float.h>
#include <stdarg.h>
#include "ref.h"

#include "game_stubs.h"

/* ---- reporting ------------------------------------------------------------------------ */
static int g_pass, g_fail;
static void report(int ok, const char *name, const char *fmt, ...)
{
    va_list ap;
    printf("%s  %-44s ", ok ? "PASS" : "FAIL", name);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    if (ok)
        g_pass++;
    else
        g_fail++;
}
static void section(const char *s) { printf("\n== %s\n", s); }
/* a worst case that a NaN cannot hide in (fmax drops a NaN argument) */
#define WMAX(w, v) ((v) != (v) ? INFINITY : fmax((w), (v)))
/* every value read back from a test kernel is scanned: a NaN or infinity
   anywhere fails the run (the last check) */
static size_t g_nonfin, g_scanned;
static void nf_scan(const float *p, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        g_nonfin += !isfinite(p[i]);
    g_scanned += n;
}

/* ---- Metal helpers ------------------------------------------------------------------- */
static id<MTLLibrary> g_tl;            /* the renderer's source with the test kernels */
static id<MTLCommandQueue> g_q;
static NSMutableDictionary *g_cp;

static id<MTLComputePipelineState> cpipe(NSString *name)
{
    id<MTLComputePipelineState> p = g_cp[name];
    NSError *err = nil;
    if (!p) {
        p = [s_dev newComputePipelineStateWithFunction:[g_tl newFunctionWithName:name] error:&err];
        if (!p) {
            fprintf(stderr, "lighttest: kernel %s: %s\n", [name UTF8String], [[err description] UTF8String]);
            exit(2);
        }
        g_cp[name] = p;
    }
    return p;
}
typedef void (^CBind)(id<MTLComputeCommandEncoder>);
typedef void (^RBind)(id<MTLRenderCommandEncoder>);
static void kern3(NSString *name, MTLSize grid, CBind bind)
{
    id<MTLCommandBuffer> cb = [g_q commandBuffer];
    id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
    id<MTLComputePipelineState> p = cpipe(name);
    [e setComputePipelineState:p];
    bind(e);
    [e dispatchThreads:grid threadsPerThreadgroup:MTLSizeMake(grid.width >= 64 ? 64 : grid.width, 1, 1)];
    [e endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
}
static void kern(NSString *name, NSUInteger n, CBind bind) { kern3(name, MTLSizeMake(n, 1, 1), bind); }
static id<MTLBuffer> B(const void *p, size_t n)
{
    return p ? [s_dev newBufferWithBytes:p length:n options:MTLResourceStorageModeShared]
             : [s_dev newBufferWithLength:n options:MTLResourceStorageModeShared];
}
static id<MTLTexture> T2(MTLPixelFormat f, int w, int h)
{
    MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:f width:(NSUInteger)w height:(NSUInteger)h mipmapped:NO];
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    td.storageMode = MTLStorageModeShared;
    return [s_dev newTextureWithDescriptor:td];
}
/* a full-screen pass of one of the renderer's pipelines (its vertex stage lvs) */
static void fullscreen(id<MTLRenderPipelineState> p, id<MTLTexture> target, MTLLoadAction la, RBind bind)
{
    MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
    id<MTLCommandBuffer> cb = [g_q commandBuffer];
    id<MTLRenderCommandEncoder> e;
    rp.colorAttachments[0].texture = target;
    rp.colorAttachments[0].loadAction = la;
    rp.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 0);
    rp.colorAttachments[0].storeAction = MTLStoreActionStore;
    e = [cb renderCommandEncoderWithDescriptor:rp];
    [e setRenderPipelineState:p];
    bind(e);
    [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [e endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
}
static double h2d(uint16_t u)
{
    _Float16 h;
    memcpy(&h, &u, 2);
    return (double)h;
}
static uint16_t d2h(double d)
{
    _Float16 h = (_Float16)d;
    uint16_t u;
    memcpy(&u, &h, 2);
    return u;
}
/* an RGBA16F texture's texels as doubles, 4 a texel */
static double *read16(id<MTLTexture> t)
{
    size_t n = t.width * t.height, i;
    uint16_t *u = malloc(n * 8);
    double *d = malloc(n * 4 * sizeof(double));
    [t getBytes:u bytesPerRow:t.width * 8 fromRegion:MTLRegionMake2D(0, 0, t.width, t.height) mipmapLevel:0];
    for (i = 0; i < n * 4; i++)
        d[i] = h2d(u[i]);
    free(u);
    return d;
}
static void write16(id<MTLTexture> t, const double *d)
{
    size_t n = t.width * t.height, i;
    uint16_t *u = malloc(n * 8);
    for (i = 0; i < n * 4; i++)
        u[i] = d2h(d[i]);
    [t replaceRegion:MTLRegionMake2D(0, 0, t.width, t.height) mipmapLevel:0 withBytes:u bytesPerRow:t.width * 8];
    free(u);
}

/* ---- a deterministic generator ---------------------------------------------------------- */
static uint64_t g_rs = 0x9E3779B97F4A7C15ull;
static double urand(void)
{
    g_rs ^= g_rs << 13;
    g_rs ^= g_rs >> 7;
    g_rs ^= g_rs << 17;
    return (double)(g_rs >> 11) * (1.0 / 9007199254740992.0);
}
static double nrand(void) { return sqrt(-2.0 * log(urand() + 1e-300)) * cos(2.0 * R_PI * urand()); }
static void udir(double d[3])
{
    double z = urand() * 2.0 - 1.0, p = 2.0 * R_PI * urand(), s = sqrt(1.0 - z * z);
    d[0] = s * cos(p);
    d[1] = s * sin(p);
    d[2] = z;
}
static double dot3(const double *a, const double *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
static double lum(const double *c) { return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]; }
static int finite4(const float *f) { return isfinite(f[0]) && isfinite(f[1]) && isfinite(f[2]); }

/* =========================================================================================
 * 1. BRDF
 * ========================================================================================= */
typedef struct { float n[4], v[4], l[4], p[4]; } TBrdf;

static void brdf_eval(const TBrdf *in, float *out, int n)
{
    id<MTLBuffer> bi = B(in, (size_t)n * sizeof *in), bo = B(NULL, (size_t)n * 16);
    kern(@"t_brdf", (NSUInteger)n, ^(id<MTLComputeCommandEncoder> e) {
        [e setBuffer:bi offset:0 atIndex:0];
        [e setBuffer:bo offset:0 atIndex:1];
        [e setTexture:s_brdfE atIndex:0];
    });
    memcpy(out, bo.contents, (size_t)n * 16);
    nf_scan((const float *)bo.contents, ((size_t)n * 16) / 4);
}

#define FUR_NR 12
#define FUR_NM 6
#define FUR_N 1024
static const float k_rough[FUR_NR] = { 0.045f, 0.1f, 0.2f, 0.25f, 0.3f, 0.4f, 0.5f, 0.55f, 0.6f, 0.7f, 0.85f, 1.0f };
static const float k_mu[FUR_NM] = { 1.0f, 0.75f, 0.5f, 0.25f, 0.1f, 0.03f };

/* the same estimators in double, from the formulas (Heitz 2014's
 * height-correlated Smith G2 with GGX, Schlick's Fresnel): spec x, lobe y
 * (F = 1); z the coupled diffuse's albedo in closed form, 1 - x (Kelemen and
 * Szirmay-Kalos 2001: the diffuse takes what the lobe leaves) */
static void furnace_ref(double r, double mu, double out[3])
{
    double a = fmax(r * r, 2e-3), a2 = a * a, v[3] = { sqrt(fmax(1.0 - mu * mu, 0.0)), 0.0, mu };
    double es = 0, el = 0, ed = 0;
    int i, j;
    for (j = 0; j < FUR_N; j++) {
        double u1 = (j + 0.5) / FUR_N, den = 1.0 + (a2 - 1.0) * u1, ch2 = (1.0 - u1) / den, sh2 = a2 * u1 / den;
        double ch = sqrt(ch2), sh = sqrt(sh2), cl = sqrt(1.0 - u1), sl = sqrt(u1);
        for (i = 0; i < FUR_N; i++) {
            double ph = 2.0 * R_PI * (i + 0.5) / FUR_N, cp = cos(ph), sp = sin(ph);
            double h[3] = { sh * cp, sh * sp, ch }, vh = dot3(v, h), l[3], hd[3], len;
            int k;
            for (k = 0; k < 3; k++)
                l[k] = 2.0 * vh * h[k] - v[k];
            if (l[2] > 0.0 && vh > 0.0) {
                double nl = l[2], nv = mu;
                double V = 0.5 / (nl * sqrt(nv * nv * (1.0 - a2) + a2) + nv * sqrt(nl * nl * (1.0 - a2) + a2));
                double lobe = 4.0 * V * nl * vh / ch, F = 0.04 + 0.96 * pow(1.0 - vh, 5.0);
                es += lobe * F;
                el += lobe;
            }
            (void)hd, (void)len, (void)sl, (void)cl;
        }
    }
    out[0] = es / ((double)FUR_N * FUR_N);
    out[1] = el / ((double)FUR_N * FUR_N);
    out[2] = 1.0 - out[0];      /* the coupled diffuse's albedo: what the lobe leaves */
    (void)ed;
}

/* car paint's clear coat (brdf with a negative roughness): the layered BRDF
 * on the GPU against the same layering in double (the coat's GGX lobe and
 * its directional albedo Ec from the formulas, the paint's coupled BRDF as
 * brdf evaluates it at 0.35, checked against its own reference above); its
 * white furnace E(mu) = Ec(mu) + (1 - Ec(mu)) E_base(mu), E_base by
 * cosine-stratified quadrature of the GPU's paint BRDF; the coat's split
 * Ec + (1 - Ec) <= 1 */
static double coat_ref(const double v[3], const double l[3])
{
    double a = fmax(0.05 * 0.05, 2e-3), a2 = a * a, h[3], hl, nh, nl = l[2], nv = fmax(v[2], 1e-3), vh;
    int k;
    for (k = 0; k < 3; k++)
        h[k] = v[k] + l[k];
    hl = sqrt(dot3(h, h));
    for (k = 0; k < 3; k++)
        h[k] /= hl;
    nh = h[2];
    vh = fmin(fmax(dot3(h, v), 0.0), 1.0);
    if (nl <= 0.0)
        return 0.0;
    double d = nh * nh * a2 + (1.0 - nh * nh), D = a2 / (R_PI * d * d);
    double V = 0.5 / (nl * sqrt(nv * nv * (1.0 - a2) + a2) + nv * sqrt(nl * nl * (1.0 - a2) + a2));
    return D * V * (0.04 + 0.96 * pow(1.0 - vh, 5.0)) * nl;
}
static void test_carpaint(void)
{
    enum { NS = 20000, NQ = 256, NMU = 8 };
    static const double mus[NMU] = { 1.0, 0.8, 0.6, 0.4, 0.25, 0.1, 0.05, 0.02 };
    TBrdf *in = calloc(3 * NS, sizeof *in);
    float *out = malloc(3 * NS * 16);
    double maxd = 0, maxf = 0, fmaxE = 0, maxEc = 0, wmu = 0;
    int i, k, m;
    for (i = 0; i < NS; i++) {   /* random v and l over the upper hemisphere, albedo 0..1 */
        double v[3], l[3], al = urand();
        udir(v); udir(l); v[2] = fabs(v[2]); l[2] = fabs(l[2]);
        for (m = 0; m < 3; m++) {
            TBrdf *b = &in[m * NS + i];
            b->n[2] = 1;
            for (k = 0; k < 3; k++)
                b->v[k] = (float)v[k], b->l[k] = (float)l[k];
            b->p[0] = (float)al;
            b->p[1] = m == 0 ? -0.35f : m == 1 ? 0.35f : 0.05f;   /* layered, the paint, the coat (albedo 0) */
            if (m == 2)
                b->p[0] = 0;
        }
    }
    brdf_eval(in, out, 3 * NS);
    static double ect[257];   /* the coat's Ec(mu) in double, over sqrt(mu) */
    for (k = 0; k <= 256; k++) {
        double o[3], x = k / 256.0;
        furnace_ref(0.05, fmax(x * x, 1e-3), o);
        ect[k] = o[0];
    }
    for (i = 0; i < NS; i++) {
        double v[3], l[3], x;
        for (k = 0; k < 3; k++)
            v[k] = in[i].v[k], l[k] = in[i].l[k];
        x = sqrt(fmin(fmax(v[2], 1e-3), 1.0)) * 256.0;
        k = (int)fmin(x, 255.0);
        double ec = ect[k] + (ect[k + 1] - ect[k]) * (x - k);
        double ref = out[(NS + i) * 4] * (1.0 - ec) + coat_ref(v, l);
        double d = fabs(out[i * 4] - ref) / fmax(ref, 1e-2);
        if (v[2] > 0.02 && d > maxd)
            maxd = d;
        maxf = fmax(maxf, ref);
    }
    for (m = 0; m < NMU; m++) {   /* the white furnace */
        double mu = mus[m], o[3], eb = 0;
        TBrdf *q = calloc(NQ * NQ, sizeof *q);
        float *qo = malloc(NQ * NQ * 16);
        for (i = 0; i < NQ * NQ; i++) {
            double r = sqrt((i / NQ + 0.5) / NQ), ph = 2.0 * R_PI * ((i % NQ) + 0.5) / NQ;
            q[i].n[2] = 1;
            q[i].v[0] = (float)sqrt(1.0 - mu * mu), q[i].v[2] = (float)mu;
            q[i].l[0] = (float)(r * cos(ph)), q[i].l[1] = (float)(r * sin(ph)), q[i].l[2] = (float)sqrt(fmax(1.0 - r * r, 0.0));
            q[i].p[0] = 1, q[i].p[1] = 0.35f;
        }
        brdf_eval(q, qo, NQ * NQ);
        for (i = 0; i < NQ * NQ; i++)   /* cosine-weighted: E = mean of f / (n.l) x pi; brdf returns f n.l */
            eb += q[i].l[2] > 1e-6 ? qo[i * 4] / q[i].l[2] * R_PI : 0.0;
        eb /= (double)NQ * NQ;
        furnace_ref(0.05, mu, o);
        double E = o[0] + (1.0 - o[0]) * eb;
        if (E > fmaxE)
            fmaxE = E, wmu = mu;
        maxEc = fmax(maxEc, o[0]);
        printf("  car paint furnace mu %.2f: coat Ec %.4f, paint %.4f, layered %.4f\n", mu, o[0], eb, E);
        free(q); free(qo);
    }
    report(fmaxE <= 1.0 + 2e-3, "car paint: white furnace of the layered BRDF <= 1", "max %.5f at mu %.2f (albedo 1; %d x %d quadrature)", fmaxE, wmu, NQ, NQ);
    report(maxEc <= 1.0, "car paint: coat + transmitted paint <= 1", "max Ec %.4f, so Ec + (1 - Ec) = 1 at most", maxEc);
    report(maxd < 2e-2, "car paint: layered BRDF, GPU vs double", "max relative |GPU - CPU| %.2e over %d directions (n.v > 0.02); largest f %.3f", maxd, NS, maxf);
    free(in); free(out);
}

static void test_brdf(void)
{
    static const uint32_t P[4] = { FUR_N, FUR_NM, FUR_NR, 0 };
    id<MTLBuffer> bo = B(NULL, (size_t)FUR_NR * FUR_NM * FUR_N * 16);
    double E[FUR_NR][FUR_NM][3], R[FUR_NR][FUR_NM][3], emax = 0, lobed = 0, totd = 0, dd = 0;
    int ir, im, j, emr = 0, emm = 0;
    const float *o;
    section("1. BRDF (brdf: GGX, height-correlated Smith, Schlick F0 0.04; Lambert coupled through the lobe's albedo table)");
    kern3(@"t_furnace", MTLSizeMake(FUR_N, FUR_NM, FUR_NR), ^(id<MTLComputeCommandEncoder> e) {
        [e setBytes:k_rough length:sizeof k_rough atIndex:0];
        [e setBytes:k_mu length:sizeof k_mu atIndex:1];
        [e setBytes:P length:sizeof P atIndex:2];
        [e setBuffer:bo offset:0 atIndex:3];
        [e setTexture:s_brdfE atIndex:0];
    });
    o = bo.contents;
    nf_scan(o, (size_t)FUR_NR * FUR_NM * FUR_N * 4);
    for (ir = 0; ir < FUR_NR; ir++)
        for (im = 0; im < FUR_NM; im++) {
            double s[3] = { 0, 0, 0 }, tot;
            for (j = 0; j < FUR_N; j++) {
                const float *p = &o[((ir * FUR_NM + im) * FUR_N + j) * 4];
                s[0] += p[0], s[1] += p[1], s[2] += p[2];
            }
            for (j = 0; j < 3; j++)
                E[ir][im][j] = s[j] / FUR_N;
            furnace_ref(k_rough[ir], k_mu[im], R[ir][im]);
            tot = E[ir][im][0] + E[ir][im][2];
            if (!isfinite(tot))
                emax = INFINITY;
            if (tot > emax)
                emax = tot, emr = ir, emm = im;
            lobed = WMAX(lobed, fabs(E[ir][im][1] - R[ir][im][1]));
            totd = WMAX(totd, fabs(tot - (R[ir][im][0] + R[ir][im][2])));
            dd = WMAX(dd, fabs(E[ir][im][2] - R[ir][im][2]));
        }
    printf("  directional albedo, white (albedo 1): specular + diffuse, by roughness and view cosine\n");
    printf("  rough |");
    for (im = 0; im < FUR_NM; im++)
        printf(" mu %.2f", k_mu[im]);
    printf(" | lobe energy (F = 1) mu 1.0, 0.5, 0.1 | its loss at mu 1\n");
    for (ir = 0; ir < FUR_NR; ir++) {
        printf("  %5.3f |", k_rough[ir]);
        for (im = 0; im < FUR_NM; im++)
            printf("  %6.4f", E[ir][im][0] + E[ir][im][2]);
        printf(" |   %6.4f %6.4f %6.4f (ref %6.4f %6.4f %6.4f) | %5.1f%%\n", E[ir][0][1], E[ir][2][1], E[ir][4][1],
               R[ir][0][1], R[ir][2][1], R[ir][4][1], 100.0 * (1.0 - E[ir][0][1]));
    }
    {   /* the coupling's table (brdf_lut) as brdf reads it: E(mu) against the lobe's
           measured albedo, and E_avg against the table's own cosine mean */
        enum { NQ = 4000 };
        float (*q)[2] = malloc(sizeof(float) * 2 * (FUR_NR * FUR_NM + FUR_NR * NQ)), *qo;
        id<MTLBuffer> bq, bo2;
        double te = 0, ta = 0;
        int n = 0, wr = 0, wm = 0, k;
        for (ir = 0; ir < FUR_NR; ir++)
            for (im = 0; im < FUR_NM; im++)
                q[n][0] = k_mu[im], q[n][1] = k_rough[ir], n++;
        for (ir = 0; ir < FUR_NR; ir++)
            for (k = 0; k < NQ; k++)
                q[n][0] = (float)((k + 0.5) / NQ), q[n][1] = k_rough[ir], n++;
        bq = B(q, sizeof(float) * 2 * (size_t)n);
        bo2 = B(NULL, sizeof(float) * 2 * (size_t)n);
        kern(@"t_btab", (NSUInteger)n, ^(id<MTLComputeCommandEncoder> e) {
            [e setTexture:s_brdfE atIndex:0];
            [e setBuffer:bq offset:0 atIndex:0];
            [e setBuffer:bo2 offset:0 atIndex:1];
        });
        qo = bo2.contents;
        nf_scan(qo, (size_t)n * 2);
        for (ir = 0; ir < FUR_NR; ir++)
            for (im = 0; im < FUR_NM; im++)
                if (fabs(qo[(ir * FUR_NM + im) * 2] - E[ir][im][0]) > te || qo[(ir * FUR_NM + im) * 2] != qo[(ir * FUR_NM + im) * 2])
                    te = WMAX(te, fabs(qo[(ir * FUR_NM + im) * 2] - E[ir][im][0])), wr = ir, wm = im;
        for (ir = 0; ir < FUR_NR; ir++) {
            double s = 0;
            const float *p = &qo[(FUR_NR * FUR_NM + ir * NQ) * 2];
            for (k = 0; k < NQ; k++)
                s += p[k * 2] * 2.0 * ((k + 0.5) / NQ) / NQ;
            ta = WMAX(ta, fabs(s - p[1]));
        }
        {   /* the split sum's DFG term (brr_metal_refl.h reads this table): relative to the brute-force integral */
            double rel = 0, rr = 0, rm = 0;
            for (ir = 0; ir < FUR_NR; ir++)
                for (im = 0; im < FUR_NM; im++) {
                    double d = fabs(qo[(ir * FUR_NM + im) * 2] - E[ir][im][0]) / E[ir][im][0];
                    if (!(d <= rel))
                        rel = d, rr = k_rough[ir], rm = k_mu[im];
                }
            report(rel < 0.01, "DFG table (split sum) against the GGX integral", "max relative error %.3f%% (roughness %.3f, mu %.2f; 1024 x 1024 quadrature)",
                   rel * 100.0, rr, rm);
        }
        report(te < 2e-3, "BRDF table: E(mu) = the lobe's albedo", "max |table - measured| %.2e (roughness %.3f, mu %.2f)", te, k_rough[wr], k_mu[wm]);
        report(ta < 1e-3, "BRDF table: E_avg = its cosine mean", "max |E_avg - integral of the table| %.2e", ta);
        free(q);
    }
    report(emax <= 1.0 + 1e-3, "white furnace: albedo <= 1", "max %.4f at roughness %.3f, mu %.2f (1,048,576 samples a cell)",
           emax, k_rough[emr], k_mu[emm]);
    report(lobed < 2e-3, "GGX lobe energy vs double reference", "max |GPU - CPU| %.2e over %d cells", lobed, FUR_NR * FUR_NM);
    report(totd < 2e-3, "white albedo vs double reference", "max |GPU - CPU| %.2e", totd);
    report(dd < 2e-3, "diffuse albedo = 1 - E_spec(v) (closed form)", "max |GPU - (1 - E_spec)| %.2e", dd);

    {   /* reciprocity */
        enum { N = 65536 };
        TBrdf *a = malloc(N * sizeof *a), *b = malloc(N * sizeof *b);
        float *oa = malloc(N * 16), *ob = malloc(N * 16);
        double worst = 0;
        int i, k;
        for (i = 0; i < N; i++) {
            double n[3], v[3], l[3];
            udir(n);
            do udir(v); while (dot3(v, n) < 0.02);
            do udir(l); while (dot3(l, n) < 0.02);
            for (k = 0; k < 3; k++) {
                a[i].n[k] = b[i].n[k] = (float)n[k];
                a[i].v[k] = b[i].l[k] = (float)v[k];
                a[i].l[k] = b[i].v[k] = (float)l[k];
            }
            a[i].p[0] = b[i].p[0] = (float)urand();
            a[i].p[1] = b[i].p[1] = (float)(0.045 + 0.955 * urand());
        }
        brdf_eval(a, oa, N);
        brdf_eval(b, ob, N);
        for (i = 0; i < N; i++) {
            double nl = 0, nv = 0, fa, fb;
            for (k = 0; k < 3; k++)
                nl += (double)a[i].n[k] * a[i].l[k], nv += (double)a[i].n[k] * a[i].v[k];
            fa = oa[i * 4] / nl;
            fb = ob[i * 4] / nv;
            worst = WMAX(worst, fabs(fa - fb) / fmax(fmax(fa, fb), 1e-30));
        }
        report(worst < 1e-4, "reciprocity f(v,l) = f(l,v)", "max relative difference %.2e over %d random triples", worst, N);
        free(a), free(b), free(oa), free(ob);
    }
    {   /* Fresnel: in the mirror direction (h = n) the specular part (albedo 0) is
           D V F n.l with D = 1 / (pi a^2) and V = 1 / (4 n.l sqrt(n.l^2 (1 - a^2) + a^2)) */
        enum { N = 90 };
        TBrdf in[2 * N];
        float out[2 * N * 4];
        double f0 = 0, worst = 0;
        int i;
        memset(in, 0, sizeof in);
        for (i = 0; i < N; i++) {
            double t = i * R_PI / 180.0;
            int k;
            for (k = 0; k < 2; k++) {
                TBrdf *p = &in[2 * i + k];
                p->n[2] = 1;
                p->v[0] = (float)sin(t), p->v[2] = (float)cos(t);
                p->l[0] = (float)-sin(t), p->l[2] = (float)cos(t);
                p->p[0] = 0.0f;
                p->p[1] = 0.5f;
            }
        }
        brdf_eval(in, out, 2 * N);
        for (i = 0; i < N; i++) {
            double t = i * R_PI / 180.0, nl = cos(t), a2 = pow(0.5 * 0.5, 2.0);
            double D = 1.0 / (R_PI * a2), V = 0.5 / (2.0 * nl * sqrt(nl * nl * (1.0 - a2) + a2)), F = out[2 * i * 4] / (D * V * nl);
            double want = 0.04 + 0.96 * pow(1.0 - cos(t), 5.0);
            if (i == 0)
                f0 = F;
            worst = WMAX(worst, fabs(F - want));
        }
        report(fabs(f0 - 0.04) < 1e-4, "Fresnel at normal incidence", "F0 %.6f (expected 0.04)", f0);
        report(worst < 1e-3, "Fresnel, Schlick over 0..89 degrees", "max |F - Schlick| %.2e", worst);
    }
}

/* =========================================================================================
 * 2. Sky and atmosphere
 * ========================================================================================= */
static id<MTLTexture> g_aT, g_aM, g_aS, g_aI;

/* the atmosphere's tables for a sun and camera, the renderer's passes in its order */
static void sky_tables(const AU *a, int all)
{
    id<MTLTexture> tg[4] = { g_aT, g_aM, g_aS, g_aI };
    id<MTLRenderPipelineState> pp[4] = { s_patmT, s_patmM, s_patmS, s_patmI };
    int k;
    for (k = all ? 0 : 2; k < 4; k++)
        fullscreen(pp[k], tg[k], MTLLoadActionDontCare, ^(id<MTLRenderCommandEncoder> e) {
            [e setFragmentBytes:a length:sizeof *a atIndex:0];
            [e setFragmentTexture:k == 3 ? g_aS : g_aT atIndex:0];
            [e setFragmentTexture:k == 3 ? g_aT : g_aM atIndex:1];
        });
}

/* CPU transmittance table, Bruneton's parameterisation with the nodes on
 * the unit range's ends, Simpson's rule per node */
#define CT_NMU 512
#define CT_NR 128
static double g_ct[CT_NR][CT_NMU][3];
static void ct_build(double mie)
{
    double H = sqrt(A_RT * A_RT - A_RG * A_RG);
    int i, j;
    for (j = 0; j < CT_NR; j++)
        for (i = 0; i < CT_NMU; i++) {
            double rho = H * j / (CT_NR - 1.0), r = sqrt(rho * rho + A_RG * A_RG);
            double dmin = A_RT - r, dmax = rho + H, d = dmin + (dmax - dmin) * i / (CT_NMU - 1.0);
            double mu = d == 0.0 ? 1.0 : fmin(fmax((H * H - rho * rho - d * d) / (2.0 * r * d), -1.0), 1.0), tau[3];
            int k;
            a_tau(r, mu, mie, tau, 500);
            for (k = 0; k < 3; k++)
                g_ct[j][i][k] = exp(-tau[k]);
        }
}
static void ct_get(double r, double mu, double T[3])
{
    double H = sqrt(A_RT * A_RT - A_RG * A_RG), rho = sqrt(fmax(r * r - A_RG * A_RG, 0.0));
    double d = fmax(a_sphere(r, mu, A_RT), 0.0), dmin = A_RT - r, dmax = rho + H;
    double x = fmin(fmax((d - dmin) / fmax(dmax - dmin, 1e-9), 0.0), 1.0) * (CT_NMU - 1), y = fmin(rho / H, 1.0) * (CT_NR - 1);
    int i = (int)fmin(x, CT_NMU - 2), j = (int)fmin(y, CT_NR - 2), k;
    double fx = x - i, fy = y - j;
    for (k = 0; k < 3; k++)
        T[k] = (g_ct[j][i][k] * (1 - fx) + g_ct[j][i + 1][k] * fx) * (1 - fy) + (g_ct[j + 1][i][k] * (1 - fx) + g_ct[j + 1][i + 1][k] * fx) * fy;
}

/* CPU multiple-scattering table (Hillaire 5.5) at the renderer's texel centres */
#define CM_N 32
static double g_cm[CM_N][CM_N][3], g_fms_max;
/* one node: NA azimuths x NZ cosines on each side of the horizon, NS steps a ray */
static void cm_node(double h, double cs, double mie, int NA, int NZ, int NS, double psi[3], double fms[3])
{
    {
        {
            double r = A_RG + fmax(h, 0.01);
            double sun[3] = { 0.0, sqrt(fmax(1.0 - cs * cs, 0.0)), cs }, L2[3] = { 0, 0, 0 }, fm[3] = { 0, 0, 0 };
            /* the sphere split at the horizon's cosine, each side stratified on its own:
               the radiance jumps there (the sunlit ground below, the sky above) */
            double mh = -sqrt(fmax(1.0 - (A_RG / r) * (A_RG / r), 0.0));
            int a, z, s, k;
            for (z = 0; z < 2 * NZ; z++)
                for (a = 0; a < NA; a++) {
                    double up = z < NZ, u = ((z % NZ) + 0.5) / NZ, wgt = (up ? 1.0 - mh : 1.0 + mh) * 0.5 / (NZ * NA);
                    double mu = up ? mh + (1.0 - mh) * u : -1.0 + (1.0 + mh) * u, th = 2.0 * R_PI * (a + 0.5) / NA, sz = sqrt(1.0 - mu * mu);
                    double d[3] = { cos(th) * sz, sin(th) * sz, mu };
                    double tg = a_ground(r, mu), len = tg > 0.0 ? tg : fmax(a_sphere(r, mu, A_RT), 0.0), dt = len / NS;
                    double tau[3] = { 0, 0, 0 }, Ls[3] = { 0, 0, 0 }, f[3] = { 0, 0, 0 };
                    for (s = 0; s < NS; s++) {
                        double t = (s + 0.5) * dt, p[3] = { d[0] * t, d[1] * t, r + d[2] * t }, rr = sqrt(dot3(p, p));
                        double ext[3], sr[3], sm, cz = dot3(p, sun) / rr, Ts[3], tm[3];
                        a_medium(rr - A_RG, mie, ext, sr, &sm);
                        ct_get(rr, cz, Ts);
                        for (k = 0; k < 3; k++) {
                            double sc = sr[k] + sm;
                            tm[k] = exp(-(tau[k] + ext[k] * dt * 0.5));
                            Ls[k] += tm[k] * (a_ground(rr, cz) > 0.0 ? 0.0 : Ts[k]) * sc / (4.0 * R_PI) * dt;
                            f[k] += tm[k] * sc * dt;
                            tau[k] += ext[k] * dt;
                        }
                    }
                    if (tg > 0.0) {
                        double pg[3] = { d[0] * tg, d[1] * tg, r + d[2] * tg }, lg = sqrt(dot3(pg, pg)), ng[3], Tg[3], c;
                        for (k = 0; k < 3; k++)
                            ng[k] = pg[k] / lg;
                        c = dot3(ng, sun);
                        ct_get(A_RG, c, Tg);
                        for (k = 0; k < 3; k++)
                            Ls[k] += exp(-tau[k]) * Tg[k] * fmax(c, 0.0) * A_ALB / R_PI;
                    }
                    for (k = 0; k < 3; k++)
                        L2[k] += Ls[k] * wgt, fm[k] += f[k] * wgt;
                }
            for (k = 0; k < 3; k++) {
                psi[k] = L2[k] / (1.0 - fm[k]);
                fms[k] = fm[k];
            }
        }
    }
}
static void cm_build(double mie)
{
    int i, j, k;
    g_fms_max = 0;
    for (j = 0; j < CM_N; j++)
        for (i = 0; i < CM_N; i++) {
            double f[3];
            cm_node((j + 0.5) / CM_N * (A_RT - A_RG), (i + 0.5) / CM_N * 2.0 - 1.0, mie, 32, 32, 160, g_cm[j][i], f);
            for (k = 0; k < 3; k++)
                g_fms_max = WMAX(g_fms_max, f[k]);
        }
}
static void cm_get(double r, double cs, double out[3])
{
    double x = fmin(fmax((cs * 0.5 + 0.5) * CM_N - 0.5, 0.0), CM_N - 1.0), y = fmin(fmax((r - A_RG) / (A_RT - A_RG) * CM_N - 0.5, 0.0), CM_N - 1.0);
    int i = (int)fmin(x, CM_N - 2), j = (int)fmin(y, CM_N - 2), k;
    double fx = x - i, fy = y - j;
    for (k = 0; k < 3; k++)
        out[k] = (g_cm[j][i][k] * (1 - fx) + g_cm[j][i + 1][k] * fx) * (1 - fy) + (g_cm[j + 1][i][k] * (1 - fx) + g_cm[j + 1][i + 1][k] * fx) * fy;
}

/* CPU sky radiance per unit sun illuminance at the top, from r0 (km) along d */
static void sky_ref(double r0, const double d[3], const double sun[3], double mie, double L[3])
{
    enum { NS = 1500 };
    double tg = a_ground(r0, d[2]), len = tg > 0.0 ? tg : fmax(a_sphere(r0, d[2], A_RT), 0.0), dt = len / NS;
    double c = dot3(d, sun), pr = a_rayleigh(c), pm = a_mie(c), tau[3] = { 0, 0, 0 };
    int s, k;
    L[0] = L[1] = L[2] = 0;
    for (s = 0; s < NS; s++) {
        double t = (s + 0.5) * dt, p[3] = { d[0] * t, d[1] * t, r0 + d[2] * t }, rr = sqrt(dot3(p, p));
        double ext[3], sr[3], sm, cz = dot3(p, sun) / rr, Ts[3], Ms[3], sh = a_ground(rr, cz) > 0.0 ? 0.0 : 1.0;
        a_medium(rr - A_RG, mie, ext, sr, &sm);
        ct_get(rr, cz, Ts);
        cm_get(rr, cz, Ms);
        for (k = 0; k < 3; k++) {
            double tm = exp(-(tau[k] + ext[k] * dt * 0.5));
            L[k] += tm * (Ts[k] * sh * (sr[k] * pr + sm * pm) + Ms[k] * (sr[k] + sm)) * dt;
            tau[k] += ext[k] * dt;
        }
    }
}

static void trans_gpu(const float (*q)[2], float (*out)[4], int n)
{
    id<MTLBuffer> bq = B(q, (size_t)n * 8), bo = B(NULL, (size_t)n * 16);
    kern(@"t_trans", (NSUInteger)n, ^(id<MTLComputeCommandEncoder> e) {
        [e setTexture:g_aT atIndex:0];
        [e setBuffer:bq offset:0 atIndex:0];
        [e setBuffer:bo offset:0 atIndex:1];
    });
    memcpy(out, bo.contents, (size_t)n * 16);
    nf_scan((const float *)bo.contents, ((size_t)n * 16) / 4);
}
static void skyview_gpu(const float (*d)[4], float (*out)[4], int n)
{
    id<MTLBuffer> bq = B(d, (size_t)n * 16), bo = B(NULL, (size_t)n * 16);
    kern(@"t_skyview", (NSUInteger)n, ^(id<MTLComputeCommandEncoder> e) {
        [e setTexture:g_aS atIndex:0];
        [e setBuffer:bq offset:0 atIndex:0];
        [e setBuffer:bo offset:0 atIndex:1];
    });
    memcpy(out, bo.contents, (size_t)n * 16);
    nf_scan((const float *)bo.contents, ((size_t)n * 16) / 4);
}

static double g_sun_peak;      /* the sun disc's radiance at its centre, cd/m2 (sunny, Coastline's sun) */

static void test_sky(void)
{
    static const double elev[] = { 90.0, 54.21, 30.0, 10.0, 5.0 };
    enum { NE = sizeof elev / sizeof elev[0] };
    const double cam = 0.002, top = 128000.0;
    AU a;
    double tz[3], worstT = 0, worstTmu = 0, worstTh = 0;
    int i, k, e;
    section("2. Sky and atmosphere (Hillaire 2020 tables, sunny: Mie x1)");
    g_aT = T2(MTLPixelFormatRGBA16Float, 256, 64);
    g_aM = T2(MTLPixelFormatRGBA16Float, 32, 32);
    g_aS = T2(MTLPixelFormatRGBA16Float, 192, 108);
    g_aI = T2(MTLPixelFormatRGBA32Float, 3, 1);
    memset(&a, 0, sizeof a);
    a.sun[2] = 1;
    a.cam[0] = (float)cam;
    a.atm[0] = 1;
    sky_tables(&a, 1);
    ct_build(1.0);
    cm_build(1.0);

    {   /* zenith transmittance against the closed form */
        float q[2][2] = { { (float)A_RG, 1.0f }, { (float)(A_RG + cam), 1.0f } }, o[2][4];
        double worst = 0;
        a_tau_zenith(1.0, tz);
        trans_gpu(q, o, 2);
        for (k = 0; k < 3; k++)
            worst = WMAX(worst, fabs(o[0][k] / exp(-tz[k]) - 1.0));
        report(worst < 2e-3, "zenith transmittance vs closed form", "GPU %.4f %.4f %.4f, analytic %.4f %.4f %.4f (tau %.5f %.5f %.5f), max rel err %.2e",
               o[0][0], o[0][1], o[0][2], exp(-tz[0]), exp(-tz[1]), exp(-tz[2]), tz[0], tz[1], tz[2], worst);
    }
    {   /* the table over heights and angles above the horizon, against exact optical depth */
        static const double hs[] = { 0.002, 0.1, 1.0, 5.0, 20.0 };
        enum { NH = 5, NMU = 80 };
        float q[NH * NMU][2], o[NH * NMU][4];
        double ex[NH * NMU][3], hz[NH];
        int n = 0, j;
        for (j = 0; j < NH; j++) {
            double r = A_RG + hs[j], muh = -sqrt(fmax(1.0 - (A_RG / r) * (A_RG / r), 0.0));
            hz[j] = muh;
            for (i = 0; i < NMU; i++) {
                double x = (i + 1.0) / NMU, mu = muh + 0.002 + (1.0 - muh - 0.002) * x * x, tau[3];
                q[n][0] = (float)r, q[n][1] = (float)mu;
                a_tau(r, mu, 1.0, tau, 4000);
                for (k = 0; k < 3; k++)
                    ex[n][k] = exp(-tau[k]);
                n++;
            }
        }
        trans_gpu(q, o, n);
        for (i = 0; i < n; i++)
            for (k = 0; k < 3; k++)
                if (fabs(o[i][k] - ex[i][k]) > worstT)
                    worstT = fabs(o[i][k] - ex[i][k]), worstTmu = q[i][1], worstTh = q[i][0] - A_RG;
        report(worstT < 0.01, "transmittance table vs exact, above horizon", "max |dT| %.4f (at h %.3f km, mu %.4f), %d points, 5 heights",
               worstT, worstTh, worstTmu, n);
    }
    {   /* the sun's illuminance at the ground, from the irradiance pass's second texel */
        double worst = 0;
        printf("  sun elevation | sun at the ground, lux: GPU   exact  | zenith sky cd/m2: GPU   CPU ref | sky on the ground, lux: GPU   CPU ref | (E_sky + E_sun) / (E_top mu_s) R G B\n");
        for (e = 0; e < NE; e++) {
            double el = elev[e] * R_PI / 180.0, sun[3] = { cos(el), 0.0, sin(el) }, tau[3], Te[3], Eg, Ee, zen[3], zr[3];
            double Esky_ref[3] = { 0, 0, 0 };
            float px[12], dz[1][4] = { { 0, 0, 1, 0 } }, oz[1][4];
            for (k = 0; k < 3; k++)
                a.sun[k] = (float)sun[k];
            sky_tables(&a, 0);
            [g_aI getBytes:px bytesPerRow:48 fromRegion:MTLRegionMake2D(0, 0, 3, 1) mipmapLevel:0];
            a_tau(A_RG + cam, sun[2], 1.0, tau, 4000);
            for (k = 0; k < 3; k++)
                Te[k] = exp(-tau[k]);
            Eg = top * (0.2126 * px[4] + 0.7152 * px[5] + 0.0722 * px[6]);
            Ee = top * lum(Te);
            worst = WMAX(worst, fabs(Eg / Ee - 1.0));
            skyview_gpu(dz, oz, 1);
            {
                double z[3] = { 0, 0, 1 };
                sky_ref(A_RG + cam, z, sun, 1.0, zr);
            }
            for (k = 0; k < 3; k++)
                zen[k] = oz[0][k];
            {   /* the sky's light on an up-facing surface, CPU: cosine-weighted hemisphere */
                int u, v2;
                for (u = 0; u < 16; u++)
                    for (v2 = 0; v2 < 32; v2++) {
                        double ph = acos(sqrt(1.0 - (u + 0.5) / 16.0)), th = (v2 + 0.5) / 32.0 * 2.0 * R_PI, L[3];
                        double d[3] = { cos(th) * sin(ph), sin(th) * sin(ph), cos(ph) };
                        sky_ref(A_RG + cam, d, sun, 1.0, L);
                        for (k = 0; k < 3; k++)
                            Esky_ref[k] += L[k] * R_PI / 512.0;
                    }
            }
            printf("  %8.2f      |        %8.0f %8.0f   |        %7.0f %7.0f   |       %7.0f %7.0f         |  %.3f %.3f %.3f\n",
                   elev[e], Eg, Ee, top * lum(zen), top * lum(zr), top * (0.2126 * px[0] + 0.7152 * px[1] + 0.0722 * px[2]),
                   top * lum(Esky_ref), (px[0] + px[4] * sun[2]) / sun[2], (px[1] + px[5] * sun[2]) / sun[2], (px[2] + px[6] * sun[2]) / sun[2]);
            if (e == 0) {
                double Lz = top * lum(zen);
                report(Lz >= 2000 && Lz <= 10000, "zenith sky luminance, sun overhead", "%.0f cd/m2 (clear sky, high sun: 2,000 to 10,000)", Lz);
            }
            {
                double re = fabs(lum(zen) / lum(zr) - 1.0), rs = fabs((0.2126 * px[0] + 0.7152 * px[1] + 0.0722 * px[2]) / lum(Esky_ref) - 1.0);
                char nm[64];
                snprintf(nm, sizeof nm, "sky vs CPU reference, sun %.0f deg", elev[e]);
                report(re < 0.05 && rs < 0.05, nm, "zenith luminance %+.2f%%, sky irradiance %+.2f%%", 100.0 * (lum(zen) / lum(zr) - 1.0),
                       100.0 * ((0.2126 * px[0] + 0.7152 * px[1] + 0.0722 * px[2]) / lum(Esky_ref) - 1.0));
                (void)re, (void)rs;
            }
            {
                double bound = 1.0 / (1.0 - A_ALB), worstb = 0;
                char nm[64];
                for (k = 0; k < 3; k++)
                    worstb = WMAX(worstb, (px[k] + px[4 + k] * sun[2]) / sun[2]);
                snprintf(nm, sizeof nm, "energy: light at ground <= 1/(1-albedo), %.0f deg", elev[e]);
                report(worstb <= bound, nm, "max over R G B %.3f (bound %.3f)", worstb, bound);
            }
        }
        report(worst < 0.01, "sun illuminance at the ground vs exact", "max relative error %.2e over %d elevations (E_top 128,000 lux x T)", worst, NE);
    }
    {   /* the multiple-scattering table against the CPU reference (Hillaire 5.5 with 1,024 directions, 160 steps) */
        double *m = read16(g_aM), worst = 0, mx[3] = { 0, 0, 0 }, rel = 0;
        int wi = 0, wj = 0, j;
        for (j = 0; j < CM_N; j++)
            for (i = 0; i < CM_N; i++)
                for (k = 0; k < 3; k++)
                    mx[k] = WMAX(mx[k], g_cm[j][i][k]);
        for (j = 0; j < 10; j++)          /* up to 31 km: where the light of the sky comes from */
            for (i = 0; i < CM_N; i++)
                for (k = 0; k < 3; k++) {
                    double ref = g_cm[j][i][k], g = m[(j * CM_N + i) * 4 + k];
                    if (fabs(g - ref) / mx[k] > worst)
                        worst = fabs(g - ref) / mx[k], wi = i, wj = j;
                    if ((i + 0.5) / CM_N * 2.0 - 1.0 > 0.1)
                        rel = WMAX(rel, fabs(g / ref - 1.0));
                }
        if (getenv("LIGHTTEST_MSDIAG")) {
            static const int cf[5][3] = { { 4, 32, 20 }, { 8, 4, 20 }, { 4, 64, 20 }, { 32, 32, 160 }, { 64, 64, 320 } };   /* the renderer's, the first version's, finer */
            static const int nd[2][2] = { { 9, 31 }, { 3, 20 } };
            int q, c2;
            for (q = 0; q < 2; q++) {
                double hh = (nd[q][0] + 0.5) / CM_N * 100.0, cc = (nd[q][1] + 0.5) / CM_N * 2.0 - 1.0;
                printf("  ms node h %.1f cs %.3f GPU %.4e %.4e %.4e\n", hh, cc, m[(nd[q][0] * CM_N + nd[q][1]) * 4], m[(nd[q][0] * CM_N + nd[q][1]) * 4 + 1],
                       m[(nd[q][0] * CM_N + nd[q][1]) * 4 + 2]);
                for (c2 = 0; c2 < 5; c2++) {
                    double ps[3], fz[3];
                    cm_node(hh, cc, 1.0, cf[c2][0], cf[c2][1], cf[c2][2], ps, fz);
                    printf("    CPU %2d az x %2d cos a side x %3d steps: %.4e %.4e %.4e  f_ms %.4f %.4f %.4f\n", cf[c2][0], cf[c2][1], cf[c2][2],
                           ps[0], ps[1], ps[2], fz[0], fz[1], fz[2]);
                }
            }
        }
        if (getenv("LIGHTTEST_MSDIAG"))
            for (j = 0; j < 10; j += 3)
                for (i = 0; i < CM_N; i += 2)
                    printf("  ms h %5.1f cs %+.3f GPU %.4e %.4e %.4e CPU %.4e %.4e %.4e\n", (j + 0.5) / CM_N * 100.0, (i + 0.5) / CM_N * 2.0 - 1.0,
                           m[(j * CM_N + i) * 4], m[(j * CM_N + i) * 4 + 1], m[(j * CM_N + i) * 4 + 2], g_cm[j][i][0], g_cm[j][i][1], g_cm[j][i][2]);
        report(worst < 0.05 && g_fms_max < 1.0, "multiple scattering table vs CPU reference",
               "max |GPU - CPU| %.2f%% of the table's peak (cos sun %.3f, h %.1f km); %.1f%% relative with the sun up; f_ms max %.4f < 1",
               100.0 * worst, (wi + 0.5) / CM_N * 2.0 - 1.0, (wj + 0.5) / CM_N * 100.0, 100.0 * rel, g_fms_max);
        free(m);
    }
    {   /* the phase functions integrate to one */
        enum { N = 200001 };
        float *c = malloc(N * 4);
        id<MTLBuffer> bc, bo;
        const float *o;
        double sr = 0, sm = 0;
        for (i = 0; i < N; i++)
            c[i] = (float)(-1.0 + 2.0 * i / (N - 1.0));
        bc = B(c, N * 4);
        bo = B(NULL, N * 8);
        kern(@"t_phase", N, ^(id<MTLComputeCommandEncoder> en) {
            [en setBuffer:bc offset:0 atIndex:0];
            [en setBuffer:bo offset:0 atIndex:1];
        });
        o = bo.contents;
        nf_scan(o, (size_t)N * 2);
        for (i = 0; i < N; i++) {
            double w = (i == 0 || i == N - 1 ? 0.5 : 1.0) * 2.0 / (N - 1.0) * 2.0 * R_PI;
            sr += o[2 * i] * w;
            sm += o[2 * i + 1] * w;
        }
        report(fabs(sr - 1.0) < 1e-4 && fabs(sm - 1.0) < 1e-3, "phase functions normalised", "Rayleigh %.6f, Cornette-Shanks g 0.8 %.6f (over the sphere)", sr, sm);
        free(c);
    }
    {   /* the sky view's direction mapping round trip */
        enum { N = 4096 };
        float d[N][4];
        id<MTLBuffer> bd, bo;
        const float *o;
        double worst = 0;
        for (i = 0; i < N; i++) {
            double v[3];
            udir(v);
            d[i][0] = (float)v[0], d[i][1] = (float)v[1], d[i][2] = (float)v[2], d[i][3] = 0;
        }
        bd = B(d, sizeof d);
        bo = B(NULL, N * 32);
        kern(@"t_svuv", N, ^(id<MTLComputeCommandEncoder> en) {
            [en setBuffer:bd offset:0 atIndex:0];
            [en setBuffer:bo offset:0 atIndex:1];
        });
        o = bo.contents;
        nf_scan(o, (size_t)N * 8);
        for (i = 0; i < N; i++) {
            double c = (o[8 * i] * d[i][0] + o[8 * i + 1] * d[i][1] + o[8 * i + 2] * d[i][2]);
            worst = WMAX(worst, acos(fmin(c, 1.0)) * 180.0 / R_PI);
        }
        report(worst < 0.05, "sky view mapping round trip", "max angle %.4f degrees over %d directions", worst, N);
    }
    {   /* the sun's disc: its radiance integrated over the disc against the sun's illuminance there */
        double el = atan2(2.5, sqrt(1.0 + 2.25)), sun[3] = { -1.0, -1.5, 2.5 }, ln = sqrt(dot3(sun, sun)), ex[3], ey[3], E = 0, Ttr;
        enum { NR = 600, NP = 64 };
        const double rmax = 0.30 * R_PI / 180.0;
        float (*d)[4] = malloc(sizeof(float) * 4 * NR * NP);
        id<MTLBuffer> bd, bo;
        const float *o;
        KU ku;
        float q[1][2], ot[1][4];
        (void)el;
        for (k = 0; k < 3; k++)
            sun[k] /= ln;
        for (k = 0; k < 3; k++)
            a.sun[k] = (float)sun[k];
        sky_tables(&a, 0);
        ex[0] = -sun[1], ex[1] = sun[0], ex[2] = 0;
        ln = sqrt(dot3(ex, ex));
        for (k = 0; k < 3; k++)
            ex[k] /= ln;
        ey[0] = sun[1] * ex[2] - sun[2] * ex[1], ey[1] = sun[2] * ex[0] - sun[0] * ex[2], ey[2] = sun[0] * ex[1] - sun[1] * ex[0];
        for (i = 0; i < NR; i++)
            for (k = 0; k < NP; k++) {
                double rr = (i + 0.5) / NR * rmax, ph = (k + 0.5) / NP * 2.0 * R_PI, v[3];
                int c;
                for (c = 0; c < 3; c++)
                    v[c] = sun[c] * cos(rr) + (ex[c] * cos(ph) + ey[c] * sin(ph)) * sin(rr);
                d[i * NP + k][0] = (float)v[0], d[i * NP + k][1] = (float)v[1], d[i * NP + k][2] = (float)v[2], d[i * NP + k][3] = 0;
            }
        memset(&ku, 0, sizeof ku);
        for (k = 0; k < 3; k++)
            ku.sun[k] = (float)sun[k];
        ku.eye[3] = (float)cam;
        ku.ktop[0] = (float)top, ku.ktop[1] = 1.0f, ku.ktop[2] = 1.0f;
        bd = B(d, sizeof(float) * 4 * NR * NP);
        bo = B(NULL, sizeof(float) * 8 * NR * NP);
        kern(@"t_skyrad", NR * NP, ^(id<MTLComputeCommandEncoder> en) {
            [en setBytes:&ku length:sizeof ku atIndex:0];
            [en setTexture:g_aS atIndex:0];
            [en setTexture:g_aT atIndex:1];
            [en setBuffer:bd offset:0 atIndex:1];
            [en setBuffer:bo offset:0 atIndex:2];
        });
        o = bo.contents;
        nf_scan(o, (size_t)NR * NP * 8);
        for (i = 0; i < NR; i++)
            for (k = 0; k < NP; k++) {
                double rr = (i + 0.5) / NR * rmax, dw = sin(rr) * (rmax / NR) * (2.0 * R_PI / NP);
                const float *p = &o[(i * NP + k) * 8 + 4];
                E += (0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2]) * dw;
            }
        g_sun_peak = 0.2126 * o[4] + 0.7152 * o[5] + 0.0722 * o[6];
        q[0][0] = (float)(A_RG + cam), q[0][1] = (float)sun[2];
        trans_gpu(q, ot, 1);
        Ttr = top * (0.2126 * ot[0][0] + 0.7152 * ot[0][1] + 0.0722 * ot[0][2]);
        report(fabs(E / Ttr - 1.0) < 0.01, "sun disc carries the sun's illuminance", "disc integral %.0f lux, sun %.0f lux: ratio %.4f", E, Ttr, E / Ttr);
        printf("  the disc's centre: %.3g cd/m2 (half float's largest is 65,504)\n", g_sun_peak);
        free(d);
    }
}

/* =========================================================================================
 * 3. Tonemap
 * ========================================================================================= */
static void neutral_gpu(const float (*c)[4], float (*o)[4], int n)
{
    id<MTLBuffer> bc = B(c, (size_t)n * 16), bo = B(NULL, (size_t)n * 16);
    kern(@"t_neutral", (NSUInteger)n, ^(id<MTLComputeCommandEncoder> e) {
        [e setBuffer:bc offset:0 atIndex:0];
        [e setBuffer:bo offset:0 atIndex:1];
    });
    memcpy(o, bo.contents, (size_t)n * 16);
    nf_scan((const float *)bo.contents, ((size_t)n * 16) / 4);
}

/* the post pass (pfs) over a small HDR image: its output pixels, BGRA */
static void post_run(id<MTLTexture> hdr, id<MTLTexture> bl, id<MTLTexture> alb, const LU *u, id<MTLBuffer> hist, uint8_t *out)
{
    id<MTLTexture> t = T2(MTLPixelFormatBGRA8Unorm, (int)hdr.width, (int)hdr.height);
    fullscreen(s_lpp, t, MTLLoadActionDontCare, ^(id<MTLRenderCommandEncoder> e) {
        [e setFragmentBytes:u length:sizeof *u atIndex:0];
        [e setFragmentTexture:hdr atIndex:0];
        [e setFragmentTexture:bl atIndex:1];
        [e setFragmentTexture:alb atIndex:2];
        [e setFragmentBuffer:hist offset:0 atIndex:1];
    });
    if (out)
        [t getBytes:out bytesPerRow:t.width * 4 fromRegion:MTLRegionMake2D(0, 0, t.width, t.height) mipmapLevel:0];
}
static id<MTLTexture> alb_lit(int w, int h)
{
    id<MTLTexture> t = T2(MTLPixelFormatRGBA8Unorm, w, h);
    uint8_t *p = malloc((size_t)w * h * 4);
    memset(p, 255, (size_t)w * h * 4);
    [t replaceRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0 withBytes:p bytesPerRow:(NSUInteger)w * 4];
    free(p);
    return t;
}

static void test_tonemap(void)
{
    enum { NG = 100000, NC = 100000 };
    float (*c)[4] = malloc(sizeof(float) * 4 * (NG + NC)), (*o)[4] = malloc(sizeof(float) * 4 * (NG + NC));
    double worst = 0, wv[3] = { 0, 0, 0 };
    int i, k, mono = 1, monol = 1;
    section("3. Tonemap (neutral: Khronos PBR Neutral)");
    for (i = 0; i < NG; i++) {
        double x = i == 0 ? 0.0 : pow(10.0, -4.0 + 7.0 * i / (NG - 1.0));
        c[i][0] = c[i][1] = c[i][2] = (float)x, c[i][3] = 0;
    }
    for (i = 0; i < NC; i++) {
        double s = pow(10.0, -3.0 + 6.0 * urand());
        for (k = 0; k < 3; k++)
            c[NG + i][k] = (float)(s * urand());
        c[NG + i][3] = 0;
    }
    neutral_gpu(c, o, NG + NC);
    for (i = 0; i < NG + NC; i++) {
        double in[3] = { c[i][0], c[i][1], c[i][2] }, r[3];
        ref_neutral(in, r);
        for (k = 0; k < 3; k++)
            if (fabs(o[i][k] - r[k]) > worst)
                worst = fabs(o[i][k] - r[k]), memcpy(wv, in, sizeof wv);
    }
    for (i = 1; i < NG; i++)
        if (o[i][0] < o[i - 1][0])
            mono = 0;
    {   /* luminance along a scaled colour */
        enum { NS = 2000 };
        float (*s)[4] = malloc(sizeof(float) * 4 * 64 * NS), (*so)[4] = malloc(sizeof(float) * 4 * 64 * NS);
        int j;
        for (j = 0; j < 64; j++) {
            double b[3] = { urand(), urand(), urand() };
            for (i = 0; i < NS; i++)
                for (k = 0; k < 4; k++)
                    s[j * NS + i][k] = k < 3 ? (float)(b[k] * pow(10.0, -3.0 + 7.0 * i / (NS - 1.0))) : 0.0f;
        }
        neutral_gpu(s, so, 64 * NS);
        for (j = 0; j < 64; j++)
            for (i = 1; i < NS; i++) {
                double l0 = 0.2126 * so[j * NS + i - 1][0] + 0.7152 * so[j * NS + i - 1][1] + 0.0722 * so[j * NS + i - 1][2];
                double l1 = 0.2126 * so[j * NS + i][0] + 0.7152 * so[j * NS + i][1] + 0.0722 * so[j * NS + i][2];
                if (l1 < l0 - 1e-6)
                    monol = 0;
            }
        free(s), free(so);
    }
    report(worst < 1e-5, "PBR Neutral vs Khronos reference (double)", "max |GPU - ref| %.2e over %d greys 1e-4..1e3 and %d colours (worst at %.3g %.3g %.3g)",
           worst, NG, NC, wv[0], wv[1], wv[2]);
    report(o[0][0] == 0.0f && o[0][1] == 0.0f && o[0][2] == 0.0f, "PBR Neutral maps 0 to 0", "neutral(0) = %g %g %g", o[0][0], o[0][1], o[0][2]);
    report(mono, "PBR Neutral monotonic, grey ramp", "%d steps 1e-4..1e3, top %.6f", NG, o[NG - 1][0]);
    report(monol, "PBR Neutral monotonic, scaled colours", "luminance non-decreasing along 64 random hues x 2,000 scales");
    {   /* the post pass with an HDR texel past half float's range (the sun's disc) */
        id<MTLTexture> hdr = T2(MTLPixelFormatRGBA16Float, 4, 4), bl = T2(MTLPixelFormatRGBA16Float, 4, 4), alb = alb_lit(4, 4);
        double px[64], z[64];
        uint8_t out[64];
        LU u;
        id<MTLBuffer> hist = B(NULL, LUM_BINS * 4);
        for (i = 0; i < 16; i++) {
            double v = i == 5 ? INFINITY : i == 6 ? 70000.0 : g_sun_peak > 0 ? fmin(g_sun_peak, 60000.0) : 60000.0;
            px[i * 4] = px[i * 4 + 1] = px[i * 4 + 2] = v, px[i * 4 + 3] = 1;
            z[i * 4] = z[i * 4 + 1] = z[i * 4 + 2] = z[i * 4 + 3] = 0;
        }
        write16(hdr, px);
        write16(bl, z);
        memset(&u, 0, sizeof u);
        u.misc[0] = 1.0f / (1.2f * exp2f(15.0f));
        u.bg[1] = 1.0f;
        post_run(hdr, bl, alb, &u, hist, out);
        report(out[5 * 4] >= out[0] && out[5 * 4] > 0 && out[5 * 4] == out[5 * 4 + 1] && out[5 * 4] == out[5 * 4 + 2],
               "post: an overflowed HDR texel shows bright",
               "texel stored as %s: output BGRA %d %d %d (a finite 60,000 shows %d %d %d)", isinf(h2d(d2h(70000.0))) ? "inf" : "finite",
               out[5 * 4], out[5 * 4 + 1], out[5 * 4 + 2], out[0], out[1], out[2]);
    }
    free(c), free(o);
}

/* =========================================================================================
 * 4. Exposure
 * ========================================================================================= */
/* one synthetic scene through the post pass and lit_exposure: the metered
 * EV100 against the band's exact mean log luminance */
static double expose_one(const double *Y, int w, int h, int weather, double *want)
{
    id<MTLTexture> hdr = T2(MTLPixelFormatRGBA16Float, w, h), alb = alb_lit(w, h);
    double *px = malloc(sizeof(double) * 4 * w * h), *s = malloc(sizeof(double) * w * h), ev, m = 0;
    int i, x, y, ns = 0, lo, hi;
    LU u;
    float ex;
    for (i = 0; i < w * h; i++) {
        px[i * 4] = px[i * 4 + 1] = px[i * 4 + 2] = Y[i];
        px[i * 4 + 3] = 1;
    }
    write16(hdr, px);
    for (y = 0; y < h; y += 4)            /* the pixels the meter reads: every 4th in x and y */
        for (x = 0; x < w; x += 4) {
            double v = h2d(d2h(Y[y * w + x]));
            double yl = 0.2126 * v + 0.7152 * v + 0.0722 * v;
            if (yl > 0)
                s[ns++] = yl;
        }
    for (i = 1; i < ns; i++) {            /* sort */
        double t = s[i];
        int j = i - 1;
        while (j >= 0 && s[j] > t)
            s[j + 1] = s[j], j--;
        s[j + 1] = t;
    }
    lo = (int)(ns * k_weather[weather].plo), hi = (int)(ns * k_weather[weather].phi);   /* the weather's band and compensation */
    for (i = lo; i < hi; i++)
        m += log2(s[i]);
    m /= (hi - lo);
    *want = fmin(fmax(m + log2(100.0 / 12.5) - k_weather[weather].comp, k_weather[weather].evlo), k_weather[weather].evhi);
    memset(&u, 0, sizeof u);
    u.misc[0] = 1.0f;
    u.bg[1] = 1.0f;                       /* the bloom texture is the image itself: mix(c, c, 0.04) = c */
    memset([s_hist[0] contents], 0, LUM_BINS * 4);
    s_vbi = 0;
    post_run(hdr, hdr, alb, &u, s_hist[0], NULL);
    s_ev = 1e9f;                          /* no adaptation: the target itself */
    ex = lit_exposure(weather);
    ev = -log2(1.2 * ex);
    free(px), free(s);
    return ev;
}

static void test_exposure(void)
{
    enum { W = 512, H = 512 };
    double *Y = malloc(sizeof(double) * W * H), want, got, worst = 0, dworst = 0;
    int i, d;
    section("4. Exposure (histogram 80 to 98.3 percentile, EV100 with K 12.5)");
    if (!s_hist[0])
        for (i = 0; i < 3; i++)
            s_hist[i] = [s_dev newBufferWithLength:LUM_BINS * 4 options:MTLResourceStorageModeShared];
    {
        struct { const char *name; int weather; int kind; double a, b, c, d; } sc[] = {
            { "log-uniform 2^4..2^13 cd/m2, sunny", 0, 0, 4, 13, 0, 0 },
            { "log-normal mu 2^9, sigma 1.5 stops, sunny", 0, 1, 9, 1.5, 0, 0 },
            { "sky 60% (2^12) and road 40% (2^8), sunny", 0, 2, 12, 0.3, 8, 1.0 },
            { "log-normal mu 2^-2, sigma 2 stops, night", 4, 1, -2, 2, 0, 0 },
            { "log-uniform 2^-6..2^3, night", 4, 0, -6, 3, 0, 0 },
            { "log-normal mu 2^8, sigma 1 stop, fog", 1, 1, 8, 1, 0, 0 },
        };
        for (d = 0; d < (int)(sizeof sc / sizeof sc[0]); d++) {
            for (i = 0; i < W * H; i++) {
                double l2;
                switch (sc[d].kind) {
                case 0: l2 = sc[d].a + (sc[d].b - sc[d].a) * urand(); break;
                case 1: l2 = sc[d].a + sc[d].b * nrand(); break;
                default: l2 = urand() < 0.6 ? sc[d].a + sc[d].b * nrand() : sc[d].c + sc[d].d * nrand(); break;
                }
                Y[i] = exp2(l2);
            }
            got = expose_one(Y, W, H, sc[d].weather, &want);
            worst = WMAX(worst, fabs(got - want));
            report(fabs(got - want) < 0.05, sc[d].name, "EV100 metered %.3f, exact %.3f, error %+.3f", got, want, got - want);
        }
    }
    {   /* a uniform scene: every pixel one luminance, swept over a stop */
        double wl = 0;
        for (d = 0; d <= 40; d++) {
            double l2 = 9.0 + d / 40.0;
            for (i = 0; i < W * H; i++)
                Y[i] = exp2(l2);
            got = expose_one(Y, W, H, 0, &want);
            if (fabs(got - want) > dworst)
                dworst = fabs(got - want), wl = l2;
        }
        report(dworst < 0.05, "uniform scene, 41 luminances over a stop", "max error %.3f EV (at log2 Y %.3f): the bins' width is %.3f EV",
               dworst, wl, 32.0 / LUM_BINS);
    }
    {   /* the meter keeps to the weather's range */
        for (i = 0; i < W * H; i++)
            Y[i] = exp2(15.0);
        got = expose_one(Y, W, H, 0, &want);
        report(fabs(got - 16.0) < 1e-4, "clamp to the weather's EV range", "a 2^15 cd/m2 scene, sunny: EV %.3f (range 12..16)", got);
        for (i = 0; i < W * H; i++)
            Y[i] = INFINITY;
        got = expose_one(Y, W, H, 0, &want);
        report(fabs(got - 16.0) < 1e-4, "an overflowed (inf) scene is metered", "every texel past half float, sunny: EV %.3f (range 12..16)", got);
    }
    {   /* the EV100 formula: a scene metered at average L shows it at 12.5/(100 x 1.2) */
        for (i = 0; i < W * H; i++)
            Y[i] = 2000.0;
        got = expose_one(Y, W, H, 0, &want);
        printf("  a uniform 2,000 cd/m2 scene: EV100 %.3f (log2(2000 x 100 / 12.5) = %.3f), shown at %.4f before the tonemap\n", got,
               log2(2000.0 * 8.0), 2000.0 / (1.2 * exp2(got)));
    }
    free(Y);
}

/* =========================================================================================
 * 5. Lamps
 * ========================================================================================= */
static void lamp_dirs(const double (*hv)[2], int n, float *out)
{
    /* the lamp built as light_scene.c builds a headlamp: axis the car's
       forward, side its right; up = side x axis.  Car frame: x forward, z up,
       right = -y */
    LightSpot l;
    float (*d)[4] = malloc(sizeof(float) * 4 * (size_t)n);
    id<MTLBuffer> bd, bo;
    int i;
    memset(&l, 0, sizeof l);
    l.kind = LIGHT_HEAD;
    l.axis[0] = 1;
    l.side[1] = -1;
    l.candela = 20000.0f;
    l.range = 150.0f;
    l.cosout = 0.3420201f;           /* the outer cone, 70 degrees, as light_scene.c sets it */
    for (i = 0; i < n; i++) {        /* a point on a screen 25 m ahead: h right, v up, degrees */
        double x = 25.0, y = -25.0 * tan(hv[i][0] * R_PI / 180.0), z = 25.0 * tan(hv[i][1] * R_PI / 180.0), ln = sqrt(x * x + y * y + z * z);
        d[i][0] = (float)(x / ln), d[i][1] = (float)(y / ln), d[i][2] = (float)(z / ln), d[i][3] = 0;
    }
    bd = B(d, sizeof(float) * 4 * (size_t)n);
    bo = B(NULL, sizeof(float) * (size_t)n);
    kern(@"t_lamp", (NSUInteger)n, ^(id<MTLComputeCommandEncoder> e) {
        [e setBytes:&l length:sizeof l atIndex:0];
        [e setBuffer:bd offset:0 atIndex:1];
        [e setBuffer:bo offset:0 atIndex:2];
    });
    memcpy(out, bo.contents, sizeof(float) * (size_t)n);
    nf_scan((const float *)bo.contents, (sizeof(float) * (size_t)n) / 4);
    free(d);
}
static int in_poly(double x, double y, const double (*p)[2], int n)
{
    int i, j, c = 0;
    for (i = 0, j = n - 1; i < n; j = i++)
        if (((p[i][1] > y) != (p[j][1] > y)) && (x < (p[j][0] - p[i][0]) * (y - p[i][1]) / (p[j][1] - p[i][1]) + p[i][0]))
            c = !c;
    return c;
}

static void test_lamp(void)
{
    /* UN ECE R112 (class B passing beam, right-hand traffic), Annex 3 /
       paragraph 6.2.4's table: test points in degrees (h: + right, v: + up),
       on the regulation's nominal aim (the cut-off's horizontal part 0.57
       degrees below H-H, paragraph 6.2.2.1); the lamp on the car is aimed
       along its forward axis, so the points are in the car's frame */
    static const struct { const char *name; double h, v, lim; int max; } pt[] = {
        { "B50L  0.57U 3.43L", -3.43, 0.57, 350, 1 },
        { "BR    1.0U  2.5R", 2.5, 1.0, 1750, 1 },
        { "75R   0.57D 1.15R", 1.15, -0.57, 10100, 0 },
        { "75L   0.57D 3.43L", -3.43, -0.57, 10600, 1 },
        { "50L   0.86D 3.43L", -3.43, -0.86, 13200, 1 },
        { "50R   0.86D 1.72R", 1.72, -0.86, 10100, 0 },
        { "50V   0.86D 0", 0.0, -0.86, 5100, 0 },
        { "25L   1.72D 9L", -9.0, -1.72, 1700, 0 },
        { "25R   1.72D 9R", 9.0, -1.72, 1700, 0 },
    };
    enum { NP = sizeof pt / sizeof pt[0] };
    static const double z3[7][2] = { { -8, 1 }, { -8, 4 }, { 8, 4 }, { 8, 2 }, { 6, 1.5 }, { 1.5, 1.5 }, { 0, 0 } };
    double (*hv)[2];
    float *o;
    int i, n = 0, j;
    section("5. Headlamp low beam (lamp: UN ECE R112 class B, 20,000 cd)");
    hv = malloc(sizeof(double) * 2 * 400000);
    o = malloc(sizeof(float) * 400000);
    for (i = 0; i < NP; i++)
        hv[n][0] = pt[i].h, hv[n][1] = pt[i].v, n++;
    lamp_dirs(hv, n, o);
    for (i = 0; i < NP; i++) {
        char nm[64];
        int ok = pt[i].max ? o[i] <= pt[i].lim : o[i] >= pt[i].lim;
        snprintf(nm, sizeof nm, "R112 %s", pt[i].name);
        report(ok, nm, "%7.0f cd (%s %.0f)", o[i], pt[i].max ? "max" : "min", pt[i].lim);
    }
    {   /* the zones on a 0.05 degree grid */
        double z3max = 0, z4min = 1e30, z1max = 0, i50r = o[5], zh = 0, zv = 0;
        int nz;
        n = 0;
        for (i = -160; i <= 160; i++)
            for (j = 0; j <= 80; j++) {
                double h = i * 0.05, v = j * 0.05;
                if (in_poly(h, v, z3, 7))
                    hv[n][0] = h, hv[n][1] = v, n++;
            }
        nz = n;
        lamp_dirs(hv, n, o);
        for (i = 0; i < nz; i++)
            if (o[i] > z3max)
                z3max = o[i], zh = hv[i][0], zv = hv[i][1];
        report(z3max <= 625, "R112 zone III (above the cut-off)", "max %.0f cd at %.2f, %.2f (max 625)", z3max, zh, zv);
        n = 0;
        for (i = -103; i <= 103; i++)
            for (j = 0; j <= 18; j++)
                hv[n][0] = i * 0.05, hv[n][1] = -0.86 - j * 0.0478, n++;
        lamp_dirs(hv, n, o);
        for (i = 0; i < n; i++)
            if (o[i] < z4min)
                z4min = o[i], zh = hv[i][0], zv = hv[i][1];
        report(z4min >= 2500, "R112 zone IV (0.86D-1.72D, 5.15L-5.15R)", "min %.0f cd at %.2f, %.2f (min 2,500)", z4min, zh, zv);
        n = 0;
        for (i = -180; i <= 180; i++)
            for (j = 0; j <= 46; j++)
                hv[n][0] = i * 0.05, hv[n][1] = -1.72 - j * 0.05, n++;
        lamp_dirs(hv, n, o);
        for (i = 0; i < n; i++)
            z1max = WMAX(z1max, o[i]);
        report(z1max <= 2.0 * i50r, "R112 zone I (1.72D-4D, 9L-9R)", "max %.0f cd (max 2 x I(50R) = %.0f)", z1max, 2.0 * i50r);
    }
    {   /* the cut-off line: where the vertical log gradient is steepest (R112's own definition), by column */
        static const double cols[] = { -6.0, -4.0, -2.5, 1.0, 2.0 };
        double worst = 0;
        printf("  cut-off by column (steepest d log I / dv): h  found  expected\n");
        for (j = 0; j < 5; j++) {
            double best = 0, bv = 0, exp_v = cols[j] < 0 ? -0.57 : -0.57 + cols[j] * tan(15.0 * R_PI / 180.0);
            n = 0;
            for (i = 0; i <= 600; i++)
                hv[n][0] = cols[j], hv[n][1] = -3.0 + i * 0.01, n++;
            lamp_dirs(hv, n, o);
            for (i = 0; i < n - 1; i++) {
                double g = log10(fmax(o[i], 1e-6)) - log10(fmax(o[i + 1], 1e-6));
                if (g > best)
                    best = g, bv = hv[i][1] + 0.005;
            }
            printf("    %5.2f  %6.3f  %6.3f\n", cols[j], bv, exp_v);
            worst = WMAX(worst, fabs(bv - exp_v));
        }
        report(worst <= 0.1, "cut-off line position", "max |found - R112 nominal| %.3f degrees (horizontal part 0.57D, 15 degree rise right of V-V)", worst);
    }
    {   /* the cut-off's sharpness, R112's G = log E(v) - log E(v + 0.1 degree), steepest by column,
           within 0.13 (sharp enough to aim) and 0.3 (the lens's blur, no hard line) */
        static const double cols[] = { -6.0, -3.43, -1.0, 0.5, 1.15, 2.0, 4.0 };
        double gmin = 1e9, gmax = 0;
        for (j = 0; j < 7; j++) {
            double best = 0;
            n = 0;
            for (i = 0; i <= 500; i++)
                hv[n][0] = cols[j], hv[n][1] = -3.0 + i * 0.01, n++;
            lamp_dirs(hv, n, o);
            for (i = 0; i + 10 < n; i++)
                best = WMAX(best, log10(fmax(o[i], 1e-6)) - log10(fmax(o[i + 10], 1e-6)));
            gmin = fmin(gmin, best), gmax = WMAX(gmax, best);
        }
        report(gmin >= 0.13 && gmax <= 0.3, "cut-off sharpness G per 0.1 degree", "%.3f..%.3f (0.13..0.3)", gmin, gmax);
    }
    {   /* no discontinuity: across the whole pattern (80L..80R, 10D..5U) on a 0.05 degree grid,
           each step between neighbours against the step before it along the same line: a jump
           shows as a step its neighbours do not share, while the cut-off's own gradient (R112's
           G of at least 0.13 a 0.1 degree, over 14% a 0.05 degree step) is shared by the steps
           around it; under 4% (1 cd added to each: 0.1 lux at 3 m, where the pattern tapers to
           zero at the outer cone): the cut-off's blur, a Gaussian of 0.28 degrees in log intensity
           over its 1.92 decades, bends the steps by 3.4% at most (1.92 ln 10 max|phi'| (0.05 /
           0.28)^2), a smoothstep's ends or a corner far more (8.8% at the old elbow) */
        double worst = 0, wh = 0, wv = 0;
        int nh = 3201, nv = 301, a, b, dir;
        float *oo = malloc(sizeof(float) * (size_t)nh * nv);
        double (*hh)[2] = malloc(sizeof(double) * 2 * (size_t)nh * nv);
        n = 0;
        for (a = 0; a < nh; a++)
            for (b = 0; b < nv; b++)
                hh[n][0] = -80.0 + a * 0.05, hh[n][1] = -10.0 + b * 0.05, n++;
        lamp_dirs(hh, n, oo);
        for (dir = 0; dir < 2; dir++)
            for (a = dir == 0; a + (dir == 0) < nh; a++)
                for (b = dir == 1; b + (dir == 1) < nv; b++) {
                    int s0 = dir == 0 ? (a - 1) * nv + b : a * nv + b - 1, s1 = a * nv + b, s2 = dir == 0 ? (a + 1) * nv + b : a * nv + b + 1;
                    double f0 = oo[s0] + 1.0, f1 = oo[s1] + 1.0, f2 = oo[s2] + 1.0;
                    double r = fabs(f2 / f1 - f1 / f0) / fmax(f1 / f0, 1.0);
                    if (r > worst)
                        worst = r, wh = hh[s1][0], wv = hh[s1][1];
                }
        report(worst < 0.04, "pattern continuity (0.05 degree steps)", "max jump %.2f%% at %.2f, %.2f (max 4%%)", worst * 100.0, wh, wv);
        free(oo);
        free(hh);
    }
    {   /* the flux the pattern carries */
        double flux = 0, step = 0.25;
        n = 0;
        for (i = 0; i < 720; i++)
            for (j = 0; j < 360; j++)
                hv[n][0] = -90.0 + (i + 0.5) * step, hv[n][1] = -45.0 + (j + 0.5) * step, n++;
        lamp_dirs(hv, n, o);
        for (i = 0; i < n; i++) {
            double h = hv[i][0] * R_PI / 180.0, v = hv[i][1] * R_PI / 180.0, s = step * R_PI / 180.0;
            /* solid angle of a screen-angle cell: d(tan h) d(tan v) / (1 + tan^2 h + tan^2 v)^1.5 */
            double th = tan(h), tv = tan(v);
            flux += o[i] * (s / (cos(h) * cos(h))) * (s / (cos(v) * cos(v))) / pow(1.0 + th * th + tv * tv, 1.5);
        }
        printf("  luminous flux of one lamp's pattern (+-90 h, +-45 v): %.0f lm\n", flux);
    }
    {   /* a rear lamp: a broad lobe, cd cos^1.5 */
        LightSpot l;
        float d[3][4] = { { 1, 0, 0, 0 }, { 0.5f, 0.8660254f, 0, 0 }, { -1, 0, 0, 0 } }, ro[3];
        id<MTLBuffer> bd = B(d, sizeof d), bo = B(NULL, sizeof ro);
        memset(&l, 0, sizeof l);
        l.kind = LIGHT_REAR, l.axis[0] = 1, l.side[1] = -1, l.candela = 80.0f, l.range = 25.0f, l.cosout = 0.0f;   /* as light_scene.c */
        kern(@"t_lamp", 3, ^(id<MTLComputeCommandEncoder> e) {
            [e setBytes:&l length:sizeof l atIndex:0];
            [e setBuffer:bd offset:0 atIndex:1];
            [e setBuffer:bo offset:0 atIndex:2];
        });
        memcpy(ro, bo.contents, sizeof ro);
        nf_scan((const float *)bo.contents, (sizeof ro) / 4);
        report(fabsf(ro[0] - 80.0f) < 1e-3f && fabs(ro[1] - 80.0 * pow(0.5, 1.5)) < 1e-3 && ro[2] == 0.0f, "rear lamp lobe",
               "on axis %.3f cd, 60 degrees %.3f (80 x 0.5^1.5 = %.3f), behind %.1f", ro[0], ro[1], 80.0 * pow(0.5, 1.5), ro[2]);
    }
    free(hv), free(o);
}

/* =========================================================================================
 * 6. Bloom
 * ========================================================================================= */
/* the renderer's bloom chain (lit_present's pass order): 13-tap downs
   into the half-size levels, then tent ups added onto the level above */
static id<MTLRenderPipelineState> g_bd32, g_bu32;    /* the same functions into RGBA32F */
static int bloom_run32(id<MTLTexture> src, __strong id<MTLTexture> *bl, int f32)
{
    int n = 0, w = (int)src.width / 2, h = (int)src.height / 2, k;
    for (; n < 6 && w >= 4 && h >= 4; n++, w /= 2, h /= 2)
        bl[n] = T2(f32 ? MTLPixelFormatRGBA32Float : MTLPixelFormatRGBA16Float, w, h);
    for (k = 0; k < n; k++) {
        id<MTLTexture> in = k ? bl[k - 1] : src;
        fullscreen(f32 ? g_bd32 : s_lbd, bl[k], MTLLoadActionDontCare, ^(id<MTLRenderCommandEncoder> e) { [e setFragmentTexture:in atIndex:0]; });
    }
    for (k = n - 1; k > 0; k--) {
        id<MTLTexture> in = bl[k];
        fullscreen(f32 ? g_bu32 : s_lbua, bl[k - 1], MTLLoadActionLoad, ^(id<MTLRenderCommandEncoder> e) { [e setFragmentTexture:in atIndex:0]; });
    }
    return n;
}
static int bloom_run(id<MTLTexture> src, __strong id<MTLTexture> *bl) { return bloom_run32(src, bl, 0); }
static double tsum(id<MTLTexture> t, int ch)
{
    double s = 0;
    size_t i;
    if (t.pixelFormat == MTLPixelFormatRGBA32Float) {
        float *f = malloc(t.width * t.height * 16);
        [t getBytes:f bytesPerRow:t.width * 16 fromRegion:MTLRegionMake2D(0, 0, t.width, t.height) mipmapLevel:0];
        for (i = 0; i < t.width * t.height; i++)
            s += f[i * 4 + ch];
        free(f);
        return s;
    }
    {
        double *d = read16(t);
        for (i = 0; i < t.width * t.height; i++)
            s += d[i * 4 + ch];
        free(d);
    }
    return s;
}

static void test_bloom(void)
{
    enum { W = 512, H = 256 };
    id<MTLTexture> src = T2(MTLPixelFormatRGBA16Float, W, H), bl[6];
    double *px = calloc((size_t)W * H * 4, sizeof(double)), worst = 0, chain[4], lvl[4][6];
    int n, i, p;
    section("6. Bloom (bdn 13-tap down, bup tent up, Jimenez 2014)");
    for (i = 0; i < W * H; i++)
        px[i * 4] = px[i * 4 + 1] = px[i * 4 + 2] = 1.5, px[i * 4 + 3] = 1;
    write16(src, px);
    n = bloom_run(src, bl);
    {
        double *d = read16(bl[0]);
        for (i = 0; i < (int)(bl[0].width * bl[0].height); i++)
            worst = WMAX(worst, fabs(d[i * 4] / n - 1.5));
        free(d);
    }
    report(worst < 2e-3, "constant image: output = input", "%d levels; max |bl0 / levels - 1.5| = %.2e", n, worst);
    for (p = 0; p < 4; p++) {   /* a delta at each of the four 2x2 phases, mid-frame */
        int x = 256 + (p & 1), y = 128 + (p >> 1), k;
        memset(px, 0, sizeof(double) * W * H * 4);
        px[(y * W + x) * 4] = 1000.0;
        write16(src, px);
        for (k = 0; k < 6; k++)
            bl[k] = nil;
        n = bloom_run(src, bl);
        /* each level's energy before the ups: run the downs alone again */
        {
            id<MTLTexture> dn[6];
            int w2 = W / 2, h2 = H / 2;
            for (k = 0; k < n; k++, w2 /= 2, h2 /= 2) {
                id<MTLTexture> in = k ? dn[k - 1] : src;
                dn[k] = T2(MTLPixelFormatRGBA16Float, w2, h2);
                fullscreen(s_lbd, dn[k], MTLLoadActionDontCare, ^(id<MTLRenderCommandEncoder> e) { [e setFragmentTexture:in atIndex:0]; });
                lvl[p][k] = tsum(dn[k], 0) * pow(4.0, k + 1) / 1000.0;
            }
        }
        chain[p] = tsum(bl[0], 0) * 4.0 / n / 1000.0;
    }
    printf("  delta impulse, energy ratio out/in per down level (phases x,y even/odd):\n");
    for (p = 0; p < 4; p++) {
        int k;
        printf("    phase %d,%d:", p & 1, p >> 1);
        for (i = 0; i < n; i++)
            printf(" %.4f", lvl[p][i]);
        printf(" | chain %.4f\n", chain[p]);
        (void)k;
    }
    {
        double avg = (chain[0] + chain[1] + chain[2] + chain[3]) / 4.0, lv = 0;
        for (i = 0; i < n; i++)
            lv = WMAX(lv, fabs((lvl[0][i] + lvl[1][i] + lvl[2][i] + lvl[3][i]) / 4.0 - 1.0));
        report(fabs(avg - 1.0) < 1e-2 && lv < 5e-3, "delta impulse: energy, half float targets", "chain out/in %.4f (mean of the 4 phases), worst level %.2e off 1",
               avg, lv);
    }
    {   /* the same chain and filters into RGBA32F: the filters' own energy */
        static const MTLPixelFormat f32 = MTLPixelFormatRGBA32Float;
        static const int sz[2][2] = { { W, H }, { 2048, 1024 } };
        double ch[2][4];
        int z;
        g_bd32 = lit_pipe(@"lvs", @"bdn", &f32, 1, 0, -1, -1);
        g_bu32 = lit_pipe(@"lvs", @"bup", &f32, 1, 0, 99, 99);
        for (z = 0; z < 2; z++) {
            int w = sz[z][0], h = sz[z][1];
            id<MTLTexture> s32 = T2(MTLPixelFormatRGBA32Float, w, h), b32[6];
            for (p = 0; p < 4; p++) {
                float *f = calloc((size_t)w * h * 4, sizeof(float));
                f[((h / 2 + (p >> 1)) * w + w / 2 + (p & 1)) * 4] = 1000.0f;
                [s32 replaceRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0 withBytes:f bytesPerRow:(NSUInteger)w * 16];
                free(f);
                n = bloom_run32(s32, b32, 1);
                ch[z][p] = tsum(b32[0], 0) * 4.0 / n / 1000.0;
            }
        }
        printf("  float targets, delta mid-frame: chain out/in %.6f at %dx%d (6 levels to %dx%d), %.6f at 2048x1024 (to 32x16)\n",
               (ch[0][0] + ch[0][1] + ch[0][2] + ch[0][3]) / 4.0, W, H, W >> 6, H >> 6, (ch[1][0] + ch[1][1] + ch[1][2] + ch[1][3]) / 4.0);
        report(fabs((ch[1][0] + ch[1][1] + ch[1][2] + ch[1][3]) / 4.0 - 1.0) < 1e-4, "delta impulse: energy, float targets",
               "chain out/in %.6f away from the edges (phases %.6f %.6f %.6f %.6f)", (ch[1][0] + ch[1][1] + ch[1][2] + ch[1][3]) / 4.0,
               ch[1][0], ch[1][1], ch[1][2], ch[1][3]);
    }
    {   /* non-finite input: the down pass's guard */
        id<MTLTexture> t = T2(MTLPixelFormatRGBA16Float, 16, 16), o = T2(MTLPixelFormatRGBA16Float, 8, 8);
        double q[16 * 16 * 4], *r;
        int bad = 0;
        for (i = 0; i < 16 * 16 * 4; i++)
            q[i] = 1.0;
        q[(5 * 16 + 5) * 4] = NAN;
        q[(9 * 16 + 9) * 4 + 1] = INFINITY;
        write16(t, q);
        fullscreen(s_lbd, o, MTLLoadActionDontCare, ^(id<MTLRenderCommandEncoder> e) { [e setFragmentTexture:t atIndex:0]; });
        r = read16(o);
        for (i = 0; i < 64 * 4; i++)
            bad += !isfinite(r[i]);
        report(bad == 0, "down pass: NaN and inf input give finite output", "%d non-finite of 256 values", bad);
        free(r);
    }
    free(px);
}

/* =========================================================================================
 * 7. De-lighting
 * ========================================================================================= */
static void delit_gpu(const float (*in)[4], float (*o)[4], int n)
{
    id<MTLBuffer> bi = B(in, (size_t)n * 32), bo = B(NULL, (size_t)n * 16);
    kern(@"t_delit", (NSUInteger)n, ^(id<MTLComputeCommandEncoder> e) {
        [e setBuffer:bi offset:0 atIndex:0];
        [e setBuffer:bo offset:0 atIndex:1];
    });
    memcpy(o, bo.contents, (size_t)n * 16);
    nf_scan((const float *)bo.contents, ((size_t)n * 16) / 4);
}

static void test_delit(void)
{
    enum { N = 20000, NK = 5 };
    static const double ks[NK] = { 1.0, 0.6, 0.35, 0.2, 0.12 };
    float (*in)[4] = malloc(sizeof(float) * 8 * N * NK), (*o)[4] = malloc(sizeof(float) * 4 * N * NK);
    double worst = 0, gworst = 0, gratio = 0;
    int i, k, c;
    section("7. De-lighting (delit: the vertex colour's brightness is baked light)");
    /* the game's colour: texture x tint x baked (a grey), the vertex colour = tint x baked */
    for (i = 0; i < N; i++) {
        double tex[3], tint[3];
        for (c = 0; c < 3; c++)
            tex[c] = urand(), tint[c] = 0.25 + 0.75 * urand();
        for (k = 0; k < NK; k++)
            for (c = 0; c < 4; c++) {
                in[2 * (k * N + i)][c] = c < 3 ? (float)tex[c] : 0;
                in[2 * (k * N + i) + 1][c] = c < 3 ? (float)(tint[c] * ks[k]) : 0;
            }
    }
    delit_gpu(in, o, N * NK);
    for (i = 0; i < N; i++)
        for (k = 1; k < NK; k++)
            for (c = 0; c < 3; c++)
                worst = WMAX(worst, fabs(o[k * N + i][c] - o[i][c]) / fmax(o[i][c], 1e-6));
    report(worst < 1e-5, "albedo independent of the baked light", "max relative change %.2e over baked x1..x0.12, %d colours", worst, N);
    /* a grey vertex colour (no tint): the albedo is the texture's linear colour times 0.85 */
    for (i = 0; i < N; i++) {
        double g = 0.05 + 0.95 * urand();
        for (c = 0; c < 4; c++) {
            in[2 * i][c] = c < 3 ? (float)(0.6 * urand()) : 0;
            in[2 * i + 1][c] = c < 3 ? (float)g : 0;
        }
    }
    delit_gpu(in, o, N);
    for (i = 0; i < N; i++)
        for (c = 0; c < 3; c++) {
            double l = ref_lin(in[2 * i][c]), want = fmin(l * 0.85, 0.9);
            if (l > 1e-3) {
                gworst = WMAX(gworst, fabs(o[i][c] / want - 1.0));
                gratio = o[i][c] / l;
            }
        }
    report(gworst < 1e-3, "grey vertex colour: albedo = 0.85 x lin(texture)", "max relative error %.2e (albedo / lin(texture) = %.4f)", gworst, gratio);
    {   /* bounds */
        int bad = 0;
        for (i = 0; i < N * NK; i++)
            for (c = 0; c < 4; c++)
                in[i][c] = (float)(urand() * 1.2 - 0.1);
        delit_gpu(in, o, N * NK / 2);
        for (i = 0; i < N * NK / 2; i++)
            for (c = 0; c < 3; c++)
                bad += !(o[i][c] >= 0.0f && o[i][c] <= 0.9f);
        report(bad == 0, "albedo within 0..0.9", "%d out of range of %d, inputs -0.1..1.1", bad, 3 * N * NK / 2);
    }
    free(in), free(o);
}

/* =========================================================================================
 * 8. Closed forms and robustness
 * ========================================================================================= */
static void test_misc(void)
{
    section("8. sRGB, normals, finite output");
    {   /* sRGB */
        enum { N = 1000001 };
        float *x = malloc(N * 4);
        id<MTLBuffer> bx, bo, bx2, bo2;
        const float *o, *o2;
        double rt = 0, el = 0, es = 0;
        int i;
        for (i = 0; i < N; i++)
            x[i] = (float)((double)i / (N - 1));
        bx = B(x, N * 4);
        bo = B(NULL, N * 8);
        kern(@"t_srgb", N, ^(id<MTLComputeCommandEncoder> e) {
            [e setBuffer:bx offset:0 atIndex:0];
            [e setBuffer:bo offset:0 atIndex:1];
        });
        o = bo.contents;
        nf_scan(o, (size_t)N * 2);
        for (i = 0; i < N; i++) {
            el = WMAX(el, fabs(o[2 * i] - ref_lin(x[i])));
            es = WMAX(es, fabs(o[2 * i + 1] - ref_srgb(x[i])));
            x[i] = o[2 * i];
        }
        bx2 = B(x, N * 4);
        bo2 = B(NULL, N * 8);
        kern(@"t_srgb", N, ^(id<MTLComputeCommandEncoder> e) {
            [e setBuffer:bx2 offset:0 atIndex:0];
            [e setBuffer:bo2 offset:0 atIndex:1];
        });
        o2 = bo2.contents;
        nf_scan(o2, (size_t)N * 2);
        for (i = 0; i < N; i++)
            rt = WMAX(rt, fabs(o2[2 * i + 1] - (double)i / (N - 1)));
        report(el < 1e-5 && es < 1e-5, "lin and srgb1 vs IEC 61966-2-1", "max |lin - ref| %.2e, |srgb1 - ref| %.2e", el, es);
        report(rt < 1e-4, "sRGB round trip srgb1(lin(x)) = x", "max error %.2e over 0..1 (8-bit step 3.9e-3)", rt);
        free(x);
    }
    {   /* the face normal from screen derivatives (facen), through a fragment pass */
        MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
        NSError *err = nil;
        id<MTLRenderPipelineState> p;
        static const float want[3][3] = { { 0, 0, 1 }, { 0, 0, 1 }, { 0, -1, 0 } };
        float got[3][4];
        int mode, ok = 1;
        d.vertexFunction = [g_tl newFunctionWithName:@"lvs"];
        d.fragmentFunction = [g_tl newFunctionWithName:@"t_facen"];
        d.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA32Float;
        p = [s_dev newRenderPipelineStateWithDescriptor:d error:&err];
        for (mode = 0; mode < 3; mode++) {
            id<MTLTexture> t = T2(MTLPixelFormatRGBA32Float, 8, 8);
            int m = mode, k;
            float px[8 * 8 * 4];
            fullscreen(p, t, MTLLoadActionDontCare, ^(id<MTLRenderCommandEncoder> e) { [e setFragmentBytes:&m length:4 atIndex:0]; });
            [t getBytes:px bytesPerRow:8 * 16 fromRegion:MTLRegionMake2D(0, 0, 8, 8) mipmapLevel:0];
            memcpy(got[mode], &px[(3 * 8 + 3) * 4], 16);
            for (k = 0; k < 8 * 8 * 4; k++)
                if (!isfinite(px[k]))
                    ok = 0;
            for (k = 0; k < 3; k++)
                if (fabsf(got[mode][k] - want[mode][k]) > 1e-5f)
                    ok = 0;
        }
        report(ok, "facen: degenerate and planar faces", "constant position -> %.0f %.0f %.0f; xy plane -> %.0f %.0f %.0f; xz plane -> %.0f %.0f %.0f",
               got[0][0], got[0][1], got[0][2], got[1][0], got[1][1], got[1][2], got[2][0], got[2][1], got[2][2]);
    }
    {   /* brdf over random and degenerate inputs */
        enum { N = 200000 };
        TBrdf *in = calloc(N, sizeof *in);
        float *o = malloc(N * 16);
        int i, k, bad = 0, badc[6] = { 0 };
        for (i = 0; i < N; i++) {
            double n[3], v[3], l[3];
            int c = i % 6;
            udir(n), udir(v), udir(l);
            if (c == 1)          /* the light straight behind the view: l = -v */
                for (k = 0; k < 3; k++)
                    l[k] = -v[k];
            if (c == 2)          /* l = v */
                for (k = 0; k < 3; k++)
                    l[k] = v[k];
            if (c == 3)          /* a zero light vector (a lamp at the point) */
                l[0] = l[1] = l[2] = 0;
            for (k = 0; k < 3; k++)
                in[i].n[k] = (float)n[k], in[i].v[k] = (float)v[k], in[i].l[k] = (float)l[k];
            in[i].p[0] = c == 4 ? 0.0f : (float)urand();
            in[i].p[1] = c == 5 ? (i & 1 ? 0.0f : 1.0f) : (float)urand();
        }
        brdf_eval(in, o, N);
        for (i = 0; i < N; i++)
            if (!finite4(&o[i * 4]))
                bad++, badc[i % 6]++;
        report(bad == 0, "brdf finite, random and degenerate", "%d non-finite of %d (random %d, l = -v %d, l = v %d, l = 0 %d, albedo 0 %d, rough 0/1 %d)",
               bad, N, badc[0], badc[1], badc[2], badc[3], badc[4], badc[5]);
        free(in), free(o);
    }
    {   /* radiance (sun, cascades, sky, lamps, glow) and fog over random scenes */
        enum { N = 100000, NL = 6 };
        LU u;
        LightSet *set = calloc(1, sizeof *set);   /* every cluster lists all NL lamps; none shadowed */
        LightSpot *ls = set->l;
        struct { float p[4], n[4], alb[4], fog[4]; } *in = calloc(N, sizeof *in);
        float *o = malloc(N * 16);
        id<MTLTexture> sm, atl;
        id<MTLBuffer> bl, bi, bo;
        int i, k, bad = 0;
        {
            MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:16 height:16 mipmapped:NO];
            MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
            id<MTLCommandBuffer> cb = [g_q commandBuffer];
            td.textureType = MTLTextureType2DArray;
            td.arrayLength = CASCADES;
            td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
            td.storageMode = MTLStorageModePrivate;
            sm = [s_dev newTextureWithDescriptor:td];
            rp.depthAttachment.texture = sm;
            rp.depthAttachment.loadAction = MTLLoadActionClear;
            rp.depthAttachment.clearDepth = 0.5;
            rp.depthAttachment.storeAction = MTLStoreActionStore;
            rp.renderTargetArrayLength = CASCADES;
            [[cb renderCommandEncoderWithDescriptor:rp] endEncoding];
            [cb commit];
            [cb waitUntilCompleted];
        }
        {   /* the lamps' atlas, cleared to the far plane */
            MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:16 height:16 mipmapped:NO];
            MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
            id<MTLCommandBuffer> cb = [g_q commandBuffer];
            td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
            td.storageMode = MTLStorageModePrivate;
            atl = [s_dev newTextureWithDescriptor:td];
            rp.depthAttachment.texture = atl;
            rp.depthAttachment.loadAction = MTLLoadActionClear;
            rp.depthAttachment.clearDepth = 1.0;
            rp.depthAttachment.storeAction = MTLStoreActionStore;
            [[cb renderCommandEncoderWithDescriptor:rp] endEncoding];
            [cb commit];
            [cb waitUntilCompleted];
        }
        memset(&u, 0, sizeof u);
        for (k = 0; k < CASCADES; k++) {
            int j;
            for (j = 0; j < 4; j++)
                u.sunvp[k][j * 5] = 0.01f / (k + 1);
            u.csplit[k] = 20.0f * (k + 1);
            u.ctexel[k] = 0.05f;
            u.cbias[k] = 0.001f;
            u.cdepth[k] = 100.0f;
        }
        u.csoft[0] = 0.00467f, u.csoft[1] = 16.0f, u.csoft[2] = 16.0f, u.csoft[3] = 0.0f;   /* the sun's radius; no contact march (needs a G-buffer) */
        u.cfwd[0] = 1;
        u.eye[0] = 0, u.eye[1] = 0, u.eye[2] = 2, u.eye[3] = NL;
        u.sun[0] = 0.3f, u.sun[1] = 0.2f, u.sun[2] = 0.93f;
        for (k = 0; k < 3; k++)
            u.sunc[k] = 100000, u.skyc[k] = 20000, u.gndc[k] = 6000, u.wb[k] = 1, u.fogc[k] = 3000;
        u.misc[1] = 1;
        u.misc[2] = 0.6f;
        u.bg[0] = 1000;
        u.fogc[3] = 1;
        for (k = 0; k < 16; k++)
            set->h.vp[k] = k % 5 == 0 ? 1.0f : 0.0f;
        set->h.eye[3] = NL;
        set->h.zk[0] = 1.0f, set->h.zk[1] = 1.0f;
        for (k = 0; k < LC_N; k++)
            set->cl[k] = NL;              /* offset 0, NL lights */
        for (k = 0; k < NL; k++)
            set->idx[k] = (uint8_t)k;
        set->h.atl[0] = 16.0f, set->h.atl[1] = 1.0f / 16.0f;
        for (k = 0; k < NL; k++) {
            ls[k].pos[0] = (float)(urand() * 40 - 20), ls[k].pos[1] = (float)(urand() * 40 - 20), ls[k].pos[2] = (float)(urand() * 2);
            ls[k].kind = k & 1 ? LIGHT_REAR : LIGHT_HEAD;
            ls[k].axis[0] = 1, ls[k].side[1] = -1;
            ls[k].candela = k & 1 ? 300.0f : 20000.0f;
            ls[k].range = 150;
            ls[k].rgb[0] = ls[k].rgb[1] = ls[k].rgb[2] = 1;
            ls[k].cosout = k & 1 ? 0.0f : 0.3420201f;
        }
        for (i = 0; i < N; i++) {
            double n[3];
            int c = i % 5;
            udir(n);
            for (k = 0; k < 3; k++) {
                in[i].p[k] = (float)(urand() * 60 - 30);
                in[i].n[k] = (float)n[k];
                in[i].alb[k] = (float)(urand() * 0.9);
            }
            if (c == 1)          /* on a lamp */
                memcpy(in[i].p, ls[i % NL].pos, 12);
            if (c == 2)          /* the normal facing away from the eye */
                for (k = 0; k < 3; k++)
                    in[i].n[k] = (float)-n[k];
            if (c == 3)          /* the sun straight behind the view (l = -v) */
                for (k = 0; k < 3; k++)
                    in[i].p[k] = u.eye[k] + u.sun[k] * (float)(1 + urand() * 30);
            in[i].alb[3] = c == 4 ? 0.0f : (float)urand();
            in[i].fog[0] = (float)urand();
            in[i].fog[1] = (float)urand();
        }
        bl = B(set, sizeof *set);
        bi = B(in, (size_t)N * sizeof *in);
        bo = B(NULL, (size_t)N * 16);
        kern(@"t_radiance", N, ^(id<MTLComputeCommandEncoder> e) {
            [e setBytes:&u length:sizeof u atIndex:0];
            [e setBuffer:bl offset:0 atIndex:1];
            [e setTexture:sm atIndex:0];
            [e setBuffer:bi offset:0 atIndex:2];
            [e setBuffer:bo offset:0 atIndex:3];
            [e setTexture:s_brdfE atIndex:1];
            [e setTexture:atl atIndex:2];
        });
        memcpy(o, bo.contents, (size_t)N * 16);
        nf_scan((const float *)bo.contents, ((size_t)N * 16) / 4);
        for (i = 0; i < N; i++)
            bad += !finite4(&o[i * 4]);
        report(bad == 0, "radiance + fog finite, random scenes", "%d non-finite of %d (on a lamp, back-facing, sun behind the view, roughness 0)", bad, N);
        free(in), free(o), free(set);
    }
    {   /* lamp, neutral, delit over degenerate inputs */
        int bad = 0, i;
        double hv[4][2] = { { 0, 0 }, { 89.99, 0 }, { 0, 89.99 }, { -45, -45 } };
        float lo[4], c[6][4] = { { 0, 0, 0, 0 }, { 1e30f, 1e30f, 1e30f, 0 }, { 1e30f, 0, 0, 0 }, { -1, -1, -1, 0 }, { 65504, 0, 1, 0 }, { 1e-30f, 0, 0, 0 } }, no[6][4];
        lamp_dirs(hv, 4, lo);
        for (i = 0; i < 4; i++)
            bad += !isfinite(lo[i]);
        neutral_gpu(c, no, 6);
        for (i = 0; i < 6; i++)
            bad += !finite4(no[i]);
        report(bad == 0, "lamp and neutral finite at extremes", "%d non-finite (lamp at 90 degrees off axis; neutral at 0, 1e30, -1, 65504)", bad);
    }
}

/* ---- main ------------------------------------------------------------------------------------- */
int main(int argc, char **argv)
{
    @autoreleasepool {
        NSError *err = nil;
        NSString *src, *ks;
        if (argc < 2) {
            fprintf(stderr, "usage: lighttest <kernels.metal> [--dump <file.metal>]\n");
            return 2;
        }
        s_dev = MTLCreateSystemDefaultDevice();
        if (!s_dev) {
            fprintf(stderr, "lighttest: no Metal device\n");
            return 2;
        }
        /* the renderer's shader source, the same expression brr_open compiles */
        src = [[[k_src stringByAppendingString:@BRR_LIGHTS_SRC] stringByAppendingString:@BRR_LIT_SRC] stringByAppendingString:@BRR_SKY_SRC];
        if (argc >= 4 && !strcmp(argv[2], "--dump")) {
            [src writeToFile:[NSString stringWithUTF8String:argv[3]] atomically:YES encoding:NSUTF8StringEncoding error:&err];
            return 0;
        }
        s_lib = [s_dev newLibraryWithSource:src options:nil error:&err];
        if (!s_lib) {
            fprintf(stderr, "lighttest: the renderer's shaders: %s\n", [[err description] UTF8String]);
            return 1;
        }
        ks = [NSString stringWithContentsOfFile:[NSString stringWithUTF8String:argv[1]] encoding:NSUTF8StringEncoding error:&err];
        if (!ks) {
            fprintf(stderr, "lighttest: %s: %s\n", argv[1], [[err description] UTF8String]);
            return 2;
        }
        g_tl = [s_dev newLibraryWithSource:[src stringByAppendingString:ks] options:nil error:&err];
        if (!g_tl) {
            fprintf(stderr, "lighttest: test kernels: %s\n", [[err description] UTF8String]);
            return 2;
        }
        g_q = [s_dev newCommandQueue];
        g_cp = [NSMutableDictionary new];
        printf("lighttest: %s; the renderer's shaders compiled (%lu characters), its lit pipelines made\n", [[s_dev name] UTF8String],
               (unsigned long)[src length]);
        s_q = g_q;                        /* the renderer's queue (brr_open makes it): lit_init draws the BRDF table on it */
        {
            double t0 = CACurrentMediaTime();
            id<MTLCommandBuffer> cb;
            lit_init();
            cb = [g_q commandBuffer];
            [cb commit];
            [cb waitUntilCompleted];
            printf("lit_init, its pipelines and the BRDF table: %.1f ms\n", (CACurrentMediaTime() - t0) * 1000.0);
        }
        if (!s_lpp || !s_lbd || !s_lbua || !s_patmT || !s_patmM || !s_patmS || !s_patmI) {
            fprintf(stderr, "lighttest: a lit pipeline failed\n");
            return 1;
        }
        test_brdf();
        test_carpaint();
        test_sky();
        test_tonemap();
        test_exposure();
        test_lamp();
        test_bloom();
        test_delit();
        test_misc();
        printf("\n");
        report(g_nonfin == 0, "every kernel output finite", "%zu non-finite of %zu values read back", g_nonfin, g_scanned);
        printf("\n%d passed, %d failed\n", g_pass, g_fail);
        return g_fail ? 1 : 0;
    }
}
