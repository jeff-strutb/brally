/* env_test.m: the sky's environment on the GPU (brr_metal_env.h, .m):
 *   1. faces: a map whose texels hold their own direction, through the cube
 *      and Metal's sampling, gives back each direction (the faces' table and
 *      the bearing's turn agree with the hardware's selection);
 *   2. the prefilter: a uniform sky stays uniform at every mip (the lobe's
 *      weights normalised), and a sky with a bright patch matches the GGX
 *      lobe's integral done texel by texel on the CPU.
 * env.sh builds and runs it (macOS). */
#import <Metal/Metal.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "light_sky.h"
#include "brr_metal_env.h"
#include "brr_metal_env_api.h"

#define PI_F 3.14159265358979f
static int s_fail;
#define CHECK(c, ...) do { if (!(c)) { s_fail++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const char *k_src =
    "#include <metal_stdlib>\nusing namespace metal;\n"
    "struct LO { float4 pos [[position]]; float2 uv; };\n"
    "vertex LO lvs(uint i [[vertex_id]]) { float2 p = float2((i << 1) & 2, i & 2); LO o; o.pos = float4(p * 2.0 - 1.0, 0, 1); o.uv = float2(p.x, 1.0 - p.y); return o; }\n"
    "struct LU { float4 sh[9]; float4 env; };\n";
static const char *k_probe =
    "kernel void probe(uint i [[thread_position_in_grid]], texturecube<float> c [[texture(0)]], const device float4 *d [[buffer(0)]],\n"
    "                  device float4 *o [[buffer(1)]], constant float &lod [[buffer(2)]]) {\n"
    "  constexpr sampler s(filter::linear, mip_filter::linear); o[i] = c.sample(s, d[i].xyz, level(lod)); }\n";

static id<MTLDevice> s_dev;
static id<MTLLibrary> s_lib;
static id<MTLCommandQueue> s_q;
static id<MTLComputePipelineState> s_pp;

static void probe(id<MTLTexture> t, const float (*d)[4], int n, float lod, float (*o)[4])
{
    id<MTLBuffer> bd = [s_dev newBufferWithBytes:d length:(NSUInteger)n * 16 options:MTLResourceStorageModeShared];
    id<MTLBuffer> bo = [s_dev newBufferWithLength:(NSUInteger)n * 16 options:MTLResourceStorageModeShared];
    id<MTLCommandBuffer> cb = [s_q commandBuffer];
    id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
    [e setComputePipelineState:s_pp];
    [e setTexture:t atIndex:0];
    [e setBuffer:bd offset:0 atIndex:0];
    [e setBuffer:bo offset:0 atIndex:1];
    [e setBytes:&lod length:4 atIndex:2];
    [e dispatchThreads:MTLSizeMake((NSUInteger)n, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
    [e endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    memcpy(o, [bo contents], (size_t)n * 16);
}

static void randdir(float d[3])
{
    float l;
    do {
        d[0] = 2.0f * rand() / (float)RAND_MAX - 1.0f;
        d[1] = 2.0f * rand() / (float)RAND_MAX - 1.0f;
        d[2] = 2.0f * rand() / (float)RAND_MAX - 1.0f;
        l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    } while (l < 0.1f || l > 1.0f);
    d[0] /= l; d[1] /= l; d[2] /= l;
}

static LightSky mksky(int w, float rot, void (*f)(const float d[3], float c[3]))
{
    LightSky s;
    int x, y;
    memset(&s, 0, sizeof s);
    s.w = w;
    s.h = w / 2;
    s.rot = rot;
    s.rgb = malloc((size_t)s.w * s.h * 3 * sizeof(float));
    for (y = 0; y < s.h; y++)
        for (x = 0; x < s.w; x++) {
            float d[3];
            light_sky_dir(&s, (float)x, (float)y, d);
            f(d, s.rgb + ((size_t)y * s.w + x) * 3);
        }
    return s;
}

static void f_dir(const float d[3], float c[3]) { c[0] = d[0] * 0.5f + 0.5f; c[1] = d[1] * 0.5f + 0.5f; c[2] = d[2] * 0.5f + 0.5f; }
static void f_one(const float d[3], float c[3]) { (void)d; c[0] = c[1] = c[2] = 1.0f; }
static const float k_patch[3] = { 0.3f, -0.5f, 0.81f };
static void f_patch(const float d[3], float c[3])
{
    float k = d[0] * k_patch[0] + d[1] * k_patch[1] + d[2] * k_patch[2], l = sqrtf(k_patch[0] * k_patch[0] + k_patch[1] * k_patch[1] + k_patch[2] * k_patch[2]);
    float a = acosf(fminf(fmaxf(k / l, -1.0f), 1.0f)), v = 0.1f + 50.0f * expf(-a * a / (2.0f * 0.08f * 0.08f));
    c[0] = v; c[1] = v * 0.5f; c[2] = 0.1f;
}

/* the prefilter's estimator's limit: the radiance weighted by n.l D(h) over the
   sphere (pdf D / 4 with n = v, the samples weighted by n.l) */
static void cpu_lobe(const LightSky *s, const float n[3], float rough, double o[3])
{
    double a2 = pow(rough, 4.0), acc[3] = { 0, 0, 0 }, ws = 0.0;
    int x, y, c;
    for (y = 0; y < s->h; y++) {
        float el = 0.5f * PI_F - PI_F * ((float)y + 0.5f) / (float)s->h;
        double om = (2.0 * M_PI / s->w) * (M_PI / s->h) * cos(el);
        for (x = 0; x < s->w; x++) {
            float l[3], h[3], hl, nl, nh;
            double dd, D;
            light_sky_dir(s, (float)x, (float)y, l);
            nl = l[0] * n[0] + l[1] * n[1] + l[2] * n[2];
            if (nl <= 0.0f)
                continue;
            h[0] = l[0] + n[0]; h[1] = l[1] + n[1]; h[2] = l[2] + n[2];
            hl = sqrtf(h[0] * h[0] + h[1] * h[1] + h[2] * h[2]);
            nh = (h[0] * n[0] + h[1] * n[1] + h[2] * n[2]) / hl;
            dd = nh * nh * (a2 - 1.0) + 1.0;
            D = a2 / (M_PI * dd * dd);
            for (c = 0; c < 3; c++)
                acc[c] += s->rgb[((size_t)y * s->w + x) * 3 + c] * nl * D * om;
            ws += nl * D * om;
        }
    }
    for (c = 0; c < 3; c++)
        o[c] = acc[c] / ws;
}

int main(void)
{
    @autoreleasepool {
        NSError *err = nil;
        NSString *src = [NSString stringWithFormat:@"%s%s%s%s", k_src, BRR_ENV_SRC, BRR_ENV_PASS_SRC, k_probe];
        int i, m;
        s_dev = MTLCreateSystemDefaultDevice();
        s_q = [s_dev newCommandQueue];
        s_lib = [s_dev newLibraryWithSource:src options:nil error:&err];
        if (!s_lib) {
            printf("FAIL: compile: %s\n", [[err description] UTF8String]);
            return 1;
        }
        s_pp = [s_dev newComputePipelineStateWithFunction:[s_lib newFunctionWithName:@"probe"] error:&err];
        srand(11);
        {   /* 1. the faces: direction in, direction out */
            LightSky s = mksky(2048, 0.7f, f_dir);
            BrrEnv e = { 0 };
            float d[512][4], o[512][4], worst = 0.0f;
            CHECK(!brr_env_build(&e, s_dev, s_lib, s_q, &s), "faces: build");
            for (i = 0; i < 512; i++) {
                randdir(d[i]);
                d[i][3] = 0;
            }
            probe(e.cube, d, 512, 0.0f, o);
            for (i = 0; i < 512; i++) {
                float er = fmaxf(fabsf(o[i][0] - (d[i][0] * 0.5f + 0.5f)), fmaxf(fabsf(o[i][1] - (d[i][1] * 0.5f + 0.5f)), fabsf(o[i][2] - (d[i][2] * 0.5f + 0.5f))));
                if (er > worst)
                    worst = er;
            }
            printf("faces: 512 directions through the cube, worst channel error %.4f\n", worst);
            CHECK(worst < 0.01f, "faces: a direction came back off by %.4f (the faces' table or the turn is wrong)", worst);
            probe(e.spec, d, 512, 0.0f, o);
            worst = 0.0f;
            for (i = 0; i < 512; i++) {
                float er = fmaxf(fabsf(o[i][0] - (d[i][0] * 0.5f + 0.5f)), fmaxf(fabsf(o[i][1] - (d[i][1] * 0.5f + 0.5f)), fabsf(o[i][2] - (d[i][2] * 0.5f + 0.5f))));
                if (er > worst)
                    worst = er;
            }
            printf("faces: the specular's mirror mip, worst %.4f\n", worst);
            CHECK(worst < 0.02f, "faces: the specular's mip 0 off by %.4f", worst);
            light_sky_free(&s);
        }
        {   /* 2a. a uniform sky stays uniform */
            LightSky s = mksky(512, 0.0f, f_one);
            BrrEnv e = { 0 };
            float d[256][4], o[256][4];
            CHECK(!brr_env_build(&e, s_dev, s_lib, s_q, &s), "uniform: build");
            for (i = 0; i < 256; i++) {
                randdir(d[i]);
                d[i][3] = 0;
            }
            for (m = 0; m < ENV_SPEC_MIPS; m++) {
                float lo = 1e9f, hi = -1e9f;
                probe(e.spec, d, 256, (float)m, o);
                for (i = 0; i < 256; i++) {
                    lo = fminf(lo, o[i][1]);
                    hi = fmaxf(hi, o[i][1]);
                }
                printf("uniform: mip %d (roughness %.3f) %.4f .. %.4f\n", m, (float)m / (ENV_SPEC_MIPS - 1), lo, hi);
                CHECK(lo > 0.995f && hi < 1.005f, "uniform: mip %d ranges %.4f .. %.4f", m, lo, hi);
            }
            light_sky_free(&s);
        }
        {   /* 2b. a bright patch against the CPU's lobe */
            LightSky s = mksky(1024, 0.0f, f_patch);
            BrrEnv e = { 0 };
            float d[8][4], o[8][4];
            float l = sqrtf(k_patch[0] * k_patch[0] + k_patch[1] * k_patch[1] + k_patch[2] * k_patch[2]);
            CHECK(!brr_env_build(&e, s_dev, s_lib, s_q, &s), "patch: build");
            {   /* every mip of the cube keeps the sphere's integral (the patch's energy) */
                static float rd[65536][4], ro[65536][4];
                double ref = 0.0;
                int y2, x2;
                for (y2 = 0; y2 < s.h; y2++)
                    for (x2 = 0; x2 < s.w; x2++)
                        ref += s.rgb[((size_t)y2 * s.w + x2) * 3] * (2.0 * M_PI / s.w) * (M_PI / s.h) * cos(0.5 * M_PI - M_PI * (y2 + 0.5) / s.h);
                for (i = 0; i < 65536; i++) {   /* a Fibonacci sphere: even cover, so the patch is integrated, not sampled */
                    float z = 1.0f - (2.0f * i + 1.0f) / 65536.0f, r = sqrtf(1.0f - z * z), ph = 2.39996323f * (float)i;
                    rd[i][0] = r * cosf(ph); rd[i][1] = r * sinf(ph); rd[i][2] = z; rd[i][3] = 0;
                }
                for (m = 0; m < 10; m++) {
                    double sum = 0.0;
                    probe(e.cube, (const float (*)[4])rd, 65536, (float)m, ro);
                    for (i = 0; i < 65536; i++)
                        sum += ro[i][0];
                    sum *= 4.0 * M_PI / 65536.0;
                    printf("patch: cube mip %d integral %.3f (map %.3f)\n", m, sum, ref);
                    CHECK(fabs(sum - ref) < 0.05 * ref, "patch: cube mip %d holds %.3f of %.3f", m, sum, ref);
                }
            }
            for (i = 0; i < 8; i++) {   /* toward the patch, beside it, away */
                float t = (float)i * 0.12f, b[3] = { k_patch[0] / l, k_patch[1] / l, k_patch[2] / l }, side[3] = { 0.8f, 0.6f, 0.0f }, n;
                d[i][0] = b[0] + t * side[0]; d[i][1] = b[1] + t * side[1]; d[i][2] = b[2] + t * side[2];
                n = sqrtf(d[i][0] * d[i][0] + d[i][1] * d[i][1] + d[i][2] * d[i][2]);
                d[i][0] /= n; d[i][1] /= n; d[i][2] /= n; d[i][3] = 0;
            }
            for (m = 1; m < ENV_SPEC_MIPS; m++) {
                float rough = (float)m / (ENV_SPEC_MIPS - 1), worst = 0.0f;
                probe(e.spec, d, 8, (float)m, o);
                for (i = 0; i < 8; i++) {
                    double r[3], er;
                    float nn[3] = { d[i][0], d[i][1], d[i][2] };
                    cpu_lobe(&s, nn, rough, r);
                    er = fabs(o[i][0] - r[0]) / fmax(r[0], 0.5);
                    if (er > worst)
                        worst = (float)er;
                    if (i == 0 || i == 4)
                        printf("patch: roughness %.3f at %d: GPU %.4f, CPU %.4f\n", rough, i, o[i][0], r[0]);
                }
                printf("patch: roughness %.3f, worst relative error %.2f%%\n", rough, 100.0f * worst);
                CHECK(worst < 0.06f, "patch: roughness %.3f off by %.2f%%", rough, 100.0f * worst);
            }
            light_sky_free(&s);
        }
    }
    printf(s_fail ? "env_test: %d FAILED\n" : "env_test: all passed\n", s_fail);
    return s_fail != 0;
}
