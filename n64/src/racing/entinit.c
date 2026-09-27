/* entinit.c -- resetting car entities
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_80226100(int *param_1);
void BrPadInit(unsigned int *param_1);
extern int D_8031B760[4][0x824];
extern unsigned int D_8036A8E0[4][0x57];
/* -- end declarations -- */

/* WHAT IT DOES: Reset every car slot for a new session: runs the per-car
 * reset on each car record (0x2090 bytes apiece) and clears its matching
 * input record. */
/* @implements 0x80200154 tgr BrEntAllReset */
void BrEntAllReset(void)
{
  int i;

  for (i = 0; i < 4; i++) {
    func_80226100(D_8031B760[i]);
    BrPadInit(D_8036A8E0[i]);
  }
}
