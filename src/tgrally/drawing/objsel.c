/* objsel.c -- stepping the selected scene object (the object viewer)
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_80025C64;                 /* number of scene objects */
extern int D_8028C744;                 /* number of excluded objects */
extern int D_8028C754;                 /* the selected object */
extern int D_8028DDE0;                 /* pending step (+1 / -1) */
extern unsigned short D_80352580[];    /* the excluded objects */
extern int D_8028A850;                 /* high resolution */
extern unsigned short D_8028DDDC;
extern int D_8028DDD0;
extern int D_8028DDD4;
extern int D_8028DDD8;
extern int D_8028AADC;                 /* frames this step */
extern unsigned long long osClockRate;
void BrPerfMark(int a, int r, int g, int b, int al);
/* -- end declarations -- */

/* WHAT IT DOES: Reset the object viewer's display state: all objects
 * shown, and its step sizes set for the screen resolution (16 texels,
 * scaled up on a high-res screen). */
/* @implements 0x80254D4C tgr BrObjViewReset */
void BrObjViewReset(void)
{
  D_8028DDDC = 0xffff;
  D_8028DDD0 = 16 << (D_8028A850 * 2);
  D_8028DDD4 = 16 << D_8028A850;
  D_8028DDD8 = 0;
}

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

/* WHAT IT DOES: The object viewer's frame end: a perf-meter mark, then
 * timing arithmetic whose results are thrown away (a frame-count loop and
 * two 1 ms cycle counts -- the display they fed is compiled out), then the
 * viewer's display state is reset. */
/* @implements 0x80254E3C tgr BrObjViewFrame */
void BrObjViewFrame(void)
{
  int t;
  float f;

  BrPerfMark(0, 255, 255, 255, 255);
  for (t = D_8028AADC * 1000; t >= 1000; t -= 16667) {
  }
  f = (float)(1000 * osClockRate / 1000000);
  f = (float)(1000 * osClockRate / 1000000);
  BrObjViewReset();
}
