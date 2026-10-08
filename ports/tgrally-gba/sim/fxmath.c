/* fxmath.c -- the 32.32 number's multiply, divide, square root, sine and cosine.
 * The host builds use 128-bit arithmetic; the GBA's multiply is in fxarm.s.
 * Sine and cosine follow libultra's sinf/cosf: the angle reduced by pi to
 * within a half turn of zero, then the same odd polynomial. */
#include "fx.h"
#ifndef FX_FLOAT

#if !defined(__arm__) || defined(__aarch64__)
/* the host's (the GBA's are gba/fxarm.s) */
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

/* sqrt of a 32.32 value: the integer square root of a << 32 (96 bits) */
fx fx_sqrt(fx a)
{
    unsigned __int128 v, r = 0, bit;
    if (a <= 0)
        return 0;
    v = (unsigned __int128)a << 32;
    bit = (unsigned __int128)1 << 94;
    while (bit > v)
        bit >>= 2;
    while (bit) {
        if (v >= r + bit) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return (fx)r;
}
#endif

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
