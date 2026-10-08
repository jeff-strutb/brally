/* fxmath.c -- the 32.32 number's multiply, divide, square root, sine and cosine.
 * The host builds use 128-bit arithmetic; the GBA's multiply is in fxarm.s.
 * Sine and cosine follow libultra's sinf/cosf: the angle reduced by pi to
 * within a half turn of zero, then the same odd polynomial. */
#include "fx.h"
#ifndef FX_FLOAT

#if !defined(__arm__) || defined(__aarch64__)
fx fx_mul(fx a, fx b)
{
    return (fx)(((__int128)a * b) >> 32);
}

fx fx_div(fx a, fx b)
{
    if (b == 0)
        return a < 0 ? INT64_MIN : INT64_MAX;
    return (fx)(((__int128)a << 32) / b);
}
#else
/* a / b, 32.32: b normalised to 31 bits and a long division of the remainder */
fx fx_div(fx a, fx b)
{
    uint64_t ua, ub, q = 0, r;
    int neg = 0, sh = 0, i;
    if (b == 0)
        return a < 0 ? INT64_MIN : INT64_MAX;
    if (a < 0) { ua = (uint64_t)-a; neg = 1; } else ua = (uint64_t)a;
    if (b < 0) { ub = (uint64_t)-b; neg ^= 1; } else ub = (uint64_t)b;
    /* (ua << 32) / ub, bit by bit over the 96-bit dividend */
    r = 0;
    for (i = 95; i >= 0; i--) {
        uint32_t bit = i >= 32 ? (uint32_t)(ua >> (i - 32)) & 1 : 0;
        r = r << 1 | bit;
        q <<= 1;
        if (r >= ub) {
            r -= ub;
            q |= 1;
        }
    }
    (void)sh;
    return neg ? -(fx)q : (fx)q;
}
#endif

/* sqrt of a 32.32 value: the integer square root of a << 32 (96 bits) */
fx fx_sqrt(fx a)
{
    uint64_t hi, lo, rem = 0, root = 0;
    int i;
    if (a <= 0)
        return 0;
    hi = (uint64_t)a >> 32;              /* a << 32 = hi:lo:0 (96 bits) */
    lo = (uint64_t)a & 0xFFFFFFFFu;
    for (i = 47; i >= 0; i--) {          /* two bits at a time, from the top */
        uint32_t two;
        int bit = i * 2;
        if (bit >= 64)
            two = (uint32_t)(hi >> (bit - 64)) & 3;
        else if (bit >= 32)
            two = (uint32_t)(lo >> (bit - 32)) & 3;
        else
            two = 0;
        rem = rem << 2 | two;
        root <<= 1;
        if (rem >= (root << 1 | 1)) {
            rem -= root << 1 | 1;
            root |= 1;
        }
    }
    return (fx)root;
}

#define PI FX(3.14159265358979323846)
#define RPI FX(0.31830988618379067154)

static fx poly(fx dx)
{
    fx xsq = fx_mul(dx, dx);
    fx p = fx_mul(fx_mul(fx_mul(fx_mul(FX(2.7183114939898219064e-06), xsq) + FX(-1.9839334836096632e-04), xsq)
                         + FX(8.3333298626838951e-03), xsq) + FX(-1.6666656235308897e-01), xsq);
    return dx + fx_mul(dx, p);
}

fx fx_sin(fx x)
{
    fx dn = fx_mul(x, RPI);
    int n = FTOI(dn >= 0 ? dn + FX(0.5) : dn - FX(0.5));     /* ROUND */
    x -= (fx)n * PI;
    return (n & 1) ? -poly(x) : poly(x);
}

fx fx_cos(fx x)
{
    fx dn;
    int n;
    if (x < 0)
        x = -x;
    dn = fx_mul(x, RPI) + FX(0.5);
    n = FTOI(dn + FX(0.5));
    x -= fx_mul(ITOF(n) - FX(0.5), PI);
    return (n & 1) ? -poly(x) : poly(x);
}
#endif
