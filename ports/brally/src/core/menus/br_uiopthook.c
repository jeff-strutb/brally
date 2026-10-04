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
/* Literal: the up/down cycling of the 0..1 option index is inlined here
 * (the port routes it through BrOptCycleAA2A24 / BrOptCycle), and the
 * chosen table entry lands in 0x10B7153C. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
int32_t BrUiOptHook_100436B0(BrUiCtl_ *pCtl)
{int v;  int s5;
if ((*(int *)&g_act1)) {
        s5 = g_brSel5D7C; v = s5 + 1;
        g_brSel5D7C = v;
        if (v > 1) {
            g_brSel5D7C = 0;
        }
    }
    else if ((*(int *)&g_act0)) {
        v = g_brSel5D7C;
        v = v - 1;
        g_brSel5D7C = v;
        if (v < 0) {
            g_brSel5D7C = 1;
        }
    }v = g_brSel5D7C;(*(int *)&g_BrDrawReflectEnable) = DAT_100abce0[v];return 1;}
