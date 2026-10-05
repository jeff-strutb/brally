/* trackdraw.c -- drawing the track: the grid cells in view and the objects
 * standing in them
 */
#include "tgr/common.h"

/* -- declarations -- */
typedef struct BrDrawSortItem { /* sorted before drawing */
  short x0;
  short key;                    /* 0x02 */
} BrDrawSortItem;
/* -- end declarations -- */
/* -- declarations: BrTrackDraw -- */
#include "tgr/gbi.h"
#include "tgr/season.h"
#include "tgr/track.h"
typedef struct BrMtx { be32_t m[16]; } BrMtx;
extern unsigned short D_80352580[];  /* the objects to draw: near first, then far */
extern unsigned char D_80351D80[];   /* per object: its grid cell's draw data */
extern Gfx *D_8028A858;
extern void *D_8028A878;        /* the viewport */
extern unsigned short D_8028A874;  /* the perspective normaliser */
extern int D_8028A898;
extern int D_8028A89C;
extern int D_8028A8A0;
extern int D_8028A8A8;
extern int D_8028A8AC;          /* the track is mirrored */
extern int D_8028AA08;          /* triangles, vertices and ... drawn */
extern int D_8028AA10;
extern int D_8028AA38;
extern int D_8028AA44;
extern int D_8028AA48;
extern int D_8028AA78;          /* fog */
extern int D_8028AA80;          /* rain */
extern int D_8028AA84;          /* snow */
extern int D_8028AA8C;          /* night */
extern int D_8028AB0C;          /* views on screen */
extern int D_8028AB58;          /* light colours */
extern int D_8028AB5C;
extern char D_8028C640[];       /* lights, 0x18 bytes each */
extern char D_8028C648[];
extern int D_8028C6A0;          /* the light in use */
extern int D_8028C740;          /* objects in the list */
extern int D_8028C744;          /* where the far part starts */
extern int D_8028C748;          /* the last object kept for the second pass */
extern int D_8028C74C;          /* render mode, cycle 1 */
extern int D_8028C750;          /* render mode, cycle 2 */
extern int D_8028C754;          /* the object hidden by the selector */
extern int D_8028C758;          /* the selector is on */
extern unsigned short D_8028C770;
extern int D_8028C774;          /* the lights' block */
extern int D_8028C778;
extern int D_8028C77C;
extern int D_8028C780;
extern int D_8028C784;
extern int D_8028C788;
extern int D_8028C78C;
extern float D_8035D51C;        /* the view matrix's largest element */
extern int D_8035D520;          /* object flags not drawn this frame */
extern float D_8031AA50[16];    /* the view matrix */
extern float D_8031AB10[16];    /* an object's matrix times the view */
extern int D_8031B360[4];
extern int D_8026FF18;          /* the game mode */
extern int D_8028B940;          /* the track */
#define D_8031C5BC TGR_PTR(BrSeason *, *TGR_PTR(TgrAddr *, 0x8031C5BC))   /* car 0\'s season (a member of D_8031B760) */    /* player one's season */
float BrMat4MaxAbs(float m[16]);
void BrFogSetup(void);
void BrViewportApply(void);
void BrPerfMark(int bar, int r, int g, int b, int a);
void BrTrackShadowDraw(int idx, unsigned int cars, int pass);
void BrTrackDrawSetup(void);
void BrObjSelCycle(void);
BrMtx *BrMtxAlloc(void);
void guMtxF2L(float *mf, BrMtx *m);
/* -- end declarations -- */
/* -- declarations: BrTrackDrawSetup -- */
#include "tgr/car.h"
#include "tgr/pad.h"
typedef struct BrDrawCell {     /* a grid cell near the camera (4 bytes) */
  unsigned char col;
  unsigned char row;
  short dist;                   /* 0x02  squared distance in cells */
} BrDrawCell;

typedef struct BrRaceEnt {      /* a race entity (0x78 bytes), as rank.c */
  char pad00[0x60];
  TgrAddr car;                   /* BrCar * -- 0x60 */
  char pad64[0x78 - 0x64];
} BrRaceEnt;
extern int D_8028BD94;          /* the rows the view's grid span covers */
extern int D_8028BD9C;
extern int D_8034E5B0[64];      /* per row: its first and last column */
extern int D_8034E6B0[64];
extern BrDrawCell D_8035D1D0[0xC1];  /* the cells to draw, nearest first */
extern int D_8028C76C;          /* the cell list overflowed */
extern int D_8026FF64;          /* the object the camera sits in, 0 for none */
extern float D_8028AACC;        /* the view's depth */
extern BrCarCam *D_8028AAF4;    /* the camera being drawn from */
extern BrCar *D_8028AAF0;       /* the car of the view being drawn */
extern int D_8028AAEC;
extern int D_8028B7F4;          /* cars racing */
extern int D_8028B7F0;          /* entities */
extern int D_80351C70[];        /* per car: draw it */
extern BrVec3 D_8035D500;
extern float D_8035D4D8[4];     /* the nearest cars ahead: distance, */
extern int D_8035D4E8[5];       /* and slot (one spare for the insertion) */
extern int D_8035D4FC;          /* and how many */
extern unsigned short D_8031B248[];  /* the objects around the camera */
extern int D_8028AB00;          /* and how many */
extern float D_802AA00C;
extern float D_8031B338[3];     /* the sun's direction */
extern BrVec3 D_8035D510;       /* the light direction this frame */
extern unsigned char D_8028A9F0[0x18];   /* the light template, as the RSP reads it */
extern BrRaceEnt D_803239A0[];
typedef struct BrViewRect { int x; int y; int w; int h; int x10; } BrViewRect;
extern BrViewRect D_8031B2C8[2];
unsigned int BrTrackGridCell(int col, int row);
unsigned short BrU16QueuePopB(unsigned short *q);
int BrGridSpanHasPoint(float x, float z);
void BrQsort(void *base, int n, int size, int (*cmp)());
void BrFill64(void *dst, int n, int value);
void BrVec3Sub(BrVec3 *out, BrVec3 *a, BrVec3 *b);
float BrVec3Dot(void *a, void *b);
void BrVec3Normalise(BrVec3 *v);
void BrVec3ScaleBy(BrVec3 *v, float s);
void * BrVpAlloc(void);
void guLookAtReflectF(float mf[4][4], void *l, float xEye, float yEye, float zEye, float xAt,
                      float yAt, float zAt, float xUp, float yUp, float zUp);
void BrProjectExtent(BrVec3 *v, int n, short *min, short *max);
/* -- end declarations -- */
/* -- declarations: BrTrackShadowDraw -- */
extern unsigned int *D_8028C75C;  /* the shadow display list: next command, */
extern unsigned int *D_8028C760;  /* its start */
extern short *D_8028C764;       /* the shadow vertices: next, */
extern short *D_8028C768;       /* and their start */
extern int D_802A3790;          /* the shadow texture */
extern int D_8028AA60;          /* clipping off */
extern unsigned char D_8028AB20;  /* the fog colour */
extern unsigned char D_8028AB24;
extern unsigned char D_8028AB28;
extern float D_802AA004;        /* the shadow's size */
extern float D_802AA008;
extern BrVec3 D_8035D1C0;       /* a car's heading on the ground */
extern float D_8031AB50[16];    /* the shadow's texture projection */
int BrFloatToInt(float f);
float BrVec3LenXY(BrVec3 *v);
void BrMat4Mul(float *out, float *a, float *b);
void BrScissorSet(int x, int y, int w, int h);
void BrFatal(char *s);
void BrCopy16(void *dst, unsigned int src);
/* -- end declarations -- */


#define G(a, b) { Gfx *g_ = D_8028A858++; tgr_wr32(&g_->words.w0, (a)); tgr_wr32(&g_->words.w1, TGR_W1(b)); }
/* all three corners of a triangle word lie off the same edge of the shadow */
#define TRIOUT(W) (clip[((W) >> 1) & 0x1F] & clip[((W) >> 9) & 0x1F] & clip[((W) >> 17) & 0x1F])

/* WHAT IT DOES: Draw one track object.  Normally just its display list.  With
 * car shadows falling on it (cars is the object's cell's shadow mask, and not
 * from the bonnet camera or for objects flagged 0x200) it is drawn normally
 * and then once more for each car whose bit is set (not the viewed car from
 * its in-car cameras): the shadow texture modulated by the car's fog, the
 * object's matrix taken to the car and mapped through a ground projection
 * scaled to the car's wheelbase, then its display list walked -- every
 * vertex copied into the shadow vertex buffer with texture coordinates from
 * that projection and tagged with the edges of the texture it falls off,
 * triangles wholly off one edge dropped -- until the shadow buffers run out.
 * pass picks the second render mode (no lit fog on the first pass, in rain
 * or at night).  PC twin: BrObjDlBuild. */
/* @t4-pass 0x80234050 1 2026-10-03 compiles 120 best 950 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80234050 2 2026-10-03 compiles 120 best 950 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x80234050 */
/* @implements 0x80234050 tgr BrTrackShadowDraw */
void BrTrackShadowDraw(int idx, unsigned int cars, int pass)
{
  BrTrackObj *obj;
  unsigned int *cmd;
  unsigned int *dl;
  unsigned int *mark;
  unsigned int *save;
  short *vtx;
  short *vtxBase;
  unsigned int c;
  unsigned int c2;
  unsigned int n;
  unsigned char *fl;
  unsigned char f;
  unsigned char clip[164];
  BrCar *car;
  BrCarModel *model;
  float s0;
  float s1;
  float k;
  float m00, m10, m20, m30, m01, m11, m21, m31;
  int i;

  vtx = D_8028C764;
  dl = D_8028C75C;
  obj = &BR_TRACKOBJS()[idx];
  cmd = BEPTR(unsigned int *, obj->dl);
  if (cars != 0 && D_8028AAF4 != &D_8028AAF0->cam4 && (BE16(obj->flags) & 0x200) == 0) {
    BrPerfMark(0, 0x50, 0xFA, 0x50, 0xFF);
    G(0xBB001001, 0xFFFFFFFF);
    G(0xE8000000, 0);
    G(0xFA001700, 0xFF0000FF);
    G(0x06000000, cmd);
    G(0xE7000000, 0);
    G(0xBA001402, 0);
    G(0xFD900000, &D_802A3790);
    G(0xF5900000, 0x07018060);
    G(0xE6000000, 0);
    G(0xF3000000, 0x077FF100);
    G(0xF5881000, 0x0009BE6F);
    G(0xF2000000, 0x000FC0FC);
    G(0xBB000001, 0xFFFFFFFF);
    G(0xBA001001, 0);
    G(0xFA001700, 0xFF0000FF);
    if (pass == 0 || D_8028AA80 != 0 || D_8028AA8C != 0) {
      G(0xB900031D, 0x00504F50);
    } else {
      G(0xB900031D, 0x00504B50);
    }
    G(0xB9000002, 1);
    G(0xF9000000, 8);
    G(0xFC567EAC, 0xFFFFF3F9);
    G(0xB6000000, 0x00070004);
    G(0xBA000602, 0xC0);
    G(0x06000000, dl);
    G(0xE7000000, 0);
    if (D_8028AA60 == 0) {
      BrScissorSet(D_8031B2C8[D_8028AAEC].x, D_8031B2C8[D_8028AAEC].y, D_8031B2C8[D_8028AAEC].w,
                   D_8031B2C8[D_8028AAEC].h);
    }
    G(0xBA000602, D_8028A8A0);
    G(0xF9000000, 0);
    if (D_8028AA78 != 0) {
      G(0xB7000000, 0x00030004);
    } else {
      G(0xB7000000, 0x00020004);
    }
    G(0xBA001001, D_8028AA44 != 0 ? 0x10000 : 0);
    G(0xFA001700, 0xFF0000FF);
    G(0xBA001402, 0x00100000);
    G(0xB900031D, D_8028C74C | D_8028C750);
    G(0xFC26A004, 0x1FFC93F8);
    D_8028AA10 += BE16(obj->vtxs) * 3;
    D_8028AA08 += BE16(obj->tris) * 3;
    D_8028AA38 += BE16(obj->x52) * 3;
    mark = 0;
    vtxBase = 0;
    for (i = 0; cars != 0; i++, cars = (int)cars >> 1) {
      if ((cars & 1) == 0 ||
          ((D_8028AAF4 == &D_8028AAF0->cams[0] || D_8028AAF4 == &D_8028AAF0->cams[2]) &&
           i == D_8028AAF0->slot)) {
        continue;
      }
      car = &D_8031B760[i];
      tgr_wr32(&dl[0], 0xE7000000);
      tgr_wr32(&dl[1], 0);
      tgr_wr32(&dl[2], 0xFB000000);
      tgr_wr32(&dl[3], BrFloatToInt(car->fog * 255.0f) & 0xFF | D_8028AB20 << 24 | D_8028AB24 << 16 | D_8028AB28 << 8);
      D_8035D1C0.x = car->mtx0[0][0];
      D_8035D1C0.y = car->mtx0[0][1];
      if (D_8035D1C0.x == 0.0f && D_8035D1C0.y == 0.0f) {
        D_8035D1C0.x = car->mtx0[2][0];
        D_8035D1C0.y = car->mtx0[2][1];
      }
      D_8035D1C0.z = 0;
      s0 = BrVec3LenXY(&D_8035D1C0);
      if (s0 < 0.5f) {
        s0 = 0.5f;
      }
      s1 = BrVec3LenXY((BrVec3 *)car->mtx0[1]);
      if (s1 < 0.5f) {
        s1 = 0.5f;
      }
      model = (BrCarModel *)TGR_PTR(char *, car->model);
      s0 = D_802AA004 / BEF(model->wheel[0][0]) / s0;
      s1 = D_802AA008 / BEF(model->wheel[0][1]) / s1;
      BrVec3Normalise(&D_8035D1C0);
      br_mat4_from((float (*)[4])D_8031AB10, (const bef_t (*)[4])obj->m);
      D_8031AB10[12] -= car->mtx0[3][0];
      D_8031AB10[13] -= car->mtx0[3][1];
      D_8031AB50[0] = D_8035D1C0.x * s0;
      D_8031AB50[4] = -(-D_8035D1C0.y * s0);
      D_8031AB50[8] = 0;
      D_8031AB50[12] = 512.0f;
      D_8031AB50[1] = -D_8035D1C0.y * s1;
      D_8031AB50[5] = D_8035D1C0.x * s1;
      D_8031AB50[9] = 0;
      D_8031AB50[13] = 512.0f;
      D_8031AB50[2] = 0;
      D_8031AB50[6] = 0;
      D_8031AB50[10] = 1.0f;
      D_8031AB50[14] = 0;
      D_8031AB50[3] = 0;
      D_8031AB50[7] = 0;
      D_8031AB50[11] = 0;
      D_8031AB50[15] = 1.0f;
      BrMat4Mul(D_8031AB10, D_8031AB10, D_8031AB50);
      k = D_8031AB10[15] + (D_8031AB10[3] + D_8031AB10[7] + D_8031AB10[11]);
      if (k == 0.0f) {
        k = 1.0f;
      } else {
        k = 1.0f / k;
      }
      D_8031AB10[0] *= k;
      D_8031AB10[4] *= k;
      D_8031AB10[8] *= k;
      D_8031AB10[12] *= k;
      D_8031AB10[1] *= k;
      D_8031AB10[5] *= k;
      D_8031AB10[9] *= k;
      D_8031AB10[13] *= k;
      dl += 4;
      if (D_8028AA60 == 0) {
        save = (unsigned int *)D_8028A858;
        D_8028A858 = (Gfx *)dl;
        BrScissorSet(((short *)car->pad2050)[0], ((short *)car->pad2050)[3],
                     ((short *)car->pad2050)[2] - ((short *)car->pad2050)[0],
                     ((short *)car->pad2050)[1] - ((short *)car->pad2050)[3]);
        dl = (unsigned int *)D_8028A858;
        D_8028A858 = (Gfx *)save;
      }
      cmd = BEPTR(unsigned int *, obj->dl);
      while ((dl - D_8028C760) < 0x3C9) {
        c = tgr_rd32(&cmd[0]);
        switch (c >> 24) {
        case 0x04:
          m00 = D_8031AB10[0];
          m10 = D_8031AB10[4];
          m20 = D_8031AB10[8];
          m30 = D_8031AB10[12];
          m01 = D_8031AB10[1];
          m11 = D_8031AB10[5];
          m21 = D_8031AB10[9];
          m31 = D_8031AB10[13];
          if (dl == mark) {
            dl -= 2;
            vtx = vtxBase;
          }
          tgr_wr32(&dl[0], c);
          n = (c >> 10) & 0x3F;
          if ((vtx + n * 8 - D_8028C768) / 8 > 1000) {
            goto next;
          }
          if (n > 32) {
            BrFatal("BAD VTX DL");
          }
          c2 = tgr_rd32(&cmd[1]);
          cmd += 2;
          tgr_wr32(&dl[1], tgr_addr32(vtx));
          dl += 2;
          mark = dl;
          vtxBase = vtx;
          for (fl = clip; n != 0; n--) {
            BrCopy16(vtx, c2);
            c2 += 0x10;
            tgr_wr16(&vtx[4], (short)(int)((short)tgr_rd16(&vtx[0]) * m00 + (short)tgr_rd16(&vtx[1]) * m10 + (short)tgr_rd16(&vtx[2]) * m20 + m30));
            tgr_wr16(&vtx[5], (short)(int)((short)tgr_rd16(&vtx[0]) * m01 + (short)tgr_rd16(&vtx[1]) * m11 + (short)tgr_rd16(&vtx[2]) * m21 + m31));
            if ((short)tgr_rd16(&vtx[4]) < 0) {
              f = 1;
            } else if ((short)tgr_rd16(&vtx[4]) < 0x400) {
              f = 0;
            } else {
              f = 2;
            }
            if ((short)tgr_rd16(&vtx[5]) < 0) {
              f |= 4;
            } else if ((short)tgr_rd16(&vtx[5]) >= 0x1000) {
              f |= 8;
            }
            *fl++ = f;
            vtx += 8;
          }
          break;
        case 0xB1:
          if (D_8028AA60 != 0) {
            tgr_wr32(&dl[0], c);
            tgr_wr32(&dl[1], tgr_rd32(&cmd[1]));
            dl += 2;
            cmd += 2;
            break;
          }
          if (TRIOUT(c) == 0) {
            c2 = tgr_rd32(&cmd[1]);
            if (TRIOUT(c2) == 0) {
              tgr_wr32(&dl[0], c);
              tgr_wr32(&dl[1], tgr_rd32(&cmd[1]));
              dl += 2;
            } else {
              tgr_wr32(&dl[0], 0xBF000000);
              tgr_wr32(&dl[1], tgr_rd32(&cmd[0]) & 0xFFFFFF);
              dl += 2;
            }
          } else {
            c2 = tgr_rd32(&cmd[1]);
            if (TRIOUT(c2) == 0) {
              tgr_wr32(&dl[0], 0xBF000000);
              tgr_wr32(&dl[1], tgr_rd32(&cmd[1]) & 0xFFFFFF);
              dl += 2;
            }
          }
          cmd += 2;
          break;
        case 0xB8:
          goto done;
        case 0xBF:
          if (D_8028AA60 != 0) {
            tgr_wr32(&dl[0], c);
            tgr_wr32(&dl[1], tgr_rd32(&cmd[1]));
            dl += 2;
            cmd += 2;
            break;
          }
          c2 = tgr_rd32(&cmd[1]);
          if (TRIOUT(c2) == 0) {
            tgr_wr32(&dl[0], c);
            tgr_wr32(&dl[1], tgr_rd32(&cmd[1]));
            dl += 2;
          }
          cmd += 2;
          break;
        default:
          cmd += 2;
          break;
        }
      }
    done:
      if (dl == mark) {
        dl -= 2;
        vtx = vtxBase;
      }
    next:;
    }
    tgr_wr32(&dl[0], 0xB8000000);
    tgr_wr32(&dl[1], 0);
    BrPerfMark(0, 0xFF, 0x80, 0x80, 0xFF);
    D_8028C764 = vtx;
    D_8028C75C = dl + 2;
  } else {
    G(0xBB001001, 0xFFFFFFFF);
    G(0xE8000000, 0);
    G(0x06000000, cmd);
    D_8028AA10 += BE16(obj->vtxs);
    D_8028AA08 += BE16(obj->tris);
    D_8028AA38 += BE16(obj->x52);
  }
}



/* WHAT IT DOES: The qsort comparison the track drawer sorts with: by the
 * 16-bit key at +2, ascending. */
/* @implements 0x80234FC0 tgr BrDrawSortCmp */
int BrDrawSortCmp(BrDrawSortItem *a, BrDrawSortItem *b)
{
  if (a->key > b->key) {
    return 1;
  }
  if (a->key < b->key) {
    return -1;
  }
  return 0;
}



/* WHAT IT DOES: Set up a view's track drawing: list the grid cells the view's
 * span covers (at most 192), each with its squared distance in cells from the
 * camera, and sort them nearest first; then walk them collecting the track
 * objects to draw (the object the camera is inside first, when it shows),
 * noting where the list passes 8 cells, the close-camera range and the view
 * depth; mark the objects near the four nearest cars ahead of the camera
 * (their shadows), gather the hide flags of the objects around the camera,
 * build the reflection look-at and the light (the sun, or at night a light
 * hung in front of the car), and clamp each car's screen extent to the
 * view. */
/* @t4-pass 0x80234FF8 1 2026-10-03 compiles 116 best 689 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80234FF8 2 2026-10-03 compiles 116 best 689 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x80234FF8 */
/* @implements 0x80234FF8 tgr BrTrackDrawSetup */
void BrTrackDrawSetup(void)
{
  int n;
  int r;
  int c;
  int cx;
  int cy;
  int i;
  int k;
  int d;
  int lim;
  int near2;
  int slot;
  unsigned int cell;
  unsigned short q[2];
  unsigned short o;
  float dot;
  float lx;
  float ly;
  BrCar *car;
  BrVec3 v;
  BrDrawCell *p;

  D_8028C76C = 0;
  n = 0;
  for (r = D_8028BD94; r <= D_8028BD9C; r++) {
    for (c = D_8034E5B0[r]; c <= D_8034E6B0[r]; c++) {
      if (n == 0xC0) {
        D_8028C76C = 1;
        goto full;
      }
      D_8035D1D0[n].col = c;
      D_8035D1D0[n].row = r;
      n++;
    }
  }
full:
  D_8035D1D0[n].col = 0xFF;
  D_8035D1D0[n].row = 0xFF;
  cx = D_8028AAF4->mtx[3][0] / 32.0f;
  if (cx == 64) {
    cx = 63;
  }
  cy = D_8028AAF4->mtx[3][1] / 32.0f;
  if (cy == 64) {
    cy = 63;
  }
  for (i = 0; i < n; i++) {
    D_8035D1D0[i].dist = (D_8035D1D0[i].col - cx) * (D_8035D1D0[i].col - cx) +
                         (D_8035D1D0[i].row - cy) * (D_8035D1D0[i].row - cy);
  }
  BrQsort(D_8035D1D0, n, 4, BrDrawSortCmp);
  BrFill64(D_80351D80, BES32(D_80025C00.nObjs), -1);
  D_8028C740 = 0;
  D_8028C778 = -1;
  D_8028C780 = -1;
  D_8028C788 = -1;
  near2 = (int)(D_8028AACC / (float)32) - 3;
  near2 = near2 * near2;
  if (((BrPadRec *)TGR_PTR(unsigned int *, D_8028AAF0->pad))->ghost == 0 || D_8028AAF4 != &D_8028AAF0->cams[3]) {
    lim = 1;
  } else {
    lim = 9;
  }
  for (p = D_8035D1D0; p->col != 0xFF; p++) {
    d = p->dist;
    if (8 < d && D_8028C778 == -1) {
      D_8028C778 = D_8028C740;
    }
    if (lim < d && D_8028C788 == -1) {
      D_8028C788 = D_8028C740;
    }
    if (near2 < d && D_8028C780 == -1) {
      D_8028C780 = D_8028C740;
    }
    if (D_8026FF64 != 0 && D_80351D80[D_8026FF64] != 0 &&
        BrGridSpanHasPoint(BEF(BR_TRACKOBJS()[D_8026FF64].m[3][0]), BEF(BR_TRACKOBJS()[D_8026FF64].m[3][1]))) {
      D_80352580[D_8028C740++] = D_8026FF64;
      D_80351D80[D_8026FF64] = 0;
    }
    cell = BrTrackGridCell(p->col, p->row);
    q[1] = cell >> 16;
    q[0] = cell;
    if (cell != 0) {
      while ((o = BrU16QueuePopB(q)) != 0) {
        if (D_80351D80[o] != 0 && D_8026FF64 != o) {
          D_80352580[D_8028C740++] = o;
          D_80351D80[o] = 0;
        }
      }
    }
  }
  D_8035D4FC = 0;
  for (i = 0; i < D_8028B7F4; i++) {
    if (D_80351C70[i] != 0) {
      BrVec3Sub(&D_8035D500, (BrVec3 *)D_8031B760[i].mtx0[3], (BrVec3 *)D_8028AAF4->mtx[3]);
      dot = BrVec3Dot(D_8028AAF4, &D_8035D500);
      if (dot >= 2.0f) {
        for (k = D_8035D4FC - 1; k >= 0 && dot < D_8035D4D8[k]; k--) {
          D_8035D4D8[k + 1] = D_8035D4D8[k];
          D_8035D4E8[k + 1] = D_8035D4E8[k];
        }
        D_8035D4D8[k + 1] = dot;
        D_8035D4E8[k + 1] = i;
        D_8035D4FC++;
      }
    }
  }
  if (D_8035D4FC > 4) {
    D_8035D4FC = 4;
  }
  for (i = 0; i < D_8035D4FC; i++) {
    slot = D_8035D4E8[i];
    for (k = 0; k < D_8031B760[slot].x2044; k++) {
      D_80351D80[D_8031B760[slot].x2004[k]] |= 1 << slot;
    }
  }
  D_8028C770 = 0;
  for (i = 0; i < D_8028AB00; i++) {
    D_8028C770 |= BE16(BR_TRACKOBJS()[D_8031B248[i]].x4a);
  }
  lx = D_8028AAF4->mtx[0][0];
  ly = D_8028AAF4->mtx[0][1];
  if (lx == 0.0f && ly == 0.0f) {
    lx = D_802AA00C;
  }
  D_8028C774 = tgr_addr32(BrVpAlloc());
  guLookAtReflectF((float (*)[4])D_8031AB10, TGR_PTR(void *, D_8028C774), lx * 50.0f, ly * 50.0f, 0, 0, 0, 0, 0, 0, 1.0f);
  if (D_8028AA80 != 0) {
    D_8035D510.x = D_8028AAF0->mtx0[0][0] * -4.0f + D_8028AAF0->mtx0[2][0];
    D_8035D510.y = D_8028AAF0->mtx0[0][1] * -4.0f + D_8028AAF0->mtx0[2][1];
    D_8035D510.z = D_8028AAF0->mtx0[0][2] * -4.0f + D_8028AAF0->mtx0[2][2];
  } else {
    D_8035D510.x = D_8031B338[0];
    D_8035D510.y = D_8031B338[1];
    D_8035D510.z = D_8031B338[2];
  }
  BrVec3Normalise(&D_8035D510);
  BrVec3ScaleBy(&D_8035D510, 120.0f);
  D_8028C6A0 = (D_8028C6A0 + 1) % 4;
  memcpy(D_8028C640 + D_8028C6A0 * 0x18, D_8028A9F0, 0x18);   /* six words, as they are */
  D_8028C640[D_8028C6A0 * 0x18 + 0x10] = (int)D_8035D510.x;
  D_8028C640[D_8028C6A0 * 0x18 + 0x11] = (int)D_8035D510.y;
  D_8028C640[D_8028C6A0 * 0x18 + 0x12] = (int)D_8035D510.z;
  for (i = 0; i < D_8028B7F0; i++) {
    car = TGR_PTR(BrCar *, D_803239A0[i].car);
    if (car != 0) {
      v.x = car->mtx0[3][0];
      v.y = car->mtx0[3][1];
      v.z = car->mtx0[3][2] - car->x2048;
      BrProjectExtent(&v, 8, (short *)car->pad2050, (short *)car->pad2050 + 2);
      if (((short *)car->pad2050)[0] < D_8031B2C8[D_8028AAEC].x) {
        ((short *)car->pad2050)[0] = D_8031B2C8[D_8028AAEC].x;
      }
      if (((short *)car->pad2050)[2] < D_8031B2C8[D_8028AAEC].x) {
        ((short *)car->pad2050)[2] = D_8031B2C8[D_8028AAEC].x;
      }
      if (((short *)car->pad2050)[1] < D_8031B2C8[D_8028AAEC].y) {
        ((short *)car->pad2050)[1] = D_8031B2C8[D_8028AAEC].y;
      }
      if (((short *)car->pad2050)[3] < D_8031B2C8[D_8028AAEC].y) {
        ((short *)car->pad2050)[3] = D_8031B2C8[D_8028AAEC].y;
      }
      if (D_8031B2C8[D_8028AAEC].w + D_8031B2C8[D_8028AAEC].x < ((short *)car->pad2050)[0]) {
        ((short *)car->pad2050)[0] = D_8031B2C8[D_8028AAEC].w + D_8031B2C8[D_8028AAEC].x;
      }
      if (D_8031B2C8[D_8028AAEC].w + D_8031B2C8[D_8028AAEC].x < ((short *)car->pad2050)[2]) {
        ((short *)car->pad2050)[2] = D_8031B2C8[D_8028AAEC].w + D_8031B2C8[D_8028AAEC].x;
      }
      if (D_8031B2C8[D_8028AAEC].h + D_8031B2C8[D_8028AAEC].y < ((short *)car->pad2050)[1]) {
        ((short *)car->pad2050)[1] = D_8031B2C8[D_8028AAEC].h + D_8031B2C8[D_8028AAEC].y;
      }
      if (D_8031B2C8[D_8028AAEC].h + D_8031B2C8[D_8028AAEC].y < ((short *)car->pad2050)[3]) {
        ((short *)car->pad2050)[3] = D_8031B2C8[D_8028AAEC].h + D_8031B2C8[D_8028AAEC].y;
      }
    }
  }
}


/* WHAT IT DOES: Draw the track's objects for one view, in two passes (the
 * near objects first, then those kept back).  The first pass culls the list
 * and sets which object flags are skipped for the weather and time of day;
 * both set up the RSP (viewport, lights, fog, render mode) and then, for each
 * object in the list: skip it if hidden or selected away, give it its own
 * matrix (a billboard-scaled one built here for objects flagged 0x2000,
 * clamped into the fixed-point range with a debug dump when it overflows),
 * switch the lighting, texture and fog state its flags ask for, draw it, and
 * undo those state changes.  (The box reaches it in one-player races in
 * clear weather: the split-screen settings, the fixed-point overflow and the
 * weather-dependent flag paths are not run yet.) */
/* @t4-pass 0x80235BAC 1 2026-09-28 compiles 21 best 1894 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80235BAC 2 2026-09-28 compiles 21 best 1894 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x80235BAC */
/* @implements 0x80235BAC tgr BrTrackDraw */
void BrTrackDraw(int pass)
{
  float om[4][4];
  int noRain;
  int billboard;
  int noNight;
  int i;
  int last;
  int split;
  int end;
  unsigned int obj;
  unsigned int fl;
  unsigned int a;
  unsigned int b;
  unsigned int c;
  BrTrackObj *o;
  BrMtx *mtx;
  float s;
  float hi;
  float lo;
  float t;

  noRain = D_8028AA80 == 0;
  billboard = 0;
  noNight = D_8028AA8C == 0;
  if (pass == 0) {
    BrTrackDrawSetup();
    D_8028C748 = -1;
    last = -1;
    D_8028C744 = D_8028C740;
    split = D_8028C740;
    D_8035D51C = BrMat4MaxAbs(D_8031AA50);
    D_8035D520 = 0;
    if (noRain && noNight) {
      D_8035D520 = 0x800;
    }
    if (D_8026FF18 != 1 && (D_8026FF18 != 5 || D_8031C5BC->round != 0)) {
      D_8035D520 |= 0x4000;
    }
    if (D_8028AA78 != 0 && (D_8028AA84 != 0 || D_8028AA8C != 0 || D_8028AA80 != 0)
        && D_8028B940 != 2 && D_8028B940 != 7) {
      D_8035D520 |= 0x20;
    }
    D_8028C784 = 0x1000;
    D_8028C77C = 0x1000;
    D_8028C78C = 0x1000;
  }
  BrPerfMark(0, 0xFF, 0x80, 0x80, 0xFF);
  if (D_8028AA78 == 0) {
    D_8028C74C = 0x0C080000;
  } else {
    D_8028C74C = 0xC8000000;
  }
  D_8028C750 = 0x112038;
  G(0x01030040, tgr_addr32(D_8028A878) - 0x80000000u);
  G(0x01060040, 0x28A8C0);
  G(0xBC00000E, D_8028A874);
  G(0x03840010, D_8028C774);
  G(0x03820010, D_8028C774 + 0x10);
  G(0xBC000002, 0x80000040);
  G(0x03860010, tgr_addr32(&D_8028C648[D_8028C6A0 * 0x18]));
  G(0x03880010, tgr_addr32(&D_8028C640[D_8028C6A0 * 0x18]));
  G(0xBC00000A, D_8028AB58);
  G(0xBC00040A, D_8028AB58);
  G(0xBC00200A, D_8028AB5C);
  G(0xBC00240A, D_8028AB5C);
  G(0xE7000000, 0);
  G(0xBA001301, 0x80000);
  G(0xBA000903, 0xC00);
  G(0xBA000801, 0);
  G(0xB9000002, 1);
  G(0xBA000602, D_8028A8A0);
  G(0xBA000402, D_8028A89C);
  G(0xBA001402, 0);
  G(0xF9000000, 0);
  G(0xBA001402, 0x100000);
  G(0xB900031D, D_8028C74C | D_8028C750);
  G(0xFC26A004, 0x1FFC93F8);
  G(0xBA001102, 0);
  G(0xBA001001, D_8028AA44 == 0 ? 0 : 0x10000);
  G(0xBA000E02, 0);
  G(0xBA000C02, D_8028A898);
  G(0xBC000006, 0);
  G(0xB6000000, 0x853200);
  a = D_8028AA78 == 0 ? 0 : 0x10000;
  b = D_8028AA48 == 0 ? 0 : 0x200;
  c = 0x2000;
  if (D_8028A8AC != D_8028A8A8) {
    c = 0x1000;
  }
  G(0xB7000000, c | 0xA0005 | b | a);
  BrViewportApply();
  G(0x01030040, tgr_addr32(D_8028A878) - 0x80000000u);
  G(0xBC00000E, D_8028A874);
  G(0x01060040, 0x28A8C0);
  G(0xBC000002, 0x80000040);
  G(0x03860010, tgr_addr32(&D_8028C648[D_8028C6A0 * 0x18]));
  G(0x03880010, tgr_addr32(&D_8028C640[D_8028C6A0 * 0x18]));
  G(0xBC00000A, D_8028AB58);
  G(0xBC00040A, D_8028AB58);
  G(0xBC00200A, D_8028AB5C);
  G(0xBC00240A, D_8028AB5C);
  BrFogSetup();
  BrFogSetup();
  G(0xF9000000, 0);
  G(0xB6000000, 0x53200);
  a = D_8028AA78 == 0 ? 0 : 0x10000;
  b = 0;
  if (D_8028AA48 != 0) {
    b = 0x200;
  }
  c = 0x2000;
  if (D_8028A8AC != D_8028A8A8) {
    c = 0x1000;
  }
  G(0xB7000000, c | 0xA0005 | b | a);
  G(0xE7000000, 0);
  G(0xBA001402, 0x100000);
  G(0xB900031D, D_8028C74C | D_8028C750);
  G(0xFC26A004, 0x1FFC93F8);
  G(0xFA001700, 0xFF0000FF);
  G(0xBA001102, 0);
  G(0xBA001001, D_8028AA44 == 0 ? 0 : 0x10000);
  G(0xBA000E02, 0);
  G(0xBA000C02, D_8028A898);
  if (D_8028AB0C == 1) {
    G(0xBC000404, 1);
    G(0xBC000C04, 1);
    G(0xBC001404, 0xFFFF);
    G(0xBC001C04, 0xFFFF);
  } else {
    G(0xBC000404, 6);
    G(0xBC000C04, 6);
    G(0xBC001404, 0xFFFA);
    G(0xBC001C04, 0xFFFA);
  }
  G(0xBB001001, 0xFFFFFFFF);
  G(0xB6000000, 0xC0000);
  G(0xE8000000, 0);
  G(0xF5100000, 0x7000000);
  G(0xF50001F0, 0x6000000);
  G(0xF5000100, 0x5000000);
  if (pass == 0) {
    BrObjSelCycle();
    i = 0;
  } else {
    last = D_8028C748;
    D_8028C740 = D_8028C744 + D_8028C748 + 1;
    split = D_8028C744;
    i = D_8028C744;
  }
  for (; i < D_8028C740; i++) {
    if (i == D_8028C778 || i == D_8028C780) {
      if (D_8028AB0C == 1) {
        G(0xBC000404, 1);
        G(0xBC000C04, 1);
        G(0xBC001404, 0xFFFF);
        G(0xBC001C04, 0xFFFF);
      } else {
        G(0xBC000404, 6);
        G(0xBC000C04, 6);
        G(0xBC001404, 0xFFFA);
        G(0xBC001C04, 0xFFFA);
      }
      if (i == D_8028C780) {
        G(0xB7000000, 0x800000);
      }
    }
    if (i < split) {
      /* the first pass: an object flagged 8 is kept back for the second */
      obj = D_80352580[i];
      o = &BR_TRACKOBJS()[obj];
      if (BE16(o->hide) & D_8028C770) {
        continue;
      }
      if (D_8028C758 != 0 && D_8028C754 == obj) {
        continue;
      }
      fl = BE16(o->flags);
      if (fl & D_8035D520) {
        continue;
      }
      if (fl & 8) {
        if (D_8028C78C == 0x1000 && D_8028C788 < i) {
          D_8028C78C = last + 1;
        }
        last++;
        D_80352580[last] = D_80352580[i];
        continue;
      }
    } else {
      obj = D_80352580[last - i + split];
      o = &BR_TRACKOBJS()[obj];
      fl = BE16(o->flags);
    }
    if ((fl & 0x2000) == 0) {
      billboard = 0;
      mtx = BrMtxAlloc();
      br_mat4_from(om, (const bef_t (*)[4])o->m);   /* the cartridge's matrix, natively */
      guMtxF2L(&om[0][0], mtx);
      G(0x01020040, tgr_addr32(mtx) - 0x80000000);
    } else {
      /* a billboard: the view's rotation scaled to the object's size, the
       * whole matrix brought back into the fixed-point range */
      if (!billboard) {
        G(0x01020040, 0x28A8C0);
        billboard = 1;
      }
      D_8031AB10[11] = BEF(o->m[0][0]) * D_8035D51C + 0.375f;
      if (D_8031AB10[11] == 0.0f) {
        D_8031AB10[11] = BEF(o->m[0][0]) * 1.1 * D_8035D51C + 0.375;
      }
      D_8031AB10[11] = 1.99975586f / D_8031AB10[11];
      D_8031AB10[12] = (BEF(o->m[3][0]) * D_8031AA50[0] + BEF(o->m[3][1]) * D_8031AA50[4]
                        + BEF(o->m[3][2]) * D_8031AA50[8] + D_8031AA50[12] * BEF(o->m[3][3]))
                       * D_8031AB10[11];
      D_8031AB10[13] = (BEF(o->m[3][0]) * D_8031AA50[1] + BEF(o->m[3][1]) * D_8031AA50[5]
                        + BEF(o->m[3][2]) * D_8031AA50[9] + D_8031AA50[13] * BEF(o->m[3][3]))
                       * D_8031AB10[11];
      D_8031AB10[14] = (BEF(o->m[3][0]) * D_8031AA50[2] + BEF(o->m[3][1]) * D_8031AA50[6]
                        + BEF(o->m[3][2]) * D_8031AA50[10] + D_8031AA50[14] * BEF(o->m[3][3]))
                       * D_8031AB10[11];
      D_8031AB10[15] = (BEF(o->m[3][0]) * D_8031AA50[3] + BEF(o->m[3][1]) * D_8031AA50[7]
                        + BEF(o->m[3][2]) * D_8031AA50[11] + D_8031AA50[15] * BEF(o->m[3][3]))
                       * D_8031AB10[11];
      D_8031AB10[11] = D_8031AB10[11] * BEF(o->m[0][0]);
      D_8031AB10[0] = D_8031AA50[0] * D_8031AB10[11];
      D_8031AB10[1] = D_8031AA50[1] * D_8031AB10[11];
      D_8031AB10[2] = D_8031AA50[2] * D_8031AB10[11];
      D_8031AB10[3] = D_8031AA50[3] * D_8031AB10[11];
      D_8031AB10[4] = D_8031AA50[4] * D_8031AB10[11];
      D_8031AB10[5] = D_8031AA50[5] * D_8031AB10[11];
      D_8031AB10[6] = D_8031AA50[6] * D_8031AB10[11];
      D_8031AB10[7] = D_8031AA50[7] * D_8031AB10[11];
      D_8031AB10[8] = D_8031AA50[8] * D_8031AB10[11];
      D_8031AB10[9] = D_8031AA50[9] * D_8031AB10[11];
      D_8031AB10[10] = D_8031AA50[10] * D_8031AB10[11];
      D_8031AB10[11] = D_8031AA50[11] * D_8031AB10[11];
      mtx = BrMtxAlloc();
      /* the translation row's extremes (hi >= 0 >= lo) */
      hi = D_8031AB10[12];
      if (D_8031AB10[12] < 0.0f) {
        hi = 0.0f;
      }
      lo = D_8031AB10[12];
      if (0.0f < D_8031AB10[12]) {
        lo = 0.0f;
      }
      t = D_8031AB10[13];
      if (D_8031AB10[13] < hi) {
        t = hi;
      }
      hi = D_8031AB10[13];
      if (lo < D_8031AB10[13]) {
        hi = lo;
      }
      lo = D_8031AB10[14];
      if (D_8031AB10[14] < t) {
        lo = t;
      }
      t = D_8031AB10[14];
      if (hi < D_8031AB10[14]) {
        t = hi;
      }
      hi = D_8031AB10[15];
      if (D_8031AB10[15] < lo) {
        hi = lo;
      }
      lo = D_8031AB10[15];
      if (t < D_8031AB10[15]) {
        lo = t;
      }
      if (32767.0 < hi || lo < -32767.0) {
        if (i == 1) {
          osSyncPrintf("Bad Final Matrix: in=%d (%s), s=%f\n-------------\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n-------------\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n-------------\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n-------------\n",
                       obj, BE32(D_80025C00.names) == 0 ? "???" : BEPTR(char *, BEPTR(be32_t *, D_80025C00.names)[obj]), D_8035D51C);
        }
        if (-lo < hi) {
          s = 32767.0f / hi;
        } else {
          s = -32767.0f / lo;
        }
        D_8031AB10[0] *= s;
        D_8031AB10[1] *= s;
        D_8031AB10[2] *= s;
        D_8031AB10[3] *= s;
        D_8031AB10[4] *= s;
        D_8031AB10[5] *= s;
        D_8031AB10[6] *= s;
        D_8031AB10[7] *= s;
        D_8031AB10[8] *= s;
        D_8031AB10[9] *= s;
        D_8031AB10[10] *= s;
        D_8031AB10[11] *= s;
        D_8031AB10[12] *= s;
        D_8031AB10[13] *= s;
        D_8031AB10[14] *= s;
        D_8031AB10[15] *= s;
      }
      guMtxF2L(D_8031AB10, mtx);
      G(0x039E0010, tgr_addr32(mtx));
      G(0x03980010, tgr_addr32(mtx) + 0x10);
      G(0x039A0010, tgr_addr32(mtx) + 0x20);
      G(0x039C0010, tgr_addr32(mtx) + 0x30);
    }
    /* the state the object's flags ask for */
    fl = BE16(o->flags);
    if (fl & 0x4A4) {
      if (fl & 0x400) {
        if (D_8028AA80 == 0 || (fl & 0x100) == 0) {
          G(0xBC00000A, 0);
          G(0xBC00040A, 0);
          G(0xBC00200A, D_8031B360[BE16(o->flags) & 3]);
          G(0xBC00240A, D_8031B360[BE16(o->flags) & 3]);
        } else {
          G(0xBC00000A, (D_8028AB58 >> 1) & 0x7F7F7F00);
          G(0xBC00040A, (D_8028AB58 >> 1) & 0x7F7F7F00);
          G(0xBC00200A, D_8031B360[BE16(o->flags) & 3]);
          G(0xBC00240A, D_8031B360[BE16(o->flags) & 3]);
        }
        fl = BE16(o->flags);
      }
      if (fl & 4) {
        G(0xB6000000, 0x3000);
        fl = BE16(o->flags);
      }
      if ((fl & 0x20) && D_8028AA78 != 0 && D_8028AA80 == 0 && D_8028AA84 == 0
          && D_8028AA8C == 0) {
        G(0xB6000000, 0x10000);
        fl = BE16(o->flags);
      }
      if ((fl & 0x80) && D_8028AA48 != 0) {
        G(0xB6000000, 0x200);
      }
    }
    if (D_8028C788 < i && i < D_8028C78C) {
      G(0xBB001001, 0xFFFFFFFF);
      G(0xE8000000, 0);
      G(0x06000000, BE32(o->dl));
      D_8028AA10 += BES16(o->vtxs);
      D_8028AA08 += BES16(o->tris);
      D_8028AA38 += BES16(o->x52);
    } else {
      BrTrackShadowDraw(obj, D_80351D80[obj], pass);
    }
    fl = BE16(o->flags);
    if (fl & 0x4A4) {
      if ((fl & 0x80) && D_8028AA48 != 0) {
        G(0xB7000000, 0x200);
        fl = BE16(o->flags);
      }
      if (fl & 0x400) {
        G(0xBC00000A, D_8028AB58);
        G(0xBC00040A, D_8028AB58);
        G(0xBC00200A, D_8028AB5C);
        G(0xBC00240A, D_8028AB5C);
        fl = BE16(o->flags);
      }
      if (fl & 4) {
        G(0xB7000000, D_8028A8AC == D_8028A8A8 ? 0x2000 : 0x1000);
      }
      if (D_8028AA78 != 0 && D_8028AA80 == 0 && D_8028AA84 == 0 && D_8028AA8C == 0
          && (BES16(o->flags) & 0x20)) {
        G(0xB7000000, 0x10000);
      }
    }
  }
  if (pass == 0) {
    end = last;
    if (D_8028C78C != 0x1000) {
      end = D_8028C78C;
    }
    D_8028C78C = last - end + split;
  }
  G(0x01020040, 0x28A8C0);
  G(0xB6000000, 0x10000);
  G(0xE7000000, 0);
  BrPerfMark(0, 0, 0xB4, 0, 0xFF);
  D_8028C748 = last;
}
