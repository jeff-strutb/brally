/* br_hudtimes.c -- drawing: the lap and split times on the HUD.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice5_63.c, an address batch and not a module.  Which two
 * times are drawn depends on the game mode, and two of the seven modes draw
 * nothing.  The caption drawer this calls, 0x10014760, is in br_text.c.
 *
 * The two race-field accessors below are file-static and had no other user
 * in slice5_63.c, so they come with it.
 *
 * slice5_63.c's preamble is carried over verbatim.  An include set that
 * looks redundant has already been shown elsewhere in this module to move
 * VC5's register allocation (see br_rdpmode.c).
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


/* XSLICE 0x106C65E4 -- slice2_18.h's name for the hi-res scale. */
extern int32_t BrG_6C65E4;

/* XSLICE 0x106C2CF8 -- slice2_18.h's name. Deliberately the RAW pointer and
 * not slice2_15.h's BrRace *, because BrRace is documented there as "logical,
 * not byte-exact" and the three fields 0x10017290 needs (+0xFB0, +0xFE4,
 * +0xFEC) are not in it. */
extern void *BrG_6C2CF8;

/* XSLICE 0x10019290 -- slice2_15.h's name. */
extern void BrSub_10019290(void);


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
