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
/* @implements 0x1006D180 glide BrCountedTotal */
/* The count load sits BETWEEN the flag test and its jump because both return
 * paths need it: spelled as two returns, VC5 hoists the common load there and
 * reuses EAX for flag then count. The temp-and-increment form put the flag in
 * EDX instead. */
int BR_THISCALL1 BrCountedTotal(const BrCounted *pObj)
{
    if (pObj->flag != 0)
        return pObj->count + 1;
    return pObj->count;
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: return the int at offset +0x10 in a state object (fastcall). */
/* @implements 0x1006D190 glide BrStateGetField10 */

int __fastcall BrStateGetField10(int param_1)

{
  return *(int *)(param_1 + 0x10);
}

#endif /* BR_MATCHING_BUILD */
