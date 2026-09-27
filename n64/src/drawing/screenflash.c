/* screenflash.c -- the white screen flash drawn over the 3-D view
 */
#include "tgr/common.h"

/* -- declarations -- */
extern float D_8028B750;
extern char * D_8028A858;
extern int D_8028A8A0;
extern int D_8028AAEC;
extern float D_802A9620;
extern float D_802A9624;
extern int D_8031B2C8;
extern int D_8031B2CC;
extern int D_8031B2D0;
extern int D_8031B2D4;
extern float D_8028B76C;
extern float D_8028B774;
extern float D_8028B758;
extern int D_8028B784;
extern float D_8028B754;
extern float D_8028B75C;
extern int D_8028A85C;
extern int D_8028AAB0;
extern int D_8028AAB4;
extern float D_8028AAD8;
extern int D_8028B740;
extern int D_8028B744;
extern int D_8028B748;
extern int D_8028B74C;
extern float D_8028B760;
extern float D_8028B764;
extern float D_8028B768;
extern float D_8028B770;
extern int D_8028B778;
extern int D_8028B77C;
extern int D_8028B780;
extern unsigned char D_802A49C8;
extern unsigned char D_802A49D0;
extern double D_802A9628;
/* -- end declarations -- */

/* WHAT IT DOES: Cancel the white screen flash: its strength goes back to
 * zero, so nothing is drawn over the view. */
/* @implements 0x80223470 tgr BrScreenFlashClear */
void BrScreenFlashClear(void)
{
  D_8028B750 = 0;
}

/* WHAT IT DOES: Does nothing. An empty function the retail build kept after
 * the screen-flash code. */
/* @implements 0x80223A68 tgr BrStub80223A68 */
void BrStub80223A68(void)
{
}

/* WHAT IT DOES: Draw the white screen flash over the current view, its
 * opacity set by the flash strength (clamped to its maximum). */
/* @t4-pass 0x80223480 1 2026-09-26 compiles 17 best 137 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80223480 2 2026-09-26 compiles 17 best 137 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80223480 3 2026-09-26 compiles 17 best 135 moved 2  (n64/tools/n64permute.py) */
/* @t4-pass 0x80223480 4 2026-09-26 compiles 41 best 135 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80223480 tgr BrScreenFlashDraw */
void BrScreenFlashDraw(void)
{
  unsigned int *puVar1;
  unsigned int *puVar2;
  int iVar3;
  
  puVar1 = D_8028A858;
  if (D_802A9620 <= D_8028B750) {
    if (D_802A9624 < D_8028B750) {
      D_8028B750 = D_802A9624;
    }
    puVar2 = 1 + D_8028A858;
    D_8028A858 = D_8028A858 + 2;
    *puVar2 = 0;
    *puVar1 = 0xe7000000;
    puVar2 = D_8028A858;
    puVar1 = D_8028A858 + 1;
    D_8028A858 = D_8028A858 + 2;
    *puVar1 = 0;
    *puVar2 = 0xba001402;
    puVar2 = D_8028A858;
    puVar1 = D_8028A858 + 1;
    D_8028A858 = D_8028A858 + 2;
    *puVar1 = 0x504340;
    *puVar2 = 0xb900031d;
    puVar2 = D_8028A858;
    puVar1 = D_8028A858 + 1;
    D_8028A858 = D_8028A858 + 2;
    *puVar1 = 0xfffdf6fb;
    *puVar2 = 0xfcffffff;
    puVar1 = D_8028A858;
    puVar2 = D_8028A858 + 2;
    *D_8028A858 = 0xfa000000;
    D_8028A858 = puVar2;
    puVar1[1] = (int)(D_8028B750 * 255.0) & 0xffU | 0xffffff00;
    puVar2 = D_8028A858;
    puVar1 = D_8028A858 + 1;
    D_8028A858 = D_8028A858 + 2;
    *puVar1 = 0xc0;
    *puVar2 = 0xba000602;
    puVar1 = D_8028A858;
    iVar3 = D_8028AAEC * 0x14;
    puVar2 = D_8028A858 + 2;
    *D_8028A858 =
         (*(int *)(&D_8031B2D4 + iVar3) + *(int *)(&D_8031B2CC + iVar3) & 0x3ffU) << 0xf6000000 | 2 |
         (*(int *)(&D_8031B2C8 + iVar3) + *(int *)(&D_8031B2D0 + iVar3) & 0x3ffU) << 0xe;
    D_8028A858 = puVar2;
    puVar1[1] = (*(unsigned int *)(&D_8031B2CC + D_8028AAEC * 0x14) & 0x3ff) << 2 |
                (*(unsigned int *)(&D_8031B2C8 + D_8028AAEC * 0x14) & 0x3ff) << 0xe;
    puVar2 = D_8028A858;
    puVar1 = D_8028A858 + 1;
    D_8028A858 = D_8028A858 + 2;
    *puVar1 = 0;
    *puVar2 = 0xe7000000;
    puVar1 = D_8028A858;
    puVar2 = D_8028A858 + 2;
    *D_8028A858 = 0xba000602;
    D_8028A858 = puVar2;
    puVar1[1] = D_8028A8A0;
  }
}

/* WHAT IT DOES: Tell whether the screen fade has finished (its current step
 * has reached its target). */
/* @implements 0x80223850 tgr BrFadeDone */
int BrFadeDone(void)
{
  return D_8028B774 == D_8028B76C;
}

/* WHAT IT DOES: Put the screen fade straight at a level: target and current
 * level both become it, so nothing is left to fade. */
/* @implements 0x8022389C tgr BrFadeSet */
void BrFadeSet(float level)
{
  D_8028B754 = level;
  D_8028B75C = D_8028B754;
}

/* WHAT IT DOES: Tell whether the screen is fading in and not about to
 * reverse. */
/* @implements 0x802238B8 tgr BrFadeIsIn */
int BrFadeIsIn(void)
{
  return 0.0f < D_8028B758 && D_8028B784 == 0;
}

/* WHAT IT DOES: Tell whether the screen is fading out, or a reversal is
 * pending. */
/* @implements 0x802238FC tgr BrFadeIsOut */
int BrFadeIsOut(void)
{
  return D_8028B758 < 0.0f || D_8028B784 != 0;
}

/* WHAT IT DOES: Tell whether the screen fade has reached its target level
 * with no reversal pending. */
/* @implements 0x80223940 tgr BrFadeAtTarget */
int BrFadeAtTarget(void)
{
  return D_8028B75C == D_8028B754 && D_8028B784 == 0;
}

/* WHAT IT DOES: Advance the screen fade by one frame: moves the level
 * towards the target at the fade speed, and when a fade-in-then-out is
 * pending turns it round at the top. */
/* @t4-pass 0x80223F54 1 2026-09-26 compiles 21 best 228 moved 2  (n64/tools/n64permute.py) */
/* @t4-pass 0x80223F54 2 2026-09-26 compiles 21 best 228 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80223F54 tgr BrFadeStep */
void BrFadeStep(void)
{
  float fVar1;
  int iVar2;
  unsigned int uVar3;
  double dVar4;
  
  fVar1 = D_8028B754;
  if (D_8028B778 == 0) {
    if (D_8028B754 != D_8028B75C) {
      D_8028B75C = D_8028B75C + D_8028B758 * D_8028AAD8;
      if (D_8028B758 < 0.0) {
        if (D_8028B75C < D_8028B754) goto LAB_80224010;
      }
      else if ((D_8028B754 <= D_8028B75C) && (D_8028B75C = D_8028B754, D_8028B784 != 0)) {
        D_8028B758 = -D_8028B758;
        D_8028B754 = 0.0;
        D_8028B784 = 0;
LAB_80224010:
        D_8028B75C = fVar1;
      }
    }
  }
  else {
    D_8028B778 = 0;
  }
  fVar1 = D_8028B758;
  iVar2 = D_8028A85C * 4;
  *(int *)(iVar2 + -0x7fce4bf0) = D_8028B740;
  *(unsigned int *)(iVar2 + -0x7fce4be8) = D_8028B744;
  D_8028B748 = 0;
  D_8028B74C = D_8028AAB4;
  if (0.0 < fVar1) {
    D_8028B744 = (int)((float)(int)D_8028AAB0 * D_8028B75C) + 3U & 0xfffffffc;
    D_8028B740 = 0;
  }
  else if (fVar1 < 0.0) {
    uVar3 = ((D_8028AAB0 - (int)((float)(int)D_8028AAB0 * D_8028B75C)) - D_8028B740) + 3 &
            0xfffffffc;
    D_8028B744 = D_8028B744 + uVar3;
    D_8028B740 = D_8028B740 + uVar3;
    if ((int)D_8028AAB0 < (int)D_8028B744) {
      D_8028B744 = D_8028AAB0;
    }
  }
  else {
    D_8028B744 = D_8028AAB0;
    D_8028B740 = 0;
  }
  if (D_8028B780 == 0) {
    if (D_8028B76C != D_8028B774) {
      D_8028B774 = D_8028B774 + D_8028B770 * D_8028AAD8;
      if (D_8028B770 < 0.0) {
        if (D_8028B774 < D_8028B76C) {
          D_8028B774 = D_8028B76C;
        }
      }
      else if (D_8028B76C < D_8028B774) {
        D_8028B774 = D_8028B76C;
      }
    }
  }
  else {
    D_8028B780 = 0;
  }
  D_802A49D0 = (char)(int)((double)D_8028B774 * D_802A9628);
  if (D_8028B77C == 0) {
    if (D_8028B760 == D_8028B768) {
      dVar4 = (double)D_8028B768;
      goto LAB_802242c4;
    }
    D_8028B768 = D_8028B768 + D_8028B764 * D_8028AAD8;
    if (D_8028B764 < 0.0) {
      if (D_8028B760 <= D_8028B768) {
        dVar4 = (double)D_8028B768;
        goto LAB_802242c4;
      }
      D_8028B768 = D_8028B760;
    }
    else {
      if (D_8028B768 <= D_8028B760) {
        dVar4 = (double)D_8028B768;
        goto LAB_802242c4;
      }
      D_8028B768 = D_8028B760;
    }
  }
  else {
    D_8028B77C = 0;
  }
  dVar4 = (double)D_8028B768;
LAB_802242c4:
  D_802A49C8 = (char)(int)(dVar4 * D_802A9628);
}
