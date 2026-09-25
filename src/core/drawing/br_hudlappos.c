/* br_hudlappos.c -- drawing: the race HUD's lap counter and position.
 *
 * BrSub_100173F0 draws the "lap N/M" (or finished) text and the finishing
 * position with its ordinal suffix; Br70Str is its NULL-safe string lookup
 * for the port arm.
 *
 * Filed out of the address batch slice6_70.c, whose preamble is carried
 * verbatim below.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>
#include <string.h>
#ifdef BR_MATCHING_BUILD
#include <stdio.h>
#endif
#include "slice1_03.h"      /* BrComCallLocked68 (0x1000C4D0) */

#include "slice6_70.h"

/* ==========================================================================
 * Constants
 * ========================================================================== */

/* 0x1003C020's frame is 0x408: two dwords of out-parameters at +0x00/+0x04
 * and the message buffer at +0x08. */
#define BR70_C020_MSG        0x400

/* 0x100173F0's frame is 0x104: the measured width at +0x10 and the format
 * buffer at +0x14. */
#define BR70_173F0_BUF       0x100

/* 0x10AA29D8's object -- see CONFLICT 4 in the header. */
#define BR70_FLAGOBJ_FLAGS   0x1Cu     /* `and dword [eax+0x1C], ~0x10` */
#define BR70_FLAGOBJ_BYTE    0x2B64u   /* `mov byte  [eax+0x2B64], 0`   */
#define BR70_FLAGOBJ_BIT10   0x10

/* The HRESULT 0x1003C020 declines to report. */
#define BR70_HR_QUIET        ((int32_t)0x88770118u)
/* E_OUTOFMEMORY -- what the original substitutes for a failed CreateEventA. */
#define BR70_HR_OUTOFMEMORY  ((int32_t)0x8007000Eu)

/* String ids 0x100173F0 looks up. */
#define BR70_STR_LAP_LONG    0xE5   /* lap prefix, non-split screen */
#define BR70_STR_LAP_DONE    0xE6   /* shown once cSplits >= nLaps  */
#define BR70_STR_POS_0       0xB3
#define BR70_STR_POS_1       0xB4
#define BR70_STR_POS_2       0xB5
#define BR70_STR_POS_N       0xB6

/* DEVIATION (both uses): BrStrGet returns NULL for an out-of-range id
 * (slice3_33.h) and the original hands the result straight to sprintf or to a
 * "%s". Substituting "" keeps the call observable without the undefined
 * behaviour; with a populated string table neither substitution can fire. */
static const char *Br70Str(int id)
{
    const char *psz = BrStrGet(id);
    return (psz != NULL) ? psz : "";
}

/* ==========================================================================
 * 6. 0x100173F0 -- lap counter and finishing-position readout
 * ========================================================================== */

/* 0x100173F0 */
/* WHAT IT DOES: draws the two pieces of race text on the heads-up display:
 * which lap you are on -- or a "finished" message once you have done them
 * all -- and, at the bottom, your position with its ordinal suffix. Both are
 * skipped in one particular game mode, and the position can be suppressed
 * separately. Note it takes its horizontal position from the first view
 * rather than from the view being drawn, which is the original's own
 * asymmetry. */
/* RESIDUE (25 masked diffs, T3a, REGNORM 0+0 -- instruction shapes are
 * identical after register normalisation). Both suffix-x expressions
 * associate their three terms differently from the original: it pairs the
 * SPILLED local first (`mov edx,[esp+0x14]; add edx,ebx`), we pair the two
 * registers. DO NOT RE-PROBE the term order -- all six permutations of
 * `w + nudge + x + 3` compile byte-identically; VC5 reassociates integer
 * sums freely, so the pairing is the allocator's, not the source's. */
/* @t4-pass 0x10014960 1 2026-09-09 probes 10 bytes 664 insns 202 regions 2 rows 0 census no  (hand, fn.py variants: literal/order/amp spellings, all inert or worse) */
/* @t4-pass 0x10014960 2 2026-09-09 probes 10 bytes 664 insns 202 regions 2 rows 0 census yes  (hand, fn.py variants: decl orders, comparison flips, format-arg forms, all inert; corpus MISS at +0x220 len 10 -- the association site is proven nowhere) */
/* @t3 0x10014960 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 664/664 insns 202/202 rows 0+0 regions 2 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is the three-term-sum association fork at the two BrTextDraw
 * x-argument sites (proven source-unreachable in the RESIDUE block above:
 * VC5 reassociates the chain unconditionally); size- and insn-exact,
 * identical register-blind multiset.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x100173F0 d3d BrSub_100173F0 */
#ifdef BR_MATCHING_BUILD
/* Orig reads cViews / iView / the race object / the suppress flag as
 * standalone globals (no BrScreenGet / BrHudGetEnv), sprintf via the IAT
 * (CSE'd into ebp), and `switch (pos - 0)` with the zero live in ebx so
 * 3rd-place leaves nudge at 0. Position is a field at +0xFF8 of the same
 * object as cSplits, not a NULL-checked pointer.
 *
 * RESIDUE 25 bytes, T3a, FIRSTDIV +0x220. Size, instruction count and the
 * register-blind multiset are exact (664/664, REGNORM 0+0) and every register
 * holds the same value as the original's; the whole gap is HOW THE THREE-TERM
 * SUM IN EACH BrTextDraw x-argument IS ASSOCIATED, at the two sites below.
 * The original pairs the two non-x terms first and folds x with the +3 into
 * the lea:
 *     add edx,ebx            ; w + nudge          (edx = w, reloaded)
 *     lea eax,[edx+esi+3]    ; + x + 3            (esi = x)
 * and, in the other arm, `add edx,edi` (the /3 quotient + w) then
 * `lea ecx,[edx+esi+3]`. The recompile pairs nudge with x instead
 * (`add ebx,esi` / `lea eax,[ebx+edx+3]`), which is the same value by a
 * different grouping. VC5 reassociates the chain unconditionally, so the
 * SOURCE CANNOT REACH IT: probed and dead, do not re-run -- writing the pair
 * first (already the case at both sites), hoisting `w + nudge` into a named
 * int temp, and the in-place `w += nudge;` form that makes w the natural
 * destination. All three are byte-identical to what is here. */
typedef struct Br70Race {
    char    _a[0xFA8];
    int32_t cSplits;
    char    _b[0xFF8 - 0xFAC];
    int32_t pos;
} Br70Race;
extern int32_t   g_brCViews;     /* 0x100AA044 */
extern int32_t   g_brIView;      /* 0x106EC798 */
extern int32_t   g_brF22AF1C;    /* 0x10226A4C */
extern Br70Race *g_pBrHudRace;   /* 0x106E9D88 */

void BrSub_100173F0(BrHudView *aViews, int a2)
{
    char    szBuf[BR70_173F0_BUF];
    int     w;
    int     x;
    int     y;
    int32_t nudge;
    int32_t pos;
    const char *pszSuffix;

    (void)a2;

    if (g_br0AA010 == 3)
        return;

    x = aViews[0].x + 0x10;

    if (g_br0BD3E8 != 0) {
        if (g_pBrHudRace->cSplits < g_br0BD3E0 || g_brCViews == 1) {
            const char *pszTag;
            y = aViews[g_brIView].y + 5;
            if (g_pBrHudRace->cSplits < g_br0BD3E0) {
                if (g_brCViews == 2)
                    pszTag = "L";
                else
                    pszTag = BrStrGet(BR70_STR_LAP_LONG);
                sprintf(szBuf, "%%y1%s%d/%d", pszTag,
                        g_pBrHudRace->cSplits + 1, g_br0BD3E0);
            } else {
                sprintf(szBuf, BrStrGet(BR70_STR_LAP_DONE));
            }
            BrSub_10019260();
            BrSub_10019280();
            BrSub_100192F0(0xF);
            y += 0xF;
            BrTextDraw(szBuf, x, y);
        }
    }

    if (g_br0BD3F8 == 0)
        return;
    if (g_brF22AF1C != 0)
        return;

    x -= 2;
    y = aViews[g_brIView].y + aViews[g_brIView].h - 0xC;

    BrSub_10019240();
    BrSub_10019280();
    BrTextSetColors(0xFF, 0xF0, 0x7D, 0xFF, 0x78, 0);

    sprintf(szBuf, "%d", g_pBrHudRace->pos + 1);

    nudge = 0;
    pos = g_pBrHudRace->pos;
    switch (pos - nudge) {
    case 0:
        pszSuffix = BrStrGet(BR70_STR_POS_0);
        nudge = -3;
        break;
    case 1:
        pszSuffix = BrStrGet(BR70_STR_POS_1);
        nudge = 1;
        break;
    case 2:
        pszSuffix = BrStrGet(BR70_STR_POS_2);
        break;
    default:
        pszSuffix = BrStrGet(BR70_STR_POS_N);
        nudge = 1;
        break;
    }

    if (g_brCViews == 1) {
        BrSub_100192F0(0x28);
        w = BrSub_100193C0(szBuf, 0x28);
        BrTextDraw(szBuf, x - 1, y - 1);
        BrSub_100192F0(0x14);
        BrTextDraw(pszSuffix, w + nudge + x + 3, y - 0xF);
    } else {
        BrSub_100192F0(0x1A);
        w = BrSub_100193C0(szBuf, 0x1A);
        BrTextDraw(szBuf, x, y);
        BrSub_100192F0(0xD);
        BrTextDraw(pszSuffix, (2 * nudge) / 3 + w + x + 3, y - 0xA);
    }

    BrSub_10019250();
}
#else
void BrSub_100173F0(BrHudView *aViews, int a2)
{
    char          szBuf[BR70_173F0_BUF];
    BrScreenInfo *pScr;
    BrHudEnv     *pEnv;
    int           x;
    int           y;

    (void)a2;   /* pushed by both call sites, never read by the original */

    if (g_br0AA010 == 3) {
        return;
    }

    pScr = BrScreenGet();
    pEnv = BrHudGetEnv();

    /* GOTCHA: view ZERO's x, not the current view's. Only the y/h below are
     * per-view. That asymmetry is the original's. */
    x = aViews[0].x + 0x10;

    /* ---- the lap counter ---------------------------------------------- */
    if (g_br0BD3E8 != 0) {
        int32_t cSplits = pEnv->pRace->cSplits;   /* +0x0FA8 */
        int32_t nLaps   = g_br0BD3E0;

        /* Drawn when the race is unfinished OR the view is full-screen. */
        if (cSplits < nLaps || pScr->cViews == 1) {
            y = aViews[pScr->iView].y + 5;

            if (cSplits < nLaps) {
                /* The bare "L" replaces string 0xE5 ONLY at two views. */
                const char *pszTag = (pScr->cViews == 2)
                                   ? g_pszBr0A73D4
                                   : Br70Str(BR70_STR_LAP_LONG);

                /* Both operands are re-read after the lookup. */
                BrSprintf(szBuf, g_pszBr0A73C8, pszTag,
                          (int)(pEnv->pRace->cSplits + 1), (int)g_br0BD3E0);
            } else {
                /* GOTCHA: the looked-up string is the FORMAT, not an
                 * argument -- a format-string hazard in the original. */
                BrSprintf(szBuf, Br70Str(BR70_STR_LAP_DONE));
            }

            BrSub_10019260();
            BrSub_10019280();
            BrSub_100192F0(0xF);
            y += 0xF;
            BrTextDraw(szBuf, x, y);
        }
    }

    /* ---- the finishing position --------------------------------------- */
    if (g_br0BD3F8 == 0) {
        return;
    }
    if (pEnv->f22AF1C != 0) {          /* 0x1022AF1C suppresses the readout */
        return;
    }

    x -= 2;
    y = aViews[pScr->iView].y + aViews[pScr->iView].h - 0xC;

    BrSub_10019240();
    BrSub_10019280();
    BrTextSetColors(0xFF, 0xF0, 0x7D, 0xFF, 0x78, 0);

    {
        int32_t     pos = (g_pBrRace0FF8 != NULL) ? *g_pBrRace0FF8 : 0;
        const char *pszSuffix;
        int32_t     nudge;
        int         w;

        BrSprintf(szBuf, g_pszBr0A73C4, (int)(pos + 1));

        /* The original re-reads +0x0FF8 for the switch. */
        pos = (g_pBrRace0FF8 != NULL) ? *g_pBrRace0FF8 : 0;

        if (pos == 0) {
            pszSuffix = Br70Str(BR70_STR_POS_0);
            nudge     = -3;
        } else if (pos == 1) {
            pszSuffix = Br70Str(BR70_STR_POS_1);
            nudge     = 1;
        } else if (pos == 2) {
            pszSuffix = Br70Str(BR70_STR_POS_2);
            nudge     = 0;
        } else {
            pszSuffix = Br70Str(BR70_STR_POS_N);
            nudge     = 1;
        }

        if (pScr->cViews == 1) {
            BrSub_100192F0(0x28);
            w = BrSub_100193C0(szBuf, 0x28);
            /* Both coordinates are nudged by one on the full-screen path. */
            BrTextDraw(szBuf, x - 1, y - 1);
            BrSub_100192F0(0x14);
            BrTextDraw(pszSuffix, w + (int)nudge + x + 3, y - 0xF);
        } else {
            BrSub_100192F0(0x1A);
            w = BrSub_100193C0(szBuf, 0x1A);
            BrTextDraw(szBuf, x, y);
            BrSub_100192F0(0xD);
            /* The original's 0x55555556 magic-multiply: (2*nudge)/3 rounded
             * TOWARD ZERO, which C's `/` already is. -3 -> -2, 1 -> 0. */
            BrTextDraw(pszSuffix,
                       (int)((2 * nudge) / 3) + w + x + 3, y - 0xA);
        }
    }

    BrSub_10019250();
}
#endif /* BR_MATCHING_BUILD */
