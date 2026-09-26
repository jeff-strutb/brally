/* credits.c -- the credits sequence
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_8021D84C(int param_1);
extern int D_80315E88;
extern int D_80315E8C;
extern int D_80315E90;
/* -- end declarations -- */

/* WHAT IT DOES: Draw the three car models the credits sequence shows. */
/* @implements 0x80206830 tgr BrCreditsDrawCars */
void BrCreditsDrawCars(void)
{
  func_8021D84C(D_80315E88);
  func_8021D84C(D_80315E8C);
  func_8021D84C(D_80315E90);
}
