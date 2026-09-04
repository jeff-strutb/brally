/* slice4_53.c -- link-closing packet 53.  See slice4_53.h.
 *
 * Nothing in this file re-implements an address that another slice already
 * owns.  Where the wanted name and an existing implementation are the same
 * address, this file contains a forwarder and says so.
 *
 * Constants were read out of orig/BRD3D.dll rather than assumed:
 *   0x100946A8 = "wb"      (so 0x1006A4A0 WRITES; it is a save, not a load)
 *   0x100B5418 = "RCfg"    (the magic, four bytes, no terminator)
 *   0x1008FA64 = 02 00 00 00   (the version dword, emitted verbatim)
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#ifdef BR_MATCHING_BUILD
#define BrCarSub9020 BrCarSub9020_port2
#include "slice4_53.h"
#undef BrCarSub9020
#else
#include "slice4_53.h"
#endif
#include "slice1_03.h"      /* BrComCallLocked68 (0x1000C4D0) */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice2_17.h"      /* BrS17BankFlip, BrRenderCountersReset       */
#include "slice2_18.h"      /* BrGfx2C210, BrGfx31227 declarations        */
#include "slice2_19.h"      /* BrSub10002240, BrSub100088B0, BrSub10037740 */
#include "slice2_20.h"      /* BrPoolEmit, BrRcaLoadCar                   */
#ifdef BR_MATCHING_BUILD
#define BrCarSub9020 BrCarSub9020_port
#include "slice2_21.h"      /* BrSinF, BrSqrtF, BrCarSub9020              */
#undef BrCarSub9020
#else
#include "slice2_21.h"      /* BrSinF, BrSqrtF, BrCarSub9020              */
#endif
#include "slice2_22.h"      /* BrDPlayLink, BrDPlaySendTag4               */
#include "slice2_24.h"      /* BrStringById, BrMenuSub10044B90, ...       */

/* slice2_16.h cannot be included here: it defines a TYPE called BrRcaFixup
 * and slice2_20.h defines a FUNCTION of that name, so the two headers cannot
 * share a translation unit.  This is the one declaration needed from it, and
 * it is copied verbatim. */
/* XSLICE 0x1007CC00 */
extern void BrGbiStackOverflow(int code);

#ifdef _MSC_VER
#pragma function(sin)
#endif

/* 0x10002240 -- slice2_19's name for the very same address. */
float BrSub10002240(float x)
{
    return BrSinF(x);
}

#ifdef _MSC_VER
#pragma function(sqrt)
#endif

/* ======================================================================
 * 2. String table (0x10074030)
 * ====================================================================== */

char *g_apBrStringTable[BR_STRING_ID_LIMIT];

/* @n64 0x8026AC80 located */
char *BrStringById(int32_t id)
{
    /* Both comparisons are unsigned in the original, so this cast is the
     * behaviour, not a convenience: a negative id fails the UPPER test. */
    uint32_t u = (uint32_t)id;

    if (u < (uint32_t)BR_STRING_ID_MIN)
        return NULL;
    if (u >= (uint32_t)BR_STRING_ID_LIMIT)
        return NULL;

    return g_apBrStringTable[u];
}

/* ======================================================================
 * 3. exit() (0x1007CC00)
 * ====================================================================== */

void BrGbiStackOverflow(int code)
{
    /* 0x1007CC00 is `exit`: doexit(code, 0, 0).  It does not return, so
     * neither does this.  slice2_16's slot-9 store after the call is dead
     * code in the original. */
    exit(code);
}

/* ======================================================================
 * 4. Two-slot vtable relay (0x100088B0)
 * ====================================================================== */

void *BrSub100088B0(void *pThis, void *a, void *b)
{
    const BrModelMgrVtbl *pVtbl = *(const BrModelMgrVtbl *const *)pThis;
    void                 *r;

    /* `a` is pushed last and so arrives first. */
    r = pVtbl->pfn0C(pThis, a, b);
    return pVtbl->pfn1C(pThis, r);
}

/* ======================================================================
 * 5. Config writer (0x1006A4A0)
 * ====================================================================== */

/* The emit list, in the original's order.  The four 0xA8 blocks at the end
 * together cover [0, 0x2A0); +0x2A0 goes out just before them; +0x2A4 is
 * never written at all. */
typedef struct BrCfgField {
    uint32_t off;
    uint32_t cb;
} BrCfgField;

static const BrCfgField g_aBrCfgFields[] = {
    { 0x2A8, 0x004 }, { 0x2AC, 0x004 }, { 0x2B0, 0x004 },
    { 0x2B4, 0x104 }, { 0x3B8, 0x400 },
    { 0x7B8, 0x004 }, { 0x7BC, 0x004 }, { 0x7C0, 0x004 }, { 0x7C4, 0x004 },
    { 0x7C8, 0x010 },
    { 0x7D8, 0x004 }, { 0x7DC, 0x004 }, { 0x7E0, 0x004 }, { 0x7E4, 0x004 },
    { 0x7E8, 0x004 }, { 0x7EC, 0x004 }, { 0x7F0, 0x004 }, { 0x7F4, 0x004 },
    { 0x7F8, 0x004 }, { 0x7FC, 0x004 }, { 0x800, 0x004 }, { 0x804, 0x004 },
    { 0x808, 0x004 }, { 0x80C, 0x004 },
    { 0x810, 0x020 }, { 0x830, 0x040 }, { 0x870, 0x004 },
    { 0x2A0, 0x004 },
    { 0x000, 0x0A8 }, { 0x0A8, 0x0A8 }, { 0x150, 0x0A8 }, { 0x1F8, 0x0A8 }
};

#define BR_CFG_FIELD_COUNT \
    ((int)(sizeof g_aBrCfgFields / sizeof g_aBrCfgFields[0]))

#ifdef BR_MATCHING_BUILD
/* ------------------------------------------------------------------
 * 0x100634B0 -- the GLIDE build of the config writer.  thiscall
 * (this in ecx, path on the stack, callee-pops), reached through the
 * proven __fastcall(this, _edx_unused, ...) shim.  Unlike the port
 * body below, the original is UNROLLED: 32 separate checked fwrites,
 * every failure jumping to one shared fclose/return-0.  The magic is
 * written as strlen(global) of a string the file also owns (repne
 * scasb intrinsic), the version dword straight from its global. */
extern char BrGlCfgMagic[];        /* 0x100B4C20  "RCfg" */
extern unsigned char BrGlCfgVersion[4]; /* 0x10077A2C  02 00 00 00 */

/* WHAT IT DOES: write the Glide renderer's settings to a file -- a magic
 * string, a version, then each setting in turn, bailing out on the first
 * short write. Returns zero if it could not open or could not finish, so a
 * half-written file is reported rather than trusted. */
/* @implements 0x100634B0 glide BrGlCfgSave */
int __fastcall BrGlCfgSave(void *pThis, int _edx_unused, const char *pszPath)
{
    unsigned char *pBase = (unsigned char *)pThis;
    FILE          *pFile;

    (void)_edx_unused;
    pFile = fopen(pszPath, "wb");
    if (pFile == NULL)
        return 0;

    if (fwrite(BrGlCfgMagic, strlen(BrGlCfgMagic), 1, pFile) != 1) goto fail;
    if (fwrite(BrGlCfgVersion, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x2A8, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x2AC, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x2B0, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x2B4, 0x104, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x3B8, 0x400, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7B8, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7BC, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7C0, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7C4, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7C8, 0x10, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7D8, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7DC, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7E0, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7E4, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7E8, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7EC, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7F0, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7F4, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7F8, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7FC, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x800, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x804, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x808, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x80C, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x810, 0x20, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x830, 0x40, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x870, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x2A0, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase, 0xA8, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x0A8, 0xA8, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x150, 0xA8, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x1F8, 0xA8, 1, pFile) != 1) goto fail;

    fclose(pFile);
    return 1;
fail:
    fclose(pFile);
    return 0;
}
#endif /* BR_MATCHING_BUILD */

int BrCfgSave1006A4A0(void *pThis, const char *pszPath)
{
    /* 0x1008FA64 holds these four bytes; emitted byte-wise so the file is
     * identical on a big-endian host. */
    static const unsigned char abVersion[4] = { 0x02, 0x00, 0x00, 0x00 };

    const unsigned char *pBase = (const unsigned char *)pThis;
    FILE                *pFile;
    int                  i;

    pFile = fopen(pszPath, "wb");
    if (pFile == NULL)
        return 0;                       /* nothing opened, nothing closed */

    /* strlen("RCfg") == 4; no terminator goes to the file. */
    if (fwrite("RCfg", 4, 1, pFile) != 1)
        goto fail;
    if (fwrite(abVersion, sizeof abVersion, 1, pFile) != 1)
        goto fail;

    for (i = 0; i < BR_CFG_FIELD_COUNT; ++i) {
        if (fwrite(pBase + g_aBrCfgFields[i].off,
                   g_aBrCfgFields[i].cb, 1, pFile) != 1)
            goto fail;
    }

    fclose(pFile);
    return 1;

fail:
    /* Every failure arm in the original jumps to the same fclose. */
    fclose(pFile);
    return 0;
}

void BrSub1006A4A0(void *pThis, void *pArg)
{
    (void)BrCfgSave1006A4A0(pThis, (const char *)pArg);
}

/* ======================================================================
 * 6. Forwarders
 * ====================================================================== */

/* 0x1002C210 */
/* WHAT IT DOES: swaps the two halves of the debug scratch area and clears
 * the one just made current, so the next frame's debug output starts on a
 * clean page while last frame's is still readable. */
void BrGfx2C210(void)
{
    BrS17BankFlip();
}

/* 0x10031227 */
/* WHAT IT DOES: zeroes the eight counters the renderer tallies each frame --
 * how much it drew and how much it skipped -- ready for the next frame. */
void BrGfx31227(void)
{
    BrRenderCountersReset();
}

/* 0x10039020 */
/* WHAT IT DOES: runs a car's particle emitter for one frame: it counts up a
 * timer and, once a quarter of a second has gone by, takes one node off the
 * free list and starts a new particle at the car with a velocity based on
 * how the car is moving. If the pool is empty nothing is spawned. */
/* @implements 0x10039020 d3d BrCarSub9020 */
#ifdef BR_MATCHING_BUILD
/* The original is the full particle-spawn body the port folded into
 * BrPoolEmit: timer accumulate + threshold, free-slot word shuffle,
 * velocity build via the vec helpers, and the slot's colour/life fields
 * from the divided distance.
 * RESIDUE (parked, REGNORM 13+13): the operand-kind wall -- orig puts the
 * e24 field on the fld side of its products (ours ranks the extern consts
 * higher; scalar/array-element/bound-pointer spellings all canonicalize
 * back), orig fcom-before-fstp on the threshold store (assignment-in-
 * condition folds to fst+fcomp), and the esi/edi this-rotation downstream
 * of both. The 0x1000EAF0 per-product kind-ladder probing is the known
 * path if this is ever revisited. */
extern float DAT_1007752c[], DAT_10077530[], DAT_10077534[], DAT_10077538[];
extern float DAT_1007753c[], DAT_10077540[], DAT_10077544[], DAT_10077548[];
extern float DAT_1007754c[], DAT_10077550[], DAT_10077554[];
extern float DAT_106e9d8c[];
extern int   DAT_10ac0c38;
extern unsigned short DAT_10ac0c40;
extern char  DAT_10ac0c48;      /* slot vec A column   */
extern char  DAT_10ac0c54;      /* slot vec B column   */
extern char  DAT_10ac0c60;      /* slot float column   */
extern char  DAT_10ac0c64;      /* slot word column    */
extern char  DAT_10ac0c66;      /* slot byte column    */
extern char  DAT_10ac0c67;      /* slot byte column    */
extern int   BrRandom(void);
extern void  BrSub10034560(void *pDst, void *pA, void *pB);
extern void  BrSub10034660(void *pDst, void *pA, void *pB, float s);
extern float BrSub100347F0(void *p);

void __fastcall BrCarSub9020(struct BrCar *pCar)
{
    char *p = (char *)pCar;
    float *pe24;
    float local[3];
    float acc, f2, t, g;
    unsigned int idx;
    int off;
    unsigned short w40old, wslot;

    pe24 = (float *)(p + 0xE24);
    acc = ((float)(BrRandom() & 0x1FFF) * DAT_1007752c[0]
           - *pe24 * DAT_10077530[0]
           - DAT_10077534[0]) * DAT_106e9d8c[0]
          + *(float *)(p + 0x105C);

    if ((*(float *)(p + 0x105C) = acc) > DAT_10077538[0]) {
        idx = (unsigned int)DAT_10ac0c38 & 0xFFFFu;
        if (idx != 0) {
            f2  = *pe24 * DAT_1007753c[0];
            off = (int)idx << 5;
            *(int *)(p + 0x105C) = 0;

            w40old = DAT_10ac0c40;
            wslot  = *(unsigned short *)(&DAT_10ac0c64 + off);
            DAT_10ac0c40 = (unsigned short)idx;
            *(unsigned short *)&DAT_10ac0c38 = wslot;
            *(unsigned short *)(&DAT_10ac0c64 + off) = w40old;

            BrVec3Scale((BrVec3 *)(void *)(&DAT_10ac0c54 + off),
                        (const BrVec3 *)(const void *)p,
                        DAT_10077540[0] - f2);
            BrSub10034560(local, p + 0xF0, p);
            BrSub10034660(local, local, p + 0x20, 0.2f);
            BrSub10034660(local, local, p + 0x10, 0.2f);

            g = (float)(BrRandom() & 0xFFFF) * DAT_10077544[0];
            BrSub10034560(&DAT_10ac0c48 + off, p + 0x1060, local);
            BrSub10034660(&DAT_10ac0c48 + off, local,
                          &DAT_10ac0c48 + off, g * g);

            *(int *)(p + 0x1060) = ((int *)local)[0];
            *(int *)(p + 0x1064) = ((int *)local)[1];
            *(int *)(p + 0x1068) = ((int *)local)[2];

            t = f2 * DAT_10077548[0] - DAT_10077534[0];
            *(float *)(&DAT_10ac0c60 + off) = t * DAT_1007754c[0];
            *(char *)(&DAT_10ac0c66 + off) =
                (char)(int)(DAT_10077550[0] / (BrSub100347F0(p + 0x1024) + t)
                            * DAT_10077554[0]);
            *(unsigned char *)(&DAT_10ac0c67 + off) = 0xFF;
        }
    }
}
#else
void BrCarSub9020(struct BrCar *pCar)
{
    BrPoolEmit(pCar);
}
#endif

/* 0x1003551B -- five bytes of prologue and epilogue, no body. */
/* WHAT IT DOES: does nothing at all. The original really is just a function
 * prologue and epilogue with no body -- most likely a routine that was
 * emptied out rather than deleted. */
/* @d3donly 0x1003551B BrSub1003551B -- exists in BRGlide only as folded/duplicated stubs; no unique twin locatable by bytes */
void BrSub1003551B(void *pCar)
{
    (void)pCar;
}

/* 0x10041B50 */
void BrSub10041B50(void)
{
    BrMenuAutoSaveName();
}

/* --- the two that need slice2_26's context ---------------------------- */

BrPhaseCtx *g_pBrSlice4PhaseCtx = NULL;

void BrSlice4SetPhaseCtx(BrPhaseCtx *pCtx)
{
    g_pBrSlice4PhaseCtx = pCtx;
}

/* 0x10044B90 */
/* WHAT IT DOES: switches the game into one particular menu phase. It is a
 * thin forwarder to the phase machinery, and it ignores the argument it is
 * passed because the original took none. */
/* port-only body; Glide match is src/core/cpp/0x1003E0E0.cpp */
void BrMenuSub10044B90(int32_t n)
{
    (void)n;                            /* the original takes no argument */

    if (g_pBrSlice4PhaseCtx != NULL)
        (void)BrPhaseActivate_10044B90(g_pBrSlice4PhaseCtx);
}

/* 0x10044A30 */
/* WHAT IT DOES: leaves the current phase for another particular one, on
 * behalf of the given world object. One of a pair of near-identical leave
 * routines that differ in which flags they touch on the way out. */
/* port-only body; Glide match is src/core/cpp/0x1003DF80.cpp */
/* RETURN VALUE: 0. 0x10044A30 ends `xor eax, eax` at 0x10044AD6, and the
 * +0x08 slot this is stored into is TESTED by 0x10048180 -- see br_phase.h. */
int32_t BrOptFn10044A30(void *pEntity)
{
    if (g_pBrSlice4PhaseCtx != NULL)
        (void)BrPhaseLeave_10044A30(g_pBrSlice4PhaseCtx, pEntity);
    return 0;
}

/* ======================================================================
 * 7. Session timer (0x1003C230)
 * ====================================================================== */

uint32_t g_brA9BFDC = 0;

static uint32_t BrPlatSetTimerStub(void *hWnd, uint32_t idEvent,
                                   uint32_t uElapseMs, void *pfnProc)
{
    (void)hWnd;
    (void)uElapseMs;
    (void)pfnProc;
    /* USER32 returns the timer identifier on success. */
    return idEvent;
}

BrPlatSetTimerFn g_pfnBrPlatSetTimer = BrPlatSetTimerStub;

/* WHAT IT DOES: starts the once-a-second session timer: it does some setup
 * and then asks the window to be poked every thousand milliseconds,
 * recording the timer's identifier and flagging that it is running. The
 * message that arrives has no callback attached, so it goes to the window's
 * normal message handling. */
/* port-only body; Glide match is src/core/generated/0x100358C0.c */
/* @n64 0x80200634 located */
int BrTimerStart1003C230(void)
{
    BrSub1003C020();

    /* SetTimer(0x10680584, 1, 1000, NULL) -- the interval really is the
     * literal 0x3E8 and the proc really is NULL (a WM_TIMER posted to the
     * window). */
    g_brA9BFDC = g_pfnBrPlatSetTimer ?
        g_pfnBrPlatSetTimer(g_brP680584, 1u, 1000u, NULL) : 0u;

    g_brA9CFFC = 1;
    return 1;
}

void BrSub1003C230(void)
{
    /* DEVIATION: the original returns 1 in eax; slice2_25 declares it void. */
    (void)BrTimerStart1003C230();
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
#include <windows.h>

/* 0x10035870 BrTimerStart now lives in src/core/startup/br_timer.c. */




#endif /* BR_MATCHING_BUILD */
