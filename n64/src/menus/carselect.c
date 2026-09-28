/* carselect.c -- the car-select screen
 */
#include "tgr/common.h"
#include "tgr/gbi.h"

/* -- declarations -- */
int BrCarModelPresent(int param_1);
extern int D_8020C688;
extern int D_8020C68C;
extern int D_80272070;
extern char *D_8031C5BC;
extern char D_8028AE24;
extern Gfx *D_8028A858;
int sprintf(char *buf, char *fmt, ...);
/* -- end declarations -- */

/* WHAT IT DOES: Draw one of the car-select screen's stat bars at (x, y),
 * w by h, in fill mode: a dark frame, the empty bar inset by 3 pixels, and
 * the filled part as the given fraction of its width. */
/* @implements 0x8020C460 tgr BrCarStatBarDraw */
void BrCarStatBarDraw(int x, int y, int w, int h, float frac)
{
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_FILL);
  gDPSetRenderMode(D_8028A858++, 0, 0);
  gDPSetCombine(D_8028A858++, 0xffffff, 0xfffdf6fb);
  gDPSetFillColor(D_8028A858++, 1);
  gDPFillRectangle(D_8028A858++, x, y, x + w, y + h);
  x += 3;
  gDPPipeSync(D_8028A858++);
  gDPSetFillColor(D_8028A858++, 0x1c1);
  gDPFillRectangle(D_8028A858++, x, y + 3, x + w - 6, y + h - 3);
  gDPPipeSync(D_8028A858++);
  gDPSetFillColor(D_8028A858++, 0x781);
  gDPFillRectangle(D_8028A858++, x, y + 3, x + (int)((w - 6) * frac), y + h - 3);
}

/* WHAT IT DOES: Tell whether car n may be picked on the car-select screen:
 * cars 9 and up only while the flag at 0x80272070 is clear, the car's bit
 * must be set in the season record's unlock mask, and its model must be
 * present. */
/* @implements 0x8020C66C tgr BrCarSelectable */
int BrCarSelectable(int n)
{
  return (D_80272070 == 0 || n < 9) && (*(unsigned short *)(D_8031C5BC + 0xcc) & (1 << n)) &&
         BrCarModelPresent(n);
}

/* WHAT IT DOES: Format a time in seconds as minutes'seconds"hundredths.
 * Same arithmetic as the PC twin BrTimeFormat (br_timefmt.c). */
/* @implements 0x8020CF44 tgr BrTimeFormat */
void BrTimeFormat(char *psz, float t)
{
  int total = (int)(t * 100.0f);
  int whole = total / 100;
  int minutes;

  total -= whole * 100;
  minutes = whole / 60;
  whole -= minutes * 60;
  sprintf(psz, "%d'%02d\"%02d", minutes, whole, total);
}

/* WHAT IT DOES: Tell whether car n has a model record loaded (its entry in
 * the car model table is non-zero). */
/* @implements 0x8021D6B8 tgr BrCarModelPresent */
int BrCarModelPresent(int param_1)
{
  return *(int *)(&D_8028AE24 + param_1 * 0x60) != 0;
}
