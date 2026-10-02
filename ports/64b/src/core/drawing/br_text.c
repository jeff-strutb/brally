/* br_text.c -- drawing: the text writer and the state it draws with.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice1_03.c, which is an address batch and not a module.
 * Everything the original reached through fixed addresses is modelled as a
 * file-static state block reachable through a Get...() accessor, so the
 * ported functions keep the original's argument lists exactly; see
 * slice1_03.h for the state layout and for the addresses.
 *
 * The glyphs themselves are br_font.c; this file is the layer above it --
 * which colours to use, what scale, where the pen goes, and the one HUD
 * caption that formats a time before handing it over.
 *
 * The alignment and flag setters below arrived from three different address
 * batches (slice5_61.c, slice5_63.c, slice6_78.c); their preambles come with
 * them, since an include set that looks redundant has already been shown
 * elsewhere in this module to move VC5's register allocation (see
 * br_rdpmode.c).
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice1_03.h"

#include <stdio.h>

static BrTextState g_text;

/* (port-only BrTextGetState removed) */


/* 0x100192A0 */
/* WHAT IT DOES: sets the two sets of three colour values that text is drawn
 * with, and raises the flag that says a colour has been chosen. */
/* @implements 0x100192A0 d3d BrTextSetColors */
/* @n64 0x8022F530 located */
/* The two colour triples and the chosen flag are separate globals in the
 * original (0x100A6C68.. and 0x104ABB4C..), not fields of the port's g_text. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
void BrTextSetColors(int a1, int a2, int a3, int a4, int a5, int a6)
{
    DAT_100a6c68 = a1;
    DAT_100a6c6c = a2;
    DAT_100a6c70 = a3;
    DAT_104abb4c = 1;
    DAT_104abb50 = a4;
    DAT_104abb54 = a5;
    DAT_104abb58 = a6;
}

/* 0x10019300 */
/* WHAT IT DOES: draws a line of text at the given position. It first tells
 * the renderer to ignore depth for this text, so the writing always sits on
 * top of the scene, then works out where the line actually starts -- as
 * given, centred, or right-aligned, measuring the string when it needs to --
 * and hands it to the glyph drawer. Note that an alignment value it does not
 * recognise leaves the horizontal position at whatever the previous call
 * used. */
/* @implements 0x10019300 d3d BrTextDraw */
/* The original: DL-emit macro (no pGfx guard), switch on the signed-char
 * align global (arms in source order 2,1,0,default; case 0 stores x and
 * falls into default's y store), direct calls to the measurer and emitter
 * with the original's arities -- the 2-arg header protos are hidden by
 * these local ones. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* align: movsx  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* pen x         */
/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* pen y         */
/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* scale         */
/* BrFontMeasure: prototype in br_funcs.h */
/* BrTextEmitString: prototype in br_funcs.h */

void BrTextDraw(const char *psz, int x, int y)
{
    const char *s = psz;    /* homed in esi before the switch */
    int *p_;

    { p_ = (*(int * *)&g_BrGfxPtr); (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2;
      *p_ = 0xb6000000; p_[1] = 1; }

    switch ((int)DAT_104abb44) {
    case 2:
        DAT_104abb28 = x - (BrFontMeasure(s, DAT_104abb30) >> 1);
        break;
    case 1:
        DAT_104abb28 = x - BrFontMeasure(s, DAT_104abb30);
        break;
    case 0:
        DAT_104abb28 = x;
        break;
    }
    /* One shared tail: VC5 DUPLICATES it into each arm (three full copies
     * in the bytes). Spelling the copies per-arm in source gets them
     * cross-jump MERGED instead -- the duplication is only reachable from
     * the single-tail form. */
    DAT_104abb2c = y;
    BrTextEmitString(s);
}

/* (port-only BrFormatTime removed) */


/* 0x100171F0 */
/* WHAT IT DOES: draws one labelled time on the heads-up display -- a lap
 * time or a split -- formatting the seconds as minutes, seconds and
 * hundredths and putting the number fifteen pixels below its label. The
 * number goes out before the label. */
/* @implements 0x100171F0 d3d BrHudDrawTimeEntry */
/* @n64 0x80238714 located */
/* The original inlines the whole of BrFormatTime: the minute/second/hundredth
 * split (magic divides by 100 and 60) and an UNBOUNDED sprintf into the
 * 32-byte stack buffer, with the prefix passed raw -- no NULL guard. */
void BrHudDrawTimeEntry(const char *pszLabel, const char *pszPrefix,
                        float fSeconds, int x, int y)
{
    char sz[32];      /* the original's local buffer is exactly 0x20 */
    int  total   = (int)(fSeconds * 100.0f);
    int  whole   = total / 100;
    int  minutes;

    total  -= whole * 100;      /* total is now the hundredths */
    minutes = whole / 60;
    whole  -= minutes * 60;     /* whole is now the seconds */

    sprintf(sz, "%s%d:%02d.%02d", pszPrefix, minutes, whole, total);

    /* the time line goes out first, 15 pixels below the label */
    BrTextDraw(sz, x, y + 15);
    BrTextDraw(pszLabel, x, y);
}


/* WHAT IT DOES: store a value into the global at 0x104ABB30. */
/* @implements 0x100168B0 glide BrSetGlobal_ABB30 */

int BrSetGlobal_ABB30(int param_1)

{
  DAT_104abb30 = param_1;
  return;
}


/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice5_61.h"

#include <string.h>

#include "slice1_03.h"   /* BrTextState / BrTextGetState, BR_TEXT_ALIGN_* */
#include "slice5_63.h"   /* g_br4B035C -- raw text-align global */
#include "slice2_15.h"   /* BrRdpRegs / BrRdpGetRegs -- 0x104BBF08 etc.   */
#include "slice2_25.h"   /* the option globals, index tables and callees   */


/* ==========================================================================
 * 0x10019290  text alignment := 1
 * ========================================================================== */

/* WHAT IT DOES: makes the next text drawn right-aligned, so it ends at the
 * position given rather than starting there. */
/* @implements 0x10016850 glide BrSub_10019290 */
/* @n64 0x8022F504 located */
void BrSub_10019290(void)
{
    DAT_104abb44 = 1;
}

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>
#include <string.h>

#define BrExt_1007AC00 BrExt_1007AC00_decl
#include "slice5_63.h"
#undef BrExt_1007AC00

#include "br_crt.h"      /* BrOperatorNew (0x1007DFE0)                       */
#include "slice1_03.h"   /* BrTextGetState, BrHudDrawTimeEntry               */
#include "slice2_25.h"   /* option globals, BrOptObj, BrStrGet, lookup tables */


/* 0x10019280 */
/* WHAT IT DOES: switches text drawing back to left-aligned, so writing that
 * follows starts at the position given rather than being centred on it. */
/* @implements 0x10016840 glide BrSub_10019280 */
void BrSub_10019280(void)
{
    DAT_104abb44 = 0;
}

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdarg.h>
#include "br_path.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice6_78.h"


/* 0x10019240 -- `mov byte [0x104B0360], 1` */
/* WHAT IT DOES: raises a one-byte flag that the text glyph drawing code
 * reads to pick between two different drawing commands. What visual
 * difference that makes is not established here. */
/* @implements 0x10016800 glide BrSub_10019240 */
/* @n64 0x8022F520 located */
void BrSub_10019240(void)
{
    g_4B0360 = 1u;
}
