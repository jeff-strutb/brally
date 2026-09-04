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
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice1_03.h"

#include <stdio.h>

static BrTextState g_text;

BrTextState *BrTextGetState(void)
{
    return &g_text;
}

/* 0x100192A0 */
/* WHAT IT DOES: sets the two sets of three colour values that text is drawn
 * with, and raises the flag that says a colour has been chosen. */
/* @implements 0x100192A0 d3d BrTextSetColors */
/* @n64 0x8022F530 located */
void BrTextSetColors(int a1, int a2, int a3, int a4, int a5, int a6)
{
    g_text.f0A74A8 = a1;
    g_text.f0A74AC = a2;
    g_text.f0A74B0 = a3;
    g_text.f4B0364 = 1;
    g_text.f4B0368 = a4;
    g_text.f4B036C = a5;
    g_text.f4B0370 = a6;
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
#ifdef BR_MATCHING_BUILD
/* The original: DL-emit macro (no pGfx guard), switch on the signed-char
 * align global (arms in source order 2,1,0,default; case 0 stores x and
 * falls into default's y store), direct calls to the measurer and emitter
 * with the original's arities -- the 2-arg header protos are hidden by
 * these local ones. */
extern int *DAT_106e7710;
extern signed char DAT_104abb44;   /* align: movsx  */
extern int DAT_104abb28;           /* pen x         */
extern int DAT_104abb2c;           /* pen y         */
extern int DAT_104abb30;           /* scale         */
extern int32_t BrFontMeasure(const char *psz, int32_t scale);
extern void BrTextEmitString(const char *psz);

void BrTextDraw(const char *psz, int x, int y)
{
    const char *s = psz;    /* homed in esi before the switch */
    int *p_;

    { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2;
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
#else
void BrTextDraw(const char *psz, int x, int y)
{
    int w;

    /* Two dwords into the display list, cursor advanced by 8 bytes. On the
     * N64 command set 0xB6 is G_CLEARGEOMETRYMODE and the payload 0x1 is
     * G_ZBUFFER, i.e. "turn the z-buffer off for this text". */
    if (g_text.pGfx != NULL) {      /* DEVIATION: original never checks */
        g_text.pGfx[0] = 0xB6000000u;
        g_text.pGfx[1] = 0x00000001u;
        g_text.pGfx += 2;
    }

    switch (g_text.align) {
    case BR_TEXT_ALIGN_CENTER:
        w = (g_text.pfnMeasure != NULL)
                ? g_text.pfnMeasure(psz, g_text.scale) : 0;
        /* the original uses `sar eax,1`, an arithmetic shift, not a signed
         * divide -- they differ for a negative measurement */
        g_text.x = x - (w >> 1);
        g_text.y = y;
        break;

    case BR_TEXT_ALIGN_RIGHT:
        w = (g_text.pfnMeasure != NULL)
                ? g_text.pfnMeasure(psz, g_text.scale) : 0;
        g_text.x = x - w;
        g_text.y = y;
        break;

    case BR_TEXT_ALIGN_LEFT:
        g_text.x = x;
        g_text.y = y;
        break;

    default:
        /* x is NOT touched here: the original's case-0 arm falls through
         * into the default arm, which only stores y. Any align value other
         * than 0/1/2 therefore reuses the previous call's x. */
        g_text.y = y;
        break;
    }

    if (g_text.pfnDrawString != NULL)
        g_text.pfnDrawString(psz);
}
#endif

void BrFormatTime(char *pszOut, size_t cbOut, const char *pszPrefix,
                  float fSeconds)
{
    /* 0x1007C8A0 is __ftol: it forces the x87 rounding mode to chop, so the
     * conversion truncates toward zero, matching a C cast. */
    int total      = (int)(fSeconds * 100.0f);
    int hundredths = total % 100;
    int minutes;
    int seconds;

    total  /= 100;
    minutes = total / 60;
    seconds = total % 60;

    /* DEVIATION: the original sprintf()s into a 32-byte stack buffer with an
     * unbounded "%s" prefix. snprintf here. */
    snprintf(pszOut, cbOut, "%s%d:%02d.%02d",
             (pszPrefix != NULL) ? pszPrefix : "",
             minutes, seconds, hundredths);
}

/* 0x100171F0 */
/* WHAT IT DOES: draws one labelled time on the heads-up display -- a lap
 * time or a split -- formatting the seconds as minutes, seconds and
 * hundredths and putting the number fifteen pixels below its label. The
 * number goes out before the label. */
/* @implements 0x100171F0 d3d BrHudDrawTimeEntry */
/* @n64 0x80238714 located */
#ifdef BR_MATCHING_BUILD
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
#else
void BrHudDrawTimeEntry(const char *pszLabel, const char *pszPrefix,
                        float fSeconds, int x, int y)
{
    char sz[32];      /* the original's local buffer is exactly 0x20 */

    BrFormatTime(sz, sizeof(sz), pszPrefix, fSeconds);

    /* the time line goes out first, 15 pixels below the label */
    BrTextDraw(sz, x, y + 15);
    BrTextDraw(pszLabel, x, y);
}
#endif

#ifdef BR_MATCHING_BUILD

/* WHAT IT DOES: store a value into the global at 0x104ABB30. */
/* @implements 0x100168B0 glide BrSetGlobal_ABB30 */

int BrSetGlobal_ABB30(int param_1)

{
  DAT_104abb30 = param_1;
  return;
}

#endif /* BR_MATCHING_BUILD */
