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
