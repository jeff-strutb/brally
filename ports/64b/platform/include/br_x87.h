/* br_x87.h: the x87 idioms the game spelled as inline asm, in C.
 *
 * `fld x; fistp n` with the control word the game never changes rounds to
 * nearest, ties to even: exactly lrint in the default rounding mode. */
#ifndef BR_X87_H
#define BR_X87_H
#include <math.h>
#include <stdint.h>
static inline int32_t br_fistp(double x) { return (int32_t)lrint(x); }
#endif
