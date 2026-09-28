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

/* -- declarations: BrTrackDraw -- */
#include "tgr/gbi.h"
#include "tgr/season.h"
typedef struct BrTrackObj {     /* a track object (0x54 bytes) */
  float m[4][4];                /* 0x00  its matrix; m[0][0] is its scale */
  int x40;
  unsigned int dl;              /* 0x44  its display list (segment 6) */
  unsigned short hide;          /* 0x48  hidden when it meets D_8028C770 */
  unsigned short x4a;
  unsigned short flags;         /* 0x4C */
  unsigned short tris;          /* 0x4E */
  unsigned short vtxs;          /* 0x50 */
  unsigned short x52;
} BrTrackObj;
typedef struct BrMtx { int m[16]; } BrMtx;
extern BrTrackObj *D_80025C60;  /* the track's objects */
extern char **D_80025C5C;       /* their names, 0 when the build has none */
extern unsigned short D_80352580[];  /* the objects to draw: near first, then far */
extern unsigned char D_80351D80[];   /* per object: its grid cell's draw data */
extern Gfx *D_8028A858;
extern int D_8028A878;          /* the viewport */
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
extern BrSeason *D_8031C5BC;    /* player one's season */
float BrMat4MaxAbs(float m[16]);
void func_802182A8(void);
void BrViewportApply(void);
void BrPerfMark(int bar, int r, int g, int b, int a);
void func_80234050(int obj, int cell, int pass);
void func_80234FF8(void);
void BrObjSelCycle(void);
BrMtx *BrMtxAlloc(void);
void guMtxF2L(float *mf, BrMtx *m);
void osSyncPrintf(char *fmt, ...);
/* -- end declarations -- */

#define G(a, b) { Gfx *g_ = D_8028A858++; g_->words.w0 = (a); g_->words.w1 = (b); }

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
    func_80234FF8();
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
  G(0x01030040, D_8028A878 - 0x80000000);
  G(0x01060040, 0x28A8C0);
  G(0xBC00000E, D_8028A874);
  G(0x03840010, D_8028C774);
  G(0x03820010, D_8028C774 + 0x10);
  G(0xBC000002, 0x80000040);
  G(0x03860010, (int)&D_8028C648[D_8028C6A0 * 0x18]);
  G(0x03880010, (int)&D_8028C640[D_8028C6A0 * 0x18]);
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
  G(0x01030040, D_8028A878 - 0x80000000);
  G(0xBC00000E, D_8028A874);
  G(0x01060040, 0x28A8C0);
  G(0xBC000002, 0x80000040);
  G(0x03860010, (int)&D_8028C648[D_8028C6A0 * 0x18]);
  G(0x03880010, (int)&D_8028C640[D_8028C6A0 * 0x18]);
  G(0xBC00000A, D_8028AB58);
  G(0xBC00040A, D_8028AB58);
  G(0xBC00200A, D_8028AB5C);
  G(0xBC00240A, D_8028AB5C);
  func_802182A8();
  func_802182A8();
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
      o = &D_80025C60[obj];
      if (o->hide & D_8028C770) {
        continue;
      }
      if (D_8028C758 != 0 && D_8028C754 == obj) {
        continue;
      }
      fl = o->flags;
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
      o = &D_80025C60[obj];
      fl = o->flags;
    }
    if ((fl & 0x2000) == 0) {
      billboard = 0;
      mtx = BrMtxAlloc();
      guMtxF2L(&o->m[0][0], mtx);
      G(0x01020040, (int)mtx - 0x80000000);
    } else {
      /* a billboard: the view's rotation scaled to the object's size, the
       * whole matrix brought back into the fixed-point range */
      if (!billboard) {
        G(0x01020040, 0x28A8C0);
        billboard = 1;
      }
      D_8031AB10[11] = o->m[0][0] * D_8035D51C + 0.375f;
      if (D_8031AB10[11] == 0.0f) {
        D_8031AB10[11] = o->m[0][0] * 1.1 * D_8035D51C + 0.375;
      }
      D_8031AB10[11] = 1.99975586f / D_8031AB10[11];
      D_8031AB10[12] = (o->m[3][0] * D_8031AA50[0] + o->m[3][1] * D_8031AA50[4]
                        + o->m[3][2] * D_8031AA50[8] + D_8031AA50[12] * o->m[3][3])
                       * D_8031AB10[11];
      D_8031AB10[13] = (o->m[3][0] * D_8031AA50[1] + o->m[3][1] * D_8031AA50[5]
                        + o->m[3][2] * D_8031AA50[9] + D_8031AA50[13] * o->m[3][3])
                       * D_8031AB10[11];
      D_8031AB10[14] = (o->m[3][0] * D_8031AA50[2] + o->m[3][1] * D_8031AA50[6]
                        + o->m[3][2] * D_8031AA50[10] + D_8031AA50[14] * o->m[3][3])
                       * D_8031AB10[11];
      D_8031AB10[15] = (o->m[3][0] * D_8031AA50[3] + o->m[3][1] * D_8031AA50[7]
                        + o->m[3][2] * D_8031AA50[11] + D_8031AA50[15] * o->m[3][3])
                       * D_8031AB10[11];
      D_8031AB10[11] = D_8031AB10[11] * o->m[0][0];
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
                       obj, D_80025C5C == 0 ? "???" : D_80025C5C[obj], D_8035D51C);
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
      G(0x039E0010, (int)mtx);
      G(0x03980010, (int)mtx + 0x10);
      G(0x039A0010, (int)mtx + 0x20);
      G(0x039C0010, (int)mtx + 0x30);
    }
    /* the state the object's flags ask for */
    fl = o->flags;
    if (fl & 0x4A4) {
      if (fl & 0x400) {
        if (D_8028AA80 == 0 || (fl & 0x100) == 0) {
          G(0xBC00000A, 0);
          G(0xBC00040A, 0);
          G(0xBC00200A, D_8031B360[o->flags & 3]);
          G(0xBC00240A, D_8031B360[o->flags & 3]);
        } else {
          G(0xBC00000A, (D_8028AB58 >> 1) & 0x7F7F7F00);
          G(0xBC00040A, (D_8028AB58 >> 1) & 0x7F7F7F00);
          G(0xBC00200A, D_8031B360[o->flags & 3]);
          G(0xBC00240A, D_8031B360[o->flags & 3]);
        }
        fl = o->flags;
      }
      if (fl & 4) {
        G(0xB6000000, 0x3000);
        fl = o->flags;
      }
      if ((fl & 0x20) && D_8028AA78 != 0 && D_8028AA80 == 0 && D_8028AA84 == 0
          && D_8028AA8C == 0) {
        G(0xB6000000, 0x10000);
        fl = o->flags;
      }
      if ((fl & 0x80) && D_8028AA48 != 0) {
        G(0xB6000000, 0x200);
      }
    }
    if (D_8028C788 < i && i < D_8028C78C) {
      G(0xBB001001, 0xFFFFFFFF);
      G(0xE8000000, 0);
      G(0x06000000, o->dl);
      D_8028AA10 += o->vtxs;
      D_8028AA08 += o->tris;
      D_8028AA38 += o->x52;
    } else {
      func_80234050(obj, D_80351D80[obj], pass);
    }
    fl = o->flags;
    if (fl & 0x4A4) {
      if ((fl & 0x80) && D_8028AA48 != 0) {
        G(0xB7000000, 0x200);
        fl = o->flags;
      }
      if (fl & 0x400) {
        G(0xBC00000A, D_8028AB58);
        G(0xBC00040A, D_8028AB58);
        G(0xBC00200A, D_8028AB5C);
        G(0xBC00240A, D_8028AB5C);
        fl = o->flags;
      }
      if (fl & 4) {
        G(0xB7000000, D_8028A8AC == D_8028A8A8 ? 0x2000 : 0x1000);
      }
      if (D_8028AA78 != 0 && D_8028AA80 == 0 && D_8028AA84 == 0 && D_8028AA8C == 0
          && (o->flags & 0x20)) {
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
