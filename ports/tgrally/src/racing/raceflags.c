/* raceflags.c -- per-race option flags
 */
#include "tgr/common.h"


/* -- declarations -- */
void BrRaceFlagsApply(void);
extern int D_8028AA78;
extern int D_8028AA80;
extern int D_8028AA84;
extern int D_8028AA8C;
extern int D_8026FF08;
#include "tgr/track.h"
#define D_80025C50 BES32(D_80025C00.x50)
/* -- end declarations -- */

/* WHAT IT DOES: Set the race-kind flags from a kind number (0-4): clears
 * the three option flags, sets the main flag for kinds 1-4 and the one
 * option flag that kind uses, then refreshes the track objects that depend
 * on them. */
/* @implements 0x80200050 tgr BrRaceSetKind */
void BrRaceSetKind(int kind)
{
  D_8028AA80 = D_8028AA8C = D_8028AA84 = 0;
  switch (kind) {
  case 0:
    D_8028AA78 = 0;
    break;
  case 1:
    D_8028AA78 = 1;
    break;
  case 2:
    D_8028AA78 = 1;
    D_8028AA8C = 1;
    break;
  case 3:
    D_8028AA78 = 1;
    D_8028AA84 = 1;
    break;
  case 4:
    D_8028AA78 = 1;
    D_8028AA80 = 1;
    break;
  }
  BrRaceFlagsApply();
}


/* WHAT IT DOES: Tell whether the race is shown split: any of the split
 * options is set, or two players are racing. */
/* @implements 0x8022F900 tgr BrRaceSplitScreen */
int BrRaceSplitScreen(void)
{
  return D_8028AA80 != 0 || D_8028AA84 != 0 || D_8028AA8C != 0 || D_80025C50 == 0 ||
         D_8026FF08 == 2;
}

