/* br_phasecur.c -- storage for the current-phase slot, 0x10AA2904.
 *
 * See port/include/br_phasecur.h for why one dword needed a module.
 */
#include "br_phasecur.h"

/* The fallback slot.  In the original this is .bss, so NULL is its initial
 * value and "no phase is current" is a state the game genuinely starts in. */
static BrPhase_ *s_phaseCur;

BrPhase_ **g_ppBrPhaseCur = &s_phaseCur;

/* WHAT IT DOES: tells the front end which single memory location holds "the
 * screen the player is looking at", so that every part of the menu system
 * writes to and reads from the same one. */
void BrPhaseCurBind(BrPhase_ **ppSlot)
{
    g_ppBrPhaseCur = (ppSlot != NULL) ? ppSlot : &s_phaseCur;
}

/* WHAT IT DOES: reports which location is currently in use, so a run can show
 * that the parts of the menu really do share one. */
BrPhase_ **BrPhaseCurSlot(void)
{
    return g_ppBrPhaseCur;
}
