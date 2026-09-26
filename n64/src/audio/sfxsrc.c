/* sfxsrc.c -- short sound-effect triggers
 */
#include "tgr/common.h"

/* -- declarations -- */
void BrSfxSrcTrigger(int param_1);
void func_80257B04(short param_1,int param_2,int param_3,int param_4);
extern int D_8028B7EC;
extern int D_8028BC04;
extern int D_8028BC10;
extern int D_8028BC14;
/* -- end declarations -- */

/* WHAT IT DOES: Play the game's ordinary beep, the one the countdown uses
 * for three, two and one. */
/* @implements 0x8022B3C4 tgr BrSfxSrcBeep */
void BrSfxSrcBeep(void)
{
  BrSfxSrcTrigger(0xd);
}

/* WHAT IT DOES: Play the countdown's second beep, the higher one used for
 * go. */
/* @implements 0x8022B3E4 tgr BrSfxSrcBeep2 */
void BrSfxSrcBeep2(void)
{
  BrSfxSrcTrigger(0xe);
}

/* WHAT IT DOES: Play sound effect n from the effect table (its sample,
 * volume and pitch) and remember it as the last one played. */
/* @implements 0x8022B370 tgr BrSfxSrcTrigger */
void BrSfxSrcTrigger(int param_1)
{
  func_80257B04(3,(&D_8028BC04)[param_1 * 6],(&D_8028BC10)[param_1 * 6],
               (&D_8028BC14)[param_1 * 6]);
  D_8028B7EC = param_1;
}
