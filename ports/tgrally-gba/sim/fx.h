/* fx.h -- the simulation's number.  Built with FX_FLOAT it is the game's own
 * float, and the expressions are the game's (a double constant is written
 * FXD(c) and promotes as the game's literal did): the transcription then has to
 * give the cartridge's results to the bit (host/simcheck.c).  Otherwise it is
 * 32.32 fixed point in an int64: the GBA's, the same arithmetic without a
 * floating-point unit, finer than a float everywhere the race goes (positions
 * to 2048, forces to some 10^5, tolerances to 10^-5).
 *
 *   FX(c), FXD(c)       a constant (float, double)
 *   FMUL(a, b)          a * b          FDIV(a, b)   a / b
 *   FSQRT(a)            sqrt           FSIN, FCOS   libultra's sinf, cosf
 *   FTOI(a), ITOF(i)    (int) a, (float) i (truncating toward zero)
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
#define FTOI(a) ((int)(a))
#define ITOF(i) ((float)(i))
#define FTOF(a) ((float)(a))              /* a double result back to the game's float */
float sqrtf(float);
#define FSQRT(a) sqrtf(a)
float fx_sinf(float);
float fx_cosf(float);
#define FSIN(a) fx_sinf(a)
#define FCOS(a) fx_cosf(a)
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
#define FTOI(a) ((int)((a) < 0 ? -((-(a)) >> 32) : (a) >> 32))
#define ITOF(i) ((fx)(i) * FX_ONE)
#define FTOF(a) (a)
#define FSQRT(a) fx_sqrt(a)
#define FSIN(a) fx_sin(a)
#define FCOS(a) fx_cos(a)
#endif

#endif
