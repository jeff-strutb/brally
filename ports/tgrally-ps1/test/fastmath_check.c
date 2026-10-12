/* fastmath_check.c: platform/fastmath against double-precision truth (host) */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define sinf fm_sinf
#define cosf fm_cosf
#define guMtxCatF fm_guMtxCatF
#define BrMat4RotateVec fm_rv
#define BrMat4RotateVecT fm_rvt
#define BrMat3MulVecRows fm_mvr
#define guMtxF2L fm_f2l
#include "../platform/fastmath/fastmath.c"
#undef sinf
#undef cosf
float __slow_sinf(float x) { return (float)sin((double)x); }
float __slow_cosf(float x) { return (float)cos((double)x); }

static uint32_t s = 7;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
static float frand(float lo, float hi) { return lo + (hi - lo) * (rnd() / 4294967296.0f); }
static int32_t ulps(float a, double t)
{
    float tf = (float)t;
    int32_t ia, ib;
    memcpy(&ia, &a, 4); memcpy(&ib, &tf, 4);
    if (ia < 0) ia = (int32_t)0x80000000 - ia;
    if (ib < 0) ib = (int32_t)0x80000000 - ib;
    return ia > ib ? ia - ib : ib - ia;
}

int main(void)
{
    int i, worst_s = 0, worst_c = 0, worst_d = 0, worst_seq = 0;
    double maxabs_s = 0;
    for (i = 0; i < 20000000; i++) {
        float x = frand(-200.0f, 200.0f), y;
        int u;
        y = fm_sinf(x);
        if (fabs((double)y - sin((double)x)) > maxabs_s) maxabs_s = fabs((double)y - sin((double)x));
        u = ulps(y, sin((double)x)); if (fabs(sin(x)) > 1e-3 && u > worst_s) worst_s = u;
        y = fm_cosf(x);
        u = ulps(y, cos((double)x)); if (fabs(cos(x)) > 1e-3 && u > worst_c) worst_c = u;
    }
    printf("sinf: worst %d ulps (|sin| > 1e-3), max abs error %.3g; cosf: worst %d ulps\n", worst_s, maxabs_s, worst_c);
    for (i = 0; i < 2000000; i++) {
        float m[4][4], v[3], o[3], q[3];
        int r, c;
        double t;
        for (r = 0; r < 4; r++) for (c = 0; c < 4; c++) m[r][c] = frand(-2, 2) * (r == 3 ? 500 : 1);
        for (c = 0; c < 3; c++) v[c] = frand(-1000, 1000);
        fm_mvr(o, m, v);
        for (c = 0; c < 3; c++) {
            int u;
            t = (double)m[0][c] * v[0] + (double)m[1][c] * v[1] + (double)m[2][c] * v[2] + m[3][c];
            u = ulps(o[c], t); if (u > worst_d && fabs(t) > 1e-2) worst_d = u;
            q[c] = 0.0f; for (r = 0; r < 3; r++) q[c] += m[r][c] * v[r]; q[c] += m[3][c];
            u = ulps(q[c], t); if (u > worst_seq && fabs(t) > 1e-2) worst_seq = u;
        }
    }
    printf("fused dot: worst %d ulps from the exact sum (the game's own float sequence: %d)\n", worst_d, worst_seq);
    {
        float mf[4][4];
        uint8_t mb[64];
        int bad = 0, r, c;
        for (i = 0; i < 1000000; i++) {
            for (r = 0; r < 4; r++) for (c = 0; c < 4; c++) mf[r][c] = frand(-30000, 30000) / (1 << (rnd() % 20));
            fm_f2l(mf, mb);
            for (r = 0; r < 4; r++) for (c = 0; c < 4; c++) {
                int32_t want = (int32_t)(mf[r][c] * 65536.0);
                int k = r * 4 + c;
                uint16_t hi = (uint16_t)(mb[k * 2] << 8 | mb[k * 2 + 1]), lo = (uint16_t)(mb[32 + k * 2] << 8 | mb[32 + k * 2 + 1]);
                if ((int32_t)((uint32_t)hi << 16 | lo) != want) bad++;
            }
        }
        printf("guMtxF2L: %d differences\n", bad);
    }
    return 0;
}
