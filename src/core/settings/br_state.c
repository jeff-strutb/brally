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
/* CLOSE, NOT MATCHING.  Orig: MOV EAX,[ECX+8] / TEST EAX,EAX / MOV EAX,[ECX+0Ch]
 * / JZ / INC / RET.  Compiler always puts flag in EDX and count in EAX, issuing
 * both loads before the TEST.  No source form changes the register assignment;
 * this is a register allocation wall, not a source order issue. */
int BR_THISCALL1 BrCountedTotal(const BrCounted *pObj)
{
    int n = pObj->count;
    if (pObj->flag != 0)
        n++;
    return n;
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
