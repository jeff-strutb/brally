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

/* @n64 0x80226D7C exact */
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
     * against 0x1007C8BF. NaN takes this path too.
     *
     * THE RANGE IS 64-BIT, NOT 32-BIT, and this file had it wrong. The store
     * at 0x1007C8B9 is `fistp QWORD`, so what overflows is the SIXTY-FOUR bit
     * range. A value between 2^31 and 2^63 -- 3e9, say -- converts perfectly
     * well; the low dword taken from it is simply the WRAPPED 32-bit result.
     * Clamping at 2^31 returned 0 for every such value, where the original
     * returns that wrapped number.
     *
     * The other four copies of this helper (slice1_02, slice2_12, slice2_16,
     * slice2_24) all clamp at 2^63 and were right. This one was the outlier,
     * and no test in the tree covered the out-of-range arm of ANY of the five
     * -- the behaviour was asserted in five comments and zero tests. Found as
     * a by-product of the test-suite mutation sweep. */
    if (!(d >= -9223372036854775808.0) || !(d < 9223372036854775808.0))
        return 0;
    return (int32_t)(int64_t)d;
}
