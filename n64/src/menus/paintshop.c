/* paintshop.c -- the paint shop
 */
#include "tgr/common.h"
#include "tgr/car.h"
#include "tgr/gbi.h"

/* -- declarations -- */
char * memcpy(char *param_1,char *param_2,int param_3);
extern unsigned char D_8028DB68;
extern unsigned char D_8028DB74;
extern unsigned char *D_8028DB78;          /* the decal texture being painted (CI4) */
extern unsigned char *D_8028DB7C;          /* its paint mask, 1 bit a texel, or 0 */
extern unsigned char D_8028DB88;           /* texture width */
extern unsigned char D_8028DB8C;           /* texture height */
extern unsigned char *D_8028DB80;
extern unsigned char D_8028DBB4;
extern unsigned char D_8028DBDC;
typedef struct BrPaintState {   /* 0x8028D110 */
  char pad00[0x1c];
  int x;                        /* 0x1C  cursor */
  int y;                        /* 0x20 */
} BrPaintState;
typedef struct BrPaintBrush {   /* 0x8028D290 */
  char pad00[0x24];
  int w;                        /* 0x24 */
  int h;                        /* 0x28 */
} BrPaintBrush;
typedef struct BrPaintBox {
  int pad[7];
  int x;                        /* 0x1C */
  int y;                        /* 0x20 */
  int w;                        /* 0x24  may be negative (mirrored) */
  int h;                        /* 0x28 */
} BrPaintBox;
extern BrPaintState D_8028D110;
extern BrPaintBrush D_8028D290;
int BrPaintCursorInRect(int *r);
extern int D_8028D12C;
extern int D_8028D130;
extern unsigned char D_8028DBBC;
extern unsigned char D_8028DBC4;
extern unsigned char D_8028DBE8;
extern float D_802AB20C;
extern float D_802AB210;
extern char D_8036A8E0;
extern char D_8036A8F8;
typedef struct BrPaintRect { int x, y, w, h; } BrPaintRect;
typedef struct BrPaintSwatch {  /* 0x14 bytes */
  int x, y, w, h;
  unsigned char r, g, b;
} BrPaintSwatch;
void BrFillRect(int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b);
void BrFillFrame(int x, int y, int w, int h, int t, unsigned char r, unsigned char g, unsigned char b);
extern BrPaintRect D_8028D480;
extern BrPaintRect D_8028D490;
extern BrPaintSwatch D_80369B98[16];
extern unsigned char D_8028DB58;       /* the chosen palette colour */
extern unsigned short *D_8028DB90;     /* the decal palette, RGBA5551 */
typedef struct BrPaintArea { int x, y, w, h; } BrPaintArea;
extern BrPaintArea D_8028DB94;         /* the paint area on screen */
void BrPaintPlot(int x, int y, unsigned char c);
extern Gfx *D_8028A858;
extern int D_8028A850;
extern int D_8028A898;                 /* the texture filter mode */
extern BrCar *D_8028AAF0;               /* the car shown */
extern BrCarCam *D_8028AAF4;            /* its camera */
extern unsigned char D_8028DBD0;        /* the view is turning */
extern unsigned char D_8028DB6C;        /* the view it turns from */
extern unsigned short D_8028DBB0;       /* frames into the turn (16 in all) */
extern BrVec3 D_8028DC08[];             /* the preset view directions */
extern float D_8028AAC0;
extern float D_8028AAC8;
extern int D_8028C334;
extern int D_8028AAB0;
extern int D_8028AAB4;
void BrVec3Normalise(BrVec3 *v);
float BrVec3Dot(BrVec3 *a, BrVec3 *b);
void BrVec3Cross(BrVec3 *out, BrVec3 *a, BrVec3 *b);
void BrVec3AddTo(BrVec3 *v, BrVec3 *a);
void BrVec3Scale(BrVec3 *out, BrVec3 *v, float s);
float BrAtan2(float x, float y);
void BrCameraSet(BrCarCam *cam, float a, float b, float c, float d);
void BrViewportSet(int x, int y, int w, int h, int scissor);
void BrGridSpanExtend(int a, int b);
void BrCarPlaceWheels(int n);
void BrCarVisibility(BrCar *car);
void func_80230554(BrCar *car, int);
/* -- end declarations -- */

/* WHAT IT DOES: Store the paint shop's working decal (2 KB) into the decal
 * buffer, move the edit point on, and mark the decal as changed so it is
 * redrawn. */
/* @implements 0x80244CA8 tgr BrPaintDecalCommit */
void BrPaintDecalCommit(void)
{
  memcpy((char *)D_8028DB80,(char *)D_8028DB78,0x800);
  D_8028DB74 = D_8028DB68;
  D_8028DBB4 = D_8028DBB4 + '\x01';
  D_8028DBDC = 1;
}

/* WHAT IT DOES: Swap two bytes in place. The paint shop uses it to reorder
 * pixel data. */
/* @implements 0x80252F50 tgr BrSwapBytes */
void BrSwapBytes(char *param_1,char *param_2)
{
  char uVar1;
  
  uVar1 = *param_1;
  *param_1 = *param_2;
  *param_2 = uVar1;
}

/* WHAT IT DOES: Draw the loaded tw by th texture as a w by h rectangle at
 * (x, y), upside down (t starts at the bottom row and steps back), in
 * 320-wide coordinates halved on a low-res screen; perspective correction
 * is off for the rectangle and back on after it.  The rectangle and its two
 * half-words are three statements on three lines: IDO's code for them
 * depends on the line breaks, and a one-line macro puts them out of order. */
/* @implements 0x80252F64 tgr BrTexRectFlipDraw */
void BrTexRectFlipDraw(int tw, int th, int x, int y, int w, int h)
{
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 12, 2, D_8028A898);
  gDPSetCombine(D_8028A858++, 0xffffff, 0xfffcf87c);
  gDPSetRenderMode(D_8028A858++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 19, 1, 0);
  if (D_8028A850 == 0) {
    x >>= 1;
    y >>= 1;
    w >>= 1;
    h >>= 1;
  }
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    _g->words.w0 = (_SHIFTL(G_TEXRECT, 24, 8) | _SHIFTL((x + w) << 2, 12, 12) | _SHIFTL((y + h) << 2, 0, 12));
    _g->words.w1 = (_SHIFTL(0, 24, 3) | _SHIFTL(x << 2, 12, 12) | _SHIFTL(y << 2, 0, 12));
  }
  gImmp1(D_8028A858++, G_RDPHALF_1, (_SHIFTL(0, 16, 16) | _SHIFTL(th << 5, 0, 16)));
  gImmp1(D_8028A858++, G_RDPHALF_2, (_SHIFTL((tw << 10) / w, 16, 16) | _SHIFTL(-(th << 10) / h, 0, 16)));
  gDPPipeSync(D_8028A858++);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 19, 1, 0x80000);
}

/* WHAT IT DOES: Move the paint shop cursor from the stick once it is pushed
 * past the dead zone, unless the cursor is locked. */
/* @t4-pass 0x8024BE78 1 2026-09-26 compiles 17 best 219 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8024BE78 2 2026-09-26 compiles 17 best 219 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8024BE78 3 2026-09-26 compiles 16 best 219 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8024BE78 tgr BrPaintStickMove */
void BrPaintStickMove(void)
{
  int iVar1;
  unsigned int *puVar2;
  float fVar3;
  
  if (D_8028DBE8 == '\0') {
    puVar2 = (unsigned int *)(&D_8036A8E0 + (unsigned int)D_8028DBBC * 0x15c);
    fVar3 = *(float *)(&D_8036A8F8 + (unsigned int)D_8028DBBC * 0x15c);
    if (fVar3 < 0.0f) {
      fVar3 = -fVar3;
    }
    if (D_802AB20C <= fVar3) {
      iVar1 = BrPaintCursorInRect((int *)&D_8028DB94);
      if ((iVar1 == 0) || (D_8028DBC4 != '\0')) {
        puVar2 = (unsigned int *)(&D_8036A8E0 + (unsigned int)D_8028DBBC * 0x15c);
        D_8028D12C = D_8028D12C +
                       (int)(*(float *)(&D_8036A8F8 + (unsigned int)D_8028DBBC * 0x15c) * 12.0f);
      }
      else {
        puVar2 = (unsigned int *)(&D_8036A8E0 + (unsigned int)D_8028DBBC * 0x15c);
        D_8028D12C = D_8028D12C +
                       (int)(*(float *)(&D_8036A8F8 + (unsigned int)D_8028DBBC * 0x15c) * 6.0f);
      }
    }
    else if ((*puVar2 & 0x201) == 0) {
      if ((*puVar2 & 0x804) != 0) {
        D_8028D12C = D_8028D12C + -1;
      }
    }
    else {
      D_8028D12C = D_8028D12C + 1;
    }
    fVar3 = (float)puVar2[7];
    if (fVar3 < 0.0f) {
      fVar3 = -fVar3;
    }
    if (D_802AB210 <= fVar3) {
      iVar1 = BrPaintCursorInRect((int *)&D_8028DB94);
      if ((iVar1 == 0) || (D_8028DBC4 != '\0')) {
        D_8028D130 = D_8028D130 -
                       (int)(*(float *)((unsigned int)D_8028DBBC * 0x15c + -0x7fc95704) * 12.0f);
      }
      else {
        D_8028D130 = D_8028D130 -
                       (int)(*(float *)((unsigned int)D_8028DBBC * 0x15c + -0x7fc95704) * 6.0f);
      }
    }
    else if ((*puVar2 & 0x402) == 0) {
      if ((*puVar2 & 0x108) != 0) {
        D_8028D130 = D_8028D130 + -1;
      }
    }
    else {
      D_8028D130 = D_8028D130 + 1;
    }
    if (D_8028D12C < 0x262) {
      if (D_8028D12C < 0x1f) {
        D_8028D12C = 0x1f;
      }
    }
    else {
      D_8028D12C = 0x261;
    }
    if (D_8028D130 < 0x16) {
      D_8028D130 = 0x16;
    }
    else if (0x1ca < D_8028D130) {
      D_8028D130 = 0x1ca;
    }
  }
}

/* WHAT IT DOES: Tell whether the paint-shop cursor is inside a box whose
 * width may be negative (a mirrored decal): x from its left edge to left +
 * |width|, y from its top to top + height. */
/* @implements 0x8024D374 tgr BrPaintCursorInBox */
int BrPaintCursorInBox(BrPaintBox *b)
{
  if (b->x <= D_8028D110.x && D_8028D110.x <= b->x + (b->w > 0 ? b->w : -b->w) && b->y <= D_8028D110.y && D_8028D110.y <= b->y + b->h) {
    return 1;
  }
  return 0;
}

/* WHAT IT DOES: Tell whether the paint-shop cursor is inside the rectangle
 * {x, y, w, h}, edges included. */
/* @implements 0x8024D3F0 tgr BrPaintCursorInRect */
int BrPaintCursorInRect(int *r)
{
  if (r[0] <= D_8028D110.x && D_8028D110.x <= r[2] + r[0] && r[1] <= D_8028D110.y && D_8028D110.y <= r[3] + r[1]) {
    return 1;
  }
  return 0;
}

/* WHAT IT DOES: The same rectangle test as BrPaintCursorInRect (a separate
 * copy in the ROM). */
/* @implements 0x8024D45C tgr BrPaintCursorInRect2 */
int BrPaintCursorInRect2(int *r)
{
  if (r[0] <= D_8028D110.x && D_8028D110.x <= r[2] + r[0] && r[1] <= D_8028D110.y && D_8028D110.y <= r[3] + r[1]) {
    return 1;
  }
  return 0;
}

/* WHAT IT DOES: Tell whether the paint-shop cursor is inside a rectangle at
 * {x, y} the size of the current brush. */
/* @implements 0x8024D4C8 tgr BrPaintCursorInBrush */
int BrPaintCursorInBrush(int *r)
{
  if (r[0] <= D_8028D110.x && D_8028D110.x <= r[0] + D_8028D290.w && r[1] <= D_8028D110.y && D_8028D110.y <= r[1] + D_8028D290.h) {
    return 1;
  }
  return 0;
}

/* WHAT IT DOES: Copy the 16 decal palette colours (RGBA5551) into the
 * palette swatches as 8-bit r, g, b, each 5-bit channel widened by
 * repeating its top bits. */
/* @implements 0x8024D53C tgr BrPaintPaletteLoad */
void BrPaintPaletteLoad(void)
{
  int i;

  for (i = 0; i < 16; i++) {
    D_80369B98[i].r = ((D_8028DB90[i] >> 8) & 0xf8) | ((D_8028DB90[i] >> 13) & 7);
    D_80369B98[i].g = ((D_8028DB90[i] >> 3) & 0xf8) | ((D_8028DB90[i] >> 8) & 7);
    D_80369B98[i].b = ((D_8028DB90[i] << 2) & 0xf8) | ((D_8028DB90[i] >> 3) & 7);
  }
}

/* WHAT IT DOES: Draw the paint shop's palette: its two black panels, the
 * sixteen colour swatches, and a light blue frame round the chosen one. */
/* @implements 0x8024D6B8 tgr BrPaintPaletteDraw */
void BrPaintPaletteDraw(void)
{
  int i;

  BrFillRect(D_8028D480.x, D_8028D480.y, D_8028D480.w, D_8028D480.h, 0, 0, 0);
  BrFillRect(D_8028D490.x, D_8028D490.y, D_8028D490.w, D_8028D490.h, 0, 0, 0);
  for (i = 0; i < 16; i++) {
    BrFillRect(D_80369B98[i].x, D_80369B98[i].y, D_80369B98[i].w, D_80369B98[i].h, D_80369B98[i].r,
               D_80369B98[i].g, D_80369B98[i].b);
  }
  BrFillFrame(D_80369B98[D_8028DB58].x - 2, D_80369B98[D_8028DB58].y - 2, D_80369B98[D_8028DB58].w + 4,
              D_80369B98[D_8028DB58].h + 4, 2, 0x20, 200, 0xff);
}

/* WHAT IT DOES: Read the colour index of texel (x, y) of a 4-bit texture
 * laid out as the decal is (odd rows' 8-texel words swapped). */
/* @implements 0x8024D844 tgr BrPaintPeek */
unsigned char BrPaintPeek(unsigned char *tex, int x, int y)
{
  int o;
  unsigned char c;

  o = (((y & 1) << 3 ^ x) >> 1) + y * (D_8028DB88 >> 1);
  if (x & 1) {
    c = tex[o] & 0xf;
  } else {
    c = tex[o] >> 4;
  }
  return c;
}

/* WHAT IT DOES: Plot one texel of colour c into the 4-bit decal texture at
 * (x, y) -- inside the texture and not masked off -- with the odd rows'
 * 8-texel words swapped as the RDP's TMEM layout wants them.
 * RESIDUE (51): temporaries are numbered one register later than the ROM's
 * from the row-width shift on; the instructions and their order match. */
/* @implements 0x8024F25C tgr BrPaintPlot */
void BrPaintPlot(int x, int y, unsigned char c)
{
  int o;

  if (x >= 0 && x < D_8028DB88 && y >= 0 && y < D_8028DB8C) {
    o = ((x ^ ((y & 1) << 3)) >> 1) + y * (D_8028DB88 >> 1);
    if (D_8028DB7C == 0) {
      if (x & 1) {
        D_8028DB78[o] = (D_8028DB78[o] & 0xf0) | c;
      } else {
        D_8028DB78[o] = (D_8028DB78[o] & 0xf) | (c << 4);
      }
    } else if (!(D_8028DB7C[(x >> 3) + y * ((D_8028DB88 + 7) >> 3)] & (1 << ((x ^ 7) & 7)))) {
      if (x & 1) {
        D_8028DB78[o] = (D_8028DB78[o] & 0xf0) | c;
      } else {
        D_8028DB78[o] = (D_8028DB78[o] & 0xf) | (c << 4);
      }
    }
  }
}

/* WHAT IT DOES: Paint a filled disc of radius r in the chosen colour
 * centred on (x, y) -- in texels, or in screen pixels over the paint area
 * when asked (a quarter scale, y flipped) -- as horizontal spans from a
 * midpoint circle walk.
 * RESIDUE (131): the ROM keeps x and the walk state in stack homes and
 * reuses the span bounds; ours holds x in a saved register. */
/* @implements 0x8025159C tgr BrPaintDisc */
void BrPaintDisc(int x, int y, int r, unsigned char screen)
{
  int i;
  int ha;
  int hb;
  int j;
  int a;
  int b;
  int err;

  if (screen) {
    x = (x - D_8028DB94.x) >> 2;
    y = (D_8028DB94.y + D_8028DB94.h - y) >> 2;
  }
  b = r * 2;
  err = -b;
  for (a = 0; a <= b; a++) {
    if (!(a & 1)) {
      ha = a >> 1;
      hb = b >> 1;
      for (i = x - ha; i <= x + ha; i++) {
        BrPaintPlot(i, y + hb, D_8028DB58);
      }
      for (i = x - hb; i <= x + hb; i++) {
        BrPaintPlot(i, y + ha, D_8028DB58);
      }
      for (i = x - ha; i <= x + ha; i++) {
        BrPaintPlot(i, y - hb, D_8028DB58);
      }
      for (i = x - hb; i <= x + hb; i++) {
        BrPaintPlot(i, y - ha, D_8028DB58);
      }
    }
    err += a;
    if (err >= 0) {
      err -= b;
      b--;
    }
  }
}

/* WHAT IT DOES: Place the paint-shop car and draw it: while a view change
 * is under way, ease the car's facing and up vectors over 16 frames from
 * the previous preset view to the new one (sidestepping through a
 * perpendicular when they are nearly opposite; view 5 tilts the car),
 * otherwise face the chosen preset; rebuild the body matrix, set the
 * camera and a viewport placed by car kind, and draw the car.  The x1/x2/x3
 * arrays are unused locals that reproduce the ROM's frame (0xD8); `/ 16` is
 * an integer so IDO keeps the divide.
 * RESIDUE (~400 raw, ~97 aligned ops): the vector copies and interpolation
 * are scheduled differently and the three pointer stores at the top come in
 * another order.  Not yet matched. */
/* @implements 0x80242BDC tgr BrPaintCarView */
void BrPaintCarView(void)
{
  BrVec3 from;
  BrVec3 to;
  float dx;
  float dy;
  float dz;
  float x1[4];
  BrVec3 side;
  BrVec3 zAxis;
  float x2[3];

  D_8028AAF0 = &D_8031B760[D_8028DBBC];
  D_8028AAF4 = &D_8031B760[D_8028DBBC].cams[3];
  D_8031B760[D_8028DBBC].cam = D_8028AAF4;
  if (D_8028DBD0 != 0) {
    from.x = D_8028DC08[D_8028DB6C].x;
    from.y = D_8028DC08[D_8028DB6C].y;
    from.z = D_8028DC08[D_8028DB6C].z;
    to.x = D_8028DC08[D_8028DB68].x;
    to.y = D_8028DC08[D_8028DB68].y;
    to.z = D_8028DC08[D_8028DB68].z;
    D_8028DBB0++;
    BrVec3Normalise(&from);
    BrVec3Normalise(&to);
    if (BrVec3Dot(&to, &from) < -0.9) {
      zAxis.x = 0.0f;
      zAxis.y = 0.0f;
      zAxis.z = 1.0f;
      BrVec3Cross(&side, &zAxis, &from);
      if (D_8028DBB0 < 8) {
        BrVec3AddTo(&to, &side);
      } else {
        BrVec3AddTo(&from, &side);
      }
    }
    dx = (to.x - from.x) / 16;
    dy = (to.y - from.y) / 16;
    dz = (to.z - from.z) / 16;
    ((BrVec3 *)D_8028AAF0->mtx0[0])->x = from.x + D_8028DBB0 * dx;
    ((BrVec3 *)D_8028AAF0->mtx0[0])->y = from.y + D_8028DBB0 * dy;
    ((BrVec3 *)D_8028AAF0->mtx0[0])->z = from.z + D_8028DBB0 * dz;
    BrVec3Normalise((BrVec3 *)D_8028AAF0->mtx0[0]);
    {
      BrVec3 upB = {0.0f, 0.0f, 0.0f};
      BrVec3 upA = {0.0f, 0.0f, 0.0f};

      if (D_8028DB68 == 5) {
        upA.x = 5.0f;
        upA.y = 5.0f;
        upA.z = 1.5f;
      } else {
        upA.z = 1.0f;
        upA.x = 0.0f;
        upA.y = 0.0f;
      }
      if (D_8028DB6C == 5) {
        upB.x = 5.0f;
        upB.y = 5.0f;
        upB.z = 1.5f;
      } else {
        upB.x = 0.0f;
        upB.y = 0.0f;
        upB.z = 1.0f;
      }
      BrVec3Normalise(&upB);
      BrVec3Normalise(&upA);
      dx = (upA.x - upB.x) / 16;
      dy = (upA.y - upB.y) / 16;
      dz = (upA.z - upB.z) / 16;
      ((BrVec3 *)D_8028AAF0->mtx0[2])->x = upB.x + D_8028DBB0 * dx;
      ((BrVec3 *)D_8028AAF0->mtx0[2])->y = upB.y + D_8028DBB0 * dy;
      ((BrVec3 *)D_8028AAF0->mtx0[2])->z = upB.z + D_8028DBB0 * dz;
    }
    BrVec3Normalise((BrVec3 *)D_8028AAF0->mtx0[2]);
    if (D_8028DBB0 == 16) {
      D_8028DBB0 = 0;
      D_8028DBD0 = 0;
    }
  } else {
    float x3[14];

    ((BrVec3 *)D_8028AAF0->mtx0[0])->x = D_8028DC08[D_8028DB68].x;
    ((BrVec3 *)D_8028AAF0->mtx0[0])->y = D_8028DC08[D_8028DB68].y;
    ((BrVec3 *)D_8028AAF0->mtx0[0])->z = D_8028DC08[D_8028DB68].z;
    BrVec3Normalise((BrVec3 *)D_8028AAF0->mtx0[0]);
  }
  BrVec3Cross((BrVec3 *)D_8028AAF0->mtx0[1], (BrVec3 *)D_8028AAF0->mtx0[2], (BrVec3 *)D_8028AAF0->mtx0[0]);
  BrVec3Normalise((BrVec3 *)D_8028AAF0->mtx0[1]);
  BrVec3Cross((BrVec3 *)D_8028AAF0->mtx0[2], (BrVec3 *)D_8028AAF0->mtx0[0], (BrVec3 *)D_8028AAF0->mtx0[1]);
  D_8028AAF0->heading = BrAtan2(D_8028AAF4->mtx[0][0], D_8028AAF4->mtx[0][1]);
  D_8028AAF0->heading = BrAtan2(D_8028AAF4->mtx[0][0], D_8028AAF4->mtx[0][1]);
  BrVec3Scale((BrVec3 *)D_8028AAF0->mtx0[3], (BrVec3 *)D_8028AAF0->mtx0[2], -0.65f);
  BrCameraSet(D_8028AAF4, D_8028AAC0, D_8028AAC8 * 0.2, 320.0f, 200.0f);
  if (D_8031B760[D_8028DBBC].kind == 1) {
    BrViewportSet(0, (D_8028AAB4 >> 1) - 3, (D_8028AAB0 >> 1) - 4, (D_8028AAB4 >> 1) - 3, 1);
  } else if (D_8031B760[D_8028DBBC].kind == 10) {
    BrViewportSet(0, (D_8028AAB4 >> 1) + 2, (D_8028AAB0 >> 1) - 4, (D_8028AAB4 >> 1) - 3, 1);
  } else {
    BrViewportSet(0, (D_8028AAB4 >> 1) - 6, (D_8028AAB0 >> 1) - 4, (D_8028AAB4 >> 1) - 3, 1);
  }
  BrGridSpanExtend(0, 0);
  BrCarPlaceWheels(D_8028DBBC);
  D_8028C334 = 1;
  BrCarVisibility(D_8028AAF0);
  func_80230554(D_8028AAF0, 0);
}
