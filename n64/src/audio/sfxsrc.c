/* sfxsrc.c -- short sound-effect triggers
 */
#include "tgr/common.h"

/* -- declarations -- */
void BrSfxSrcTrigger(int param_1);
void func_80257B04(short param_1,int param_2,int param_3,int param_4);
extern int D_8028B7EC;
typedef struct BrSfxSrc {       /* a sound-effect source, 0x18 bytes */
  int x0;                       /* 0x00  the three words passed to the player */
  int x4;
  int x8;
  int xc;                       /* 0x0C */
  int x10;                      /* 0x10 */
  int x14;
} BrSfxSrc;
extern BrSfxSrc D_8028BC04[];
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
void BrSfxSrcTrigger(int n)
{
  func_80257B04(3, D_8028BC04[n].x0, D_8028BC04[n].xc, D_8028BC04[n].x10);
  D_8028B7EC = n;
}

