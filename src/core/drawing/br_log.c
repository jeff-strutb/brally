/* br_log.c -- drawing: the on-screen message.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.  The
 * logger is here and not in startup/ because what it actually does is paint
 * a message over the screen; nothing about it is about bringing the game up.
 *
 * Filed out of the address batches, which are not modules.  0x10008EF0 is
 * the dead end itself: it shuts the picture down, draws one centred line on
 * a blank screen and then spins until Escape quits the game.
 *
 * slice4_52.c's preamble comes with it, below its own section.
 *
 * slice2_19.c's preamble is carried over verbatim.  An include set that
 * looks redundant has already been shown elsewhere in this module to move
 * VC5's register allocation (see br_rdpmode.c).
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


#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl (this, r, g, b).  Original is thiscall with
 * ret 0xC; hide that prototype so the definition can take the struct-arg
 * __fastcall shape that reproduces it. */
#define BrRgbSinkSet BrRgbSinkSet_hdr
#endif
#ifdef BR_MATCHING_BUILD
/* slice2_19.h / br_seg.h declare these cdecl with a leading state pointer the
 * originals do not have.  Hide those prototypes so BrModelLoad can call them
 * with the shapes the bytes show. */
#define BrSub100088B0 BrSub100088B0_cdecl
#define BrSegSetBases BrSegSetBases_cdecl
#endif
#include "slice2_19.h"
#ifdef BR_MATCHING_BUILD
#undef BrSub100088B0
#undef BrSegSetBases
typedef struct { void *p; } BrModelLoadArg;
extern int g_brModelMgr;                        /* 0x10AC0810 */
void * __fastcall BrSub100088B0(void *pThis, BrModelLoadArg a,
                                BrModelLoadArg b);
void BrSegSetBases(uint32_t n64Base, uint32_t hostBase);
#endif
#ifdef BR_MATCHING_BUILD
#undef BrRgbSinkSet
#endif

#include <string.h>

/* 0x10035BA7  The parameter is never read. */
/* WHAT IT DOES: writes out whatever message was last handed to the routine
 * below. It ignores the argument it is given and reads the stored one
 * instead. */
/* @implements 0x10035BA7 d3d BrLogEmit */
void BrLogEmit(void *ignored)
{
    (void)ignored;
    BrLogPrint(g_BrLogArg);
}

/* WHAT IT DOES: records a message and writes it out at once. Worth knowing
 * because elsewhere in the tree this same address is reached under the name
 * "BrFatal" -- it is not fatal, it only logs. */
/* @implements 0x10035BBA d3d BrLogSet */
/* @n64 0x8021E1F4 located */
void BrLogSet(void *p)
{
    g_BrLogArg = p;
    BrLogEmit(NULL);
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
/* WHAT IT DOES: the game's dead end -- shuts the current picture down, draws
 * one line of text centred on an otherwise blank screen, and never returns.
 * Port arm of the same function. */
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
