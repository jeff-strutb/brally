/* entinit.c -- resetting car entities
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_80226100(int param_1);
void BrPadInit(int param_1);
extern int D_8031B760;
extern int D_8036A8E0;
extern int D_8036AE50;
/* -- end declarations -- */

/* WHAT IT DOES: Reset every car slot for a new session: runs the per-car
 * reset on each car record (0x2090 bytes apiece) and clears its matching
 * input record. */
/* @t4-pass 0x80200154 1 2026-09-26 compiles 16 best 5 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80200154 2 2026-09-26 compiles 16 best 2 moved 3  (n64/tools/n64permute.py) */
/* @t4-pass 0x80200154 3 2026-09-26 compiles 16 best 2 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80200154 tgr BrEntAllReset */
void BrEntAllReset(void)
{
  char *puVar1;
  char *puVar2;
  
  puVar1 = &D_8036A8E0;
  puVar2 = &D_8031B760;
  do {
    func_80226100(puVar2);
    BrPadInit(puVar1);
    puVar2 = puVar2 + 0x2090;
    puVar1 = puVar1 + 0x15c;
  } while (puVar1 != (char *)(&D_8036AE50));
}
