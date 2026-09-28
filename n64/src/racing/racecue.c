/* racecue.c -- the race-start cue table (countdown timings)
 */
#include "tgr/common.h"

/* -- declarations -- */
typedef struct BrRaceCue {      /* 0x10 bytes; the table ends at a 0 next */
  int start;                    /* 0x00 */
  int len;                      /* 0x04 */
  int x8;
  void *next;                   /* 0x0C */
} BrRaceCue;
extern BrRaceCue D_80270194[];
extern void *D_802701A0;          /* the first cue's next: 0 when the table is empty */
/* -- end declarations -- */

/* WHAT IT DOES: Step every race-start cue's start time back one frame.  The
 * PC twin (br_racebegin.c) has the same shape. */
/* @implements 0x8020072C tgr BrRaceCueRewind */
void BrRaceCueRewind(void)
{
  BrRaceCue *p;

  if (D_802701A0 == 0) {
    return;
  }
  p = D_80270194;
  do {
    p->start--;
    p++;
  } while (p->next != 0);
}
