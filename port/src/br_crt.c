/* br_crt.c -- host-CRT stand-ins for the original's statically linked CRT.
 * See br_crt.h. Deliberately NOT decompiled.
 */
#include "br_crt.h"

#include <math.h>
#include <stdlib.h>

void *BrOperatorNew(uint32_t cb)
{
    /* malloc, not calloc: the original does not zero. A previous round had one
     * pass identify this as calloc, which would have silently handed callers
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

    /* __ftol keeps the LOW dword of a 64-bit fistp. Out of range the x87
     * stores the 64-bit indefinite 0x8000000000000000, whose low dword is 0 --
     * so the return is 0, not 0x80000000 and not a saturation. Verified
     * against 0x1007C8BF. NaN takes this path too. */
    if (!(d >= -2147483648.0 && d <= 2147483647.0))
        return 0;
    return (int32_t)d;
}
