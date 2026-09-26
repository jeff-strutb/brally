/* anim.c -- animation playback modes for a model's animated parts
 */
#include "tgr/common.h"

/* -- declarations -- */
int func_8021D6DC();
/* -- end declarations -- */

/* WHAT IT DOES: Play every animation in the set through once and stop at
 * the end: clears both the repeat and the bounce-back bits. */
/* @implements 0x8021D7E0 tgr BrAnimSetOnce */
void BrAnimSetOnce(int param_1)

{
  func_8021D6DC(param_1,0,3);
}

/* WHAT IT DOES: Play every animation in the set on repeat, restarting from
 * the beginning each time round: sets repeat, clears bounce-back. */
/* @implements 0x8021D804 tgr BrAnimSetLoop */
void BrAnimSetLoop(int param_1)

{
  func_8021D6DC(param_1,1,2);
}

/* WHAT IT DOES: Play every animation in the set back and forth: sets both
 * the repeat and the bounce-back bits. */
/* @implements 0x8021D828 tgr BrAnimSetPingPong */
void BrAnimSetPingPong(int param_1)

{
  func_8021D6DC(param_1,3,0);
}
