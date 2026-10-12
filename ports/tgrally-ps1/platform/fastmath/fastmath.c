/* fastmath.c: the game's hottest maths for a CPU without an FPU.
 *
 * A sum of float products done as the R3000 does integers: each operand
 * unpacked once, the 24-bit mantissas multiplied exactly (32 x 32 -> 64),
 * the products aligned to the largest and summed in 64 bits, the total
 * rounded once to the nearest float (the game's code rounds after every
 * multiply and add: a result here is at least as close to the true sum, and
 * may differ from the game's in its last bit).  The trigonometry is 64-bit
 * fixed point (reduction by pi/2 to 40 fraction bits, Taylor polynomials to
 * 30), within a few units in the last place.  guMtxF2L is exact.  A
 * denormal, infinite or NaN operand goes the game's own way.
 *
 * These replace the game's functions of the same names: link.sh weakens the
 * core's definitions (fastmath.txt) so these are the ones linked. */
#include <stdint.h>
#include <string.h>

typedef union { float f; uint32_t u; } F;

/* ---- the fused sum -------------------------------------------------------------- *
 * 32-bit arithmetic only (the R3000 has no 64-bit shifts): each product's top
 * 30 bits (of 48) with its exponent, aligned to the largest by one shift,
 * summed with 2 bits of headroom, normalised and rounded once. */
#define MAXT 5

static int clz32(uint32_t x)
{
    int n = 0;
    if (!(x >> 16)) { n += 16; x <<= 16; }
    if (!(x >> 24)) { n += 8; x <<= 8; }
    if (!(x >> 28)) { n += 4; x <<= 4; }
    if (!(x >> 30)) { n += 2; x <<= 2; }
    if (!(x >> 31)) n += 1;
    return n;
}

/* the same sum carried exactly in 64 bits: for when the 32-bit one cancelled
 * down to too few bits */
static int fdot64(const float *a, int sa, const float *b, int sb, int n, const float *add, float *out)
{
    uint64_t mag[MAXT];
    int32_t ex[MAXT], E = -100000, k = 0, i, p;
    uint32_t neg = 0;
    int64_t sum = 0;
    uint64_t m;
    F r;
    for (i = 0; i < n; i++) {
        uint32_t xu = ((const uint32_t *)a)[i * sa], yu = ((const uint32_t *)b)[i * sb];
        uint32_t ea = (xu >> 23) & 0xFF, eb = (yu >> 23) & 0xFF;
        if (ea == 0 || eb == 0)
            continue;                           /* (zeros: the caller checked the rest) */
        mag[k] = (uint64_t)((xu & 0x7FFFFF) | 0x800000) * ((yu & 0x7FFFFF) | 0x800000);
        ex[k] = (int32_t)(ea + eb);
        if ((xu ^ yu) >> 31)
            neg |= 1u << k;
        if (ex[k] > E)
            E = ex[k];
        k++;
    }
    if (add) {
        uint32_t xu = *(const uint32_t *)add, ea = (xu >> 23) & 0xFF;
        if (ea) {
            mag[k] = (uint64_t)((xu & 0x7FFFFF) | 0x800000) << 23;
            ex[k] = (int32_t)ea + 127;
            if (xu >> 31)
                neg |= 1u << k;
            if (ex[k] > E)
                E = ex[k];
            k++;
        }
    }
    for (i = 0; i < k; i++) {
        int32_t d = E - ex[i];
        uint64_t v = d >= 56 ? 0 : (mag[i] << 8) >> d;
        sum += (neg >> i) & 1 ? -(int64_t)v : (int64_t)v;
    }
    r.u = 0;
    if (!sum) {
        *out = r.f;
        return 1;
    }
    if (sum < 0) {
        r.u = 0x80000000u;
        sum = -sum;
    }
    m = (uint64_t)sum;
    p = 63;
    while (!(m >> p))
        p--;
    {
        int32_t e = p + E - 308 + 127;
        uint32_t mant;
        if (p > 23) {
            int sh = p - 23;
            uint64_t rem = m & (((uint64_t)1 << sh) - 1), half = (uint64_t)1 << (sh - 1);
            mant = (uint32_t)(m >> sh);
            if (rem > half || (rem == half && (mant & 1)))
                mant++;
            if (mant >> 24) {
                mant >>= 1;
                e++;
            }
        } else {
            mant = (uint32_t)m << (23 - p);
        }
        if (e <= 0 || e >= 0xFF)
            return 0;
        r.u |= (uint32_t)e << 23 | (mant & 0x7FFFFF);
    }
    *out = r.f;
    return 1;
}

/* sum of n products a[i*sa] * b[i*sb], plus `add` when given; 0 when an operand
 * is not a normal number or zero (the caller then goes the slow way) */
static int fdot(const float *a, int sa, const float *b, int sb, int n, const float *add, float *out)
{
    int32_t mag[MAXT], ex[MAXT], E = -100000, k = 0, i, sum = 0;
    F r;
    for (i = 0; i < n; i++) {
        uint32_t xu = ((const uint32_t *)a)[i * sa], yu = ((const uint32_t *)b)[i * sb];
        uint32_t ea = (xu >> 23) & 0xFF, eb = (yu >> 23) & 0xFF, hi;
        if (ea - 1 >= 0xFE || eb - 1 >= 0xFE) {
            if ((xu << 1) == 0 && eb != 0xFF)
                continue;                       /* zero times a finite number */
            if ((yu << 1) == 0 && ea != 0xFF)
                continue;
            return 0;
        }
        {
            uint64_t p = (uint64_t)((xu & 0x7FFFFF) | 0x800000) * ((yu & 0x7FFFFF) | 0x800000);
            hi = (uint32_t)(p >> 32) << 14 | (uint32_t)p >> 18;     /* the top 30 of 48 bits */
        }
        mag[k] = (int32_t)(((xu ^ yu) >> 31) ? -(int32_t)(hi >> 2) : (int32_t)(hi >> 2));   /* under 2^28 */
        ex[k] = (int32_t)(ea + eb);             /* the value: mag * 2^(ex - 280) */
        if (ex[k] > E)
            E = ex[k];
        k++;
    }
    if (add) {
        uint32_t xu = *(const uint32_t *)add, ea = (xu >> 23) & 0xFF;
        if (ea == 0xFF || (ea == 0 && (xu << 1)))
            return 0;
        if (ea) {
            uint32_t m = ((xu & 0x7FFFFF) | 0x800000) << 3;          /* m * 2^(ea - 150) as mag * 2^(ex - 280) */
            mag[k] = (xu >> 31) ? -(int32_t)m : (int32_t)m;
            ex[k] = (int32_t)ea + 127;
            if (ex[k] > E)
                E = ex[k];
            k++;
        }
    }
    if (!k) {
        r.u = 0;
        *out = r.f;
        return 1;
    }
    for (i = 0; i < k; i++) {
        int32_t d = E - ex[i];
        sum += d >= 31 ? 0 : mag[i] >> d;       /* (at most 5 terms under 2^28: no overflow) */
    }
    if (!sum) {
        r.u = 0;
        *out = r.f;
        return 1;
    }
    {
        uint32_t m, s = 0, mant;
        int p, sh;
        int32_t e;
        if (sum < 0) {
            s = 0x80000000u;
            sum = -sum;
        }
        m = (uint32_t)sum;
        p = 31 - clz32(m);                      /* the leading bit */
        if (p < 26 && k > 1)                    /* cancelled: too few of the bits left for one rounding */
            return fdot64(a, sa, b, sb, n, add, out);
        /* a term's value is mag * 2^(ex - 280) (the product >> 20); the sum's leading bit
         * weighs 2^(p + E - 280): biased exponent p + E - 280 + 127 */
        e = p + E - 153;
        if (p > 23) {
            uint32_t rem, half;
            sh = p - 23;
            rem = m & ((1u << sh) - 1);
            half = 1u << (sh - 1);
            mant = m >> sh;
            if (rem > half || (rem == half && (mant & 1)))
                mant++;
            if (mant >> 24) {
                mant >>= 1;
                e++;
            }
        } else {
            mant = m << (23 - p);
        }
        if (e <= 0 || e >= 0xFF)
            return 0;
        r.u = s | (uint32_t)e << 23 | (mant & 0x7FFFFF);
    }
    *out = r.f;
    return 1;
}

/* the straight-line form for 3 or 4 products and an optional added float:
 * operands as raw words, no arrays, everything in registers */
#define TERM(xu, yu, m, e) do {                                                         \
        uint32_t ea_ = ((xu) >> 23) & 0xFF, eb_ = ((yu) >> 23) & 0xFF;                 \
        if (ea_ - 1 >= 0xFEu || eb_ - 1 >= 0xFEu) {                                    \
            if (((xu) << 1) == 0 && eb_ != 0xFF) { m = 0; e = -100000; }               \
            else if (((yu) << 1) == 0 && ea_ != 0xFF) { m = 0; e = -100000; }          \
            else return 0;                                                             \
        } else {                                                                       \
            uint64_t p_ = (uint64_t)(((xu) & 0x7FFFFF) | 0x800000) * (((yu) & 0x7FFFFF) | 0x800000); \
            uint32_t h_ = ((uint32_t)(p_ >> 32) << 12) | ((uint32_t)p_ >> 20);         \
            m = (((xu) ^ (yu)) >> 31) ? -(int32_t)h_ : (int32_t)h_;                    \
            e = (int32_t)(ea_ + eb_);                                                  \
        }                                                                              \
    } while (0)

static int fsum_tail(int32_t m0, int32_t e0, int32_t m1, int32_t e1, int32_t m2, int32_t e2, int32_t m3, int32_t e3,
                     int32_t m4, int32_t e4, float *out)
{
    int32_t E = e0, sum, d;
    uint32_t m, s = 0, mant;
    int p;
    int32_t e;
    F r;
    if (e1 > E) E = e1;
    if (e2 > E) E = e2;
    if (e3 > E) E = e3;
    if (e4 > E) E = e4;
    d = E - e0; sum = d >= 31 ? 0 : m0 >> d;
    d = E - e1; sum += d >= 31 ? 0 : m1 >> d;
    d = E - e2; sum += d >= 31 ? 0 : m2 >> d;
    d = E - e3; sum += d >= 31 ? 0 : m3 >> d;
    d = E - e4; sum += d >= 31 ? 0 : m4 >> d;
    if (!sum) {
        r.u = 0;
        *out = r.f;
        return 1;
    }
    if (sum < 0) {
        s = 0x80000000u;
        sum = -sum;
    }
    m = (uint32_t)sum;
    p = 31 - clz32(m);
    if (p < 26)
        return -1;                              /* cancelled: the exact way */
    e = p + E - 153;
    {
        int sh = p - 23;
        uint32_t rem = m & ((1u << sh) - 1), half = 1u << (sh - 1);
        mant = m >> sh;
        if (rem > half || (rem == half && (mant & 1)))
            mant++;
        if (mant >> 24) {
            mant >>= 1;
            e++;
        }
    }
    if (e <= 0 || e >= 0xFF)
        return 0;
    r.u = s | (uint32_t)e << 23 | (mant & 0x7FFFFF);
    *out = r.f;
    return 1;
}

/* a0*b0 + a1*b1 + a2*b2 (+ c): 1 done, 0 the slow way */
static int fdot3(const float *a, int sa, const float *b, int sb, const float *c, float *out)
{
    const uint32_t *A = (const uint32_t *)a, *B = (const uint32_t *)b;
    int32_t m0, e0, m1, e1, m2, e2, m3 = 0, e3 = -100000, k;
    TERM(A[0], B[0], m0, e0);
    TERM(A[sa], B[sb], m1, e1);
    TERM(A[2 * sa], B[2 * sb], m2, e2);
    if (c) {
        uint32_t xu = *(const uint32_t *)c, ea = (xu >> 23) & 0xFF;
        if (ea == 0xFF || (ea == 0 && (xu << 1)))
            return 0;
        if (ea) {
            uint32_t mm = ((xu & 0x7FFFFF) | 0x800000) << 3;
            m3 = (xu >> 31) ? -(int32_t)mm : (int32_t)mm;
            e3 = (int32_t)ea + 127;
        }
    }
    if (e0 < -1000 && e1 < -1000 && e2 < -1000 && e3 < -1000) {
        *out = 0.0f;
        return 1;
    }
    k = fsum_tail(m0, e0, m1, e1, m2, e2, m3, e3, 0, -100000, out);
    if (k < 0) {
        extern uint32_t fm_calls[8];
        fm_calls[7]++;
        return fdot64(a, sa, b, sb, 3, c, out);
    }
    if (!k) {
        extern uint32_t fm_calls[8];
        fm_calls[6]++;
    }
    return k;
}

static int fdot4(const float *a, int sa, const float *b, int sb, float *out)
{
    const uint32_t *A = (const uint32_t *)a, *B = (const uint32_t *)b;
    int32_t m0, e0, m1, e1, m2, e2, m3, e3, k;
    TERM(A[0], B[0], m0, e0);
    TERM(A[sa], B[sb], m1, e1);
    TERM(A[2 * sa], B[2 * sb], m2, e2);
    TERM(A[3 * sa], B[3 * sb], m3, e3);
    if (e0 < -1000 && e1 < -1000 && e2 < -1000 && e3 < -1000) {
        *out = 0.0f;
        return 1;
    }
    k = fsum_tail(m0, e0, m1, e1, m2, e2, m3, e3, 0, -100000, out);
    if (k < 0)
        return fdot64(a, sa, b, sb, 4, NULL, out);
    return k;
}

/* ---- the game's matrix functions ----------------------------------------------------- */
uint32_t fm_calls[8];                           /* (TGR_PERF: calls of each) */
void guMtxCatF(float mf[4][4], float nf[4][4], float res[4][4])
{
    fm_calls[0]++;
    float t[4][4];
    int i, j, k;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            if (!fdot4(&mf[i][0], 1, &nf[0][j], 4, &t[i][j])) {
                t[i][j] = 0.0f;                 /* the game's way */
                for (k = 0; k < 4; k++)
                    t[i][j] += mf[i][k] * nf[k][j];
            }
    memcpy(res, t, sizeof t);
}

void BrMat4RotateVec(float out[3], float m[4][4], float v[3])
{
    fm_calls[1]++;
    float t[3];
    int i, k;
    for (i = 0; i < 3; i++)
        if (!fdot3(&m[i][0], 1, v, 1, NULL, &t[i])) {
            t[i] = 0.0f;
            for (k = 0; k < 3; k++)
                t[i] += m[i][k] * v[k];
        }
    out[0] = t[0]; out[1] = t[1]; out[2] = t[2];
}

void BrMat4RotateVecT(float out[3], float m[4][4], float v[3])
{
    fm_calls[2]++;
    float t[3];
    int i, k;
    for (i = 0; i < 3; i++)
        if (!fdot3(&m[0][i], 4, v, 1, NULL, &t[i])) {
            t[i] = 0.0f;
            for (k = 0; k < 3; k++)
                t[i] += m[k][i] * v[k];
        }
    out[0] = t[0]; out[1] = t[1]; out[2] = t[2];
}

void BrMat3MulVecRows(float out[3], float m[4][4], float v[3])
{
    fm_calls[3]++;
    float t[3];
    int i, k;
    for (i = 0; i < 3; i++)
        if (!fdot3(&m[0][i], 4, v, 1, &m[3][i], &t[i])) {
            t[i] = 0.0f;
            for (k = 0; k < 3; k++)
                t[i] += m[k][i] * v[k];
            t[i] += m[3][i];
        }
    out[0] = t[0]; out[1] = t[1]; out[2] = t[2];
}

/* float to s15.16 truncated, exactly as (long)(f * 65536.0): the product is exact */
static int32_t ftofix32(float f)
{
    F x;
    int32_t e, sh;
    uint32_t m, v;
    x.f = f;
    e = (int32_t)((x.u >> 23) & 0xFF);
    if (e < 127 - 16)
        return 0;                               /* |f| < 2^-16 (denormals and zero too) */
    sh = e - 150 + 16;                          /* the mantissa's shift into s15.16 */
    if (sh >= 8)
        return (int32_t)0x80000000;             /* out of range (the RSP's matrices never are) */
    m = (x.u & 0x7FFFFF) | 0x800000;
    v = sh >= 0 ? m << sh : m >> -sh;
    return (x.u >> 31) ? -(int32_t)v : (int32_t)v;
}

typedef struct { uint8_t b[64]; } MtxBytes;

void guMtxF2L(float mf[4][4], void *mp)
{
    uint8_t *ai = (uint8_t *)mp, *af = ai + 32;
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 2; j++) {
            int32_t e1 = ftofix32(mf[i][j * 2]), e2 = ftofix32(mf[i][j * 2 + 1]);
            uint32_t w1 = ((uint32_t)e1 & 0xffff0000u) | (((uint32_t)e2 >> 16) & 0xffff);
            uint32_t w2 = (((uint32_t)e1 << 16) & 0xffff0000u) | ((uint32_t)e2 & 0xffff);
            ai[0] = (uint8_t)(w1 >> 24); ai[1] = (uint8_t)(w1 >> 16); ai[2] = (uint8_t)(w1 >> 8); ai[3] = (uint8_t)w1;
            af[0] = (uint8_t)(w2 >> 24); af[1] = (uint8_t)(w2 >> 16); af[2] = (uint8_t)(w2 >> 8); af[3] = (uint8_t)w2;
            ai += 4;
            af += 4;
        }
}

/* ---- sine and cosine in fixed point ---------------------------------------------------- */
#define Q30 (1 << 30)
static int32_t mulq30(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 30); }

/* sin and cos of r (|r| <= pi/4, Q30) */
static int32_t sin_q30(int32_t r)
{
    int32_t r2 = mulq30(r, r), p;
    p = 2959;                                   /* 1/362880 in Q30 */
    p = mulq30(p, r2) - 213044;                 /* 1/5040 */
    p = mulq30(p, r2) + 8947849;                /* 1/120 */
    p = mulq30(p, r2) - 178956971;              /* 1/6 */
    p = mulq30(p, r2) + Q30;
    return mulq30(p, r);
}

static int32_t cos_q30(int32_t r)
{
    int32_t r2 = mulq30(r, r), p;
    p = -296;                                   /* -1/3628800 */
    p = mulq30(p, r2) + 26630;                  /* 1/40320 */
    p = mulq30(p, r2) - 1491308;                /* 1/720 */
    p = mulq30(p, r2) + 44739243;               /* 1/24 */
    p = mulq30(p, r2) - 536870912;              /* 1/2 */
    return mulq30(p, r2) + Q30;
}

/* a Q30 value (|v| <= 2^30) to the nearest float */
static float q30_to_f(int32_t v)
{
    F r;
    uint32_t m, s = 0;
    int p;
    if (!v) {
        r.u = 0;
        return r.f;
    }
    if (v < 0) {
        s = 0x80000000u;
        v = -v;
    }
    m = (uint32_t)v;
    p = 31;
    while (!(m >> p))
        p--;
    {
        int32_t e = p - 30 + 127;
        uint32_t mant;
        if (p > 23) {
            int sh = p - 23;
            uint32_t rem = m & ((1u << sh) - 1), half = 1u << (sh - 1);
            mant = m >> sh;
            if (rem > half || (rem == half && (mant & 1)))
                mant++;
            if (mant >> 24) {
                mant >>= 1;
                e++;
            }
        } else {
            mant = m << (23 - p);
        }
        r.u = s | (uint32_t)e << 23 | (mant & 0x7FFFFF);
    }
    return r.f;
}

/* reduce |x| by pi/2: the quadrant and the remainder (Q30); 0 when |x| is
 * too large or not finite for this (the slow way then) */
static int reduce(float x, int32_t *quad, int32_t *rem)
{
    F f;
    int32_t e, sh;
    int64_t X40, q, r;
    uint32_t m;
    f.f = x;
    e = (int32_t)((f.u >> 23) & 0xFF);
    if (e >= 127 + 16)
        return 0;                               /* |x| >= 65536, inf, NaN */
    m = (f.u & 0x7FFFFF) | 0x800000;
    if (e < 127 - 40) {                         /* tiny: the angle itself */
        *quad = 0;
        *rem = 0;
        return 2;
    }
    sh = e - 150 + 24;                          /* |x| in Q24 */
    {
        int64_t X24 = sh >= 0 ? (int64_t)m << sh : (int64_t)(m >> -sh);
        q = (X24 * 2670177 + ((int64_t)1 << 45)) >> 46;      /* round(|x| * 2/pi): 2/pi in Q22 */
        X40 = sh + 16 >= 0 ? (int64_t)m << (sh + 16) : (int64_t)(m >> -(sh + 16));
        r = X40 - q * 1727108826179LL;          /* pi/2 in Q40 */
    }
    *quad = (int32_t)q;
    *rem = (int32_t)(r >> 10);
    return 1;
}

float __slow_sinf(float x);
float __slow_cosf(float x);

float sinf(float x)
{
    fm_calls[4]++;
    int32_t q, r, v;
    int k = reduce(x, &q, &r);
    F f;
    if (!k)
        return __slow_sinf(x);
    if (k == 2)
        return x;
    v = (q & 1) ? cos_q30(r) : sin_q30(r);
    if (q & 2)
        v = -v;
    f.f = x;
    if (f.u >> 31)
        v = -v;                                 /* sin is odd */
    return q30_to_f(v);
}

float cosf(float x)
{
    fm_calls[5]++;
    int32_t q, r, v;
    int k = reduce(x, &q, &r);
    if (!k)
        return __slow_cosf(x);
    if (k == 2)
        return 1.0f;
    v = (q & 1) ? -sin_q30(r) : cos_q30(r);
    if (q & 2)
        v = -v;
    return q30_to_f(v);
}
