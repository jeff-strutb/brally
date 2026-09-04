/* slice5_63.c -- decompiled from BRD3D.dll, pass-63 packet (slice 5).
 *
 * See slice5_63.h for what is here, what is not, and the gotchas.
 *
 * ---------------------------------------------------------------------------
 * WHY slice1_06.h IS NOT INCLUDED
 * ---------------------------------------------------------------------------
 * This file needs slice2_25.h (the option globals, BrOptObj, BrStrGet,
 * BrSub1003F2B0, the lookup tables) AND slice1_06.h (BrOptSave / BrOptAvailB,
 * which are 0x1003E310 and 0x1003F320 already decompiled). Those two headers
 * cannot coexist: both define `BrDPlayVtbl`, with different contents. That
 * collision predates this packet.
 *
 * The five slice1_06 declarations this file needs are therefore repeated
 * below, VERBATIM from slice1_06.h, behind the guard slice1_06.h itself uses,
 * so that if the collision is ever fixed and slice1_06.h ends up included
 * first this file picks up the real declarations instead. No second
 * implementation of either function is created: the definitions in
 * slice1_06.c are the ones that run.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>
#endif
#include <string.h>

#ifdef BR_MATCHING_BUILD
#define BrExt_1007AC00 BrExt_1007AC00_decl
#endif
#include "slice5_63.h"
#ifdef BR_MATCHING_BUILD
#undef BrExt_1007AC00
#endif

#include "br_crt.h"      /* BrOperatorNew (0x1007DFE0)                       */
#include "slice1_03.h"   /* BrTextGetState, BrHudDrawTimeEntry               */
#include "slice2_25.h"   /* option globals, BrOptObj, BrStrGet, lookup tables */

/* ==========================================================================
 * Borrowed from slice1_06.h -- see the note at the top of the file.
 * ========================================================================== */
#ifndef SLICE1_06_H
#define BR_OPT_CFG_COUNT     6    /* 0x100AC648 + 4*i, contiguous */
#define BR_OPT_SEL_COUNT     7    /* 0x10AA2A00  + 4*i            */
#define BR_OPT_SCRATCH_COUNT 12   /* 0x10B4E710  + 4*i, packed    */

typedef struct BrOptState {
    int32_t aCfg[BR_OPT_CFG_COUNT];
    int32_t aSel[BR_OPT_SEL_COUNT];
} BrOptState;

typedef struct BrOptScratch {
    int32_t a[BR_OPT_SCRATCH_COUNT];
} BrOptScratch;

typedef struct BrOptCaps {
    int32_t  mode;           /* 0x100AA010 */
    int32_t  fForceAvailA;   /* 0x10AA28F8 */
    int32_t  fLowAlwaysB;    /* 0x10AA28FC */
    int32_t  fRebaseB;       /* 0x10AA28F4 */
    int32_t  fLowAlways;     /* 0x10AA28F0 */
    int32_t  fAlt;           /* 0x10AA289C */
    uint32_t maskPair;       /* 0x10AA27E0 (low 16) / 0x10AA27E2 (high 16) */
    uint32_t maskA;          /* 0x10A9D010 */
    uint32_t maskAMode;      /* 0x100AB3EC */
    uint32_t maskB;          /* 0x10AA2598 */
    uint32_t maskBMode6;     /* 0x100AB3E8 */
    int16_t  maskBDefault;   /* 0x100AB3E4 -- SIGN-extended when used */
    int32_t  nAlwaysB;       /* 0x10AD0984 */
} BrOptCaps;

void    BrOptSave(BrOptScratch *pDst, const BrOptState *pSrc);
int32_t BrOptAvailA(const BrOptCaps *pCaps, uint32_t n);
int32_t BrOptAvailB(const BrOptCaps *pCaps, uint32_t n);
#endif /* SLICE1_06_H */

/* ==========================================================================
 * Cross-slice dependencies
 * ========================================================================== */

/* 0x106C0680, the display-list write cursor, is reached through slice2_15's
 * accessor rather than slice2_18's BrG_6C0680 -- slice2_15 is the module that
 * calls 0x10031688 and it must see the same cursor. */

/* XSLICE 0x106C65E4 -- slice2_18.h's name for the hi-res scale. */
extern int32_t BrG_6C65E4;

/* XSLICE 0x106C2CF8 -- slice2_18.h's name. Deliberately the RAW pointer and
 * not slice2_15.h's BrRace *, because BrRace is documented there as "logical,
 * not byte-exact" and the three fields 0x10017290 needs (+0xFB0, +0xFE4,
 * +0xFEC) are not in it. */
extern void *BrG_6C2CF8;

/* XSLICE 0x10019290 -- slice2_15.h's name. */
extern void BrSub_10019290(void);

/* XSLICE 0x100940A4 -- slice2_11.h's name. */
extern int g_brCdEnabled;

/* XSLICE 0x10A9BFDC / the SetTimer hook -- slice4_53.h's names. Declared by
 * hand (not via slice4_53.h) so the BrPlatSetTimerFn typedef is not
 * re-declared; the types are compatible with slice4_53.h's. */
extern uint32_t g_brA9BFDC;
extern uint32_t (*g_pfnBrPlatSetTimer)(void *hWnd, uint32_t idEvent,
                                       uint32_t uElapseMs, void *pfnProc);

/* Callees with no name anywhere in the port yet. Positional names, in the
 * BrSub<ADDR> form slice2_25.h uses. */

/* XSLICE 0x10002870 -- CD play, path A (g_brCdEnabled == 1). */
extern void BrSub10002870(int track);
/* XSLICE 0x100027F0 -- CD play, path B. */
extern void BrSub100027F0(int track);
/* XSLICE 0x1003E3A0 -- the inverse of BrOptSave; restores the twelve pairs. */
extern void BrSub1003E3A0(void);
/* XSLICE 0x1003CC70 -- called with 0x10277B40. */
extern void BrSub1003CC70(void *p);
/* 0x1007A840 is the D3D-only gate in front of 0x1007AC00's one call;
 * BRGlide.dll has no counterpart and the Glide twin 0x10058F90 calls the
 * body unconditionally.  Declared only for the matching build, which diffs
 * against BRD3D.dll. */
#ifdef BR_MATCHING_BUILD
extern int BrSub1007A840(void);
#endif
/* XSLICE 0x1007A940 (Glide 0x10058E20 -- byte-identical, shared.csv `body`) */
extern int BrSub1007A940(void);

/* ==========================================================================
 * Globals this packet owns
 *
 * Every one of these was unnamed everywhere else in the port; see the header.
 * ========================================================================== */
uint8_t     g_br4B0358;                      /* 0x104B0358 */
signed char g_br4B035C;                      /* 0x104B035C -- BrTextState::align */
int         g_br4B0348;                      /* 0x104B0348 -- BrTextState::scale */
int32_t  g_br0BD3EC;                      /* 0x100BD3EC */
int32_t  g_aBrB4E710[BR63_SCRATCH_COUNT]; /* 0x10B4E710 */

int32_t  g_brAA28F8;                      /* 0x10AA28F8 */
int32_t  g_brAA28F4;                      /* 0x10AA28F4 */
int32_t  g_brAA28F0;                      /* 0x10AA28F0 */
uint32_t g_brAA27E0;                      /* 0x10AA27E0 */
uint32_t g_brA9D010;                      /* 0x10A9D010 */
uint32_t g_br0AB3EC;                      /* 0x100AB3EC */
uint32_t g_brAA2598;                      /* 0x10AA2598 */
uint32_t g_br0AB3E8;                      /* 0x100AB3E8 */
int16_t  g_br0AB3E4;                      /* 0x100AB3E4 */

int32_t  g_brAA2A10;                      /* 0x10AA2A10 */
int32_t  g_brAA2A14;                      /* 0x10AA2A14 */

uint8_t  g_aBrAA26F4[4];                  /* 0x10AA26F4 */

int32_t  g_brAA27EC;                      /* 0x10AA27EC */
int32_t  g_brAA27F0;                      /* 0x10AA27F0 */
int32_t  g_brAA27F4;                      /* 0x10AA27F4 */
int32_t  g_brAA27F8;                      /* 0x10AA27F8 */
int32_t  g_brAA28A0;                      /* 0x10AA28A0 */
int32_t  g_brAA28A4;                      /* 0x10AA28A4 */
int32_t  g_brAA28AC;                      /* 0x10AA28AC */
int8_t   g_brAA28B8;                      /* 0x10AA28B8 */
int32_t  g_brAA28C4;                      /* 0x10AA28C4 */

char     g_aBrAA2518[BR63_TEXT_MAX];      /* 0x10AA2518 */
char     g_aBrA9D618[BR63_TEXT_MAX];      /* 0x10A9D618 */
char     g_aBrA9D018[BR63_TEXT_MAX];      /* 0x10A9D018 */

/* ==========================================================================
 * 1. Stubs and thunks
 * ========================================================================== */

/* 0x10075330 -- `c3`. */
/* WHAT IT DOES: nothing. It is called with a real pointer from two places and
 * ignores it -- a single return instruction in the shipped game. This is the
 * original's own emptiness, not a gap in the transcription. */
/* @d3donly 0x10075330 BrGbiCall10075330 -- exists in BRGlide only as folded/duplicated stubs; no unique twin locatable by bytes */
void BrGbiCall10075330(void *pv)
{
    (void)pv;
}

/* 0x10058F90 (Glide) / 0x1007AC00 (D3D)
 *
 * BUILD DIVERGENCE -- A WHOLE GUARD, and the port had the D3D one.
 *
 *     Glide 0x10058F90, 12 bytes, FIVE instructions:
 *         call 0x10058E20 / neg eax / sbb eax,eax / neg eax / ret
 *
 *     D3D   0x1007AC00, 22 bytes:
 *         call 0x1007A840 / test eax,eax / jne +1 / ret
 *         call 0x1007A940 / neg / sbb / neg / ret
 *
 * config/shared.csv pairs 0x1007A940 with 0x10058E20 as `shared`/`body` --
 * byte-identical, so the CALLEE is the same routine in both builds.  The gate
 * is not: 0x1007A840 is class `unknown` with no glide_va, and a scan of
 * BRGlide.dll finds no counterpart of its 244-byte body.  It enumerates
 * display devices (its strings include "%s (Primary)" and it writes the
 * adapter global 0x118AC238), which is exactly the kind of thing the Glide
 * build reaches through glide2x.dll instead of doing itself.
 *
 * So UNDER GLIDE THE BODY ALWAYS RUNS.  The port's `if (... == 0) return;`
 * suppressed it whenever the D3D-only enumerator would have failed -- a gate
 * that does not exist in the reference build, standing in front of the only
 * call that has side effects.  Removed, and BrSub1007A840 with it: nothing
 * else references it and keeping a declaration for a function the reference
 * build does not contain invites it back.
 *
 * The `neg eax / sbb eax,eax / neg eax` tail is `(v != 0)` in both builds and
 * is dropped in both, because the caller declares this void. */
/* WHAT IT DOES: asks one question and throws the answer away, so the only
 * thing it accomplishes is whatever that call does along the way. What the
 * question is has not been established; the purpose is unclear. */
/* @implements 0x10058F90 glide BrExt_1007AC00 */
#ifdef BR_MATCHING_BUILD
/* Glide 0x10058F90 is 12 bytes: CALL 0x10058E20 / NEG EAX / SBB EAX,EAX /
 * NEG EAX / RET.  No BrSub1007A840 gate -- that guard exists only in D3D.
 * `!= 0` keeps EAX alive so VC5 /O2 emits neg/sbb/neg rather than a tail jmp. */
int BrExt_1007AC00(void)
{
    return BrSub1007A940() != 0;
}
#else
void BrExt_1007AC00(void)
{
    /* Kept as a call so the side effects of 0x10058E20 (== D3D 0x1007A940)
     * still happen; the comparison itself is dead.  No 0x1007A840 gate:
     * Glide 0x10058F90 has none. */
    (void)(BrSub1007A940() != 0);
}
#endif

/* ==========================================================================
 * 2. Text / HUD state pokes
 * ========================================================================== */

/* 0x10019260 */
/* @n64 0x8022F514 located */
void BrSub_10019260(void)
{
    g_br4B0358 = 0;
}

/* 0x10019270 */
void BrSub_10019270(void)
{
    BrTextGetState()->align = BR_TEXT_ALIGN_CENTER;   /* 2 */
}

/* 0x10019280 */
/* WHAT IT DOES: switches text drawing back to left-aligned, so writing that
 * follows starts at the position given rather than being centred on it. */
/* @implements 0x10016840 glide BrSub_10019280 */
void BrSub_10019280(void)
{
    g_br4B035C = 0;
}

/* 0x100192F0 */
void BrSub_100192F0(int size)
{
    BrTextGetState()->scale = size;
}

/* ==========================================================================
 * 3. 0x10017290
 * ========================================================================== */

/* String ids the two arms pick between. */
#define BR63_STR_10017290_A  0xE7
#define BR63_STR_10017290_B  0xE8
#define BR63_STR_10017290_C  0xE9

/* Byte offsets into the 0x106C2CF8 block. Raw offsets on purpose -- see the
 * BrG_6C2CF8 note above. */
#define BR63_RACE_COUNT   0x0FA8
#define BR63_RACE_TIME_A  0x0FB0
#define BR63_RACE_TIME_B  0x0FE4
#define BR63_RACE_TIME_C  0x0FEC

/* DEVIATION: the three floats and the count are read with memcpy from a byte
 * pointer rather than through a struct, because no byte-exact struct for the
 * 0x106C2CF8 block exists in the port. */
static float Br63RaceFloat(uint32_t off)
{
    float f;
    memcpy(&f, (const unsigned char *)BrG_6C2CF8 + off, sizeof f);
    return f;
}

static int32_t Br63RaceInt(uint32_t off)
{
    int32_t v;
    memcpy(&v, (const unsigned char *)BrG_6C2CF8 + off, sizeof v);
    return v;
}

/* WHAT IT DOES: draws the lap and split times down the right-hand edge of the
 * screen -- which two times are shown depends on the game mode, and two of
 * the seven modes show nothing at all. In split screen the two lines are
 * drawn on top of one another, because the gap between them is only applied
 * in the full-screen layout. */
/* @implements 0x10017290 d3d BrSub_10017290 */
#ifdef BR_MATCHING_BUILD
/* Orig reads the width, cViews, iView, the mode and the lap bound as
 * STANDALONE ABSOLUTE globals (no BrScreenGet / BrHudGetEnv -- slice5_61.c
 * and slice6_70.c record the same correction), reaches the race block through
 * one pointer global and reads the three times as RAW DWORDS off it, and
 * pushes the split prefix's ADDRESS rather than loading a pointer. The
 * accessors Br63RaceFloat / Br63RaceInt are static with many callers, so MSVC
 * declines to inline them and each becomes a call the original does not have.
 *
 * The times are moved with `mov edx,[ecx+0xFEC]; push edx` -- integer moves,
 * i.e. the dword-pun spelling -- so the draw takes them as a 32-bit word
 * here; the port's float prototype is the same four bytes. */
typedef struct Br63Race {
    char    _a[0x0FA8];
    int32_t cLaps;              /* +0x0FA8 */
    char    _b[0x0FB0 - 0x0FAC];
    uint32_t timeA;             /* +0x0FB0 */
    char    _c[0x0FE4 - 0x0FB4];
    uint32_t timeB;             /* +0x0FE4 */
    char    _d[0x0FEC - 0x0FE8];
    uint32_t timeC;             /* +0x0FEC */
} Br63Race;

extern int32_t   g_br0BCBF4;    /* the gate */
extern int32_t   g_brScreenCx;  /* 0x100A7514 */
extern int32_t   g_brCViews;    /* 0x100AA044 */
extern int32_t   g_brIView;     /* 0x106EC798 */
extern int32_t   g_brHudMode;   /* 0x100A9360 */
extern int32_t   g_brLapBound;  /* 0x100BCBE8 */
extern Br63Race *g_pBr63Race;   /* 0x106E9D88 */
extern char      g_aBr63Prefix[];   /* 0x100A6B80 -- an ARRAY, address pushed */

extern void BrHudDrawTimeEntryW(const char *pszLabel, const char *pszPrefix,
                                uint32_t time, int32_t x, int32_t y);

void BrSub_10017290(BrHudView *aViews)
{
    int32_t x, y, dy;
    uint32_t mode;

    if (g_br0BCBF4 == 0) {
        return;
    }

    x  = g_brScreenCx - 0x10;
    /* `dec/neg/sbb/and 0xFFFFFFE2/add 0x1E`: 0x1E when cViews == 1, else 0. */
    dy = (g_brCViews == 1) ? 0x1E : 0;
    y  = aViews[g_brIView].y + 0x14;

    BrSub_10019260();
    BrSub_10019290();
    BrSub_100192F0(0x0F);

    /* `cmp eax,6 / ja` -- unsigned, so a negative mode also falls out. */
    mode = (uint32_t)g_brHudMode;
    if (mode > 6u) {
        return;
    }

    switch (mode) {
    case 0u: case 1u: case 2u: case 6u:
        if (g_brCViews == 1) {
            BrHudDrawTimeEntryW(BrStrGet(BR63_STR_10017290_A),
                                g_aBr63Prefix, g_pBr63Race->timeC, x, y);
        }
        /* `>=`, not `<`: the original's `jl` leaves the B arm INLINE and
         * jumps to the C arm, so B is the then-branch. */
        if (g_pBr63Race->cLaps >= g_brLapBound) {
            BrHudDrawTimeEntryW(BrStrGet(BR63_STR_10017290_B),
                                g_aBr63Prefix, g_pBr63Race->timeB, x, y + dy);
        } else {
            BrHudDrawTimeEntryW(BrStrGet(BR63_STR_10017290_C),
                                g_aBr63Prefix, g_pBr63Race->timeA, x, y + dy);
        }
        break;

    case 3u:
        if (g_brCViews == 1) {
            BrHudDrawTimeEntryW(BrStrGet(BR63_STR_10017290_B),
                                g_aBr63Prefix, g_pBr63Race->timeB, x, y);
        }
        BrHudDrawTimeEntryW(BrStrGet(BR63_STR_10017290_C),
                            g_aBr63Prefix, g_pBr63Race->timeA, x, y + dy);
        break;

    default:
        /* modes 4 and 5: the jump table sends both straight to the epilogue */
        break;
    }
}
#else
void BrSub_10017290(BrHudView *aViews)
{
    const BrScreenInfo *pScr;
    const BrHudEnv     *pEnv;
    int32_t x, y, dy;
    uint32_t mode;

    if (g_br0BD3EC == 0) {
        return;
    }

    pScr = BrScreenGet();
    pEnv = BrHudGetEnv();

    x  = pScr->cx - 0x10;
    /* `dec/neg/sbb/and 0xFFFFFFE2/add 0x1E`: 0x1E when cViews == 1, else 0. */
    dy = (pScr->cViews == 1) ? 0x1E : 0;
    y  = aViews[pScr->iView].y + 0x14;

    BrSub_10019260();
    BrSub_10019290();
    BrSub_100192F0(0x0F);

    /* `cmp eax,6 / ja` -- unsigned, so a negative mode also falls out. */
    mode = (uint32_t)g_br0AA010;
    if (mode > 6u) {
        return;
    }

    switch (mode) {
    case 0u: case 1u: case 2u: case 6u:
        if (pScr->cViews == 1) {
            BrHudDrawTimeEntry(BrStrGet(BR63_STR_10017290_A),
                               pEnv->pszSplitPrefix,
                               Br63RaceFloat(BR63_RACE_TIME_C), x, y);
        }
        if (Br63RaceInt(BR63_RACE_COUNT) < g_br0BD3E0) {
            BrHudDrawTimeEntry(BrStrGet(BR63_STR_10017290_C),
                               pEnv->pszSplitPrefix,
                               Br63RaceFloat(BR63_RACE_TIME_A), x, y + dy);
        } else {
            BrHudDrawTimeEntry(BrStrGet(BR63_STR_10017290_B),
                               pEnv->pszSplitPrefix,
                               Br63RaceFloat(BR63_RACE_TIME_B), x, y + dy);
        }
        break;

    case 3u:
        if (pScr->cViews == 1) {
            BrHudDrawTimeEntry(BrStrGet(BR63_STR_10017290_B),
                               pEnv->pszSplitPrefix,
                               Br63RaceFloat(BR63_RACE_TIME_B), x, y);
        }
        BrHudDrawTimeEntry(BrStrGet(BR63_STR_10017290_C),
                           pEnv->pszSplitPrefix,
                           Br63RaceFloat(BR63_RACE_TIME_A), x, y + dy);
        break;

    default:
        /* modes 4 and 5: the jump table sends both straight to the epilogue */
        break;
    }
}
#endif

/* ==========================================================================
 * 4. 0x10031688
 * ========================================================================== */

/* One 8-byte command. The original reads the cursor, writes, and bumps by 8. */
static BrGfxCmd *Br63GfxAlloc(void)
{
    BrGfxOut *pOut = BrGfxGetOut();
    BrGfxCmd *pCmd = pOut->pCur;
    pOut->pCur = pCmd + 1;
    return pCmd;
}

static void Br63GfxEmit(uint32_t w0, uint32_t w1)
{
    BrGfxCmd *pCmd = Br63GfxAlloc();
    pCmd->w0 = w0;
    pCmd->w1 = w1;
}

void BrSub_10031688(int32_t x, int32_t y, int32_t w, int32_t h,
                    int32_t c0, int32_t c1, int32_t c2)
{
    uint32_t colour, lr, ul;

    if (BrG_6C65E4 != 0) {
        /* DEVIATION: shifted through uint32_t -- `<<` on a negative signed
         * value is undefined in C99, and these four are signed in the
         * original's declaration. The bit pattern is identical. */
        x = (int32_t)((uint32_t)x << 1);
        y = (int32_t)((uint32_t)y << 1);
        w = (int32_t)((uint32_t)w << 1);
        h = (int32_t)((uint32_t)h << 1);
    }

    /* `mov word ptr [ebp-4], cx`: only 16 bits are kept, and both later reads
     * mask with 0xFFFF, so the discarded upper half never matters. */
    colour = (((uint32_t)c0 << 8) & 0xF800u)
           | (((uint32_t)c1 << 3) & 0x07C0u)
           | ((uint32_t)(c2 >> 2) & 0x003Eu)   /* sar: ARITHMETIC shift */
           | 1u;
    colour &= 0xFFFFu;

    Br63GfxEmit(0xE7000000u, 0x00000000u);
    Br63GfxEmit(0xB900031Du, 0x0F0A4000u);
    Br63GfxEmit(0xBA001402u, 0x00300000u);
    Br63GfxEmit(0xF7000000u, colour | (colour << 16));

    /* GOTCHA (see the header): the lower-right corner is scaled a SECOND time
     * by BrG_6C65E4 while the upper-left corner is not scaled at all here.
     * `shl reg, cl` on x86 masks the count to 5 bits; done explicitly because
     * a shift of >= 32 is undefined in C. */
    {
        unsigned sh = (unsigned)BrG_6C65E4 & 31u;
        uint32_t lrx = ((((uint32_t)x + (uint32_t)w) << sh) - 1u) & 0xFFFu;
        uint32_t lry = ((((uint32_t)y + (uint32_t)h) << sh) - 1u) & 0xFFFu;
        lr = 0xE1000000u | (lrx << 12) | lry;
        ul = (((uint32_t)x & 0xFFFu) << 12) | ((uint32_t)y & 0xFFFu);
    }
    Br63GfxEmit(lr, ul);

    Br63GfxEmit(0xE7000000u, 0x00000000u);
    Br63GfxEmit(0xBA001402u, 0x00000000u);
}

/* ==========================================================================
 * 5. 0x10074090 -- quaternion product
 * ========================================================================== */

/* DEVIATION: the original evaluates on the x87 stack, which under MSVC's
 * default control word carries 53-bit (double) intermediates and rounds to
 * float only at the four fstp instructions. The intermediates below are
 * therefore `double` and only the stores narrow, which is closer to the
 * original than float intermediates would be.
 *
 * The evaluation ORDER is the original's, not the textbook one: the packet
 * accumulates a2*b0 + a3*b1 first, and every term is added or subtracted in
 * the sequence the fxch/faddp/fsubp chain dictates. */
void BrSub10074090(BrVec4 *pDst, const BrVec4 *pA, const BrVec4 *pB)
{
    const double a0 = (double)pA->f00, a1 = (double)pA->f04;
    const double a2 = (double)pA->f08, a3 = (double)pA->f0C;
    const double b0 = (double)pB->f00, b1 = (double)pB->f04;
    const double b2 = (double)pB->f08, b3 = (double)pB->f0C;

    /* All eight components are read above, before any store: pDst may alias
     * either input, exactly as the original allows. */
    const double r0 = ((a0 * b0 - a1 * b1) - a2 * b2) - a3 * b3;
    const double r1 = ((a1 * b0 + a0 * b1) - a3 * b2) + a2 * b3;
    const double r2 = ((a2 * b0 + a3 * b1) + a0 * b2) - a1 * b3;
    const double r3 = ((a3 * b0 - a2 * b1) + a1 * b2) + a0 * b3;

    /* Stored in the original's order: +0x0C, +0x08, +0x04, +0x00. */
    pDst->f0C = (float)r3;
    pDst->f08 = (float)r2;
    pDst->f04 = (float)r1;
    pDst->f00 = (float)r0;
}

/* ==========================================================================
 * 6. The options block
 * ========================================================================== */

/* 0x1003E310 -- gather, then hand off to slice1_06's BrOptSave. The interleave
 * is BrOptSave's; it is not repeated here.
 *
 * aSel[1] (0x10AA2A04) is never read by BrOptSave and never written by the
 * original, so it is zeroed rather than sourced. */
void BrSub1003E310(void)
{
    BrOptState   st;
    BrOptScratch scratch;

    st.aCfg[0] = g_br0AC648;
    st.aCfg[1] = g_br0AC64C;
    st.aCfg[2] = g_br0AC650;
    st.aCfg[3] = g_br0AC654;
    st.aCfg[4] = g_br0AC658;
    st.aCfg[5] = g_br0AC65C;

    st.aSel[0] = g_brAA2A00;
    st.aSel[1] = 0;
    st.aSel[2] = g_brAA2A08;
    st.aSel[3] = g_brAA2A0C;
    st.aSel[4] = g_brAA2A10;
    st.aSel[5] = g_brAA2A14;
    st.aSel[6] = g_brAA2A18;

    BrOptSave(&scratch, &st);
    memcpy(g_aBrB4E710, scratch.a, sizeof g_aBrB4E710);
}

/* The BrOptCaps slice1_06 wants, assembled from the loose globals. */
/* WHAT IT DOES: collects the scattered "what is available in this game mode"
 * settings into one bundle, so the routine that decides whether a given menu
 * choice can be picked has them all in one place. NOTE FOR ANYONE CHECKING
 * THE TRANSCRIPTION: the address claimed on the line below does not match
 * this body -- that address is the replay forwarder, transcribed elsewhere in
 * this tree -- so treat the claim, not the code, as the thing to verify. */
static void Br63CapsGather(BrOptCaps *pCaps)
{
    pCaps->mode         = g_br0AA010;
    pCaps->fForceAvailA = g_brAA28F8;
    pCaps->fLowAlwaysB  = g_brAA28FC;
    pCaps->fRebaseB     = g_brAA28F4;
    pCaps->fLowAlways   = g_brAA28F0;
    pCaps->fAlt         = g_brAA289C;
    pCaps->maskPair     = g_brAA27E0;
    pCaps->maskA        = g_brA9D010;
    pCaps->maskAMode    = g_br0AB3EC;
    pCaps->maskB        = g_brAA2598;
    pCaps->maskBMode6   = g_br0AB3E8;
    pCaps->maskBDefault = g_br0AB3E4;
    pCaps->nAlwaysB     = g_brAD0984;
}

/* 0x1003F320 */
int BrSub1003F320(int index)
{
    BrOptCaps caps;
    Br63CapsGather(&caps);
    return (int)BrOptAvailB(&caps, (uint32_t)index);
}

/* 0x1003E510 */

/* `cmp eax,0x1F / jle` -- the wrap bound is inclusive. */
#define BR63_AC654_MAX  0x1F

void BrExt_1003E510(void)
{
    int32_t start, v;

    BrSub1003E3A0();

    g_br094350 = g_br0AC65C;

    if (g_br0AA010 == 6) {
        BrSub10044540();
    }

    /* --- advance 0x100AC654 to the next selectable track ------------------ */
    start = g_br0AC654;
    v     = start;
    if (BrSub1003F320(start) == 0) {
        for (;;) {
            v = g_br0AC654 + 1;
            g_br0AC654 = v;
            if (v > BR63_AC654_MAX) {
                v = 0;
                g_br0AC654 = 0;
            }
            /* Wrapped back onto the index that was already rejected: give up
             * and use it anyway. */
            if (v == start) {
                break;
            }
            if (BrSub1003F320(v) != 0) {
                v = g_br0AC654;
                break;
            }
        }
    } else {
        v = g_br0AC654;
    }

    g_br22B34C = g_aBrAC420[v];
    g_br09435C = g_aBrAC4A0[g_br0AC64C];
    g_br094358 = g_aBrAC4B0[g_br0AC650];
    g_br094354 = g_aBrAC518[g_brAA2A08];

    if (g_br0AA010 == 0) {
        /* index = byte1 + 12 * byte0, into 2-byte records */
        uint32_t idx = (uint32_t)g_aBrAA26F4[1]
                     + 12u * (uint32_t)g_aBrAA26F4[0];
        g_br0B380C = (int32_t)g_aBr0B3820[idx * 2u];
        g_br22B350 = (int32_t)g_aBr0B3820[idx * 2u + 1u];
        BrSub1005FCF0();
        return;
    }

    /* --- advance 0x100AC648 to the next selectable entry ------------------ */
    start = g_br0AC648;
    v     = start;
    if (BrSub1003F2B0(start) == 0) {
        for (;;) {
            int32_t bound;

            v = g_br0AC648 + 1;
            /* Recomputed EVERY step, inside the loop. */
            bound = (g_brAA28FC != 0) ? BR_OPT_AC648_MAX_EXTRA
                                      : BR_OPT_AC648_MAX_BASE;
            g_br0AC648 = v;
            if (v > bound) {
                v = 0;
                g_br0AC648 = 0;
                /* NOT a mistake and NOT symmetric with the 0x100AC654 loop
                 * above: the original `jmp`s PAST the `cmp eax,esi / je` here
                 * (0x1003E5FB -> 0x1003E601), so the wrap itself never counts
                 * as "back where we started".
                 *
                 * CONSEQUENCE (a real hang in the original): if this loop
                 * starts at index 0 and nothing is ever selectable, it never
                 * terminates -- 0 is the only index the wrap can produce and
                 * the give-up test is skipped exactly there. The 0x100AC654
                 * loop does not have this bug because its wrap falls THROUGH
                 * into the give-up test. Preserved deliberately. */
            } else if (v == start) {
                break;      /* wrapped onto the rejected index; use it */
            }
            if (BrSub1003F2B0(v) != 0) {
                v = g_br0AC648;
                break;
            }
        }
    } else {
        v = g_br0AC648;
    }

    g_br0B380C = g_aBrAC4D8[v];
    g_br0BD3E0 = g_br0AC658;
    g_br22B350 = g_aBrAC4C0[g_brAA2A00];
    BrSub1005FCF0();
}

/* ==========================================================================
 * 7. 0x1005FBC0
 * ========================================================================== */

/* The four halfwords summed at the end start 0x1E bytes into the 0x10AA26F0
 * block (0x10AA270E) and step by 8 per index. */
#define BR63_AA270E_OFF     0x1E
#define BR63_AA270E_STRIDE  8
#define BR63_AA270E_TERMS   4

/* WHAT IT DOES: commits the choices the player has been making on the setup
 * screens -- track, opponents, control layout -- into the settings the race
 * itself will read, and builds the two "N of M" strings the screen displays
 * (both counted from one rather than from zero). Its argument only controls
 * whether it also totals up four numbers from a settings table at the end;
 * everything else happens either way. */
/* @implements 0x1005FBC0 d3d BrExt_1005FBC0 */
extern uint8_t  DAT_10ac5a4c, DAT_10ac5a4d;   /* 0x10AA26F4[0], [1] */
extern uint16_t DAT_10ac5b3a;                /* high half of 0x10AA27E0 */
void BrExt_1005FBC0(int32_t a)
{
    int32_t v;

    g_brAA28B8 = (int8_t)DAT_10ac5a4c;
    g_br094354 = g_brAA27EC;
    g_br094358 = g_brAA27F4;
    g_brAA28A4 = (int32_t)DAT_10ac5a4d;     /* movzx: byte 1, zero-extended */
    g_brB4E1D0 = g_brAA27F8;
    g_brAA28A0 = g_aBrAA26F0[0];
    g_br09435C = g_brAA27F0;

    /* `dec/je` three times: 1, 2, 3 select records 1, 2, 3 and EVERYTHING
     * else -- including 0 -- selects record 0. A SWITCH, not an if-else-if
     * chain: the chain compares (`cmp eax,1`) and puts the default last,
     * the switch decrements and puts the default FIRST, which is the
     * original's layout.
     *
     * SOLVED, and the old note here said how: the two bytes ARE separate
     * globals.  Spelled as one array VC5 merged them into a single dword load
     * and took byte 1 out of `ah`; as DAT_10ac5a4c / DAT_10ac5a4d it emits the
     * original's two independent byte loads.  Same story twice more below --
     * the 0x10AA27E0 high half is its own 16-bit global, not a shift, and the
     * halfword base is a POINTER so VC5 folds the array address into the
     * lea's displacement instead of adding it separately.  Together: 71/71
     * instructions and a register-blind gap of 0+0, from 5+4.
     *
     * WHAT IS LEFT is 288 bytes against 289 with RAW 15+15 and REGNORM 0+0 --
     * the instruction multiset is IDENTICAL and only the schedule and the
     * register naming differ, so the one-byte gap is an encoding-length
     * difference from that naming (an accumulator form where the original
     * uses a general register or the reverse).  The visible symptom is at the
     * very top: the original stores the byte third (`al` load, block-base
     * load, byte store) where we hoist a third dword load ahead of the store.
     * T3a -- do not grind. */
    v = g_brAA27F8;
    switch (v) {
    case 1:
        g_brB4E1D4 = &g_aBrB4DF30[1];
        break;
    case 2:
        g_brB4E1D4 = &g_aBrB4DF30[2];
        break;
    case 3:
        g_brB4E1D4 = &g_aBrB4DF30[3];
        break;
    default:
        g_brB4E1D4 = &g_aBrB4DF30[0];
        break;
    }

    /* Both counters are printed PLUS ONE. */
#ifdef BR_MATCHING_BUILD
    /* THIS site calls MSVCRT's imported sprintf (0x118F0570), not the
     * in-DLL BrSprintf at 0x1007C830 -- two calls, so VC5 caches the import
     * pointer in esi and issues `call esi` twice. And the format is the
     * literal, not the `g_pszBr0A73C4` pointer variable: the original pushes
     * the string's address as an immediate, which a pointer read cannot be. */
    sprintf(g_aBrAA2518, "%d", g_brAA28A0 + 1);
    sprintf(g_aBrA9D618, "%d", g_brAA28A4 + 1);
#else
    BrSprintf(g_aBrAA2518, g_pszBr0A73C4, g_brAA28A0 + 1);
    BrSprintf(g_aBrA9D618, g_pszBr0A73C4, g_brAA28A4 + 1);
#endif

    g_brAA28AC = g_brAA28A4;

    if (a != 0) {
        /* movsx: the index byte is SIGNED, so a byte >= 0x80 indexes
         * BACKWARDS off the front of the block. Faithfully reproduced. */
        /* A POINTER, not an int offset: the original folds the block's
         * address into the lea displacement (`lea eax,[eax*8+0x10ac5a66]`),
         * which it can only do if the base is part of the address
         * expression.  An int `base` added to the array afterwards costs a
         * second lea. */
        const unsigned char *pHw = (const unsigned char *)g_aBrAA26F0
                                 + (int32_t)g_brAA28B8 * BR63_AA270E_STRIDE
                                 + BR63_AA270E_OFF;
        uint32_t sum  = 0;
        int      i;

        /* DEVIATION: the halfwords are read with memcpy from a byte view of
         * the 0x10AA26F0 block because 0x10AA270E is not 4-byte aligned, so a
         * uint16_t * into an int32_t array would be misaligned. */
        for (i = 0; i < BR63_AA270E_TERMS; ++i) {
            uint16_t hw;
            memcpy(&hw, pHw + i * (int)sizeof hw, sizeof hw);
            sum += hw;
        }
        g_brAA28C4 = (int32_t)sum;
    }

    /* GOTCHA: 0x10AA2A10 takes the LOW half of the 0x10AA27E0 dword and
     * 0x10AA2A14 the HIGH half; both are OR-ed IN, not assigned.  The HIGH
     * half is read as its OWN 16-bit global (`mov dx, word ptr [0x10ac5b3a]`,
     * i.e. the dword's address + 2), not as a shift of the dword -- a `>> 16`
     * costs a shr and a register copy and misses the `xor`. */
    g_brAA2A10 |= (int32_t)(g_brAA27E0 & 0xFFFFu);
    g_brAA2A14 |= (int32_t)DAT_10ac5b3a;
}

/* ==========================================================================
 * 8. 0x10043E70
 * ========================================================================== */

/* The original's operator-new literal. Kept as documentation only: the
 * allocation below uses BR_PHASE_ALLOC_SIZE, because 0xC8 under-allocates
 * the phase object by 104 bytes on a 64-bit host. */
#define BR63_OPTOBJ_ORIG_SIZE  0xC8

void BrExt_10043E70(int32_t a)
{
    (void)a;   /* pushed by the caller, never read */

    if (g_brPAA2948 == NULL) {
        /* HARDENING (port): BR_PHASE_ALLOC_SIZE, not the 0xC8 literal.
         *
         * This was `BrOperatorNew(BR63_OPTOBJ_SIZE)` with BR63_OPTOBJ_ORIG_SIZE ==
         * 0xC8 -- the original's literal, and precisely the allocation
         * CONVENTIONS.md singles out: "0xC8 under-allocates the phase object
         * by 104 bytes on a 64-bit host". BrOptObjCtor resolves at the host
         * link to slice6_73.c's faithful body, which writes all 304 bytes of
         * a BrPhase_, so this was a 104-byte heap overflow every time the
         * phase at 0x10AA2948 was first built. */
        BrOptObj *p = (BrOptObj *)BrOperatorNew(BR_PHASE_ALLOC_SIZE);

        /* operator new does NOT zero (see CONTRACT); the ctor fills it. */
        p = (p != NULL) ? BrOptObjCtor(p) : NULL;

        g_brPAA2948 = p;
        g_brPAA2904 = p;
        if (p == NULL) {
            return;
        }

        p->pfnEnter = BrOptFn10056FF0;
        /* The original re-reads 0x10AA2948 for both the `this` and the call. */
        g_brPAA2948->pfnEnter(g_brPAA2948);
        g_brPAA2904->f0C = 1;
        g_brPAA2904->f68 = 1;
    } else {
        g_brPAA2904 = g_brPAA2948;
    }

    if (g_brA9CFFC == 0 && g_brA9D000 == 0
        && (g_brAA287C == 0 || g_brAA287C == 1)) {
        BrSub1003C020();
    }
}

/* ==========================================================================
 * 9. 0x1003C1E0 / 0x1003D130
 * ========================================================================== */

void BrSub1003C1E0(void)
{
    int32_t fPost;

    BrSub1003C020();

    /* DEVIATION: USER32's SetTimer goes through slice4_53's hook rather than
     * a second one of this packet's own. Argument order mirrors the original's
     * push order: (hWnd, id, ms, proc). */
    g_brA9BFDC = (g_pfnBrPlatSetTimer != NULL)
               ? g_pfnBrPlatSetTimer(g_brP680584, 1u, 1000u, NULL)
               : 0u;

    /* 0x10AA29D4 is sampled BEFORE 0x10A9CFFC is written. */
    fPost = (g_brPAA29D4 != NULL);
    g_brA9CFFC = 1;
    if (fPost) {
        BrSub1003CC70(g_brP277B40);
    }
    /* The original returns 1; slice2_25 declares this void. */
}

void BrSub1003D130(void *pDesc)
{
    size_t cch = strlen(g_aBrA9D018);

    /* `cmp ecx,1 / jbe`: 0 and 1 both skip the copy. */
    if (cch > 1u) {
        memcpy(pDesc, g_aBrA9D018, cch + 1u);
    }

    /* Always, even when the copy was skipped. */
    {
        int32_t zero = 0;
        memcpy((unsigned char *)pDesc + BR63_DESC_ZERO_OFF, &zero, sizeof zero);
    }
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD

/* WHAT IT DOES: thiscall predicate: 1 if `param_2` is non-NULL, this->+0x10 is non-NULL and
 * all three words of param_2 are <= the corresponding words of this->+0x10 (a bounds-fits
 * test). Spelled as __fastcall with an unused EDX slot (BR_THISCALL1 idiom). */
/* @implements 0x10058CC0 glide BrBoundsFits_10058CC0 */

int __fastcall BrBoundsFits_10058CC0(int param_1,int _edx_unused,int *param_2)
{
  int *piVar1;
  
  if ((((param_2 != (int *)0x0) && (piVar1 = *(int **)(param_1 + 0x10), piVar1 != (int *)0x0)) &&
      (param_2[2] <= piVar1[2])) && ((*param_2 <= *piVar1 && (param_2[1] <= piVar1[1])))) {
    return 1;
  }
  return 0;
}

void BrOperatorDelete(void *p);


#endif /* BR_MATCHING_BUILD */
