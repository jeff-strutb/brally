/* br_uiopthook.c -- menus: step the Specular option on to its next setting
 * when the player activates that row (0x100436B0). The other five adapters in
 * the family are one-line port glue with no original body of their own and
 * stayed in slice7_80.c.
 *
 * Filed out of slice7_80.c, whose preamble it keeps verbatim below so the
 * compiler's view of the body is unchanged.  The original banner follows.
 *
 * slice7_80.c -- the option-changing control hooks.  See slice7_80.h for what
 * this module is, why all six bodies live in slice2_25.c, and why the seam at
 * the bottom writes one original word into three host objects.
 *
 * NOTHING IS DECOMPILED IN THIS FILE.  Every hook is a one-line adapter over
 * an existing, verified body; the rest is installation, the input seam, and
 * read-only observation.  If a body ever turns out to be wrong, it is
 * slice2_25.c that is wrong -- this file cannot hide it and must not acquire
 * a second opinion.
 */
#include "slice7_80.h"

/* XSLICE port/include/slice2_25.h:576-577, 599-602 -- the body the port arm
 * adapts over.  Copied verbatim from slice7_80.c; `int (void)` is not a
 * mistake, the original never reads its stack argument. */
extern int BrOptCycleAA2A24(void);   /* 0x100436B0 */

/* WHAT IT DOES: steps the Specular option on to its next setting when the
 * player activates that row. */
/* @implements 0x100436B0 d3d BrUiOptHook_100436B0 */
#ifdef BR_MATCHING_BUILD
/* Literal: the up/down cycling of the 0..1 option index is inlined here
 * (the port routes it through BrOptCycleAA2A24 / BrOptCycle), and the
 * chosen table entry lands in 0x10B7153C. */
extern int DAT_10ac6734;
extern int DAT_10ac6730;
extern int DAT_10ac5d7c;
extern int DAT_100abce0[];
extern int DAT_10b7153c;
int32_t BrUiOptHook_100436B0(BrUiCtl_ *pCtl)
{int v;  int s5;
if (DAT_10ac6734) {
        s5 = DAT_10ac5d7c; v = s5 + 1;
        DAT_10ac5d7c = v;
        if (v > 1) {
            DAT_10ac5d7c = 0;
        }
    }
    else if (DAT_10ac6730) {
        v = DAT_10ac5d7c;
        v = v - 1;
        DAT_10ac5d7c = v;
        if (v < 0) {
            DAT_10ac5d7c = 1;
        }
    }v = DAT_10ac5d7c;DAT_10b7153c = DAT_100abce0[v];return 1;}
#else
int32_t BrUiOptHook_100436B0(BrUiCtl_ *pCtl)
{
    (void)pCtl;
    return (int32_t)BrOptCycleAA2A24();
}
#endif
