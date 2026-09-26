/* seasonmenu.c -- the season screen's lap-count setting
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_802723D0;
extern unsigned char D_802A49C4;
/* -- end declarations -- */

/* WHAT IT DOES: Raise the race length one step (to at most the eleventh
 * entry of the lap-count table) and set the number of laps from the table. */
/* @implements 0x80211840 tgr BrLapsMore */
void BrLapsMore(void)
{
  if (D_802723D0 < 10) {
    D_802723D0 = D_802723D0 + 1;
  }
  D_802A49C4 = (char)*(int *)(D_802723D0 * 4 + -0x7fd8db78);
}

/* WHAT IT DOES: Lower the race length one step (to at least the first entry
 * of the lap-count table) and set the number of laps from the table. */
/* @implements 0x8021187C tgr BrLapsFewer */
void BrLapsFewer(void)
{
  if (0 < D_802723D0) {
    D_802723D0 = D_802723D0 + -1;
  }
  D_802A49C4 = (char)*(int *)(D_802723D0 * 4 + -0x7fd8db78);
}
