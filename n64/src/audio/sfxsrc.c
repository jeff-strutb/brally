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
typedef struct BrSndVec { float x, y, z; } BrSndVec;
typedef struct BrSndNearest {   /* the nearest positional sound, 0x8028B7A0 */
  BrSndVec pos;                 /* 0x00 */
  BrSndVec posPrev;             /* 0x0C */
  void *pObj;                   /* 0x18 */
  void *pObjPrev;               /* 0x1C */
  BrSndVec objPosPrev;          /* 0x20 */
  int f84;                      /* 0x2C  (the PC record's names) */
  int f88;                      /* 0x30 */
  int f8C;                      /* 0x34 */
  int f90;                      /* 0x38 */
  float metric;                 /* 0x3C  distance of the best offer */
  float f98;                    /* 0x40  its playback rate */
  int f9C;                      /* 0x44  its volume */
  int fA0;                      /* 0x48 */
} BrSndNearest;
extern BrSndNearest D_8028B7A0;
extern int D_8028B940;                  /* the chosen track */
void BrSndNearestOffer(int f8C, int f84, int f9C, float hz, void *pPos, void *pListener);
float BrVec3Dist(void *a, void *b);
void BrVec3Sub(float out[3], float a[3], float b[3]);
float BrVec3Dot(float a[3], float b[3]);
float BrVec3Length(float v[3]);
void BrVec3DivBy(float v[3], float d);
extern float D_8028AAD8;                /* seconds this frame */
/* -- end declarations -- */

/* WHAT IT DOES: The Doppler pitch factor for a sound: with the listener
 * moving from lPrev to l and the source from sPrev to s this frame, their
 * speeds along the line between them give (1 + vl/c) / (1 - vs/c), c being
 * the speed of sound, 343. */
/* @implements 0x8022B0F8 tgr BrSndDoppler */
float BrSndDoppler(float l[3], float lPrev[3], float s[3], float sPrev[3])
{
  float dir[3];
  float len;
  float lv[3];
  float sv[3];
  float vs;
  float vl;

  BrVec3Sub(dir, l, s);
  BrVec3Sub(sv, s, sPrev);
  BrVec3Sub(lv, l, lPrev);
  len = BrVec3Length(dir);
  if (len != 0.0f) {
    BrVec3DivBy(dir, len);
  }
  vs = -BrVec3Dot(lv, dir) / D_8028AAD8;
  vl = BrVec3Dot(sv, dir) / D_8028AAD8;
  return (vl / 343.0f + 1.0f) / (1.0f - vs / 343.0f);
}

/* WHAT IT DOES: Offer a positional sound: if its source is nearer the
 * listener than the best offer so far this frame, it becomes the nearest
 * sound (position, listener, ids, rate and volume). */
/* @implements 0x8022B494 tgr BrSndNearestOffer */
void BrSndNearestOffer(int f8C, int f84, int f9C, float hz, void *pPos, void *pListener)
{
  float d;

  d = BrVec3Dist(pPos, (char *)pListener + 0x30);
  if (d < D_8028B7A0.metric) {
    D_8028B7A0.pos.x = ((BrSndVec *)pPos)->x;
    D_8028B7A0.pos.y = ((BrSndVec *)pPos)->y;
    D_8028B7A0.pos.z = ((BrSndVec *)pPos)->z;
    D_8028B7A0.pObj = pListener;
    D_8028B7A0.metric = d;
    D_8028B7A0.f84 = f84;
    D_8028B7A0.f8C = f8C;
    D_8028B7A0.f98 = hz;
    D_8028B7A0.f9C = f9C;
  }
}

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


/* WHAT IT DOES: Forget the nearest positional sound's choice so the next
 * offer wins (distance back to "far"). */
/* @implements 0x8022B404 tgr BrSndNearestInvalidate */
void BrSndNearestInvalidate(void)
{
  D_8028B7A0.f84 = -1;
  D_8028B7A0.f8C = -1;
  D_8028B7A0.metric = 8589934592.0f;
}

/* WHAT IT DOES: Reset the nearest positional sound completely: no source
 * playing, no object, zero positions, no choice. */
/* @implements 0x8022B428 tgr BrSndNearestReset */
void BrSndNearestReset(void)
{
  D_8028B7EC = -1;
  D_8028B7A0.pos.x = 0.0f;
  D_8028B7A0.pos.y = 0.0f;
  D_8028B7A0.pos.z = 0.0f;
  D_8028B7A0.posPrev.x = 0.0f;
  D_8028B7A0.posPrev.y = 0.0f;
  D_8028B7A0.posPrev.z = 0.0f;
  D_8028B7A0.pObj = 0;
  D_8028B7A0.pObjPrev = 0;
  D_8028B7A0.objPosPrev.x = 0.0f;
  D_8028B7A0.objPosPrev.y = 0.0f;
  D_8028B7A0.objPosPrev.z = 0.0f;
  D_8028B7A0.f84 = -1;
  D_8028B7A0.f88 = -1;
  D_8028B7A0.f8C = -1;
  D_8028B7A0.f90 = -1;
  D_8028B7A0.metric = 8589934592.0f;
  D_8028B7A0.fA0 = 0;
  D_8028B7A0.f9C = 0;
}

/* WHAT IT DOES: Offer the current track's ambient sound (full volume; at
 * 22 kHz on tracks 2 and 7, 11 kHz on 4 and 9) as the nearest positional
 * sound. */
/* @implements 0x8022B954 tgr BrSndNearestOfferTrack */
void BrSndNearestOfferTrack(int f8C, void *pPos, void *pListener)
{
  int f84 = -1;
  float hz = 11000.0f;
  int f9C = 0x80;

  switch (D_8028B940) {
  case 2:
  case 7:
    f84 = 0;
    hz = 22000.0f;
    f9C = 0xff;
    break;
  case 4:
  case 9:
    f84 = 0;
    hz = 11000.0f;
    f9C = 0xff;
    break;
  }
  if (f84 != -1) {
    BrSndNearestOffer(f8C, f84, f9C, hz, pPos, pListener);
  }
}

/* WHAT IT DOES: Offer the default positional sound (sample 15, loud) on
 * tracks 4 and 9. */
/* @implements 0x8022BA10 tgr BrSndNearestOfferDefault */
void BrSndNearestOfferDefault(int f8C, void *pPos, void *pListener)
{
  int f84 = -1;
  int mode = D_8028B940;
  int f9C = 0x80;
  if (mode == 4 || mode == 9) {
    f84 = 0xf;
    f9C = 0x180;
  }
  if (f84 != -1) {
    BrSndNearestOffer(f8C, f84, f9C, 11000.0f, pPos, pListener);
  }
}
