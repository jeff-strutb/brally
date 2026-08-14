/* br_crt.c -- host-CRT stand-ins for the original's statically linked CRT.
 * See br_crt.h. Deliberately NOT decompiled.
 */
#include "br_crt.h"

#include <math.h>
#include <stdlib.h>

void *BrOperatorNew(uint32_t cb)
{
    /* malloc, not calloc: the original does not zero. A previous round had one
     * agent identify this as calloc, which would have silently handed callers
     * zeroed memory the original never provides. */
    if (cb == 0)
        cb = 1;                     /* _nh_malloc clamps 0 -> 1 */
    return malloc((size_t)cb);
}

void BrOperatorDelete(void *p)
{
    free(p);
}

int32_t BrFtolTrunc(float f)
{
    double d = (double)f;

    /* __ftol keeps the low dword of a 64-bit fistp; out of range that is the
     * indefinite value 0x80000000, NOT a saturation to INT32_MAX/MIN. */
    if (!(d >= -2147483648.0 && d <= 2147483647.0))
        return (int32_t)0x80000000;
    return (int32_t)d;
}
