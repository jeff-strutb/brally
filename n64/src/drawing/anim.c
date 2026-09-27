/* anim.c -- animation playback modes for a model's animated parts
 */
#include "tgr/common.h"

/* -- declarations -- */
typedef struct BrAnim { char pad00[0x10]; unsigned short flags; } BrAnim;   /* 0x10: bit 0 repeat, bit 1 bounce */
typedef struct BrAnimList { int n; BrAnim *anim[1]; } BrAnimList;
typedef struct BrAnimSet { int x0; BrAnimList *list; } BrAnimSet;
void BrAnimSetFlags(BrAnimSet *set, int on, int off);
/* -- end declarations -- */

/* WHAT IT DOES: Set and clear playback-mode bits on every animation in a
 * set (a set with no list is left alone). */
/* @implements 0x8021D6DC tgr BrAnimSetFlags */
void BrAnimSetFlags(BrAnimSet *set, int on, int off)
{
  int i;
  int n;
  BrAnim *a;

  off = ~off;
  if (set->list != 0) {
    n = set->list->n;
    for (i = 0; i < n; i++) {
      a = set->list->anim[i];
      a->flags |= on;
      a->flags &= off;
    }
  }
}

/* WHAT IT DOES: Play every animation in the set through once and stop at
 * the end: clears both the repeat and the bounce-back bits. */
/* @implements 0x8021D7E0 tgr BrAnimSetOnce */
void BrAnimSetOnce(int param_1)

{
  BrAnimSetFlags((BrAnimSet *)param_1,0,3);
}

/* WHAT IT DOES: Play every animation in the set on repeat, restarting from
 * the beginning each time round: sets repeat, clears bounce-back. */
/* @implements 0x8021D804 tgr BrAnimSetLoop */
void BrAnimSetLoop(int param_1)

{
  BrAnimSetFlags((BrAnimSet *)param_1,1,2);
}

/* WHAT IT DOES: Play every animation in the set back and forth: sets both
 * the repeat and the bounce-back bits. */
/* @implements 0x8021D828 tgr BrAnimSetPingPong */
void BrAnimSetPingPong(int param_1)

{
  BrAnimSetFlags((BrAnimSet *)param_1,3,0);
}
