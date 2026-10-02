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
 * slice4_52.c's preamble comes with it, below its own section, and
 * slice6_72.c's with 0x1006FF50 -- the de-duplicating accumulator that
 * builds the running message buffer.
 *
 * slice2_19.c's preamble is carried over verbatim.  An include set that
 * looks redundant has already been shown elsewhere in this module to move
 * VC5's register allocation (see br_rdpmode.c).
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
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


/* Header prototype is cdecl (this, r, g, b).  Original is thiscall with
 * ret 0xC; hide that prototype so the definition can take the struct-arg
 * __fastcall shape that reproduces it. */
#define BrRgbSinkSet BrRgbSinkSet_hdr
/* slice2_19.h / br_seg.h declare these cdecl with a leading state pointer the
 * originals do not have.  Hide those prototypes so BrModelLoad can call them
 * with the shapes the bytes show. */
#define BrSub100088B0 BrSub100088B0_cdecl
#define BrSegSetBases BrSegSetBases_cdecl
#include "slice2_19.h"
#undef BrSub100088B0
#undef BrSegSetBases
typedef struct { void *p; } BrModelLoadArg;
/* 64-bit core: declared once, in br_globals.h or its struct's header */                        /* 0x10AC0810 */
/* BrSub100088B0: prototype in br_funcs.h */
/* BrSegSetBases: prototype in br_funcs.h */
#undef BrRgbSinkSet

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
/* Original: direct calls and globals, no host struct. The 0x8000 DL
 * buffer is a plain local (chkstk probe); Escape spin via the IAT. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* screen width */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* DL write cursor */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* submit hook */
/* 64-bit core: GetAsyncKeyState is declared by the platform headers */
/* 64-bit core: Sleep is declared by the platform headers */
/* BrClearFlag_AB504: prototype in br_funcs.h */
/* BrTextFlag358Clear: prototype in br_funcs.h */
/* BrSet_10019270: prototype in br_funcs.h */
/* BrSetGlobal_ABB30: prototype in br_funcs.h */
/* BrTextDraw: prototype in br_funcs.h */
/* BrSub100325B0: prototype in br_funcs.h */

void BrLogPrint(const void *p)
{
    int aDl[0x2000];

    BrClearFlag_AB504();
    (*(int * *)&g_BrGfxPtr) = aDl;
    BrTextFlag358Clear();
    BrSet_10019270();
    BrSetGlobal_ABB30(0x14);

    BrTextDraw((const char *)p, BrGbiRectG_A7514 / 2, 0xDC);

    {
        int *p_ = (*(int * *)&g_BrGfxPtr);
        (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2;
        p_[0] = (int)0xB8000000;          /* G_ENDDL */
        p_[1] = 0;
    }
    DAT_10b73530(aDl);

    for (;;) {
        if (GetAsyncKeyState(0x1B) != 0)
            BrExt_10038F30(1);
        Sleep(1);
    }
}

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <string.h>

#include "slice6_72.h"



/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: append a string to a running log buffer, but only if it is
 * not already in there -- a crude de-duplicating accumulator, so a message
 * repeated every frame appears once. Adds a separator after each new entry. */
/* @implements 0x1006FF50 glide FUN_1006ff50 */
/* auto-filed from ghidra --refine; transforms: as-is */

void FUN_1006ff50(char *param_1)

{
  if (strstr(DAT_118ee590, param_1) == 0) {
    strcat(DAT_118ee590, param_1);
    strcat(DAT_118ee590, g_strA);
  }
}

