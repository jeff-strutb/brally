/* fx.h -- the simulation's number.  Built with FX_FLOAT it is the game's own
 * float, and the expressions are the game's (a double constant is written
 * FXD(c) and promotes as the game's literal did): the transcription then has to
 * give the cartridge's results to the bit (host/simcheck.c).  Otherwise it is
 * 32.32 fixed point in an int64: the GBA's, the same arithmetic without a
 * floating-point unit, finer than a float everywhere the race goes (positions
 * to 2048, forces to some 10^5, tolerances to 10^-5).
 *
 *   FX(c), FXD(c)       a constant (float, double)
 *   FMUL(a, b)          a * b          FDIV(a, b)   a / b (fixed: a reciprocal, multiplies only)
 *   FDIVK(a, c)         a / the constant c (fixed: a times 1 / c)
 *   FSQRT(a)            sqrt           FSIN, FCOS   libultra's sinf, cosf
 *   FTOI(a), ITOF(i)    (int) a, (float) i (truncating toward zero)
 *   FX_STEP(v)          a loop's step stays a step (fixed: the compiler makes no
 *                       divide or modulo of it)
 * Addition, subtraction, negation and comparison are C's own. */
#ifndef FX_H
#define FX_H
#include <stdint.h>

#ifdef FX_FLOAT
typedef float fx;
#define FX(c) ((float)(c))
#define FXD(c) ((double)(c))
#define FMUL(a, b) ((a) * (b))
#define FDIV(a, b) ((a) / (b))
#define FDIVK(a, c) ((a) / FX(c))
#define FTOI(a) ((int)(a))
#define ITOF(i) ((float)(i))
#define FTOF(a) ((float)(a))              /* a double result back to the game's float */
float sqrtf(float);
#define FSQRT(a) sqrtf(a)
float fx_sinf(float);
float fx_cosf(float);
#define FSIN(a) fx_sinf(a)
#define FCOS(a) fx_cosf(a)
#define FX_STEP(v) ((void)0)
#else
typedef int64_t fx;
#define FX_ONE ((int64_t)1 << 32)
#define FX(c) ((fx)((c) * 4294967296.0 + ((c) < 0 ? -0.5 : 0.5)))
#define FXD(c) FX(c)
fx fx_mul(fx a, fx b);
fx fx_div(fx a, fx b);
fx fx_sqrt(fx a);
fx fx_sin(fx a);
fx fx_cos(fx a);
#define FMUL(a, b) fx_mul((a), (b))
#define FDIV(a, b) fx_div((a), (b))
#define FDIVK(a, c) fx_mul((a), FX(1.0 / (c)))   /* by a constant: its reciprocal (no divide) */
#define FTOI(a) ((int)((a) < 0 ? -((-(a)) >> 32) : (a) >> 32))
#define ITOF(i) ((fx)(i) * FX_ONE)
#define FTOF(a) (a)
#define FSQRT(a) fx_sqrt(a)
#define FSIN(a) fx_sin(a)
#define FCOS(a) fx_cos(a)
#define FX_STEP(v) __asm__("" : "+r"(v))
/* fx_mul in line, for ARM code (gba/fxarm.s's, word for word): the product's bits 32..95 */
static inline __attribute__((always_inline)) fx fx_muli(fx a, fx b)
{
    uint32_t al = (uint32_t)a, ah = (uint32_t)((uint64_t)a >> 32);
    uint32_t bl = (uint32_t)b, bh = (uint32_t)((uint64_t)b >> 32);
    uint64_t t = ((uint64_t)al * bl) >> 32;
    uint32_t hi;
    t += (uint64_t)ah * bl;
    t += (uint64_t)al * bh;
    hi = (uint32_t)(t >> 32) + ah * bh;
    if ((int32_t)ah < 0)
        hi -= bl;
    if ((int32_t)bh < 0)
        hi -= al;
    return (fx)(((uint64_t)hi << 32) | (uint32_t)t);
}
#endif

#endif
