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

/* ==========================================================================
 * 0x10074030  BrStrGet
 * ========================================================================== */

/* WHAT IT DOES: fetches one of the game's pieces of on-screen wording by
 * number -- every menu caption, button label and message comes through here.
 * A number that is not in the table gives nothing back rather than an error. */
/* @implements 0x10074030 d3d BrStrGet */
const char *BrStrGet(int id)
{
    /* br_bits.h's BrHandleLookup IS this function with the table address
     * turned into an argument; the original INLINES it here (byte shape:
     * both range tests jump to one shared `return NULL`).  The range test
     * is unsigned, which is what makes a negative id fall out as NULL. */
    if ((uint32_t)id >= 1u && (uint32_t)id < (uint32_t)BR_STR_TABLE_COUNT) {
        return (const char *)g_apBrStrTable[id];
    }
    return NULL;
}

/* ==========================================================================
 * 0x10010960 / 0x10010980  BrPolyDistX / BrPolyDistY
 * ========================================================================== */

/* WHAT IT DOES: tells the shape-trimming code how far a corner lies from the
 * left edge of the screen -- which is just its horizontal position, so a
 * negative answer means the corner is off to the left. Its neighbour below does
 * the same for the top edge. */
/* @implements 0x1000DEC0 glide BrPolyDistX */
float BrPolyDistX(const struct BrScrPt *pPt)
{
    return ((const BrScrPt *)pPt)->f0C;
}

float BrPolyDistY(const struct BrScrPt *pPt)
{
    return ((const BrScrPt *)pPt)->f10;
}

/* ==========================================================================
 * 0x1003BD50  BrRandom
 * ========================================================================== */

/* WHAT IT DOES: the game's random number source. Each call moves the shared
 * generator on one step and hands back the new value, which is always positive
 * because the generator only ever produces 31 bits (masked by 0x7FFFFFFF). */
/* @implements 0x100353D0 glide BrRandom */
/* @implements 0x1003BD50 d3d BrRandom */
#ifdef BR_MATCHING_BUILD
/* Glide 0x100353D0: seed * 16807 & 0x7FFFFFFF via LEA chain (41 B, 2 relocs).
 * Compiler loads seed into ECX, builds EAX = ECX*16807 via shifts+LEA,
 * masks to 31 bits, stores back, returns. D3D's state is at 0x10A9BFD0 via
 * BrDPlayRandStep; Glide has its own global at 0x10AC3060. */
static int32_t g_brAC3060; /* 0x10AC3060 -- Glide RNG state, separate from D3D's */
int BrRandom(void)
{
    uint32_t s = (uint32_t)g_brAC3060;
    s = (s * 16807u) & 0x07FFFFFFu;
    g_brAC3060 = (int32_t)s;
    return (int)s;
}
#else
int BrRandom(void)
{
    /* The D3D build uses g_brA9BFD0 and 27-bit mask via BrDPlayRandStep. */
    return (int)BrDPlayRandStep(&g_brA9BFD0);
}
#endif

/* ==========================================================================
 * 0x1005FF30  BrMenuSub1005FF30
 * ========================================================================== */

/* WHAT IT DOES: forgets everything the game currently believes about the
 * keyboard -- which keys are held, which were just pressed, and what was held
 * last frame -- so that keys still down when a screen changes do not carry over
 * and register again. It only clears the first 64 entries of each table, not
 * all of them. */
/* @implements 0x1005FF30 d3d BrMenuSub1005FF30 */
void BrMenuSub1005FF30(void)
{
    /* Three inlined `rep stosd` of 0x40 dwords.  BrTables64Clear is the same
     * body as a callee; the original does not call it.  The size is a dword
     * count, not an element count of the two 256-entry int32 arrays. */
    memset(g_BrDikState, 0, BR_TABLE64_COUNT * sizeof(uint32_t));
    memset(g_BrDikEdge,  0, BR_TABLE64_COUNT * sizeof(uint32_t));
    memset(g_BrDikPrev,  0, BR_TABLE64_COUNT * sizeof(uint32_t));
}

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
 * 0x10060260  BrSub10060260
 * ========================================================================== */

/* WHAT IT DOES: purpose unclear. Observably it ignores whatever it is handed
 * and calls one other routine with two fixed globals -- the input object and a
 * window handle -- so it exists to supply that pair rather than to do anything
 * itself. What the routine it calls is for is not established here. */
/* 0x100603A0 is __thiscall in the original -- `this` in ecx, the one stack
 * argument cleaned by the callee (`ret 4`); see slice5_60.h.  VC5's C compiler
 * has no __thiscall keyword, but __fastcall places the first REGISTER-ELIGIBLE
 * argument in ecx, and a struct is never register-eligible, so a 4-byte struct
 * in second position is forced onto the stack.  That reproduces thiscall's
 * register/stack split and its callee-cleanup exactly. */
#ifdef BR_MATCHING_BUILD
typedef struct { void *p; } BrSub603A0Arg;
typedef void(__fastcall *BrSub603A0ThisCall)(void *pThis, BrSub603A0Arg arg);
#endif

/* @implements 0x10060260 d3d BrSub10060260 */
void BrSub10060260(void *pThis)
{
    /* Both operands come from globals.  The declared parameter has no
     * counterpart in the original and is discarded -- see the header. */
    (void)pThis;
#ifdef BR_MATCHING_BUILD
    {
        BrSub603A0Arg arg;
        arg.p = g_brP680584;
        ((BrSub603A0ThisCall)BrSub100603A0)((void *)g_pBrAA2E80, arg);
    }
#else
    BrSub100603A0((void *)g_pBrAA2E80, g_brP680584);
#endif
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

/* ==========================================================================
 * 0x10008CF0  BrLogPrint
 * ========================================================================== */

/* WHAT IT DOES: the game's dead end. It shuts the current picture down, draws
 * one line of text centred on an otherwise blank screen, and then never
 * returns -- it sits spinning, and the only thing that gets the player out is
 * pressing Escape, which quits the game. This is what a fatal message looks
 * like from the inside. */
/* @implements 0x10008CF0 d3d BrLogPrint */
#ifdef BR_MATCHING_BUILD
/* Original: direct calls and globals, no host struct. The 0x8000 DL
 * buffer is a plain local (chkstk probe); Escape spin via the IAT. */
extern int  DAT_100a7514;               /* screen width */
extern int *DAT_106e7710;               /* DL write cursor */
extern void (*DAT_10b73530)(void *);    /* submit hook */
__declspec(dllimport) short __stdcall GetAsyncKeyState(int vk);
__declspec(dllimport) void  __stdcall Sleep(unsigned long ms);
extern void BrClearFlag_AB504(void);
extern void BrTextFlag358Clear(void);
extern void BrSet_10019270(void);
extern int  BrSetGlobal_ABB30(int v);
extern void BrTextDraw(const char *psz, int x, int y);
extern void BrSub100325B0(int code);    /* glide 0x100325B0, never returns */

void BrLogPrint(const void *p)
{
    int aDl[0x2000];

    BrClearFlag_AB504();
    DAT_106e7710 = aDl;
    BrTextFlag358Clear();
    BrSet_10019270();
    BrSetGlobal_ABB30(0x14);

    BrTextDraw((const char *)p, DAT_100a7514 / 2, 0xDC);

    {
        int *p_ = DAT_106e7710;
        DAT_106e7710 = DAT_106e7710 + 2;
        p_[0] = (int)0xB8000000;          /* G_ENDDL */
        p_[1] = 0;
    }
    DAT_10b73530(aDl);

    for (;;) {
        if (GetAsyncKeyState(0x1B) != 0)
            BrSub100325B0(1);
        Sleep(1);
    }
}
#else
/* @implements 0x10008CF0 d3d BrLogPrint */
void BrLogPrint(const void *p)
{
    const BrLogHost *pH = g_pBrLogHost;
    /* DEVIATION: `_alloca(0x8000)` in the original.  The function never
     * returns, so a local has the same lifetime. */
    uint32_t  aDl[BR_LOGPRINT_DL_BYTES / sizeof(uint32_t)];
    uint32_t *pCur;

    if (pH == NULL) {
        return;   /* DEVIATION: unhosted */
    }

    pH->pfn10016990();
    *pH->ppDlCursor = aDl;
    pH->pfn10019260();
    pH->pfn10019270();
    pH->pfn100192F0(0x14);

    /* `cdq / sub eax,edx / sar eax,1` -- a SIGNED halving, not `>> 1`. */
    pH->pfnTextDraw((const char *)p, (int)(*pH->pnScreenW / 2),
                    BR_LOGPRINT_TEXT_Y);

    pCur = *pH->ppDlCursor;
    *pH->ppDlCursor = pCur + 2;       /* `add ecx,8` -- eight BYTES */
    pCur[0] = 0xB8000000u;            /* G_ENDDL */
    pCur[1] = 0u;
    pH->pfnSubmit(aDl);

    for (;;) {
        /* `test ax,ax` -- only the low 16 bits are consulted. */
        if ((uint16_t)pH->pfnKeyAsync(BR_LOGPRINT_VK_ESCAPE) != 0) {
            pH->pfnShutdown(1);
        }
        pH->pfnSleep(1);
    }
}
#endif

#ifdef BR_MATCHING_BUILD
/* ==========================================================================
 * 0x10008EC0 (glide)  BrLogFatalPrintf
 * ========================================================================== */

/* WHAT IT DOES: formats a fatal message into a fresh 0x400-byte buffer and
 * exits with code 1.  The buffer is never printed or freed -- the original
 * really does allocate, format, and die. */
/* @implements 0x10008EC0 glide BrLogFatalPrintf */
void BrLogFatalPrintf(const char *pFmt, ...)
{
    va_list ap;
    char   *pBuf;

    pBuf = (char *)BrOperatorNew(0x400);
    va_start(ap, pFmt);
    vsprintf(pBuf, pFmt, ap);
    exit(1);
}

/* ==========================================================================
 * 0x10008F90 (glide)  BrObjSelCycle
 * ========================================================================== */

/* The scene-DL selection state (see br_scenedl.c for the list's producer). */
extern int      DAT_10273308;       /* pending cycle step, consumed here    */
extern int      DAT_10396ea8;       /* current selected object index        */
extern int      DAT_106eed3c;       /* index count (wrap bound)             */
extern int      DAT_1035fb9c;       /* sorted-list entry count              */
extern uint16_t DAT_1035e710[];     /* sorted object index list             */

/* WHAT IT DOES: applies a pending selection step, wrapping at both ends,
 * and keeps stepping until it lands on index 0 or on an index present in
 * the sorted object list; then clears the pending step. */
/* @implements 0x10008F90 glide BrObjSelCycle */
void BrObjSelCycle(void)
{
    int       i;
    uint16_t *p;

    if (DAT_10273308 != 0) {
        for (;;) {
            DAT_10396ea8 = DAT_10396ea8 + DAT_10273308;
            if (DAT_10396ea8 >= DAT_106eed3c) {
                DAT_10396ea8 = 0;
            }
            if (DAT_10396ea8 < 0) {
                DAT_10396ea8 = DAT_106eed3c - 1;
            }
            if (DAT_10396ea8 == 0) break;
            i = 0;
            if (0 < DAT_1035fb9c) {
                p = DAT_1035e710;
                do {
                    if (DAT_10396ea8 == *p) goto LAB_selDone;
                    i = i + 1;
                    p = p + 1;
                } while (i < DAT_1035fb9c);
            }
        }
LAB_selDone: ;
        DAT_10273308 = 0;
    }
}
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
/* @implements 0x10051990 d3d BrUi51990ScreenNew */
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
extern int DAT_117a5f28;
extern float _DAT_1007720c;
int FUN_10069A80();
int FUN_10069a80();

/* WHAT IT DOES: return the float at offset +0x10 in a struct, cast to double. */
/* @implements 0x1000DEE0 glide BrGetFieldFloat */

double BrGetFieldFloat(int param_1)

{
  return (double)*(float *)(param_1 + 0x10);
}

/* WHAT IT DOES: return (constant at 0x1007720C) minus the float at +0xC, as double. */
/* @implements 0x1000DED0 glide BrGetFieldFloatSubC */

double BrGetFieldFloatSubC(int param_1)

{
  return (double)_DAT_1007720c - (double)*(float *)(param_1 + 0xc);
}

/* WHAT IT DOES: return (constant at 0x1007720C) minus the float at +0x10, as double. */
/* @implements 0x1000DEF0 glide BrGetFieldFloatSub10 */

double BrGetFieldFloatSub10(int param_1)

{
  return (double)_DAT_1007720c - (double)*(float *)(param_1 + 0x10);
}

/* WHAT IT DOES: forward a parameter to FUN_10069A80 with a fixed first argument. */
/* @implements 0x10069DC0 glide BrSub69DC0 */

int BrSub69DC0(int param_1)

{
  FUN_10069a80(&DAT_117a5f28,param_1);
  return;
}

/* WHAT IT DOES: copy the last path component of `param_1` (after the final backslash)
 * into `param_2`. Both strlen and strcpy are the /Oi inline forms (repne scasb, rep
 * movsd + movsb). stdcall per the trailing ret 8. */
/* @implements 0x10008D70 glide BrPathBasename */

void __stdcall BrPathBasename(char *param_1,char *param_2)

{
  unsigned int uVar2;
  char *pcVar4;
  
  uVar2 = strlen(param_1);
  for (pcVar4 = param_1 + ((uVar2 + 1) - 2); (pcVar4 != param_1 && (pcVar4[-1] != '\\'));
      pcVar4 = pcVar4 + -1) {
  }
  strcpy(param_2,pcVar4);
  return;
}

#endif /* BR_MATCHING_BUILD */
