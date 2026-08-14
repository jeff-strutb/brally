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
#include "slice4_53.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice2_17.h"      /* BrS17BankFlip, BrRenderCountersReset       */
#include "slice2_18.h"      /* BrGfx2C210, BrGfx31227 declarations        */
#include "slice2_19.h"      /* BrSub10002240, BrSub100088B0, BrSub10037740 */
#include "slice2_20.h"      /* BrPoolEmit, BrRcaLoadCar                   */
#include "slice2_21.h"      /* BrSinF, BrSqrtF, BrCarSub9020              */
#include "slice2_22.h"      /* BrDPlayLink, BrDPlaySendTag4               */
#include "slice2_24.h"      /* BrStringById, BrMenuSub10044B90, ...       */

/* slice2_16.h cannot be included here: it defines a TYPE called BrRcaFixup
 * and slice2_20.h defines a FUNCTION of that name, so the two headers cannot
 * share a translation unit.  This is the one declaration needed from it, and
 * it is copied verbatim. */
/* XSLICE 0x1007CC00 */
extern void BrGbiStackOverflow(int code);

/* ======================================================================
 * 1. x87 one-liners
 * ====================================================================== */

/* 0x10002240 */
float BrSinF(float x)
{
    /* DEVIATION: `fsin` computes in 80-bit and is undefined for |x| >= 2^63
     * (it returns the operand untouched with C2 set).  sinf has neither
     * property.  No call site approaches the limit. */
    return sinf(x);
}

/* 0x10002240 -- slice2_19's name for the very same address. */
float BrSub10002240(float x)
{
    return BrSinF(x);
}

/* 0x10002250 */
float BrSqrtF(float x)
{
    return sqrtf(x);
}

/* ======================================================================
 * 2. String table (0x10074030)
 * ====================================================================== */

char *g_apBrStringTable[BR_STRING_ID_LIMIT];

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
void BrGfx2C210(void)
{
    BrS17BankFlip();
}

/* 0x10031227 */
void BrGfx31227(void)
{
    BrRenderCountersReset();
}

/* 0x10039020 */
void BrCarSub9020(struct BrCar *pCar)
{
    BrPoolEmit(pCar);
}

/* 0x10037740 */
void BrSub10037740(void *pCar, void *pArg)
{
    /* DEVIATION: pArg is declared void* by slice2_19 but is an integer index
     * in the original.  DEVIATION: cbDest is slice2_20's port-only bound;
     * the original has none.  0x15F88 is the stride its only caller
     * (0x10035520) uses to compute pCar, so it is the true extent. */
    BrRcaLoadCar(pCar, (size_t)BR_RCA_CAR_STRIDE, (int)(intptr_t)pArg);
}

/* 0x1003551B -- five bytes of prologue and epilogue, no body. */
void BrSub1003551B(void *pCar)
{
    (void)pCar;
}

/* 0x1003DA40 */
void BrSub1003DA40(BrOptUi *pUi, int a)
{
    /* The gate slice2_22 takes as an argument is the global 0x10AA288C. */
    (void)BrDPlaySendTag4((const BrDPlayLink *)(const void *)pUi,
                          g_brAA288C, (uint32_t)a);
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
void BrMenuSub10044B90(int32_t n)
{
    (void)n;                            /* the original takes no argument */

    if (g_pBrSlice4PhaseCtx != NULL)
        (void)BrPhaseActivate_10044B90(g_pBrSlice4PhaseCtx);
}

/* 0x10044A30 */
void BrOptFn10044A30(BrOptObj *pThis)
{
    if (g_pBrSlice4PhaseCtx != NULL)
        (void)BrPhaseLeave_10044A30(g_pBrSlice4PhaseCtx, pThis);
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
