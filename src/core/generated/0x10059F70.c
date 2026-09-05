/* Auto-generated from Ghidra decompilation — 0x10059F70 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h>

#ifndef true
#define true 1
#define false 0
#endif
#ifndef NAN
unsigned long _ghidra_nan_bits = 0x7FC00000;
#define NAN (*(float*)&_ghidra_nan_bits)
#endif

typedef int (*funcptr)();

/* WHAT IT DOES: convert a bottom-up 24-bit BGR bitmap into a top-down
 * 32-bit RGBA image: walks the source rows from the last one upward and,
 * per pixel, reads B, G, R and writes R, G, B, 0xFF. The same walk as
 * BrSurfBlt24, widening instead of packing to 565. Called by the RGBA
 * bitmap loader (0x1005A210, br_bmp.c). The row pointer is the pBits
 * parameter advanced in place, and the column count is copied to x only
 * inside the `if (cx)` -- both decide which register each value takes. */
/* @implements 0x10059F70 glide BrBmpWiden24ToRgba */
void BrBmpWiden24ToRgba(unsigned char *pDst, const unsigned char *pBits,
                        int cx, int cy, int cbWidthBytes)
{
  int y;

  pBits += (cy - 1) * cbWidthBytes;
  if (cy == 0) return;
  y = cy;
  do {
    const unsigned char *pSrc = pBits;
    int x;

    if (cx != 0) {
      x = cx;
      do {
        unsigned char b, g, r;

        b = *pSrc++;
        g = *pSrc++;
        r = *pSrc++;
        *pDst++ = r;
        *pDst++ = g;
        *pDst++ = b;
        *pDst++ = 0xFF;
      } while (--x);
    }
    pBits -= cbWidthBytes;
  } while (--y);
}


#endif /* BR_MATCHING_BUILD */
