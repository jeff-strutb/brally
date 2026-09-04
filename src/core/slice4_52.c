/* slice4_52.c -- BRD3D.dll, a later pass.  See slice4_52.h, especially the note
 * about the packet listing being mis-paired: everything below was decompiled
 * from asm/ at the address named on the `WANTED AS` line, not from the body
 * printed under it in work/slice4/agent52.asm.
 *
 * Float literals are the exact values of the 32-bit patterns the original
 * pushes (195.0f == 0x43430000, 460.0f == 0x43E60000, ...).
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice4_52.h"
#include "slice1_03.h"      /* BrComCallLocked68 (0x1000C4D0) */

#include "slice3_33.h"   /* BrUiScreen / BrUiCtl / BrUiPhase, BrOperatorNew,
                          * BrUiCtlCtor, BrErrShow  (pulls slice1_06.h)      */
#include "slice1_07.h"   /* BrTables64Clear                                  */
#include "slice3_39.h"   /* g_BrDikState / g_BrDikEdge / g_BrDikPrev,
                          * g_pBrAA2E80                                      */
#include "slice2_22.h"   /* BrDPlayRandStep, BrDPlaySendTag3, BrDPlayLink    */
#include "slice2_14.h"   /* BrScrPt                                          */
#include "slice1_01.h"   /* BrAdler32                                        */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==========================================================================
 * Storage this packet owns
 * ========================================================================== */

void         *g_apBrStrTable[BR_STR_TABLE_COUNT];          /* 0x11829370 */
uint32_t      g_brA9BFD0;                                  /* 0x10A9BFD0 */
BrUiAssetRec  g_aBrUiAssetRec[BR_UIASSET_REC_COUNT];       /* 0x10A9E360 */
int32_t       g_brA9D070;                                  /* 0x10A9D070 */
uint32_t      g_brAA28D4;                                  /* 0x10AA28D4 */
char          g_brB5D94[] = "RSea";                        /* 0x100B5D94 */
unsigned char g_brAD0990[BR_SEASON_TAIL_SIZE];             /* 0x10AD0990 */

const BrShutdownHost *g_pBrShutdownHost;
const BrLogHost      *g_pBrLogHost;
const BrUi51990Ctx   *g_pBrUi51990Ctx;

/* 0x100353D0 / 0x1003BD50 BrRandom now lives in
 * src/core/startup/br_random.c. */

/* ==========================================================================
 * 0x10048470  BrUiScreenCtor
 * ========================================================================== */

struct BrUiScreen *BrUiScreenCtor(struct BrUiScreen *pThis)
{
    BrUiScreen *p = (BrUiScreen *)pThis;
    int32_t     i;

    p->f10  = 0;
    p->cCtl = 0;
    for (i = 0; i < BR_UI_SCREEN_CTL_MAX; ++i) {
        p->apCtl[i] = NULL;
    }
    /* The two floats are cleared as dwords, i.e. to +0.0f. */
    p->fX     = 0.0f;
    p->fY     = 0.0f;
    p->pOwner = NULL;
    p->cSel   = 0;

    /* DEVIATION: the original also writes the vtable 0x1008F6F8 to +0x00 and
     * zeroes +0x04, +0x08, +0x0C and the word at +0x346.  slice3_33.h's
     * BrUiScreen begins at +0x10 and has none of them.  See the header. */

    return pThis;   /* the original returns `this` in eax */
}

/* ==========================================================================
 * 0x1005F530  BrSub1005F530
 * ========================================================================== */

void BrSub1005F530(void)
{
    int32_t i;

    if (g_brA9D070 == 0) {
        return;
    }
    /* `cmp word [0x10AA28D4], di` with di == 0 and `jbe`: an empty table
     * leaves immediately. */
    if ((uint16_t)g_brAA28D4 == 0) {
        return;
    }

    for (i = 0; ; ++i) {
        BrUiAssetObj *pObj = g_aBrUiAssetRec[i].pObj;

        if (pObj != NULL) {
            pObj->pVtbl->pfnRelease(pObj);
            g_aBrUiAssetRec[i].pObj = NULL;
        }

        /* The bound is re-read every iteration, after the release. */
        if (i + 1 >= (int32_t)(g_brAA28D4 & 0xFFFFu)) {
            break;
        }
        /* DEVIATION (memory safety): the original walks by pointer and has no
         * upper bound at all.  The table is BR_UIASSET_REC_COUNT records long,
         * so a count past that would run off the end. */
        if (i + 1 >= BR_UIASSET_REC_COUNT) {
            break;
        }
    }
}

/* ==========================================================================
 * 0x1003D9F0  BrSub1003D9F0
 * ========================================================================== */

/* ==========================================================================
 * 0x100709A0  BrMenuSub100709A0
 * ========================================================================== */

/* WHAT IT DOES: writes the championship season out to its save file. The file
 * starts with a four-letter marker and a checksum of the season data so a
 * corrupted or foreign file can be spotted on load, then the season block
 * itself, five loose settings, and a trailing block. If any of the large writes
 * fails it gives up quietly, leaving a half-written file behind. */
/* port-only body; Glide match is src/core/generated/0x10069930.c */
void BrMenuSub100709A0(void)
{
    unsigned long sum;
    int32_t       nSum;
    FILE         *pf;

    /* adler32(0, NULL, 0) is the seed request; it returns 1 and ignores the
     * first argument entirely. */
    sum = BrAdler32(0, NULL, 0);
    sum = BrAdler32(sum, (const unsigned char *)g_brPACED34,
                    BR_SEASON_BLOCK_SIZE);
    nSum = (int32_t)sum;

    pf = fopen(g_pszBrRallySeasonDat, "wb");   /* mode string at 0x100946A8 */
    if (pf == NULL) {
        return;                                 /* original returns 0 */
    }

    /* Checked writes are (ptr, 1, n); the five loose dwords below are
     * (ptr, 4, 1) and are not checked.  Both quirks are the original's. */
    if (fwrite(g_brB5D94, 1, 4, pf) != 4) {
        fclose(pf);
        return;
    }
    if (fwrite(&nSum, 1, 4, pf) != 4) {
        fclose(pf);
        return;
    }
    if (fwrite(g_brPACED34, 1, BR_SEASON_BLOCK_SIZE, pf)
            != (size_t)BR_SEASON_BLOCK_SIZE) {
        fclose(pf);
        return;
    }

    (void)fwrite(&g_brAA2A08, 4, 1, pf);
    (void)fwrite(&g_br0AC64C, 4, 1, pf);
    (void)fwrite(&g_br0AC650, 4, 1, pf);
    (void)fwrite(&g_br0AC654, 4, 1, pf);
    (void)fwrite(&g_br0AC65C, 4, 1, pf);

    if (fwrite(g_brAD0990, 1, BR_SEASON_TAIL_SIZE, pf)
            != (size_t)BR_SEASON_TAIL_SIZE) {
        fclose(pf);
        return;
    }

    fclose(pf);
    /* original returns 1 here; slice2_24.h types the function void */
}

/* ==========================================================================
 * 0x10038F30  BrSub10038F30
 * ========================================================================== */

void BrSub10038F30(int code)
{
    const BrShutdownHost *pH = g_pBrShutdownHost;

    if (pH == NULL) {
        return;   /* DEVIATION: unhosted, there is nothing to shut down */
    }

    if (*pH->ppAA2904 != NULL && *pH->pn0AC300 != 0) {
        (*pH->ppAA2904)->f68 = 0;
        /* The global is re-read here in the original. */
        (*pH->ppAA2904)->pVtbl->f18(*pH->ppAA2904, 0);
    }

    pH->pfn1002C4A0();
    pH->pfn10016990();
    if (pH->pfnB501CC != NULL) {
        pH->pfnB501CC();
    }
    pH->pfn10079550();
    pH->pfn10078BC0();
    pH->pfn10078DB0();
    pH->pfn10073730();
    if (*pH->pn22AF18 != 0) {
        pH->pfn10005BE0(1);
    }
    pH->pfn1003BFD0();
    pH->pfn1003BF60();
    if (*pH->pn0940A4 != 0) {
        pH->pfn10002CF0();
    }
    pH->pfn10008B80();          /* a bare `ret` in this build */
    if (pH->pfn18AA0D0 != NULL) {
        pH->pfn18AA0D0();
    }
    if (pH->pfn690A28 != NULL) {
        pH->pfn690A28();
    }
    pH->pfn10061620();
    pH->pfn10008970();
    pH->pfn1002AEA0();
    pH->pfn10074050();
    pH->pfnCoUninitialize();
    pH->pfnExit(code);          /* 0x1007CC00 is exit(); does not return */
}


#ifdef BR_MATCHING_BUILD
/* 0x10008EC0 BrLogFatalPrintf now lives in src/core/startup/br_fatal.c. */

/* ==========================================================================
 * 0x10008A70 (glide)  BrVt8A70CallPair
 * ========================================================================== */

/* A struct argument is never register-eligible, so the callee sees ECX
 * `this` plus one STACK dword and no EDX setup (BrTextBoxDeleteDtor's trick). */
typedef struct { int v; } BrVt8A70Arg;
typedef int (__fastcall *BrVtFn8A70)(int *pThis, BrVt8A70Arg arg);

/* WHAT IT DOES: thiscall pair through the object's vtable -- slot +0x0C
 * transforms the argument, slot +0x24 consumes the result.  ECX
 * copy-propagation: the first call reuses the entry ECX, only the second
 * reloads `this`. */
/* Glide match is src/core/cpp/0x10008A70.cpp (true C++ thiscall; the
 * push-before-ecx order is unreachable from the C fastcall twin). */
void __fastcall BrVt8A70CallPair(int *pThis, BrVt8A70Arg param_2)
{
    int *vt;
    BrVt8A70Arg a;

    /* RESIDUE (3B): the original pushes the first call's result BEFORE
     * reloading ecx with `this`; both probed spellings order it after. */
    vt = (int *)*pThis;
    a.v = ((BrVtFn8A70)vt[3])(pThis, param_2);
    ((BrVtFn8A70)vt[9])(pThis, a);
}
#endif /* BR_MATCHING_BUILD */

/* ==========================================================================
 * 0x10051990  BrOptFn10051990
 * ==========================================================================
 *
 * The screen and control prologues below are byte-identical to the ones
 * slice3_33.c calls BrUiScreenNew / BrUiCtlNew.  They are repeated rather than
 * shared because those are file-static there; integration should hoist one
 * copy when it merges.  Both carry the same two DEVIATIONs slice3_33.c
 * records: the array writes are bounded, and an allocation failure returns
 * instead of dereferencing NULL after the (fatal) error report.
 */

/* WHAT IT DOES: makes a fresh, empty page for a menu screen and hangs it off
 * the menu phase that owns it, giving it the standard starting position that
 * every screen's rows are then laid out from. If there is no memory for it the
 * player gets a fatal error box. This is the opening move every screen builder
 * makes. */
/* port-only body; Glide match is src/core/cpp/0x1004A840.cpp */
static BrUiScreen *BrUi51990ScreenNew(const BrUi51990Ctx *pCtx,
                                      BrUiPhase *pPhase, float fY)
{
    BrUiScreen *pScr;
    uint16_t    i;

    i = pPhase->cScreen;
    pPhase->f12 = 0;
    if (i < BR_UI_PHASE_SCREEN_MAX) {
        pPhase->aF6C[i] = 1;
    }

    pScr = (BrUiScreen *)BrOperatorNew(
               BR_ALLOC_SIZE(BrUiScreen, BR_UI_SCREEN_ORIG_SIZE));
    pScr = (pScr != NULL) ? BrUiScreenCtor(pScr) : NULL;

    /* The counter is re-read here rather than reusing `i`. */
    i = pPhase->cScreen;
    if (i < BR_UI_PHASE_SCREEN_MAX) {
        pPhase->apScreen[i] = pScr;
    }
    if (pScr == NULL) {
        BrErrShow(pCtx->pErrHost, 4);
    }
    pPhase->cScreen++;

    if (pScr == NULL) {
        return NULL;
    }

    pScr->pOwner = pPhase;
    pScr->f10    = 0;
    pScr->fX     = 195.0f;   /* 0x43430000 */
    pScr->fY     = fY;
    return pScr;
}

static BrUiCtl *BrUi51990CtlNew(const BrUi51990Ctx *pCtx, BrUiScreen *pScr)
{
    BrUiCtl *pCtl;

    pCtl = (BrUiCtl *)BrOperatorNew(
               BR_ALLOC_SIZE(BrUiCtl, BR_UI_CTL_ORIG_SIZE));
    pCtl = (pCtl != NULL) ? BrUiCtlCtor(pCtl) : NULL;

    /* Stored BEFORE the null test, exactly as the original does. */
    if (pScr->cCtl < BR_UI_SCREEN_CTL_MAX) {
        pScr->apCtl[pScr->cCtl] = pCtl;
    }
    if (pCtl == NULL) {
        BrErrShow(pCtx->pErrHost, 4);
    }
    return pCtl;
}

void BrOptFn10051990(struct BrOptObj *pThis)
{
    const BrUi51990Ctx *pCtx  = g_pBrUi51990Ctx;
    BrUiPhase          *pPhase = (BrUiPhase *)(void *)pThis;
    BrUiScreen         *pScr;
    BrUiCtl            *pCtl;

    if (pCtx == NULL) {
        return;   /* DEVIATION: unhosted */
    }

    pScr = BrUi51990ScreenNew(pCtx, pPhase, 130.0f);   /* 0x43020000 */
    if (pScr == NULL) {
        return;
    }

    /* 0x10051A7D -- the unnamed root control.  Owner is the PHASE, not the
     * screen, at every f38 site in the family. */
    pCtl = BrUi51990CtlNew(pCtx, pScr);
    if (pCtl == NULL) { return; }
    pCtl->pVtbl->f38(pCtl, pPhase, 0.0f, 0.0f, 9, 2, 5, 0, 0);
    pScr->cCtl++;

    /* 0x10051AE4 -- absolute (0, 29), not relative to fX/fY. */
    pCtl = BrUi51990CtlNew(pCtx, pScr);
    if (pCtl == NULL) { return; }
    pCtl->pVtbl->f38(pCtl, pPhase, 0.0f, 29.0f, 9, 2, 5, 0, 0x4E);
    pScr->cCtl++;

    /* 0x10051B50 -- absolute (13, 7). */
    pCtl = BrUi51990CtlNew(pCtx, pScr);
    if (pCtl == NULL) { return; }
    pCtl->pVtbl->f38(pCtl, pPhase, 13.0f, 7.0f, 9, 2, 5, 0, 0x4F);
    pScr->cCtl++;

    /* 0x10051BC0 -- (16, 153) */
    pCtl = BrUi51990CtlNew(pCtx, pScr);
    if (pCtl == NULL) { return; }
    pCtl->pVtbl->f38(pCtl, pPhase, 16.0f, 153.0f, 9, 2, 5, 1, 0x47);
    pCtl->pfn04 = pCtx->p1003F440;
    pScr->cCtl++;

    /* 0x10051C38 -- (392, 181) */
    pCtl = BrUi51990CtlNew(pCtx, pScr);
    if (pCtl == NULL) { return; }
    pCtl->pVtbl->f38(pCtl, pPhase, 392.0f, 181.0f, 9, 2, 5, 1, 0x48);
    pCtl->pfn04 = pCtx->p1003F540;
    pScr->cCtl++;

    /* 0x10051CB0 -- the only selectable control, and the only one with text.
     * f1E20C is 2 here, not the family's usual 3. */
    pCtl = BrUi51990CtlNew(pCtx, pScr);
    if (pCtl == NULL) { return; }
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 460.0f, 0x102001, 2, 5, 0, -1);
    pCtl->pfn0C  = pCtx->p10047360;
    pCtl->pfn08  = pCtx->p10047120;
    pCtl->pfn04  = pCtx->p100471F0;
    pCtl->f1E20C = 2;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x42), 1, 0, pCtx->p0AB438);
    pScr->cCtl++;
    pScr->cSel++;

    /* The original returns 1; the declared return type is void. */
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD



#endif /* BR_MATCHING_BUILD */
