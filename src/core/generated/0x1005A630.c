/* Auto-generated from Ghidra decompilation — 0x1005A630 */
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

/* WHAT IT DOES: scan a 32-bit-per-pixel image region (x0..x1, y0..y1,
 * rows packed at the region's own width) for a pixel whose first byte is
 * zero and whose next two bytes are equal -- the livery loader's test for
 * "this image contains a colour-key pixel". Returns 1 on the first such
 * pixel, 0 if there is none or the image pointer is NULL. */
/* Three source shapes are load-bearing here; all three were found by probe
 * sweep and each is worth a byte:
 *   - the row stride is a LOCAL (rb) assigned at the top of the row body.
 *     Spelling the advance `pImg += w * 4` folds the scale into one
 *     `lea edi,[edi+esi*4]`; the original materialises `lea eax,[esi*4]`
 *     and then `add edi,eax`, which only happens when the product is a
 *     named value.  VC5 does NOT hoist it out of the row loop.
 *   - the row loop is a `while` with `y++` BEFORE the pointer advance.  A
 *     plain `for (y = 0; y < h; y++)` emits `add edi,eax` before `inc ebp`;
 *     the original has them the other way round.
 *   - the pixel test is an early-out `continue` with its OWN `p += 4`, not
 *     `&&` and not a nested if.  With a single increment site VC5 builds a
 *     BIASED induction pointer (`lea eax,[edi+2]`, then `[eax-2]/[eax-1]/
 *     [eax]`); the duplicated increment defeats the bias and gives the
 *     original's `cmp byte ptr [eax],0` / `[eax+1]` / `[eax+2]`. */
/* @implements 0x1005A630 glide BrImgRegionHasKey */
int BrImgRegionHasKey(const char *pImg, int x0, int x1, int y0, int y1)
{
  const char *p;
  int rb;
  int w;
  int h;
  int x;
  int y;

  if (pImg == NULL) return 0;
  w = x1 - x0;
  h = y1 - y0;
  y = 0;
  while (y < h) {
    rb = w * 4;
    p = pImg;
    for (x = 0; x < w; x++) {
      if (p[0] != 0) {
        p += 4;
        continue;
      }
      if (p[1] == p[2]) {
        return 1;
      }
      p += 4;
    }
    y++;
    pImg += rb;
  }
  return 0;
}


#endif /* BR_MATCHING_BUILD */
