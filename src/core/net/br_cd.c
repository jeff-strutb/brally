/* br_cd.c -- net.
 *
 * Filed out of slice1_01.c. The preamble below is that file's,
 * copied verbatim: these bodies are byte-exact only under the
 * view the compiler had of them -- same includes, same
 * typedefs, same struct layouts. Do not trim it without
 * re-sweeping.
 */
/* slice1_01.c -- BRD3D.dll 0x10001000-0x10004910, a later pass. See slice1_01.h. */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice1_01.h"

#include <stdlib.h>

/* ---------------------------------------------------------------------------
 * 0x10001000 -- zlib adler32.
 *
 * Identified by the two magic constants the original carries verbatim:
 * 0x15B0 (NMAX = 5552) as the outer chunk cap and 0xFFF1 (BASE = 65521) as
 * the modulus, plus the DO16 unrolled body at 0x1000104E..0x100010E4.
 *
 * Argument order is the zlib one, traced through the prologue: after
 * `push esi` / `push edi` the reads at [esp+0xC] land on arg2 (buf) and arg1
 * (adler) respectively, and after `push ebx` the read at [esp+0x18] lands on
 * arg3 (len).
 *
 * The len == 0 path is reached by `test ebx,ebx / jbe`; after a `test` the
 * carry flag is clear, so jbe is just je -- it is an equality test, not the
 * signed/unsigned comparison it looks like.
 */
unsigned long BrAdler32(unsigned long adler, const unsigned char *pBuf,
                        unsigned int len)
{
    unsigned long s1 = adler & 0xFFFFuL;
    unsigned long s2 = (adler >> 16) & 0xFFFFuL;
    unsigned int  k;

    if (pBuf == NULL) {
        return 1uL;
    }

    while (len > 0u) {
        k = (len < 5552u) ? len : 5552u;
        len -= k;

        /* DO16, then the remainder one byte at a time. */
        while (k >= 16u) {
            unsigned int i;
            for (i = 0u; i < 16u; ++i) {
                s1 += *pBuf++;
                s2 += s1;
            }
            k -= 16u;
        }
        while (k > 0u) {
            s1 += *pBuf++;
            s2 += s1;
            --k;
        }

        s1 %= 65521uL;
        s2 %= 65521uL;
    }

    return (s2 << 16) | s1;
}

/* ---------------------------------------------------------------------------
 * 0x10002A20 (partial) -- 8-bit volume to the backend's 0..10000 scale.
 *
 * The original builds 10000*v with four `lea [r+r*4]` (x5 each, so x625) and
 * a `shl 4`, then divides by 255 with the signed magic 0x80808081 / sar 7 /
 * sign fixup. v is masked to 8 bits before any of that, so the input is
 * always non-negative and the sign fixup never fires.
 */
/* WHAT IT DOES: converts a music volume from the 0-255 scale the game's own
 * settings use into the 0-10000 scale Windows CD audio wants. A volume of
 * exactly 256 comes out as silence rather than full, because only the low
 * byte is looked at. */
/* BrCdVolumeScale moved below the CD-globals declarations (it references the
 * three CD-enable guards and the g_575454 backend pointer). */

/* ---------------------------------------------------------------------------
 * 0x10002DE0 -- 64x64 u16 grid sample.
 *
 * The four guards are `fcomp` against 0.0f (0x1008F09C) and 2048.0f
 * (0x1008F0A0), read back with fnstsw / `test ah,1`, i.e. the C0 bit, i.e.
 * "ST < operand". Order in the original is x>=0, x<2048, y>=0, y<2048; x is
 * the one loaded twice through [esp+8] once esi has been pushed.
 *
 * The scale is 0x1008F0A4 = 0.03125f = 1/32, exact in binary, so no rounding
 * question arises before the truncation. 0x1007C8A0 is __ftol (it sets the
 * x87 rounding field to 0xC00 = toward zero and does `fistp qword`).
 *
 * Only AL of each conversion is consumed (`movzx si, al` / `movzx ax, al`),
 * so a value of 256 or more would wrap -- unreachable given the 2048 guard,
 * but reproduced with the mask below rather than assumed away.
 *
 * DEVIATION: the grid base was the global at 0x106C7C6C; it is a parameter.
 */
/* WHAT IT DOES: looks up a place in the world on a coarse 64-by-64 grid --
 * each square covering thirty-two world units, so the grid spans a square
 * region a couple of thousand units across -- and hands back both that
 * square's value and the difference to the square next along, so a caller can
 * blend between the two. WHAT THE GRID HOLDS IS NOT ESTABLISHED HERE. A
 * position outside the covered region answers zero, which is indistinguishable
 * from a square whose value genuinely is zero. */

#ifdef BR_MATCHING_BUILD

int FUN_10002580();
extern int DAT_1021c778;
int BrSub10075020();
int BrWindowEarStartup();
extern int g_brCdTrackLast;
extern int g_brCdEnabled;
extern int g_brCdMediaOk;
extern int g_brCdPlaying;
extern int g_brCdTrackCur;
extern int g_brCdTrackFirst;

/* WHAT IT DOES: open the CD-music layer: refcount g_brCdPlaying, and on the first open
 * seed rand from the tick source, reset track state, start the EAR window path, and run
 * the device probe (0x10002580). */
/* @implements 0x10002910 glide BrCdStartup */

int BrCdStartup(int param_1)

{
  unsigned int _Seed;
  int uVar1;
  
  if (g_brCdEnabled == 0) {
    return 1;
  }
  g_brCdPlaying = g_brCdPlaying + 1;
  if (g_brCdPlaying != 1) {
    return 1;
  }
  _Seed = BrSub10075020();
  srand(_Seed);
  g_brCdTrackCur = 2;
  g_brCdTrackFirst = 0;
  g_brCdTrackLast = 0;
  BrWindowEarStartup(param_1);
  g_brCdMediaOk = 0;
  DAT_1021c778 = 0;
  uVar1 = FUN_10002580();
  return uVar1;
}

#endif /* BR_MATCHING_BUILD */
