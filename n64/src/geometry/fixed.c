/* fixed.c -- number conversions
 */
#include "tgr/common.h"

/* -- declarations -- */

/* -- end declarations -- */

/* WHAT IT DOES: Convert a float to an int, truncating toward zero. */
/* @implements 0x802562E0 tgr BrFloatToInt */
int BrFloatToInt(float param_1)
{
  return (int)param_1;
}
