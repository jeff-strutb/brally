/* slice6_70.c -- Boss Rally (BRD3D.dll), slice 6, packet 70.
 *
 * See slice6_70.h for the packet's four name conflicts, the globals this file
 * owns, and the per-function GOTCHAs. The tail of this file records the
 * DEVIATIONs and the argument for each of the six addresses left out.
 *
 * Every function here was read out of work/slice6/packet70.asm at the address
 * its WANTED name encodes, and carries that address in its comment. All twelve
 * banner/body pairings in the packet were re-checked against the body's own
 * `sub_XXXXXXXX @ XXXXXXXX` line: all twelve agree.
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
 * Storage this module owns. See slice6_70.h for why each one lands here.
 * ========================================================================== */

void    *g_br277B44  = NULL;
int32_t  g_brA9D004  = 0;
int32_t  g_brA9D068  = 0;
int32_t  g_brA9D06C  = 0;
int32_t  g_brAA2A04  = 0;
int32_t  g_brAA28B0  = 0;
int32_t  g_brAA28B4  = 0;
int32_t  g_brAA28BC  = 0;
int32_t  g_brAA28C0  = 0;
int32_t  g_brAA28D0  = 0;
int32_t  g_brAA26E8  = 0;
int32_t  g_br0BD3E8  = 0;
int32_t  g_br0BD3F8  = 0;

int32_t  g_aBrA9DBD8[BR70_AA26F0_COUNT];
uint32_t g_a220B20[BR70_220B20_COUNT];

/* Read out of orig/BRD3D.dll with tools/pe.py, not assumed. 0x100A73C8's two
 * leading percents are a literal '%' for the text renderer's "%y1" colour
 * escape -- it is NOT a printf conversion. */
const char *g_pszBr0A73C8 = "%%y1%s%d/%d";
const char *g_pszBr0A73D4 = "L";

const int32_t *g_pBrRace0FF8 = NULL;

/* Platform hooks. NULL means "not wired"; each call site documents what the
 * port does then. */
int32_t (*g_pfnBrPlatKillTimer)(void *hWnd, uint32_t idEvent) = NULL;
void   *(*g_pfnBrPlatCreateEvent)(void) = NULL;
int32_t (*g_pfnBrDPlayInitConn)(struct BrDPlay *pThis, void *pConnection,
                                uint32_t dwFlags) = NULL;

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
 * 1. 0x1003C020 -- (re)select the DirectPlay service provider
 * ========================================================================== */

/* 0x1003C020 */
void BrSub1003C020(void)
{
    char     szMsg[BR70_C020_MSG];
    void    *pConn = NULL;      /* frame +0x00, zeroed before KillTimer */
    void    *pOut2 = NULL;      /* frame +0x04, written but never read  */
    int32_t  hr;

    /* DEVIATION: USER32 KillTimer behind a hook, following slice4_53's
     * precedent for SetTimer. 0x10A9BFDC is slice4_53's global. */
    if (g_pfnBrPlatKillTimer != NULL) {
        (void)g_pfnBrPlatKillTimer(g_brP680584, g_brA9BFDC);
    }
    BrSub1003C550();

    hr = BrSub1003D480(&pConn, &pOut2);
    if (pConn == NULL) {
        goto report;            /* reports 0x1003D480's HRESULT */
    }

    /* The ADDRESS of the pointer global, not the interface it holds. */
    hr = BrSub1003C520(&g_brP277B40);
    /* Bumped between the call and the test, so failures count too. */
    g_brA9D004++;
    if (hr < 0) {
        goto report;
    }
    if (g_brP277B40 == NULL) {
        /* `hr` is still 0x1003C520's >= 0 result here: the original formats a
         * SUCCESS code into the error string on this path. Preserved. */
        goto report;
    }

    /* IDirectPlay4A::InitializeConnection(pConn, 0), vtable slot +0x98. */
    hr = (g_pfnBrDPlayInitConn != NULL)
       ? g_pfnBrDPlayInitConn(g_brP277B40, pConn, 0u)
       : BR70_HR_OUTOFMEMORY;
    if (hr < 0) {
        goto report;
    }

    /* Modes 2 and 3 skip the whole timer restart and go straight to the event. */
    if (g_brAA287C != 2 && g_brAA287C != 3) {
        if (g_brPAA29D4 != NULL) {
            hr = BrSub1003CC70(g_brP277B40);
            if (hr < 0) {
                goto report;
            }
        }
        /* DEVIATION: SetTimer through slice4_53's hook. Argument order mirrors
         * the original's push order: (hWnd, id, ms, proc). */
        g_brA9BFDC = (g_pfnBrPlatSetTimer != NULL)
                   ? g_pfnBrPlatSetTimer(g_brP680584, 1u, 1000u, NULL)
                   : 0u;
        g_brA9CFFC = 1;
    }

    if (g_br277B44 != NULL) {
        return;
    }
    /* CreateEventA(NULL, FALSE, FALSE, NULL) -- four literal zeroes. */
    g_br277B44 = (g_pfnBrPlatCreateEvent != NULL)
               ? g_pfnBrPlatCreateEvent()
               : NULL;
    if (g_br277B44 != NULL) {
        return;
    }
    hr = BR70_HR_OUTOFMEMORY;

report:
    if (hr != BR70_HR_QUIET) {
        /* GOTCHA: formatted into a 1KB stack buffer and thrown away, exactly
         * as 0x1003C150 and 0x1003C260 do. The call is the observable part. */
        BrSprintf(szMsg,
                  "Could not select service provider because of error 0x%08X",
                  (unsigned int)hr);
    }
}

/* 0x1003C020 -- slice2_26.h's name for the same address (slice2_26.c:138).
 * Both listings in the packet are the one body; forwarding rather than
 * growing a second copy. */
void BrExt_1003C020(void)
{
    BrSub1003C020();
}

/* ==========================================================================
 * 2. 0x1003BF60 -- leave the session
 * ========================================================================== */

/* 0x1003BF60 */
/* WHAT IT DOES: shuts a multiplayer session down: clears the player slots,
 * stops the once-a-second timer, tears down the networking, and -- except in
 * two particular modes -- clears a flag and a byte on the shared object. It
 * finishes by clearing four session flags whatever else happened. */
/* port-only body; Glide match is src/core/generated/0x100355F0.c */
void BrExt_1003BF60(void)
{
    BrSub100586A0();                    /* br_slots.h's BrSlotsReset */

    if (g_pfnBrPlatKillTimer != NULL) {
        (void)g_pfnBrPlatKillTimer(g_brP680584, g_brA9BFDC);
    }
    if (g_brAA2884 != 0) {
        BrSub10072270();
    }
    BrSub1003C550();

    if (g_brAA287C != 2 && g_brAA287C != 3 && g_brPAA29D8 != NULL) {
        /* See CONFLICT 4: the declared type models only +0x1C, the object is
         * an entity record and +0x2B64 is its last dword. Reached byte-wise;
         * both offsets are below any pointer member, so they hold on LP64. */
        unsigned char *p = (unsigned char *)g_brPAA29D8;
        int32_t        flags;

        p[BR70_FLAGOBJ_BYTE] = 0;

        /* The original RE-READS 0x10AA29D8 between the two writes. */
        p = (unsigned char *)g_brPAA29D8;
        memcpy(&flags, p + BR70_FLAGOBJ_FLAGS, sizeof flags);
        flags &= ~(int32_t)BR70_FLAGOBJ_BIT10;
        memcpy(p + BR70_FLAGOBJ_FLAGS, &flags, sizeof flags);
    }

    /* Unconditional, and in this order. */
    g_brA9CFFC = 0;
    g_brAA2884 = 0;
    g_br22AF18 = 0;
    g_brAA2888 = 0;
}

/* 0x1003BF60 -- slice2_25.h's name for the same address. */
void BrSub1003BF60(void)
{
    BrExt_1003BF60();
}

/* ==========================================================================
 * 3. 0x1003E680 -- reset the options / session block
 * ========================================================================== */

/* 0x1003E680 */
void BrExt_1003E680(void)
{
    /* One straight run of stores, in the original's order. The `1`s all come
     * from the same eax the first BrSprintf argument is taken from. */
    g_brA9D068 = 0;
    g_br0AC648 = 2;
    g_brAA2A00 = 0;
    g_brAA2A04 = 0;
    g_brAA2A08 = 0;
    g_br0AC64C = 1;
    g_br0AC650 = 1;
    g_br0AC654 = 1;
    g_br0AC658 = 3;
    g_brAA2A10 = 0;
    g_brAA2A14 = 0;
    g_brAA28A0 = 0;
    g_brAA28A4 = 0;
    g_brAA28AC = 0;
    g_brAA28B0 = 0;
    g_brAA28B4 = 0;
    g_brAA28B8 = 0;             /* a BYTE store in the original */
    g_brAA28BC = 0;
    g_brAA28C0 = 0;
    g_brAA26E8 = 0;
    g_brA9D06C = 0;
    g_brAA28C4 = 0;
    g_brAA28C8 = 0;
    g_brAA28D0 = 0;
    g_brAA289C = 0;

    BrSprintf(g_aBrAA2518, g_pszBr0A73C4, 1);

    /* GOTCHA: 0x10AA28A4 was zeroed sixteen stores ago, so this ALWAYS
     * formats "1". The read is kept because it is what the original does and
     * because any future reordering of the block would change the result. */
    BrSprintf(g_aBrA9D618, g_pszBr0A73C4, (int)(g_brAA28A4 + 1));

    BrSub1003E1D0();

    /* 0x10AA289C is cleared a SECOND time here. */
    g_brAA289C = 0;

    memset(g_aBrAA26F0,  0, sizeof g_aBrAA26F0);    /* rep stosd, ecx = 0x53 */
    memset(g_aBrA9DBD8,  0, sizeof g_aBrA9DBD8);    /* rep stosd, ecx = 0x53 */
    memset(g_a220B20,    0, sizeof g_a220B20);      /* rep stosd, ecx = 0x46 */

    /* A WORD store: only the low half of the dword slice5_63.h models. */
    g_brAA27E0 = (g_brAA27E0 & 0xFFFF0000u) | 0x0102u;

    /* Immediately after the memset that just cleared it. NOT BrInit220B20,
     * which writes 8 here and then calls 0x10035BD1. */
    g_a220B20[0] = 0xFFFFFFFFu;

    BrSub1003E510();
}

/* 0x1003E680 -- slice2_25.h's name for the same address. */
void BrSub1003E680(void)
{
    BrExt_1003E680();
}

/* ==========================================================================
 * 4. 0x1003DB00 -- the tag-7 DirectPlay send
 * ========================================================================== */

/* ==========================================================================
 * 5. 0x1003C150 -- host a session
 * ========================================================================== */

/* 0x1003C150 */
void BrExt_1003C150(void)
{
    /* CONFLICT 1: slice4_50.c:243 already IS this body (the listing in this
     * packet matches it instruction for instruction, including the formatted-
     * and-discarded "Could not host session because of error 0x%08X"). The
     * contract forbids a second copy, so slice2_26.h's name forwards. */
    BrSub1003C150();
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

/* ==========================================================================
 * WHAT IS NOT IN THIS FILE, AND WHY
 * ==========================================================================
 *
 * 0x1004CAC0  BrOptFn1004CAC0 (slice2_25.h:464) -- ALREADY IMPLEMENTED.
 *   slice3_33.c:694 is this exact body, as
 *   `BrExt_1004CAC0(BrUiBuildCtx *, BrUiPhase *)`. Re-decompiling it would
 *   duplicate a body, which the contract forbids.
 *   What the wanted name needs is an ADAPTER, and the adapter cannot be
 *   written from here: slice3_33.c's first parameter is a synthetic bundle of
 *   ~30 globals (BrUiBuildCtx) that nothing constructs anywhere in the port
 *   yet. Manufacturing a second BrUiBuildCtx in this file would give those
 *   thirty addresses a second owner. That is an integration decision.
 *
 *   Note also that the original is a ONE-argument __cdecl: `mov ebp,[esp+1C]`
 *   picks up the only stack argument and `push ecx` at the top is the MSVC
 *   EH-frame slot, not a `this`. So slice2_25.h's one-argument shape is the
 *   right one and slice3_33.h's first parameter is the injected context.
 *
 * 0x1004D1F0, 0x1004DB00, 0x10053CF0, 0x10058750 -- THE FOUR REMAINING
 *   BUILDERS. br_phase.h fixes half of the blocker slice5_60.h described: the
 *   phase object now has one canonical layout with +0x10 / +0x14 / +0x6C, so
 *   `void BrExt_1004D1F0(BrPhase_ *)` is finally a declarable signature.
 *   The OTHER half is untouched. Reading 0x1004D1F0's listing:
 *     - the screen and control objects are slice3_33.h's BrUiScreen /
 *       BrUiCtl, whose vtable slots f34/f38 take a `BrUiPhase *` -- a
 *       DIFFERENT, incompatible model of the same object (apScreen[22] where
 *       br_phase.h has aPages[20] + pCur). Passing a BrPhase_ * through them
 *       needs a cast that is only sound because the pointer is never
 *       field-accessed on the far side, and that soundness argument has to be
 *       made once, centrally, not four times here.
 *     - the four hooks it installs (0x10047360, 0x10045460, 0x10045520,
 *       0x100465E0) are not all in slice3_33.h's BrUiBuildHooks: only
 *       0x10047360 is. Adding the other three means either editing that
 *       header (outside this packet's three files) or standing up a second,
 *       overlapping hook table -- i.e. a fresh owner for addresses
 *       slice3_33.h already claims.
 *   0x10053CF0 (3669 bytes) and 0x10058750 (4109 bytes) have the same shape
 *   and the same blocker, several times over.
 *   ONE merged build-context, the way br_phase.h merged the phase, unblocks
 *   all four at once. Writing them against a private copy of that context
 *   would link cleanly and be silently wrong at every hook install, which is
 *   the outcome the contract rules out.
 *
 * 0x10062C50  BrSub10062C50 (slice3_45.h:261) -- 1909 bytes that initialise a
 *   suspension / wheel model on the entity: four repeats of
 *   {0x10074870(&blockA), 0x10074450(&blockB, &blockA)} at +0x164/+0x370,
 *   +0x788, +0x57C, +0x994, thirteen calls to 0x100746E0 building two linked
 *   chains at +0xBA0..+0xC00 and +0xC20..+0xD00, and roughly 150 individual
 *   dword stores between +0x180 and +0xE94.
 *   The DECLARED parameter type is slice3_45.h's BrEnt, and BrEnt models NONE
 *   of that range -- every one of those offsets falls inside `pad140`,
 *   `pad260`, `pad300` or `pad350`. Only +0x29C4 (pRec) and the two trailing
 *   byte stores at +0xE78/+0xE80 have names, and the record fields it reads
 *   (pRec +0x80EC..+0x80FC) are not modelled either.
 *   Porting it therefore means either extending slice3_45.h (this packet may
 *   create only three files) or writing ~150 raw byte-offset stores into a
 *   struct the header explicitly says is not byte-exact. Both are the
 *   "wrong-but-plausible" outcome. Left out deliberately.
 *
 * ==========================================================================
 * DEVIATIONS, gathered
 * ==========================================================================
 *  1. USER32 KillTimer, USER32 SetTimer and KERNEL32 CreateEventA are behind
 *     hooks (no Win32 in portable code). SetTimer and 0x10A9BFDC are
 *     slice4_53's, reused rather than duplicated; KillTimer and CreateEventA
 *     are new here. An unwired KillTimer skips the call; an unwired
 *     CreateEventA takes the failure path, which is the path that produces
 *     0x8007000E.
 *  2. The IDirectPlay4A vtable slot +0x98 (InitializeConnection) is behind a
 *     hook because slice2_25.h's BrDPlayVtbl models only slots 0..31 and this
 *     header must not redefine it. Unwired, it reports 0x8007000E.
 *  3. BrExt_1003DB00's `void *` payload word is narrowed to 32 bits; the
 *     original writes it into an eight-byte wire payload.
 *  4. BrStrGet's NULL return is replaced with "" at the two places the
 *     original would hand it to sprintf (once as the FORMAT).
 *  5. The race block's +0x0FF8 is injected as `g_pBrRace0FF8` rather than
 *     read through slice2_15.h's BrRace, which has no such field and is
 *     documented as logical-not-byte-exact. NULL reads as 0.
 *  6. `g_aBrA9DBD8` and `g_a220B20` get storage here. slice2_20.c:113 already
 *     externs `uint32_t g_a220B20[0x46]` and defines nothing; the name and
 *     extent are kept identical so the two resolve to one object.
 */

uint8_t *g_brPAA29E4 = NULL;

#ifdef BR_MATCHING_BUILD
typedef struct { int i; } BrC9B0Arg;
typedef struct BrC9B0Vtbl {
    void *pad[11];
    void (__fastcall *f2C)(void *pThis, BrC9B0Arg a);
} BrC9B0Vtbl;
#endif

/* WHAT IT DOES: the same walk as BrSub1003C9B0, over the list object at
 * 0x10AA29D4 instead of 0x10AA29E4. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x1003D070 d3d BrSub1003D070 */
#endif
void BrSub1003D070(void)
{
    unsigned char *p;
    unsigned int   n;
    unsigned int   i;

    p = (unsigned char *)g_brPAA29D4;
    if (p != NULL) {
        n = 0;
        i = 0;
        n = *(unsigned short *)(p + 0x1E164);
        if (n > 0) {
            do {
#ifdef BR_MATCHING_BUILD
                BrC9B0Arg a;
#endif
                unsigned char *pSub;

                pSub = (unsigned char *)g_brPAA29D4 + 0x3838;
#ifdef BR_MATCHING_BUILD
                a.i = (int)i;
                (*(BrC9B0Vtbl **)pSub)->f2C(pSub, a);
#else
                {
                    void **pVtbl = *(void ***)pSub;
                    ((void (*)(void *, unsigned))pVtbl[11])(pSub, i);
                }
#endif
                i++;
            } while (i < n);
        }
    }
}
