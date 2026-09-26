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
/* @implements 0x80225EE4 tgr BrRandStep */
void BrRandStep(void)
{
  D_8028B790 = D_8028B790 * 0x41a7 & 0x7ffffff;
}
