/* objsel.c -- stepping the selected scene object (the object viewer)
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_80025C64;                 /* number of scene objects */
extern int D_8028C744;                 /* number of excluded objects */
extern int D_8028C754;                 /* the selected object */
extern int D_8028DDE0;                 /* pending step (+1 / -1) */
extern unsigned short D_80352580[];    /* the excluded objects */
/* -- end declarations -- */

/* WHAT IT DOES: Apply a pending step to the selected object, wrapping round
 * and skipping the excluded ones (object 0 always stops the walk), then
 * clear the step.  The PC twin is BrObjSelCycle (br_obj.c). */
/* @implements 0x80254D90 tgr BrObjSelCycle */
void BrObjSelCycle(void)
{
  int i;

  if (D_8028DDE0 != 0) {
    for (;;) {
      D_8028C754 = D_8028C754 + D_8028DDE0;
      if (D_8028C754 >= D_80025C64) {
        D_8028C754 = 0;
      }
      if (D_8028C754 < 0) {
        D_8028C754 = D_80025C64 - 1;
      }
      if (D_8028C754 == 0) {
        break;
      }
      for (i = 0; i < D_8028C744; i++) {
        if (D_8028C754 == D_80352580[i]) {
          goto done;
        }
      }
    }
done:
    D_8028DDE0 = 0;
  }
}
