/* Matching TU for 0x100031D0 -- 64x64 grid lookup returning a value/delta pair. */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h>

extern unsigned short *g_pBrGrid16;            /* 0x106EED44 -- 64 x 64 u16 */

/* WHAT IT DOES: reads cell (a, b) of a 64x64 table of 16-bit values and the
 * cell after it, and packs them as `value | ((next - value) << 16)` -- the
 * base and step a caller interpolates between.  Out-of-range coordinates
 * give 0. */
/* @implements 0x100031D0 glide BrGrid16Pair */
unsigned int BrGrid16Pair(int a, int b)
{
    unsigned short x, y, i, i2;
    unsigned int   lo, hi;

    if (a < 0 || a >= 0x40 || b < 0 || b >= 0x40)
        return 0;
    x  = (unsigned char)a;
    y  = (unsigned char)b;
    i  = (y << 6) + x;
    i2 = i + 1;
    lo = g_pBrGrid16[i];
    hi = g_pBrGrid16[i2];
    return ((hi + lo * 0xFFFF) << 16) | lo;
}

#endif /* BR_MATCHING_BUILD */
