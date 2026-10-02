/* br_state.c -- see br_state.h. */
#include "br_state.h"

/* (port-only BrIsAnyActive removed) */


/* 0x10073F40 -- reads count first, then conditionally increments, so a set
 * flag adds exactly one. */
/* WHAT IT DOES: adds up a running count plus one for a flag, giving the
 * total including whatever is pending. The count is read before the flag is
 * examined, so a set flag adds exactly one. */
/* @implements 0x1006D180 glide BrCountedTotal */
/* @n64 0x80257A80 located */
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

/* -- Ghidra-matched functions --------------------------- */
/* WHAT IT DOES: return the int at offset +0x10 in a state object (fastcall). */
/* @implements 0x1006D190 glide BrStateGetField10 */

int __fastcall BrStateGetField10(char * param_1)

{
  return *(int *)(param_1 + 0x10);
}

