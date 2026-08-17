/* br_state.c -- see br_state.h. */
#include "br_state.h"

int BrIsAnyActive(const BrActiveFlags *p)
{
    if (p->a0) return 1;
    if (p->a1) return 1;
    if (p->a2) return 1;
    if (p->a3) return 1;
    /* Inverted on purpose -- see the header. Checks after this are only
     * reached when the override is clear. */
    if (p->override) return 0;
    if (p->a5) return 1;
    if (p->a6) return 1;
    if (p->a7) return 1;
    if (p->a8) return 1;
    return 0;
}

/* 0x10073F40 -- reads count first, then conditionally increments, so a set
 * flag adds exactly one. */
/* WHAT IT DOES: adds up a running count plus one for a flag, giving the
 * total including whatever is pending. The count is read before the flag is
 * examined, so a set flag adds exactly one. */
/* @implements 0x10073F40 d3d BrCountedTotal */
int BrCountedTotal(const BrCounted *pObj)
{
    int n = pObj->count;
    if (pObj->flag != 0)
        n++;
    return n;
}
