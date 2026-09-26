/* fixed.c -- number conversions
 */
#include "tgr/common.h"

/* -- declarations -- */

/* -- end declarations -- */

/* WHAT IT DOES: Convert a float to an int, truncating toward zero. */
/* @t4-pass 0x802562E0 1 2026-09-26 compiles 1 best 3 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x802562E0 2 2026-09-26 compiles 1 best 3 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x802562E0 3 2026-09-26 compiles 1 best 3 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x802562E0 4 2026-09-26 compiles 1 best 3 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x802562E0 tgr BrFloatToInt */
int BrFloatToInt(float param_1)
{
  return (int)param_1;
}
