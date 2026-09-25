/* br_dikedge.c -- controls: keyboard press-edge detection.
 *
 * BrMenuSub1005FF60 (glide 0x10058FD0) turns the polled DirectInput key
 * state into per-key "pressed this frame" edges: a key flags once, on the
 * frame it goes down, not every frame it is held.
 *
 * Filed out of the address batch slice3_39.c, with that file's preamble.
 */


#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <string.h>

#include "slice1_07.h"   /* BrDevSlot -- see the note in slice3_39.h */
#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl; matching needs thiscall.  Rename the cdecl
 * declaration so the definition below can wear a different convention. */
#define BrTextBoxDeleteDtor BrTextBoxDeleteDtor_cdecl
#define BrTextBoxMeasureA  BrTextBoxMeasureA_cdecl
#define BrTextBoxMeasureB  BrTextBoxMeasureB_cdecl
#endif
#ifdef BR_MATCHING_BUILD
#define BrTextBoxInit BrTextBoxInit_port
#include "slice3_39.h"
#undef BrTextBoxInit
#else
#include "slice3_39.h"
#endif
#ifdef BR_MATCHING_BUILD
#undef BrTextBoxDeleteDtor
#undef BrTextBoxMeasureA
#undef BrTextBoxMeasureB
#endif

/* WHAT IT DOES: works out which keys were pressed this frame as opposed to
 * merely being held down. For each key it records whether it is down now and
 * flags it only if it was up on the previous frame -- so a held key
 * registers once, not every frame. */
/* @t4-pass 0x10058FD0 1 2026-09-10 probes 57 bytes 66 insns 17 regions 1 rows 2 census yes  (tools/crank.py) */
/* @t4-pass 0x10058FD0 2 2026-09-10 probes 57 bytes 66 insns 17 regions 1 rows 2 census yes  (tools/crank.py) */
/* @t3 0x10058FD0 2026-09-10 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 66/67 insns 17/17 rows 1+1 regions 1 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * residue after tools/crank.py: 57 compiles this pass, levers accepted: none;
 * every candidate and score is in build/match/crank.log.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x1005FF60 d3d BrMenuSub1005FF60 */
void BrMenuSub1005FF60(void)
{
    int i;

    for (i = 0; i < BR_DIK_COUNT; ++i) {
        /* The edge slot IS the notPrev temporary: the original stores the
         * zero-test into g_BrDikEdge[i] and reloads it to AND the down bit in
         * -- a store, a reload and a second store where a local would have
         * kept the value in a register.  Spelling it with a local came out
         * two instructions short (15 against 17); this is the shape. */
        int32_t down;

        g_BrDikEdge[i] = (g_BrDikPrev[i] == 0) ? 1 : 0;
        down = (int32_t)((g_BrDikState[i] >> 7) & 1u);
        g_BrDikPrev[i] = down;
        g_BrDikEdge[i] &= down;
    }
}
