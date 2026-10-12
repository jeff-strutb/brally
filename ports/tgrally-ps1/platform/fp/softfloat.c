/* softfloat.c: single-precision arithmetic for the R3000, which has no FPU.
 *
 * The game's code is float throughout; the compiler's runtime (libgcc's
 * soft-fp) does each operation in general code.  These take two normal
 * numbers (and zeros, for multiplication) the short way, rounding to nearest
 * even exactly as IEEE 754 does, so every result is the bit pattern an FPU
 * gives; anything else (denormals, infinities, NaNs, a result that over- or
 * underflows) goes to libgcc's own, kept under the names __gcc_* (link.sh
 * renames them in a copy of libgcc).  Checked against the host's FPU over
 * hundreds of millions of operands (test/softfloat_check.c). */
#include <stdint.h>

typedef union { float f; uint32_t u; } F;

float __gcc_addsf3(float a, float b);
float __gcc_subsf3(float a, float b);
float __gcc_mulsf3(float a, float b);
int __gcc_ltsf2(float a, float b);

#define EXP(u) (((u) >> 23) & 0xFF)
#define NORMAL(e) ((uint32_t)((e) - 1) < 0xFEu)

float __mulsf3(float fa, float fb)
{
    F a, b, r;
    uint32_t ea, eb, ma, mb, m, rem, half, sign;
    int32_t e;
    uint64_t p;
    a.f = fa;
    b.f = fb;
    ea = EXP(a.u);
    eb = EXP(b.u);
    sign = (a.u ^ b.u) & 0x80000000u;
    if (!NORMAL(ea) || !NORMAL(eb)) {
        if (((a.u << 1) == 0 && eb != 0xFF) || ((b.u << 1) == 0 && ea != 0xFF)) {
            r.u = sign;                         /* a zero times a finite number */
            return r.f;
        }
        return __gcc_mulsf3(fa, fb);
    }
    ma = (a.u & 0x7FFFFF) | 0x800000;
    mb = (b.u & 0x7FFFFF) | 0x800000;
    p = (uint64_t)ma * mb;                      /* 2^46 <= p < 2^48 */
    e = (int32_t)(ea + eb) - 127;
    if (p >> 47) {
        m = (uint32_t)(p >> 24);
        rem = (uint32_t)p & 0xFFFFFF;
        half = 0x800000;
        e++;
    } else {
        m = (uint32_t)(p >> 23);
        rem = (uint32_t)p & 0x7FFFFF;
        half = 0x400000;
    }
    if (rem > half || (rem == half && (m & 1)))
        m++;
    if (m >> 24) {
        m >>= 1;
        e++;
    }
    if ((uint32_t)(e - 1) >= 0xFEu)
        return __gcc_mulsf3(fa, fb);            /* overflow or underflow */
    r.u = sign | ((uint32_t)e << 23) | (m & 0x7FFFFF);
    return r.f;
}

/* a + b for normal a, b: |a| >= |b| arranged; 3 guard bits and a sticky one */
static float add_normal(uint32_t a, uint32_t b, float fa, float fb, int sub)
{
    F r;
    uint32_t ea, eb, ma, mb, m, d, g, sign;
    int32_t e;
    if ((a & 0x7FFFFFFF) < (b & 0x7FFFFFFF)) {
        uint32_t t = a;
        a = b;
        b = t;
    }
    ea = EXP(a);
    eb = EXP(b);
    sign = a & 0x80000000u;
    ma = ((a & 0x7FFFFF) | 0x800000) << 3;
    mb = ((b & 0x7FFFFF) | 0x800000) << 3;
    d = ea - eb;
    if (d) {
        if (d >= 27)
            mb = 1;                             /* only the sticky bit is left */
        else
            mb = (mb >> d) | ((mb << (32 - d)) != 0);
    }
    e = (int32_t)ea;
    if (!((a ^ b) & 0x80000000u)) {
        m = ma + mb;
        if (m >> 27) {
            m = (m >> 1) | (m & 1);
            e++;
        }
    } else {
        m = ma - mb;
        if (!m) {
            r.u = 0;                            /* an exact zero is +0 in round to nearest */
            return r.f;
        }
        while (!(m >> 26)) {
            m <<= 1;
            e--;
        }
        if (e <= 0)
            return sub ? __gcc_subsf3(fa, fb) : __gcc_addsf3(fa, fb);
    }
    g = m & 7;
    m >>= 3;
    if (g > 4 || (g == 4 && (m & 1)))
        m++;
    if (m >> 24) {
        m >>= 1;
        e++;
    }
    if (e >= 0xFF)
        return sub ? __gcc_subsf3(fa, fb) : __gcc_addsf3(fa, fb);
    r.u = sign | ((uint32_t)e << 23) | (m & 0x7FFFFF);
    return r.f;
}

float __addsf3(float fa, float fb)
{
    F a, b;
    a.f = fa;
    b.f = fb;
    if (!NORMAL(EXP(a.u)) || !NORMAL(EXP(b.u))) {
        if ((b.u << 1) == 0 && NORMAL(EXP(a.u)))
            return fa;                          /* x + 0 */
        if ((a.u << 1) == 0 && NORMAL(EXP(b.u)))
            return fb;
        return __gcc_addsf3(fa, fb);
    }
    return add_normal(a.u, b.u, fa, fb, 0);
}

float __subsf3(float fa, float fb)
{
    F a, b;
    a.f = fa;
    b.f = fb;
    if (!NORMAL(EXP(a.u)) || !NORMAL(EXP(b.u))) {
        if ((b.u << 1) == 0 && NORMAL(EXP(a.u)))
            return fa;                          /* x - 0 */
        return __gcc_subsf3(fa, fb);
    }
    return add_normal(a.u, b.u ^ 0x80000000u, fa, fb, 1);
}

/* comparisons: libgcc's conventions (lt, le: >0 when unordered; gt, ge: <0) */
static int cmp_ordered(uint32_t a, uint32_t b)
{
    int32_t ia, ib;
    if (((a | b) << 1) == 0)
        return 0;                               /* +0 == -0 */
    ia = (a & 0x80000000u) ? -(int32_t)(a & 0x7FFFFFFF) : (int32_t)a;
    ib = (b & 0x80000000u) ? -(int32_t)(b & 0x7FFFFFFF) : (int32_t)b;
    return ia < ib ? -1 : ia > ib;
}

#define ISNAN(u) (((u) & 0x7FFFFFFF) > 0x7F800000u)

int __ltsf2(float fa, float fb)
{
    F a, b;
    a.f = fa;
    b.f = fb;
    if (ISNAN(a.u) || ISNAN(b.u))
        return 1;
    return cmp_ordered(a.u, b.u);
}
int __lesf2(float fa, float fb) { return __ltsf2(fa, fb); }
int __eqsf2(float fa, float fb) { return __ltsf2(fa, fb); }
int __nesf2(float fa, float fb) { return __ltsf2(fa, fb); }

int __gtsf2(float fa, float fb)
{
    F a, b;
    a.f = fa;
    b.f = fb;
    if (ISNAN(a.u) || ISNAN(b.u))
        return -1;
    return cmp_ordered(a.u, b.u);
}
int __gesf2(float fa, float fb) { return __gtsf2(fa, fb); }

/* float to int: truncation; outside the range or NaN as libgcc gives it */
int __gcc_fixsfsi(float a);
int __fixsfsi(float fa)
{
    F a;
    uint32_t e, m;
    a.f = fa;
    e = EXP(a.u);
    if (e < 127)
        return 0;                               /* |a| < 1 */
    if (e >= 127 + 31)
        return __gcc_fixsfsi(fa);
    m = (a.u & 0x7FFFFF) | 0x800000;
    m = e >= 150 ? m << (e - 150) : m >> (150 - e);
    return (a.u & 0x80000000u) ? -(int)m : (int)m;
}

/* int to float: exact below 2^24, else rounded to nearest even */
float __floatsisf(int i)
{
    F r;
    uint32_t m, sign = 0, e = 150, g;
    if (!i) {
        r.u = 0;
        return r.f;
    }
    if (i < 0) {
        sign = 0x80000000u;
        m = 0u - (uint32_t)i;
    } else {
        m = (uint32_t)i;
    }
    if (m >> 24) {
        int sh = 0;
        uint32_t t = m;
        while (t >> 24) {
            t >>= 1;
            sh++;
        }
        g = m & ((1u << sh) - 1);
        m >>= sh;
        e += (uint32_t)sh;
        {
            uint32_t half = 1u << (sh - 1);
            if (g > half || (g == half && (m & 1)))
                m++;
        }
        if (m >> 24) {
            m >>= 1;
            e++;
        }
    } else {
        while (!(m >> 23)) {
            m <<= 1;
            e--;
        }
    }
    r.u = sign | (e << 23) | (m & 0x7FFFFF);
    return r.f;
}
