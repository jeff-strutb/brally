/* rand.c -- the game's random number generator
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_8028B790;
/* -- end declarations -- */

/* WHAT IT DOES: Seed the random number generator. The race mode seeds it
 * with the fixed value 123 when a race starts, so the random sequence is
 * the same every race. */
/* @implements 0x80225ED8 tgr BrRandSeed */
void BrRandSeed(int param_1)
{
  D_8028B790 = param_1;
}

/* WHAT IT DOES: Advance the random number generator one step (seed times
 * 16807, kept to 27 bits). */
/* @t4-pass 0x80225EE4 1 2026-09-26 compiles 14 best 5 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80225EE4 2 2026-09-26 compiles 14 best 5 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80225EE4 3 2026-09-26 compiles 11 best 5 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80225EE4 tgr BrRandStep */
void BrRandStep(void)
{
  D_8028B790 = D_8028B790 * 0x41a7 & 0x7ffffff;
}
