/* particles.c -- the particle pool: 256 records of 0x20 bytes on index
 * linked lists (index 0 ends a list), one free and one live.
 */
#include "tgr/common.h"
#include "tgr/car.h"
#include "tgr/gbi.h"

/* -- declarations -- */
/* A particle (0x20 bytes). */
typedef struct BrParticle {
    float pos[3];               /* 0x00 */
    float vel[3];               /* 0x0C */
    float size;                 /* 0x18 */
    unsigned short next;        /* 0x1C  index of the next on its list */
    unsigned char x1e;          /* 0x1E */
    unsigned char x1f;          /* 0x1F */
} BrParticle;
extern BrParticle D_80366A80[256];      /* indexed from 1 */
extern short D_80368A7C;
extern unsigned short D_8028C830;       /* head of the free list */
extern unsigned short D_8028C834;       /* head of the live list */
extern unsigned short D_8028C838;
extern unsigned short D_8028C83C;
extern int D_8028B7F4;                  /* cars in the race */
extern float D_8028AAD8;                /* seconds this frame */
extern float D_803634D0[3];             /* the wind: every particle drifts with it */
extern int D_8028AA80;                  /* race-kind flags (BrRaceSetKind) */
extern int D_8028AA84;
extern int D_8028AA8C;
typedef struct BrWeatherObj {           /* 0x78 bytes */
    char pad00[0x60];
    int x60;                            /* 0x60  its emitter, 0 for none */
    char pad64[0x78 - 0x64];
} BrWeatherObj;
extern BrWeatherObj D_803239A0[];
extern int D_8028B7F0;                  /* entries in D_803239A0 */
void func_8023B178(int e);
void BrSkidStep();
void BrWheelSprayEmit();
void func_8023CD60(void);
void BrSkidDraw(void);
void BrParticleListDraw(int n, int r, int g, int b);
extern Gfx *D_8028A858;
extern int D_8028A898;
extern int D_8028A89C;
extern int D_8028A8A0;
extern int D_802A3790;
int BrRandStep(void);
void BrVec3Scale(BrVec3 *pOut, BrVec3 *pV, float s);
void BrVec3Sub(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB);
void BrVec3MulAdd(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB, float s);
float BrVec3Length(BrVec3 *pV);
/* -- end declarations -- */

/* WHAT IT DOES: A car's exhaust smoke for one frame: its emit timer counts
 * up at a rate that grows with 0xDF4 plus a random part, and past a
 * quarter second a particle comes off the free list onto the live list,
 * blown backwards along the car, started between the last spot and one
 * behind the rear wheel (a random square's way), sized by the same rate
 * and made fainter the faster the car goes.  The PC twin is BrCarSub9020.
 * The rate is written out in the timer sum and named inside the branch
 * (IDO shares the product); the constants are literals (read-only, so the
 * product survives the stores between). */
/* @implements 0x8023B178 tgr BrCarSmokeEmit */
void BrCarSmokeEmit(BrCar *car)
{
  BrVec3 v;
  float g;
  int n;
  BrParticle *p;
  float f;
  float t;
  int r;

  r = BrRandStep();
  car->x1010 = car->x1010 + D_8028AAD8 * (1.0f + car->xdf4 * 0.001f + (float)(r & 0x1fff) * (1.0f / 65536.0f));
  if (car->x1010 > 0.25f && (n = D_8028C830) != 0) {
    f = car->xdf4 * 0.001f;
    car->x1010 = 0.0f;
    p = &D_80366A80[n];
    D_8028C830 = p->next;
    p->next = D_8028C834;
    D_8028C834 = n;
    BrVec3Scale((BrVec3 *)p->vel, (BrVec3 *)car, -1.5f - f);
    BrVec3Sub(&v, (BrVec3 *)car->wheelMtx[2][3], (BrVec3 *)car);
    BrVec3MulAdd(&v, &v, (BrVec3 *)car->mtx0[2], 0.2f);
    BrVec3MulAdd(&v, &v, (BrVec3 *)car->mtx0[1], 0.2f);
    g = (float)(BrRandStep() & 0xffff) * 1.5259021893143654e-05f;
    BrVec3Sub((BrVec3 *)p->pos, &car->smokeAt, &v);
    BrVec3MulAdd((BrVec3 *)p->pos, &v, (BrVec3 *)p->pos, g * g);
    car->smokeAt.x = v.x;
    car->smokeAt.y = v.y;
    car->smokeAt.z = v.z;
    f *= 0.1f;
    t = 1.0f + f;
    p->size = t * 0.15f;
    p->x1e = 1.0f / (BrVec3Length(&car->velfd8) + t) * 255.0f;
    p->x1f = 0xff;
  }
}

/* -- declarations: BrSkidStep -- */
extern int D_8028B940;          /* the track */
/* A wheel's ground contact as the spray and skid marks read it (0x208 bytes, four from
 * car+0x350). */
typedef struct BrSprayWheel {
    char pad000[0x1A0];
    unsigned char surface;      /* 0x1A0  1-3 throw spray (3 onto the second list) */
    char pad1a1[0x1B4 - 0x1A1];
    int contact;                /* 0x1B4  non-zero on the ground */
    char pad1b8[0x208 - 0x1B8];
} BrSprayWheel;
void BrVec3MulAddTo(BrVec3 *pOut, BrVec3 *pV, float s);
void BrVec3SubFrom(BrVec3 *pA, BrVec3 *pB);
void BrVec3ScaleBy(BrVec3 *pV, float s);
float BrVec3DistSq(BrVec3 *pA, BrVec3 *pB);
void BrVec3Lerp(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB, float t);
typedef struct BrTrackObj {             /* a track object (0x54 bytes) */
    float m[4][4];
    int x40;
    unsigned int dl;
    unsigned short hide;
    unsigned short x4a;
    unsigned short flags;               /* 0x4C  0x10: no skid marks on snow */
    unsigned short tris;
    unsigned short vtxs;
    unsigned short x52;
} BrTrackObj;
extern BrTrackObj *D_80025C60;          /* the track's objects */
float BrVec3Dot(BrVec3 *a, BrVec3 *b);
void BrVec3Zero(BrVec3 *v);
/* -- end declarations -- */

/* WHAT IT DOES: Lay a car's skid marks for one frame (when the race kind
 * draws them): per wheel a clock runs at four per second and past 0.75
 * lays a point; a grounded wheel keeps its mark alive for three frames on
 * its surface.  A live mark on loose ground (1.5 wide, 2.5 on surface 4,
 * snow only on the snow tracks off the trigger) is spread across the car,
 * scaled by speed (on snow by the sideways slip instead, wider at the
 * rear) and swept by the velocity; the point sits a quarter behind the
 * wheel, a little in from the side, jittered toward the last point.  The
 * new point's width and the trail's newest vertex (relative to the car)
 * are written at the head of the wheel's trail.  The frame is the ROM's
 * with the unused one-int arrays; stores of 0 and the decrements by 1 are
 * integer literals (the 0.0f compares keep their own zero register).
 * RESIDUE (6): two lui's of the mark width scheduled one slot later than
 * the ROM's, and the scale's spill slot 0x64 (ROM 0x70).  500 permuter
 * compiles leave it. */
/* @implements 0x8023B418 tgr BrSkidStep */
void BrSkidStep(car)
BrCar *car;
{
  BrVec3 save;
  BrVec3 d;
  BrVec3 at;
  int u0[1];
  float fade;
  int u1[1];
  float s;
  int u2[1];
  int hold;
  int u3[1];
  int onSnow;
  BrSprayWheel *w[4];
  int u4[1];
  float slip;
  int u5[1];
  float f;
  int k;
  int surf;
  float g;
  int u6[1];

  onSnow = 0;
  if (car->x2000 != 0 && (D_80025C60[car->x1fc0[0]].flags & 0x10)) {
    onSnow = 1;
  }
  if (D_8028AA84 == 0 || D_8028B940 == 2 || D_8028B940 == 7) {
    w[0] = (BrSprayWheel *)((char *)car + 0x968);
    w[1] = (BrSprayWheel *)((char *)car + 0x558);
    w[2] = (BrSprayWheel *)((char *)car + 0x350);
    w[3] = (BrSprayWheel *)((char *)car + 0x760);
    fade = (1.0f - 50.0f / (car->xfe4[0] + 50.0f)) * 3.0f;
    f = BrVec3Dot((BrVec3 *)car, &car->velfd8);
    if (f < 0) {
      slip = -f;
    } else {
      slip = f;
    }
    for (k = 0; k < 4; k++) {
      car->skidClock[k] += D_8028AAD8 * 4.0f;
      if (car->skidClock[k] > 0.75f) {
        hold = 0;
        if (car->skidClock[k] >= 1.7f) {
          car->skidClock[k] -= 1;
        } else {
          car->skidClock[k] = 0;
        }
        car->skidEmit[k] = 1;
      } else {
        hold = 1;
        car->skidEmit[k] = 0;
      }
      if (w[k]->contact != 0) {
        car->skidLife[k] = 3.0f;
        car->skidSurf[k] = w[k]->surface;
      } else if (car->skidLife[k] != 0.0f) {
        car->skidLife[k] -= 1;
      }
      surf = car->skidSurf[k];
      if (car->skidLife[k] != 0.0f) {
        if (hold) {
          s = 1.5f;
        } else if (surf == 4) {
          s = 2.5f;
        } else if (surf == 3 && D_8028AA8C != 0 && !onSnow) {
          s = 1.5f;
          if (!hold) {
            car->skidClock[k] = 7.75f;
          }
        } else {
          goto flat;
        }
        if (!hold) {
          BrVec3Scale(&d, (BrVec3 *)car->mtx0[2], s);
          if (surf != 3) {
            BrVec3MulAddTo(&d, (BrVec3 *)car->mtx0[1], k != 0 && k < 3 ? -s : s);
          }
          if (surf != 3) {
            BrVec3ScaleBy(&d, fade);
            BrVec3MulAddTo(&d, &car->velfd8, 0.3f);
            BrVec3MulAddTo(&d, (BrVec3 *)car->mtx0[1], BrVec3Dot((BrVec3 *)car->mtx0[1], &car->velfd8) * 0.5f);
          } else {
            f = (1.0f - 25.0f / (2.24f * slip + 25.0f)) * 3.0f;
            BrVec3ScaleBy(&d, f);
            if (f != 0.0f) {
              if (k >= 2) {
                d.z += d.z;
              }
              BrVec3MulAddTo(&d, (BrVec3 *)car->mtx0[1], BrVec3Dot((BrVec3 *)car->mtx0[1], &car->velfd8) * 0.3f);
            }
          }
        }
        BrVec3MulAdd(&at, (BrVec3 *)car->wheelMtx[k][3], (BrVec3 *)car->mtx0[2], -0.25f);
        if (surf != 3 || k < 2) {
          BrVec3MulAddTo(&at, (BrVec3 *)car->mtx0[1], k != 0 && k < 3 ? -0.15f : 0.15f);
        }
        save.x = at.x;
        save.y = at.y;
        save.z = at.z;
        if (!hold) {
          if (BrVec3DistSq(&at, &car->skidAt[k]) < 256.0f) {
            g = (float)(BrRandStep() & 0xffff) * 1.5259021893143654e-05f;
            BrVec3Lerp(&at, &car->skidAt[k], &at, g * g);
          }
        }
        car->skidAt[k].x = save.x;
        car->skidAt[k].y = save.y;
        car->skidAt[k].z = save.z;
      } else {
      flat:
        BrVec3Zero(&d);
        at.x = car->wheelMtx[k][3][0];
        at.y = car->wheelMtx[k][3][1];
        at.z = car->wheelMtx[k][3][2];
      }
      if (!hold) {
        car->skidPt[k][0].half[0] = d.x * 127.0f;
        car->skidPt[k][0].half[1] = d.y * 127.0f;
        car->skidPt[k][0].half[2] = d.z * 127.0f;
        if (surf == 3 && D_8028AA8C != 0) {
          car->skidPt[k][0].pos[2] = 0;
          car->skidPt[k][0].pos[0] = car->skidPt[k][0].half[0];
          car->skidPt[k][0].pos[1] = car->skidPt[k][0].half[1];
        } else {
          car->skidPt[k][0].pos[2] = 0;
          car->skidPt[k][0].pos[0] = car->skidPt[k][0].half[0] >> 1;
          car->skidPt[k][0].pos[1] = car->skidPt[k][0].half[1] >> 1;
        }
      }
      car->skidKind[k][0] = surf;
      car->skidVtx[k][1].ob[0] = (at.x - car->pos1d78.x) * 127.0f;
      car->skidVtx[k][1].ob[1] = (at.y - car->pos1d78.y) * 127.0f;
      car->skidVtx[k][1].ob[2] = (at.z - car->pos1d78.z) * 127.0f;
      car->skidVtx[k][0].ob[0] = car->skidVtx[k][1].ob[0];
      car->skidVtx[k][0].ob[1] = car->skidVtx[k][1].ob[1];
      car->skidVtx[k][0].ob[2] = car->skidVtx[k][1].ob[2];
    }
  }
}

/* -- declarations: BrSkidDraw -- */
extern Mtx *D_8028A878;
extern char D_802A2978[];       /* the skid-mark texture */
#define gSP2Triangles(pkt, v00, v01, v02, f0, v10, v11, v12, f1)        \
{                                                                       \
    Gfx *_g = (Gfx *)(pkt);                                             \
                                                                        \
    _g->words.w0 = (_SHIFTL((v00) * 2, 16, 8) | _SHIFTL((v01) * 2, 8, 8) | \
                    _SHIFTL((v02) * 2, 0, 8) | _SHIFTL(0xb1, 24, 8));   \
    _g->words.w1 = (_SHIFTL((v10) * 2, 16, 8) | _SHIFTL((v11) * 2, 8, 8) | \
                    _SHIFTL((v12) * 2, 0, 8));                          \
}
/* -- end declarations -- */

/* WHAT IT DOES: Draw the skid marks, unless the race kind steps the
 * particle pool (tracks 2 and 7 draw them anyway): textured, decal render
 * mode, culling off, the projection loaded; then for every car its matrix
 * and, per wheel, its 32 mark vertices, with a quad between each pair of
 * the wheel's eight marked points that has some width; culling back on.
 * An empty slot is skipped with `continue`: that is what makes IDO turn
 * the index loop into the ROM's pointer walk with its count > 0 guard. */
/* @implements 0x8023BB50 tgr BrSkidDraw */
void BrSkidDraw(void)
{
  int i;
  char *car;
  int g;
  int v;
  BrSkidPt *pt;

  if (D_8028AA84 == 0 || D_8028B940 == 2 || D_8028B940 == 7) {
    gDPPipeSync(D_8028A858++);
    gRaw(D_8028A858++, 0xba001402, 0x100000);
    gRaw(D_8028A858++, 0xfcff99ff, 0xfffe7e38);
    gRaw(D_8028A858++, 0xb900031d, 0xc184a50);
    gRaw(D_8028A858++, 0xb7000000, 4);
    gSPClearGeometryMode(D_8028A858++, 0x23000);
    {
      Gfx *_g = D_8028A858++;

      _g->words.w0 = 0x1030040;
      _g->words.w1 = D_8028A878;
    }
    {
      Gfx *_g = D_8028A858++;

      _g->words.w0 = 0xfd900000;
      _g->words.w1 = D_802A2978;
    }
    {
      Gfx *_g = D_8028A858++;

      _g->words.w0 = 0xf5900000;
      _g->words.w1 = 0x7018050;
    }
    gDPLoadSync(D_8028A858++);
    {
      Gfx *_g = D_8028A858++;

      _g->words.w0 = 0xf3000000;
      _g->words.w1 = 0x71ff400;
    }
    gDPPipeSync(D_8028A858++);
    {
      Gfx *_g = D_8028A858++;

      _g->words.w0 = 0xf5800400;
      _g->words.w1 = 0x18050;
    }
    {
      Gfx *_g = D_8028A858++;

      _g->words.w0 = 0xf2000000;
      _g->words.w1 = 0x7c0fc;
    }
    {
      Gfx *_g = D_8028A858++;

      _g->words.w0 = 0xf2002002;
      _g->words.w1 = 0x7e0fe;
    }
    {
      Gfx *_g = D_8028A858++;

      _g->words.w0 = 0xbb000001;
      _g->words.w1 = 0xffffffff;
    }
    for (i = 0; i < D_8028B7F0; i++) {
      if ((car = (char *)D_803239A0[i].x60) == 0) {
        continue;
      }
      gRaw(D_8028A858++, 0x1060040, car + 0x1D88);
      for (g = 0; g < 4; g++) {
        pt = (BrSkidPt *)(car + 0x19E8 + g * 0xD8);
        gRaw(D_8028A858++, 0x40081ff, car + 0x1110 + g * 0x240);
        for (v = 0; v < 28; v += 4) {
          if (pt[0].half[0] != 0 || pt[0].half[1] != 0 || pt[0].half[2] != 0
              || pt[1].half[0] != 0 || pt[1].half[1] != 0 || pt[1].half[2] != 0) {
            gSP2Triangles(D_8028A858++, v, v + 4, v + 1, 0, v + 1, v + 4, v + 5, 0);
          }
          pt++;
        }
      }
      gSPPopMatrix(D_8028A858++, 0);
    }
    gRaw(D_8028A858++, 0xb7000000, 0x2000);
  }
}

/* -- declarations: BrSkidAge -- */
extern int D_8026FF10;                  /* the race is paused */
void guTranslate(void *m, float x, float y, float z);
void guScale(void *m, float x, float y, float z);
void guMtxCatL(void *a, void *b, void *r);
void *memcpy(void *dst, void *src, unsigned int n);
/* -- end declarations -- */

/* WHAT IT DOES: Age every car's skid trails one frame (not while paused):
 * the trail origin follows the car in whole 1/127 steps and the trails'
 * matrix is rebuilt (translate, then 1/127 scale); per wheel a newly laid
 * point takes its surface's colours (dark earth, grey snow on the snow
 * tracks, tarmac by track, halved or lightened by race kind; an unknown
 * surface ends the mark) and shifts in at the head; then each of the
 * eight older quads shifts back when a point was laid, fades out over the
 * last four, takes its texture column and row, and moves with the origin
 * (older points also spread by their width and sink by gravity).  The
 * surface switch is in the ROM's body order (4, 0, 3, unknown); the frame
 * has two unused ints above the drop.
 * RESIDUE (3): the low quad's z for the sink test sits in a0, the ROM's
 * in v0; compare/assignment spellings and 400 permuter compiles leave it. */
/* @implements 0x8023BF60 tgr BrSkidAge */
void BrSkidAge(void)
{
  int dt;
  int i;
  short dx;
  short dy;
  short dz;
  int j;
  int k;
  int kind;
  BrSkidVtx *v0;
  BrSkidVtx *v1;
  BrCar *car;
  short *h;
  int a;
  int u[1];
  short drop;
  Mtx m;

  dt = D_8028AAD8 * 4096.0f;
  drop = 9.81f * D_8028AAD8 * 127.0f;
  if (D_8026FF10 != 0) {
    return;
  }
  for (i = 0; i < D_8028B7F0; i++) {
    if ((car = (BrCar *)D_803239A0[i].x60) == 0) {
      continue;
    }
    dx = (car->pos1d78.x - car->mtx0[3][0]) * 127.0f;
    dy = (car->pos1d78.y - car->mtx0[3][1]) * 127.0f;
    dz = (car->pos1d78.z - car->mtx0[3][2]) * 127.0f;
    car->pos1d78.x -= dx * 0.007874016f;
    car->pos1d78.y -= dy * 0.007874016f;
    car->pos1d78.z -= dz * 0.007874016f;
    guTranslate(car->mtx, car->pos1d78.x, car->pos1d78.y, car->pos1d78.z);
    guScale(&m, 0.007874016f, 0.007874016f, 0.007874016f);
    guMtxCatL(&m, car->mtx, car->mtx);
    for (k = 0; k < 4; k++) {
      if (car->skidEmit[k] != 0) {
        v0 = &car->skidVtx[k][0];
        v1 = &car->skidVtx[k][1];
        switch (car->skidKind[k][0]) {
        case 4:
          if (D_8028B940 == 2 || D_8028B940 == 7) {
            if (D_8028AA84 != 0) {
              v0->cn[0] = 70;
              v0->cn[1] = 100;
              v0->cn[2] = 100;
              v1->cn[0] = 215;
              v1->cn[1] = 235;
              v1->cn[2] = 195;
            } else {
              v0->cn[0] = 50;
              v0->cn[1] = 100;
              v0->cn[2] = 95;
              v1->cn[0] = 210;
              v1->cn[1] = 240;
              v1->cn[2] = 190;
            }
          } else if (D_8028B940 == 3 || D_8028B940 == 8) {
            v0->cn[0] = 16;
            v0->cn[1] = 16;
            v0->cn[2] = 0;
            v1->cn[0] = 64;
            v1->cn[1] = 64;
            v1->cn[2] = 16;
          } else {
            v0->cn[0] = 48;
            v0->cn[1] = 24;
            v0->cn[2] = 8;
            v1->cn[0] = 128;
            v1->cn[1] = 96;
            v1->cn[2] = 64;
          }
          if (D_8028AA8C != 0) {
            v0->cn[0] = v0->cn[0] / 2 + 32;
            v0->cn[1] = v0->cn[1] / 2 + 32;
            v0->cn[2] = v0->cn[2] / 2 + 32;
            v1->cn[0] = v1->cn[0] / 2 + 32;
            v1->cn[1] = v1->cn[1] / 2 + 32;
            v1->cn[2] = v1->cn[2] / 2 + 32;
          } else if (D_8028AA80 != 0) {
            v0->cn[0] >>= 1;
            v0->cn[1] >>= 1;
            v0->cn[2] >>= 1;
            v1->cn[0] >>= 1;
            v1->cn[1] >>= 1;
            v1->cn[2] >>= 1;
          }
          break;
        case 0:
          v0->cn[0] = v1->cn[0] = 0x30;
          v0->cn[1] = v1->cn[1] = 0x20;
          v0->cn[2] = v1->cn[2] = 0x10;
          break;
        case 3:
          if (D_8028AA8C == 0) {
            goto none;
          }
          v0->cn[0] = 100;
          v0->cn[1] = 104;
          v0->cn[2] = 108;
          v1->cn[0] = 160;
          v1->cn[1] = 168;
          v1->cn[2] = 176;
          break;
        default:
        none:
          car->skidKind[k][0] = -1;
          v1->cn[0] = car->skidVtx[k][5].cn[0];
          v1->cn[1] = car->skidVtx[k][5].cn[1];
          v1->cn[2] = car->skidVtx[k][5].cn[2];
          v0->cn[0] = car->skidVtx[k][4].cn[0];
          v0->cn[1] = car->skidVtx[k][4].cn[1];
          v0->cn[2] = car->skidVtx[k][4].cn[2];
          break;
        }
        memcpy(&car->skidVtx[k][4], &car->skidVtx[k][0], 0x40);
        memcpy(&car->skidPt[k][1], &car->skidPt[k][0], 0x18);
        car->skidKind[k][1] = car->skidKind[k][0];
      }
      for (j = 8; j != 0; j--) {
        v0 = &car->skidVtx[k][j * 4];
        v1 = &car->skidVtx[k][j * 4 + 1];
        if (j == 1 || car->skidEmit[k] != 0) {
          memcpy(v0, &car->skidVtx[k][j * 4 - 4], 0x40);
          memcpy(&car->skidPt[k][j], &car->skidPt[k][j - 1], 0x18);
          car->skidKind[k][j] = car->skidKind[k][j - 1];
        }
        kind = car->skidKind[k][j];
        if (kind == -1) {
          v1->cn[3] = v0->cn[3] = 0;
        } else if (j >= 5) {
          v1->cn[3] = v0->cn[3] = (8 - j) * 0xff / 4;
        } else {
          v1->cn[3] = v0->cn[3] = 0xff;
        }
        if (kind == 3) {
          v0->tc[0] = 0x410;
          v1->tc[0] = 0x10;
        } else {
          v0->tc[0] = 0x10;
          v1->tc[0] = 0x410;
        }
        v1->tc[1] = v0->tc[1] = j << 11;
        if (kind == -1 || j == 1 || (j == 2 && car->skidEmit[k] != 0)) {
          v0->ob[0] += dx;
          v0->ob[1] += dy;
          v0->ob[2] += dz;
          v1->ob[0] += dx;
          v1->ob[1] += dy;
          v1->ob[2] += dz;
        } else if (j != 8) {
          v0->ob[0] += ((car->skidPt[k][j].pos[0] * dt) >> 12) + dx;
          v0->ob[1] += ((car->skidPt[k][j].pos[1] * dt) >> 12) + dy;
          v0->ob[2] += dz;
          h = car->skidPt[k][j].half;
          v1->ob[0] += ((car->skidPt[k][j].half[0] * dt) >> 12) + dx;
          v1->ob[1] += ((h[1] * dt) >> 12) + dy;
          v1->ob[2] += ((h[2] * dt) >> 12) + dz;
          if (v1->ob[2] < v0->ob[2]) {
            v1->ob[2] = v0->ob[2];
          } else {
            h[2] -= drop;
          }
        }
      }
    }
  }
}

/* -- declarations: BrWheelSprayEmit -- */
/* -- end declarations -- */

/* WHAT IT DOES: A car's wheel spray for one frame, when it has a surface
 * impact level and goes faster than 40: each wheel's timer counts up by
 * the frame rate and the impact level, and past 0.75 a wheel on the ground
 * on surface 1-3 takes a particle off the free list onto one of the two
 * falling lists (surface 3 the second), thrown along the car's velocity,
 * up (the rear pair more), out to its side (the front pair), less of it
 * the faster the car goes; it starts at the wheel, lifted by half 0x2048,
 * pulled toward the wheel's last spot when that is near, and fades from
 * the speed.  Declared without a prototype: BrParticleFrame passes the
 * emitter as an int, and a prototyped pointer call recolours its loops.
 * The speed is a local assigned in the test (the rate's frame-time product
 * is then evaluated first); the unusable wheel is a `continue` (that
 * numbers the timer cursor s6 and car+0x20 s5 as in the ROM).
 * RESIDUE (1): the surface chain's last test -- the ROM emits bne v0,a0
 * (byte, 3), ours bne a0,v0.  Spellings of the chain, a local byte, splits
 * and 562 permuter compiles leave it. */
/* @implements 0x8023C800 tgr BrWheelSprayEmit */
void BrWheelSprayEmit(car)
BrCar *car;
{
  float at[3];
  int unused[3];
  int i;
  float rate;
  BrSprayWheel *w[4];
  BrSprayWheel *wh;
  int n;
  BrParticle *p;
  float keep;
  float speed;

  if (car->sndImpact != 0 && (speed = car->xfe4[0]) > 40.0f) {
    rate = D_8028AAD8 * 0.5f + speed * 0.00066006603f;
    for (i = 0; i < 4; i++) {
      car->sprayTime[i] += rate * ((float)(unsigned int)car->sndImpact * 0.03f);
      if (car->sprayTime[i] > 0.75f) {
        car->sprayTime[i] = 0.0f;
        w[0] = (BrSprayWheel *)((char *)car + 0x968);
        w[1] = (BrSprayWheel *)((char *)car + 0x558);
        w[2] = (BrSprayWheel *)((char *)car + 0x350);
        w[3] = (BrSprayWheel *)((char *)car + 0x760);
        wh = w[i];
        if (wh->contact == 0 || (wh->surface != 1 && wh->surface != 2 && wh->surface != 3)
            || (n = D_8028C830) == 0) {
          continue;
        }
        {
          p = &D_80366A80[n];
          D_8028C830 = p->next;
          if (wh->surface == 3) {
            p->next = D_8028C83C;
            D_8028C83C = n;
          } else {
            p->next = D_8028C838;
            D_8028C838 = n;
          }
          BrVec3Scale((BrVec3 *)p->vel, &car->velfd8, 0.2f);
          BrVec3MulAddTo((BrVec3 *)p->vel, (BrVec3 *)car->mtx0[2], i < 2 ? 0.25f : 2.0f);
          if (i < 2) {
            BrVec3MulAddTo((BrVec3 *)p->vel, (BrVec3 *)car->mtx0[1], i != 0 ? -0.5f : 0.5f);
          }
          BrVec3SubFrom((BrVec3 *)p->vel, (BrVec3 *)car);
          keep = 1.0f - 50.0f / (car->xfe4[0] + 50.0f);
          BrVec3ScaleBy((BrVec3 *)p->vel, keep);
          BrVec3Sub((BrVec3 *)p->pos, (BrVec3 *)car->wheelMtx[i][3], (BrVec3 *)car);
          p->pos[2] += car->x2048 * 0.5f;
          at[0] = p->pos[0];
          at[1] = p->pos[1];
          at[2] = p->pos[2];
          if (BrVec3DistSq((BrVec3 *)p->pos, &car->sprayAt[i]) < 256.0f) {
            float g = (float)(BrRandStep() & 0xffff) * 1.5259021893143654e-05f;

            BrVec3Lerp((BrVec3 *)p->pos, &car->sprayAt[i], (BrVec3 *)p->pos, g * g);
          }
          car->sprayAt[i].x = at[0];
          car->sprayAt[i].y = at[1];
          car->sprayAt[i].z = at[2];
          p->size = 0.4f;
          p->x1e = 25;
          p->x1f = (int)(239.0f * keep * 0.7f + 16.0f);
        }
      }
    }
  }
}

/* WHAT IT DOES: Step every live particle one frame: it grows, drifts with
 * the wind plus its own velocity scaled by its strength (x1f * x1e / 65280,
 * with a rise of 0.8 on z), and x1e becomes 5.7375 / size^2.  A particle
 * whose strength falls below 1/32 goes back on the free list.
 * RESIDUE (40): the ROM reads the next index before the position update and
 * keeps a copy of it for the loop (an extra register, p in a1); ours reads
 * it after.  Reading it first, index types and loop shapes score worse. */
/* @t4-pass 0x8023CBFC 1 2026-09-29 compiles 25 best 40 moved 4  (n64/tools/n64permute.py) */
/* @t4-pass 0x8023CBFC 2 2026-09-29 compiles 20 best 40 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8023CBFC */
/* @implements 0x8023CBFC tgr BrParticleStep */
void BrParticleStep(void)
{
  unsigned short *link;
  int n;
  int cur;
  BrParticle *p;
  float f;
  float grow;

  grow = 0.3f * D_8028AAD8;
  link = &D_8028C834;
  n = *link;
  while (n != 0) {
    cur = n;
    p = &D_80366A80[cur];
    p->size += grow;
    f = (float)(int)(p->x1f * p->x1e) * (1.0f / 65280.0f);
    p->pos[0] = p->pos[0] + (D_803634D0[0] + p->vel[0] * f * D_8028AAD8);
    p->pos[1] = (D_803634D0[1] + p->vel[1] * f * D_8028AAD8) + p->pos[1];
    p->pos[2] += (D_803634D0[2] + (p->vel[2] * f + 0.8f) * D_8028AAD8);
    p->x1e = (int)(5.7375f / (p->size * p->size));
    n = p->next;
    if (f < 0.03125f) {
      *link = n;
      p->next = D_8028C830;
      D_8028C830 = cur;
    } else {
      link = &p->next;
    }
  }
}

/* WHAT IT DOES: Step the two falling-particle lists one frame: as
 * BrParticleStep, but the particles also fall (z velocity loses
 * 19.62 * dt), x1e becomes 102 / size, and a particle is freed when its
 * strength falls below 1/32 or it falls faster than 30.
 * RESIDUE (72): the ROM re-reads p->next for the unlink and chooses the
 * list head per branch; ours reuses the early read (one more live register,
 * s0 saved).  Read order, index types, alias-breaking spellings and 294
 * permuter compiles leave it. */
/* @t4-pass 0x8023CD60 1 2026-09-29 compiles 26 best 72 moved 9  (n64/tools/n64permute.py) */
/* @t4-pass 0x8023CD60 2 2026-09-29 compiles 26 best 72 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8023CD60 */
/* @implements 0x8023CD60 tgr BrParticleFallStep */
void BrParticleFallStep(void)
{
  unsigned short *link;
  int n;
  int next;
  int k;
  BrParticle *p;
  float f;
  float grow;

  grow = 0.7f * D_8028AAD8;
  for (k = 0; k < 2; k++) {
    if (k != 0) {
      link = &D_8028C838;
    } else {
      link = &D_8028C83C;
    }
    n = *link;
    while (n != 0) {
      next = D_80366A80[n].next;
      p = &D_80366A80[n];
      p->size += grow;
      f = (float)(int)(p->x1f * p->x1e) * (1.0f / 65280.0f);
      p->pos[0] = p->pos[0] + (D_803634D0[0] + p->vel[0] * f * D_8028AAD8);
      p->pos[1] = (D_803634D0[1] + p->vel[1] * f * D_8028AAD8) + p->pos[1];
      p->pos[2] = p->pos[2] + (D_803634D0[2] + (p->vel[2] * f + 0.8f) * D_8028AAD8);
      p->vel[2] = p->vel[2] - D_8028AAD8 * 19.62f;
      p->x1e = (int)(102.0f / p->size);
      if (f < 0.03125f || p->vel[2] < -30.0f) {
        *link = p->next;
        p->next = D_8028C830;
        D_8028C830 = n;
      } else {
        link = &p->next;
      }
      n = next;
    }
  }
}

/* WHAT IT DOES: Empty the particle pool: chain every record onto the free
 * list (1 -> 2 -> ... -> 256), leave the live list empty, and clear each
 * racing car's 0x1010 field. */
/* @implements 0x8023CF30 tgr BrParticleReset */
void BrParticleReset(void)
{
  int i;
  BrParticle *p;

  for (i = 1, p = &D_80366A80[1]; i < 256; i++, p++) {
    p->next = i + 1;
  }
  D_80368A7C = 0;
  D_8028C830 = 1;
  for (i = 0; i < D_8028B7F4; i++) {
    D_8031B760[i].x1010 = 0.0f;
  }
  D_8028C834 = 0;
  D_8028C838 = 0;
  D_8028C83C = 0;
}

/* WHAT IT DOES: The particle frame: the pool is emptied on the first call;
 * then, by race kind, either the live particles are stepped and every
 * object's emitter runs (0x8023B178), or the other effect steps (0x8023CD60)
 * and each emitter runs 0x8023C800 -- in both cases followed by 0x8023B418
 * -- or, for the remaining kinds, only 0x8023B418 runs. */
/* @implements 0x8023CFC4 tgr BrParticleFrame */
void BrParticleFrame(void)
{
  static int D_8028C840 = 0;
  int i;
  BrWeatherObj *o;

  if (D_8028C840 == 0) {
    BrParticleReset();
    D_8028C840 = 1;
  }
  if (D_8028AA84 != 0) {
    BrParticleStep();
    for (i = 0, o = D_803239A0; i < D_8028B7F0; i++, o++) {
      if (o->x60 != 0) {
        func_8023B178(o->x60);
        BrSkidStep(o->x60);
      }
    }
  } else if (D_8028AA80 == 0 && D_8028AA8C == 0) {
    func_8023CD60();
    for (i = 0, o = D_803239A0; i < D_8028B7F0; i++, o++) {
      if (o->x60 != 0) {
        BrWheelSprayEmit(o->x60);
        BrSkidStep(o->x60);
      }
    }
  } else {
    for (i = 0, o = D_803239A0; i < D_8028B7F0; i++, o++) {
      if (o->x60 != 0) {
        BrSkidStep(o->x60);
      }
    }
  }
}

/* -- declarations: BrParticleListDraw -- */
typedef struct BrView { int x; int y; int w; int h; int car; } BrView;
extern BrView D_8031B2C8[2];
extern int D_8028AAEC;                  /* the view being drawn */
extern int D_8028A8A8;
extern int D_8028A8AC;
void BrMat4TransformPoint4(float out[4], float v[3], float m[4][4]);
extern float D_80368A80[4][4];          /* the falling particles' billboard */
/* -- end declarations -- */

/* WHAT IT DOES: Draw one particle list as screen-facing sprites in colour
 * r, g, b: each particle is carried through the billboard matrix and, when
 * in front of the camera and inside the view (allowing for its size), drawn
 * as a scissored texture rectangle at its projected depth, 3/4 as wide as
 * high, its alpha the product of its two strength bytes.  The x axis flips
 * with the mirrored view.  The ROM's float tests are <=/>= early-outs
 * (continue); the view's centre is precomputed.
 * RESIDUE (352): register allocation -- the ROM keeps the list index in
 * v0 (dead across the transform call) and p in s1 with only s0/s1 saved;
 * ours keeps the index and a copy of p in saved registers, which moves
 * every spill slot.  Loop forms (for/while/do, a local index, a ushort
 * parameter) and 120 declaration orders leave it. */
/* @implements 0x8023D134 tgr BrParticleListDraw */
void BrParticleListDraw(int n, int r, int g, int b)
{
  float v[4];
  int x;
  int y;
  int w;
  int h;
  float sw;
  float sx;
  BrParticle *p;
  float inv;
  float size;
  float lim;
  int hw;
  int hh;
  int cx;
  int cy;

  w = D_8031B2C8[D_8028AAEC].w;
  h = D_8031B2C8[D_8028AAEC].h;
  x = (D_8031B2C8[D_8028AAEC].x * 2 + w) * 2;
  y = (D_8031B2C8[D_8028AAEC].y * 2 + h) * 2;
  sw = (float)(w * 2);
  sx = sw;
  if (D_8028A8A8 != D_8028A8AC) {
    sx = -sw;
  }
  for (; n != 0; n = p->next) {
    p = &D_80366A80[n];
    BrMat4TransformPoint4(v, p->pos, D_80368A80);
    if (v[3] <= 0.001f && v[3] >= -0.001f) {
      continue;
    }
    inv = 1.0f / v[3];
    v[2] *= inv;
    if (v[2] >= 1.0f || v[2] <= -0.3f) {
      continue;
    }
    v[0] *= inv;
    size = p->size * inv * 4.0f;
    lim = 1.0f + size;
    if (v[0] <= -lim || v[0] >= lim) {
      continue;
    }
    v[1] *= inv;
    if (v[1] <= -lim || v[1] >= lim) {
      continue;
    }
    hw = sw * 0.75f * size;
    if (hw <= 0) {
      continue;
    }
    hh = sw * size;
    if (hh <= 0) {
      continue;
    }
    cx = (int)(v[0] * sx) + x;
    cy = (int)(v[1] * (float)(h * 2)) + y;
    gDPPipeSync(D_8028A858++);
    gDPSetPrimColor(D_8028A858++, 0xff, 0xff, r, g, b, (p->x1f * p->x1e) >> 8);
    gRaw(D_8028A858++, 0xee000000, ((int)(v[2] * 16352.0f) + 0x3fe0) << 16);
    gSPScisTextureRectangle(D_8028A858++, cx - hw, cy - hh, cx + hw, cy + hh, 0, 0, 0x7e0,
                            0x1f800 / hw, -0x1f800 / hh);
  }
}

/* -- declarations: BrParticleFallDraw -- */
extern BrCar *D_8028AAF4;               /* the camera's car */
extern float D_8031AB10[4][4];
extern float D_8031AB50[4][4];
extern float D_8031AA90[4][4];
int BrMat4Inverse(float out[4][4], float in[4][4]);
void BrMat4Mul(float r[4][4], float a[4][4], float b[4][4]);
/* -- end declarations -- */

/* WHAT IT DOES: Draw the two falling-particle lists: the particle sprite
 * as a texture block, then the billboard matrix -- the inverse of the
 * camera car's matrix turned into screen axes (x' = -y, y' = -z, z' = -x)
 * and carried through D_8031AA90; with the race kind that has no falling
 * particles nothing is drawn; otherwise in the track's colours (tracks 1
 * and 6 one pair, 4 and 9 another, the rest a third).  The render state
 * is restored after.  The camera car is copied to a local first: read
 * inline, IDO loads the pointer's %hi after the matrix's. */
/* @implements 0x8023D714 tgr BrParticleFallDraw */
void BrParticleFallDraw(void)
{
  BrCar *cam;

  gDPPipeSync(D_8028A858++);
  gRaw(D_8028A858++, 0xba001402, 0);
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xbb000001;
    _g->words.w1 = 0xffffffff;
  }
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xba000c02;
    _g->words.w1 = D_8028A898;
  }
  gDPSetCombine(D_8028A858++, 0xff97ff, 0xff2dfeff);
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xfd900000;
    _g->words.w1 = (unsigned int)&D_802A3790;
  }
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xf5900000;
    _g->words.w1 = 0x07018060;
  }
  gDPLoadSync(D_8028A858++);
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xf3000000;
    _g->words.w1 = 0x077ff100;
  }
  gDPPipeSync(D_8028A858++);
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xf5881000;
    _g->words.w1 = 0x00018060;
  }
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xf2000000;
    _g->words.w1 = 0x000fc0fc;
  }
  gRaw(D_8028A858++, 0xba000e02, 0);
  gRaw(D_8028A858++, 0xba001301, 0);
  cam = D_8028AAF4;
  BrMat4Inverse(D_8031AB10, cam->mtx0);
  D_8031AB50[0][0] = 0.0f;
  D_8031AB50[0][1] = 0.0f;
  D_8031AB50[0][3] = 0.0f;
  D_8031AB50[1][1] = 0.0f;
  D_8031AB50[1][2] = 0.0f;
  D_8031AB50[1][3] = 0.0f;
  D_8031AB50[2][0] = 0.0f;
  D_8031AB50[2][2] = 0.0f;
  D_8031AB50[2][3] = 0.0f;
  D_8031AB50[3][0] = 0.0f;
  D_8031AB50[3][1] = 0.0f;
  D_8031AB50[3][2] = 0.0f;
  D_8031AB50[0][2] = -1.0f;
  D_8031AB50[1][0] = -1.0f;
  D_8031AB50[2][1] = -1.0f;
  D_8031AB50[3][3] = 1.0f;
  BrMat4Mul(D_8031AB10, D_8031AB10, D_8031AB50);
  memcpy(D_8031AB50, D_8031AB10, 0x40);
  BrMat4Mul(D_80368A80, D_8031AB10, D_8031AA90);
  gRaw(D_8028A858++, 0xb9000201, 4);
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xba000602;
    _g->words.w1 = 0xc0;
  }
  if (D_8028AA80 == 0) {
    gRaw(D_8028A858++, 0xba000402, 0xc0);
    gRaw(D_8028A858++, 0xb900031d, 0xf0a0233);
    gRaw(D_8028A858++, 0xf9000000, 0);
    switch (D_8028B940) {
    case 4:
    case 9:
      BrParticleListDraw(D_8028C838, 0x70, 0x58, 0x38);
      BrParticleListDraw(D_8028C83C, 0x70, 0x68, 0x58);
      break;
    case 1:
    case 6:
      BrParticleListDraw(D_8028C838, 0x60, 0x54, 0x38);
      BrParticleListDraw(D_8028C83C, 0x60, 0x5c, 0x50);
      break;
    default:
      BrParticleListDraw(D_8028C838, 0xa0, 0x88, 0x60);
      BrParticleListDraw(D_8028C83C, 0x70, 0x68, 0x58);
      break;
    }
  }
  gDPPipeSync(D_8028A858++);
  gRaw(D_8028A858++, 0xba001301, 0x80000);
  gRaw(D_8028A858++, 0xb9000201, 0);
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xba000602;
    _g->words.w1 = D_8028A8A0;
  }
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xba000402;
    _g->words.w1 = D_8028A89C;
  }
  gRaw(D_8028A858++, 0xb9000002, 1);
}

/* WHAT IT DOES: Draw the particles: one-cycle mode with the 64x64 I8
 * particle sprite (0x802A3790) loaded as a texture block, texture filter from
 * its setting, no LUT or perspective, primitive depth, no dither; for the
 * race kind that steps the particle pool the live list is drawn in pale blue
 * (0xE0, 0xE0, 0xFF) through 0x8023D134.  Then perspective, pixel depth, the
 * two dither settings and the alpha-compare threshold are restored and
 * 0x8023BB50 runs.  The texture-load commands are written as multi-line
 * blocks: on one line IDO schedules their two words the other way round. */
/* @implements 0x8023DBC0 tgr BrParticleDraw */
void BrParticleDraw(void)
{
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, 0);
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xbb000001;
    _g->words.w1 = 0xffffffff;
  }
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 12, 2, D_8028A898);
  gDPSetCombine(D_8028A858++, 0xff97ff, 0xff2dfeff);
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xfd900000;
    _g->words.w1 = (unsigned int)&D_802A3790;
  }
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xf5900000;
    _g->words.w1 = 0x07018060;
  }
  gDPLoadSync(D_8028A858++);
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xf3000000;
    _g->words.w1 = 0x077ff100;
  }
  gDPPipeSync(D_8028A858++);
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xf5881000;
    _g->words.w1 = 0x00018060;
  }
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xf2000000;
    _g->words.w1 = 0x000fc0fc;
  }
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 14, 2, 0);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 19, 1, 0);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_L, 2, 1, 4);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 6, 2, 0xc0);
  if (D_8028AA84 != 0) {
    gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 4, 2, 0x80);
    gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_L, 3, 29, 0x504b50);
    BrParticleListDraw(D_8028C834, 0xe0, 0xe0, 0xff);
  }
  gDPPipeSync(D_8028A858++);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 19, 1, 0x80000);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_L, 2, 1, 0);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 6, 2, D_8028A8A0);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 4, 2, D_8028A89C);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_L, 0, 2, 1);
  BrSkidDraw();
}
