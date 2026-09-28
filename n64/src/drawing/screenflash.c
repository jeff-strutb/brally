/* screenflash.c -- the white screen flash drawn over the 3-D view, and the
 * three fades stepped each frame: screen, effects volume, music volume
 */
#include "tgr/common.h"
#include "tgr/gbi.h"

/* -- declarations -- */
extern float D_8028B750;
extern Gfx *D_8028A858;
extern int D_8028A8A0;
extern int D_8028AAEC;
extern float D_802A9620;
extern float D_802A9624;
extern float D_8028B76C;
extern float D_8028B774;
extern float D_8028B758;
extern int D_8028B784;
extern float D_8028B754;
extern float D_8028B75C;
extern int D_8028A85C;
extern int D_8028AAB0;
extern int D_8028AAB4;
extern int D_8028A850;
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
typedef struct BrViewRect { int x; int y; int w; int h; int x10; } BrViewRect;
extern BrViewRect D_8031B2C8[];
extern int D_8031B410[2];               /* per frame buffer: the left bar last drawn there */
int BrFadeOutDone(void);
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
/* @implements 0x80223480 tgr BrScreenFlashDraw */
void BrScreenFlashDraw(void)
{
  if (!(D_8028B750 < D_802A9620)) {
    if (D_8028B750 > D_802A9624) {
      D_8028B750 = D_802A9624;
    }
    gDPPipeSync(D_8028A858++);
    gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
    gDPSetRenderMode(D_8028A858++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPSetCombine(D_8028A858++, 0xffffff, 0xfffdf6fb);   /* G_CC_PRIMITIVE, G_CC_PRIMITIVE */
    gDPSetPrimColor(D_8028A858++, 0, 0, 255, 255, 255, (int)(255.0f * D_8028B750));
    gDPSetColorDither(D_8028A858++, G_CD_DISABLE);
    gDPFillRectangle(D_8028A858++, D_8031B2C8[D_8028AAEC].x, D_8031B2C8[D_8028AAEC].y,
                     D_8031B2C8[D_8028AAEC].x + D_8031B2C8[D_8028AAEC].w,
                     D_8031B2C8[D_8028AAEC].y + D_8031B2C8[D_8028AAEC].h);
    gDPPipeSync(D_8028A858++);
    gDPSetColorDither(D_8028A858++, D_8028A8A0);
  }
}


/* WHAT IT DOES: Start a screen fade towards level (0 black .. 1 clear) over
 * the given seconds. Fading up starts at once; fading down while a fade-up
 * is still running only marks a reversal, which happens at the top. */
/* @implements 0x80223688 tgr BrFadeTo */
void BrFadeTo(float level, float seconds)
{
  D_8028B778 = 1;
  if (level < D_8028B75C || level == 0.0f) {
    if (D_8028B75C == 1.0f || D_8028B758 <= 0.0) {
      D_8028B758 = -1.0f / seconds;
      D_8028B754 = level;
    } else {
      D_8028B784 = 1;
    }
  } else {
    D_8028B754 = level;
    D_8028B758 = 1.0f / seconds;
  }
}


/* WHAT IT DOES: Start the sound-effects volume fading towards level (0..1)
 * over the given seconds. */
/* @implements 0x80223750 tgr BrSfxFadeTo */
void BrSfxFadeTo(float level, float seconds)
{
  D_8028B780 = 1;
  D_8028B76C = level;
  if (D_8028B76C < D_8028B774 || D_8028B76C == 0.0f) {
    D_8028B770 = -1.0f / seconds;
  } else {
    D_8028B770 = 1.0f / seconds;
  }
}


/* WHAT IT DOES: Start the music volume fading towards level (0..1) over the
 * given seconds. */
/* @implements 0x802237D0 tgr BrMusicFadeTo */
void BrMusicFadeTo(float level, float seconds)
{
  D_8028B77C = 1;
  D_8028B760 = level;
  if (D_8028B760 < D_8028B768 || D_8028B760 == 0.0f) {
    D_8028B764 = -1.0f / seconds;
  } else {
    D_8028B764 = 1.0f / seconds;
  }
}


/* WHAT IT DOES: Tell whether the sound-effects volume fade has finished
 * (its level has reached its target). */
/* @implements 0x80223850 tgr BrSfxFadeDone */
int BrSfxFadeDone(void)
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

/* WHAT IT DOES: Tell whether a screen fade-out has finished: fading down,
 * fully dark, and no reversal pending. */
/* @implements 0x80223988 tgr BrFadeOutDone */
int BrFadeOutDone(void)
{
  return D_8028B758 < 0.0f && D_8028B75C == 0.0f && D_8028B784 == 0;
}


/* WHAT IT DOES: Tell whether a screen fade-in has finished: fading up,
 * fully clear, and no reversal pending. */
/* @implements 0x802239F4 tgr BrFadeInDone */
int BrFadeInDone(void)
{
  return 0.0f < D_8028B758 && D_8028B75C == 1.0f && D_8028B784 == 0;
}


/* WHAT IT DOES: While a fade is under way, black out what lies outside
 * the clip window's left and right edges: the left bar from where the
 * other frame buffer's bar ended, the right bar only for a few frames (and
 * the whole screen once the fade reaches black); a finished fade-out
 * restarts that count.  The prim colour is a multi-line block (its w0 store
 * first), and each bar's edges are assigned where the ROM copies them. */
/* @implements 0x80223A70 tgr BrFadeBarsDraw */
void BrFadeBarsDraw(void)
{
  static int frames = 2;        /* 0x8028B788 */
  int l;
  int r;

  if (D_8028B75C != 1.0f) {
    gDPPipeSync(D_8028A858++);
    gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
    gDPSetRenderMode(D_8028A858++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCombine(D_8028A858++, 0xffffff, 0xfffdf6fb);   /* G_CC_PRIMITIVE, G_CC_PRIMITIVE */
    gDPSetScissor(D_8028A858++, G_SC_NON_INTERLACE, 0, 0, D_8028AAB0 << D_8028A850, D_8028AAB4 << D_8028A850);
    {
      Gfx *_g = (Gfx *)(D_8028A858++);

      _g->words.w0 = 0xfa00ffff;
      _g->words.w1 = 0;
    }
    if (D_8028B740 != 0) {
      l = D_8031B410[D_8028A85C ^ 1];
      if (D_8028B740 < l) {
        l = 0;
      }
      r = D_8028B740;
      if (l != 0) {
        l--;
      }
      if (BrFadeOutDone()) {
        frames = 3;
      }
      gDPFillRectangle(D_8028A858++, l << D_8028A850, 0, (r << D_8028A850) - 1, (D_8028AAB4 << D_8028A850) - 1);
    }
    if (D_8028B744 < D_8028AAB0) {
      l = D_8028B744;
      r = D_8028AAB0;
      if (frames != 0) {
        frames--;
        gDPFillRectangle(D_8028A858++, l << D_8028A850, 0, (r << D_8028A850) - 1, (D_8028AAB4 << D_8028A850) - 1);
      }
    } else if (D_8028B75C == 0.0f) {
      r = D_8028AAB0;
      if (frames != 0) {
        frames--;
        gDPFillRectangle(D_8028A858++, 0 << D_8028A850, 0, (r << D_8028A850) - 1, (D_8028AAB4 << D_8028A850) - 1);
      }
    }
    gDPPipeSync(D_8028A858++);
  }
}


/* WHAT IT DOES: Dim the whole screen: a black rectangle over it with
 * alpha 255 * (1 - level), so level 1 leaves the picture as it is and 0
 * blacks it out. */
/* @implements 0x80223DE0 tgr BrScreenDim */
void BrScreenDim(float level)
{
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, 0x100000);                 /* G_CYC_2CYCLE */
  gDPSetRenderMode(D_8028A858++, 0x0c080000, G_RM_CLD_SURF2); /* G_RM_PASS */
  gDPSetCombine(D_8028A858++, 0xffffff, 0xfffdf638);
  gDPSetPrimColor(D_8028A858++, 255, 255, 0, 0, 0, 255 - (int)(255.0f * level));
  gDPSetColorDither(D_8028A858++, G_CD_DISABLE);
  gDPFillRectangle(D_8028A858++, 0, 0, D_8028AAB0 << D_8028A850, D_8028AAB4 << D_8028A850);
  gDPPipeSync(D_8028A858++);
  gDPSetColorDither(D_8028A858++, D_8028A8A0);
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
      if (D_8028B758 < 0.0f) {
        if (D_8028B75C < D_8028B754) goto LAB_80224010;
      }
      else if ((D_8028B754 <= D_8028B75C) && (D_8028B75C = D_8028B754, D_8028B784 != 0)) {
        D_8028B758 = -D_8028B758;
        D_8028B754 = 0.0f;
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
  if (0.0f < fVar1) {
    D_8028B744 = (int)((float)(int)D_8028AAB0 * D_8028B75C) + 3U & 0xfffffffc;
    D_8028B740 = 0;
  }
  else if (fVar1 < 0.0f) {
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
      if (D_8028B770 < 0.0f) {
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
  D_802A49D0 = (char)(int)((double)D_8028B774 * 255.0);
  if (D_8028B77C == 0) {
    if (D_8028B760 == D_8028B768) {
      dVar4 = (double)D_8028B768;
      goto LAB_802242c4;
    }
    D_8028B768 = D_8028B768 + D_8028B764 * D_8028AAD8;
    if (D_8028B764 < 0.0f) {
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
  D_802A49C8 = (char)(int)(dVar4 * 255.0);
}
