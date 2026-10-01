/* br_crt.c: the MSVC runtime entry points the core calls by name. */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "br_crt.h"

/* operator new: malloc, never zeroed; _nh_malloc clamps a 0-byte request
 * to 1. */
void *BrOperatorNew(size_t cb)
{
    if (cb == 0)
        cb = 1;
    return malloc(cb);
}

void BrOperatorDelete(void *p)
{
    free(p);
}

/* __ftol: truncate through a 64-bit fistp and keep the low dword. Outside
 * the 64-bit range (and for NaN) the x87 stores the integer indefinite,
 * whose low dword is 0. */
int32_t BrFtolTrunc(float f)
{
    double d = (double)f;

    if (!(d >= -9223372036854775808.0) || !(d < 9223372036854775808.0))
        return 0;
    return (int32_t)(int64_t)d;
}
