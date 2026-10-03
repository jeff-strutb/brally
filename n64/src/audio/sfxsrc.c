/* sfxsrc.c -- short sound-effect triggers
 */
#include "tgr/common.h"

/* -- declarations -- */
void BrSfxSrcTrigger(int param_1);
void func_80257B04(short param_1,int param_2,int param_3,int param_4);
extern int D_8028B7EC;
typedef struct BrSfxSrc {       /* a car sound sample, 0x18 bytes */
  unsigned char *data;          /* 0x00  where it was unpacked */
  int rom;                      /* 0x04  its packed file */
  int x8;
  unsigned int size;            /* 0x0C  unpacked size */
  unsigned int loop;            /* 0x10  loop start (-1 in the table: the end) */
  unsigned int loopLen;         /* 0x14  bytes copied past the end for the loop (-1: all) */
} BrSfxSrc;
extern BrSfxSrc D_8028BC04[16];
int BrRomReadSize(int rom);
unsigned int BrRomUnpack(void *dst, int rom, unsigned int *out);
void BrFatal(char *msg);
void BrSfxVoicePlay(short v, unsigned int start, unsigned int end, unsigned int loop);
void osSyncPrintf();
extern int D_8028B7F4;                  /* player count */
extern unsigned char D_80324550[];      /* the car sound buffer, 0x29FE0 bytes */
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
typedef struct BrSndVoice {     /* a mixer voice, 0x18 bytes */
  unsigned int pos;
  int x4;
  unsigned long long rate;      /* 0x08  32.32 step, 0 = silent */
  int vol;                      /* 0x10 */
  unsigned int baseVol;         /* 0x14  packed left << 16 | right */
} BrSndVoice;
extern BrSndVoice D_802A4920[6];
extern unsigned int D_802A497C;
typedef struct BrSndCarLink { char pad00[0x68]; unsigned int flags; } BrSndCarLink;
extern BrSndCarLink *D_8031C630;        /* car 0's link record */
typedef struct BrSndView { int x, y, w, h; int car; } BrSndView;
extern BrSndView D_8031B2C8[2];         /* the players' views */
typedef struct BrSndCar { char pad0000[0xF48]; int camMode; char padf4c[0x2090 - 0xF4C]; } BrSndCar;
extern BrSndCar D_8031B760[4];
extern int D_8028AB0C;                  /* number of players */
void BrSndPan(float pos[3], float m[4][4], float *left, float *right, int *vol, int narrow);
float BrSndDoppler(float l[3], float lPrev[3], float s[3], float sPrev[3]);
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

/* WHAT IT DOES: Stereo placement of a sound at pos for a listener whose
 * matrix is m: the source's offset along the listener's side axis, clamped
 * to +-10 (and scaled by 0.4 when narrow), gives the left and right levels
 * (the quieter side boosted by 1.7, the louder pulled 60% of the way to
 * full); the volume is 1024 over the distance, taken as at least 32. */
/* @implements 0x8022B1D4 tgr BrSndPan */
void BrSndPan(float pos[3], float m[4][4], float *left, float *right, int *vol, int narrow)
{
  float d[3];
  float l;
  float r;
  float dist;

  BrVec3Sub(d, pos, m[3]);
  l = d[0] * m[1][0] + d[1] * m[1][1] + d[2] * m[1][2];
  if (10.0f < l) {
    l = 10.0f;
  } else if (l < -10.0f) {
    l = -10.0f;
  }
  if (narrow != 0) {
    l *= 0.4f;
  }
  l = (l + 10.0f) / 20.0f;
  r = 1.0f - l;
  if (l < r) {
    l *= 1.7f;
    r += 0.6f * (1.0f - r);
  } else {
    r *= 1.7f;
    l += 0.6f * (1.0f - l);
  }
  *left = l;
  *right = r;
  dist = BrVec3Length(d);
  if (dist < 32.0) {
    dist = 32.0f;
  }
  *vol = 1024.0f / dist;
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

/* WHAT IT DOES: Once a frame, act on the nearest positional sound (voice
 * 3).  Nothing happens while car 0 is out of the race, or while a one-shot
 * effect still plays (once it ends the one-shot is forgotten).  No sound:
 * the voice is silenced.  A different winner from last frame: its level is
 * halved.  The same winner: Doppler against the listener (a negative base
 * rate compares the current position with itself), pan and volume from the
 * listener matrix, the sample started the first time, and -- when the
 * viewed cars use camera mode 0 -- the 32.32 rate and the packed stereo
 * level (halved on the first frame).  Then this frame's winner becomes
 * last frame's.
 * RESIDUE (128 words, instruction count 261/263): register colouring -- the
 * ROM holds f8C in a0 and pObj in v1 where ours has v1/v0, and the tail
 * reaches objPosPrev.z through pObj + 0x30 in v0 (then reloads pObj).
 * Levers that landed: the frame (an unused int above ratio and seven unused
 * floats below gainB), the early exit as a goto past the reset (it splits
 * the base setup onto the skip edge), the first packed store through its
 * own absolute symbol, the Doppler result assigned before the multiply. */
/* @t4-pass 0x8022B534 1 2026-10-03 compiles 119 best 128 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8022B534 2 2026-10-03 compiles 118 best 128 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8022B534 */
/* @implements 0x8022B534 tgr BrSndNearestCommit */
void BrSndNearestCommit(void)
{
  int unused;
  float ratio;
  int vol;
  float gainA;
  float gainB;
  float unused2[7];

  if (D_8031C630->flags & 1)
    return;
  if (D_8028B7EC != -1) {
    if (D_802A4920[3].rate != 0)
      return;
    D_8028B7EC = -1;
    D_8028B7A0.f90 = -1;
    D_8028B7A0.fA0 = 0;
    goto skip;
  }
skip:
  if (D_8028B7A0.f8C == -1) {
    D_802A4920[3].baseVol = 0;
    D_802A4920[3].vol = 0;
    D_802A4920[3].rate = 0;
  } else if (D_8028B7A0.f8C == D_8028B7A0.f90 && D_8028B7A0.pObj == D_8028B7A0.pObjPrev) {
    if (D_8028B7A0.f98 < 0.0f) {
      ratio = BrSndDoppler(&D_8028B7A0.pos.x, &D_8028B7A0.pos.x,
                           (float *)((char *)D_8028B7A0.pObj + 0x30),
                           &D_8028B7A0.objPosPrev.x);
      ratio = -D_8028B7A0.f98 * ratio;
      BrSndPan(&D_8028B7A0.pos.x, D_8028B7A0.pObj, &gainA, &gainB, &vol, 1);
    } else {
      ratio = BrSndDoppler(&D_8028B7A0.pos.x, &D_8028B7A0.posPrev.x,
                           (float *)((char *)D_8028B7A0.pObj + 0x30),
                           &D_8028B7A0.objPosPrev.x);
      ratio = D_8028B7A0.f98 * ratio;
      BrSndPan(&D_8028B7A0.pos.x, D_8028B7A0.pObj, &gainA, &gainB, &vol, 0);
    }
    vol = vol * D_8028B7A0.f9C >> 8;
    if (D_8028B7A0.fA0 == 0) {
      D_8028B7A0.fA0 = 1;
      BrSfxVoicePlay(3, (unsigned int)D_8028BC04[D_8028B7A0.f84].data, D_8028BC04[D_8028B7A0.f84].size,
                     D_8028BC04[D_8028B7A0.f84].loop);
    }
    if (D_8031B760[D_8031B2C8[0].car].camMode == 0
        && (D_8028AB0C == 1 || D_8031B760[D_8031B2C8[1].car].camMode == 0)) {
      D_802A4920[3].rate = (double)(ratio * (1.0f / 11000.0f)) * 4294967296.0;
      if (D_802A4920[3].baseVol == 0) {
        D_802A497C = ((int)((float)((int)(vol * gainA) << 16) + gainB * vol) >> 1) & 0x7FFF7FFF;
      } else {
        D_802A4920[3].baseVol = (float)((int)(vol * gainA) << 16) + gainB * vol;
      }
    }
    D_8028B7A0.posPrev.x = D_8028B7A0.pos.x;
    D_8028B7A0.posPrev.y = D_8028B7A0.pos.y;
    D_8028B7A0.posPrev.z = D_8028B7A0.pos.z;
    D_8028B7A0.objPosPrev.x = ((float *)D_8028B7A0.pObj)[12];
    D_8028B7A0.objPosPrev.y = ((float *)D_8028B7A0.pObj)[13];
    D_8028B7A0.objPosPrev.z = ((float *)D_8028B7A0.pObj)[14];
  } else {
    D_8028B7A0.fA0 = 0;
    D_802A4920[3].baseVol = (D_802A4920[3].baseVol >> 1) & 0x7FFF7FFF;
  }
  D_8028B7A0.f90 = D_8028B7A0.f8C;
  D_8028B7A0.pObjPrev = D_8028B7A0.pObj;
  D_8028B7A0.f88 = D_8028B7A0.f84;
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
  func_80257B04(3, (int)D_8028BC04[n].data, D_8028BC04[n].size, D_8028BC04[n].loop);
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

/* WHAT IT DOES: Load the 16 car sound samples into the car sound buffer
 * (0x29FE0 bytes at 0x80324550): each sample's size is read from ROM, a -1
 * loop start or loop length becomes the size, the sample is unpacked and its
 * first loop-length bytes are copied again after its end (so a looping voice
 * can read past it); a sample that would not fit is pointed at the buffer
 * start and reported.  Prints the space used, is fatal on overflow, and
 * starts sample 0 on voice 0 -- and on voices 2 and 4 with two and three
 * players.
 * RESIDUE (44): the three voice starts.  The ROM loads each argument through
 * its own lui (a3, a2, a1 in that order); ours keeps the table's address in
 * s0 across the calls. */
/* @t4-pass 0x8022BAA0 1 2026-10-03 compiles 116 best 44 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8022BAA0 2 2026-10-03 compiles 116 best 44 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8022BAA0 */
/* @implements 0x8022BAA0 tgr BrCarSfxLoad */
void BrCarSfxLoad(void)
{
  unsigned int base;
  unsigned int pos;
  int i;
  unsigned int j;

  base = (unsigned int)D_80324550;
  pos = base;
  for (i = 0; i < 16; i++) {
    D_8028BC04[i].size = BrRomReadSize(D_8028BC04[i].rom);
    if (D_8028BC04[i].loop == -1) {
      D_8028BC04[i].loop = D_8028BC04[i].size;
    }
    if (D_8028BC04[i].loopLen == -1) {
      D_8028BC04[i].loopLen = D_8028BC04[i].size;
    }
    if (pos + D_8028BC04[i].size + D_8028BC04[i].loopLen > base + 0x29fe0) {
      D_8028BC04[i].data = (unsigned char *)base;
      osSyncPrintf("ERROR: Sound buffer allocation overflow on car sound %d\n", i);
    } else {
      D_8028BC04[i].data = (unsigned char *)pos;
      BrRomUnpack((void *)pos, D_8028BC04[i].rom, 0);
      for (j = 0; j < D_8028BC04[i].loopLen; j++) {
        D_8028BC04[i].data[j + D_8028BC04[i].size] = D_8028BC04[i].data[j];
      }
    }
    pos += D_8028BC04[i].size + D_8028BC04[i].loopLen;
  }
  osSyncPrintf("Car sound effect space used: %d/%d\n", pos - base, 0x29fe0);
  if (pos - base > 0x29fe0) {
    BrFatal("Car sound effect overflow");
  }
  BrSfxVoicePlay(0, (unsigned int)D_8028BC04[0].data, D_8028BC04[0].size, D_8028BC04[0].loop);
  if (D_8028B7F4 > 1) {
    BrSfxVoicePlay(2, (unsigned int)D_8028BC04[0].data, D_8028BC04[0].size, D_8028BC04[0].loop);
  }
  if (D_8028B7F4 > 2) {
    BrSfxVoicePlay(4, (unsigned int)D_8028BC04[0].data, D_8028BC04[0].size, D_8028BC04[0].loop);
  }
}
