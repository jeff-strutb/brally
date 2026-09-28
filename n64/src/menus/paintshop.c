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
extern BrCarModel *D_8028AB08;
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
extern unsigned char D_8028DB70;
extern unsigned char D_8028DAC0;        /* the chosen preset */
extern BrPaintRect D_8028D4A0[];
extern BrPaintRect D_8028DAC4;
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
extern int D_8028DB08;
extern unsigned char D_8028DB54;
extern unsigned char D_8028DB64;
extern unsigned char D_8028DB60;
extern unsigned char D_8028CF5C;
extern unsigned char D_8028CF8C;
extern unsigned char D_8028DBC0;
extern unsigned char D_8028DBC8;
extern unsigned char D_8028DBCC;
extern unsigned char D_8028DBD4;
extern unsigned char D_8028DBD8;
extern unsigned char D_8028DBE0;
extern unsigned char D_8028DBE4;
extern unsigned char D_8028DBEC;
extern int D_8028DB0C[];               /* per decal slot */
void BrPaintDecalCommit(void);
unsigned char BrPaintPeek(unsigned char *tex, int x, int y);
extern unsigned char D_8028CFBC;         /* pending flip: 1 left-right, 2 top-bottom */
extern unsigned char D_80369DAB;        /* the mirrored view */
void BrFillPoint(int x, int y, unsigned char c);
extern unsigned char D_8028DAB8;        /* the two dash colours */
extern unsigned char D_8028DABC;
typedef struct BrImage {
  unsigned char *data;
  int x4;
  int x8;
  unsigned char siz;
  char pad0d[3];
  int w;
  unsigned int h;
  unsigned int stripH;
  int x;
  int y;
  int drawW;
  int drawH;
} BrImage;
typedef struct BrGlyph {
  int x0;
  int x4;
  unsigned char c;
  char pad9[3];
  int off;
  int w;
  int kernL;
  int kernR;
} BrGlyph;
void BrImageDrawPart(BrImage *img, int s, int t, int sw, int th, int x, int y, int w, int h,
                     unsigned char r, unsigned char g, unsigned char b);
extern unsigned char D_8028CF2C;
extern BrImage D_8028D2C0;
extern BrGlyph D_8028D540[50];
extern unsigned char D_8028DB5C;
extern unsigned char D_8028DBA8;
extern unsigned char D_8028DBAC;
extern BrGlyph *D_80369E68[];
/* -- end declarations -- */

/* WHAT IT DOES: Take the chosen preset's rectangle, and make every view
 * slot (previous, current, saved) the current view. */
/* @implements 0x802533F4 tgr BrPaintPresetApply */
void BrPaintPresetApply(void)
{
  D_8028DAC4 = D_8028D4A0[D_8028DAC0];
  D_8028DB6C = D_8028DB68;
  D_8028DB70 = D_8028DB68;
  D_8028DB74 = D_8028DB68;
}

/* WHAT IT DOES: Read the colour index of texel (x, y) of the decal being
 * painted. */
/* @implements 0x8024D7D8 tgr BrPaintGet */
unsigned char BrPaintGet(int x, int y)
{
  int o;
  unsigned char c;

  o = (((y & 1) << 3 ^ x) >> 1) + y * (D_8028DB88 >> 1);
  if (x & 1) {
    c = D_8028DB78[o] & 0xf;
  } else {
    c = D_8028DB78[o] >> 4;
  }
  return c;
}

/* WHAT IT DOES: If the decal has changed, copy the working decal (2 KB)
 * into the car model's texture for the current decal slot, step the edit
 * point back and clear the changed mark. */
/* @implements 0x80244D04 tgr BrPaintDecalApply */
void BrPaintDecalApply(void)
{
  if (D_8028DBDC != 0) {
    memcpy(D_8028AB08->parts[D_8028AB08->decalPart[D_8028DB74]].a, (char *)D_8028DB80, 0x800);
    D_8028DBB4--;
    D_8028DBDC = 0;
  }
}

/* WHAT IT DOES: The character for index i of the name-entry character set
 * (a NUL, blanks, digits, capitals, punctuation; 66 entries, no
 * terminator); a space past its end. */
/* @implements 0x80253460 tgr BrPaintCharset */
char BrPaintCharset(unsigned char i)
{
  char set[0x42] = "\000               0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#'*+,-./:=?@";

  if (i < 0x42) {
    return set[i];
  }
  return ' ';
}

/* WHAT IT DOES: Reset the paint shop for a new session: cursor to the
 * centre, decal slot 2 selected everywhere, the default colour and brush,
 * and every mode and changed flag cleared. */
/* @implements 0x80244BA8 tgr BrPaintReset */
void BrPaintReset(void)
{
  D_8028D110.x = 0xf4;
  D_8028D110.y = 0x110;
  D_8028DB68 = 2;
  D_8028DB6C = D_8028DB68;
  D_8028DB74 = D_8028DB68;
  D_8028DB08 = D_8028DB0C[D_8028DB68];
  D_8028DAC0 = 2;
  D_8028DB54 = 0;
  D_8028DB58 = 3;
  D_8028DB64 = 1;
  D_8028DB60 = 1;
  D_8028DBB0 = 0;
  D_8028CF5C = 0;
  D_8028CF8C = 0;
  D_8028DBB4 = 0;
  D_8028DBC0 = 0;
  D_8028DBC4 = 0;
  D_8028DBC8 = 1;
  D_8028DBCC = 1;
  D_8028DBD0 = 0;
  D_8028DBD4 = 0;
  D_8028DBD8 = 1;
  D_8028DBDC = 0;
  D_8028DBE0 = 0;
  D_8028DBE4 = 0;
  D_8028DBEC = 0;
}

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

/* WHAT IT DOES: Fill the decal texels under a screen rectangle (either
 * corner order) with the chosen colour: screen to texel is a quarter, with
 * y turned upside down.  Each corner's two conversions share a source line
 * (IDO orders the copies into the corner registers by line). */
/* @implements 0x8024F7D4 tgr BrPaintFillRect */
void BrPaintFillRect(int sx0, int sy0, int sx1, int sy1)
{
  int x1;
  int x0;
  int y1;
  int x;
  int t;
  int y;

  x0 = (sx0 - D_8028DB94.x) >> 2; sy0 = (D_8028DB94.y + D_8028DB94.h - sy0) >> 2;
  x1 = (sx1 - D_8028DB94.x) >> 2; y1 = (D_8028DB94.y + D_8028DB94.h - sy1) >> 2;
  if (x1 < x0) {
    t = x0;
    x0 = x1;
    x1 = t;
  }
  if (y1 < sy0) {
    t = sy0;
    sy0 = y1;
    y1 = t;
  }
  for (y = sy0; y < y1; y++) {
    for (x = x0; x < x1; x++) {
      BrPaintPlot(x, y, D_8028DB58);
    }
  }
}

/* WHAT IT DOES: Draw a rectangle outline on the decal between two screen
 * corners (either order), in the chosen colour: one texel wide when the
 * brush is size 0 or 1, else a band as wide as the brush centred on each
 * edge. */
/* @implements 0x8024F8CC tgr BrPaintFrameRect */
void BrPaintFrameRect(int sx0, int sy0, int sx1, int sy1)
{
  int x1;
  int y1;
  int r;
  int x0;
  int t;

  x0 = (sx0 - D_8028DB94.x) >> 2; sy0 = (D_8028DB94.y + D_8028DB94.h - sy0) >> 2;
  x1 = (sx1 - D_8028DB94.x) >> 2; y1 = (D_8028DB94.y + D_8028DB94.h - sy1) >> 2;
  if (x1 < x0) {
    t = x0;
    x0 = x1;
    x1 = t;
  }
  if (y1 < sy0) {
    t = sy0;
    sy0 = y1;
    y1 = t;
  }
  r = D_8028D4A0[D_8028DAC0].w >> 1;
  if (r == 0) {
    for (sy1 = sy0; sy1 < y1; sy1++) {
      BrPaintPlot(x0, sy1, D_8028DB58);
    }
    for (sx1 = x0; sx1 <= x1; sx1++) {
      BrPaintPlot(sx1, y1, D_8028DB58);
    }
    for (sy1 = sy0; sy1 < y1; sy1++) {
      BrPaintPlot(x1, sy1, D_8028DB58);
    }
    for (sx1 = x0; sx1 < x1; sx1++) {
      BrPaintPlot(sx1, sy0, D_8028DB58);
    }
  } else {
    for (sy1 = sy0 - r; sy1 < y1 + r; sy1++) {
      for (sx1 = x0 - r; sx1 < x0 + r; sx1++) {
        BrPaintPlot(sx1, sy1, D_8028DB58);
      }
    }
    for (sy1 = y1 - r; sy1 < y1 + r; sy1++) {
      for (sx1 = x0; sx1 < x1; sx1++) {
        BrPaintPlot(sx1, sy1, D_8028DB58);
      }
    }
    for (sy1 = sy0 - r; sy1 < y1 + r; sy1++) {
      for (sx1 = x1 - r; sx1 < x1 + r; sx1++) {
        BrPaintPlot(sx1, sy1, D_8028DB58);
      }
    }
    for (sy1 = sy0 - r; sy1 < sy0 + r; sy1++) {
      for (sx1 = x0; sx1 < x1; sx1++) {
        BrPaintPlot(sx1, sy1, D_8028DB58);
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

/* WHAT IT DOES: Apply the pending decal flip: store the working decal into
 * the decal buffer, then redraw every texel of the working decal from that
 * stored copy mirrored left-right (flip 1) or top-bottom (flip 2); clear
 * the pending flip and copy the saved dirty flag back. */
/* @implements 0x80248DB4 tgr BrPaintFlipApply */
void BrPaintFlipApply(void)
{
  int x;
  int y;

  BrPaintDecalCommit();
  if (D_8028CFBC == 1) {
    for (y = 0; y < D_8028DB8C; y++) {
      for (x = 0; x < D_8028DB88; x++) {
        BrPaintPlot(D_8028DB88 - x - 1, y, BrPaintPeek(D_8028DB80, x, y));
      }
    }
  } else if (D_8028CFBC == 2) {
    for (y = 0; y < D_8028DB8C; y++) {
      for (x = 0; x < D_8028DB88; x++) {
        BrPaintPlot(x, D_8028DB8C - y - 1, BrPaintPeek(D_8028DB80, x, y));
      }
    }
  }
  D_8028CFBC = 0;
  D_8028DB60 = D_8028DB64;
}

/* WHAT IT DOES: Copy the decal to the car's other side: when the mirror
 * is asked for (0x8028DBCC), switch the paint view to its opposite (0<->1,
 * 2<->3, 6<->7), load that side's decal, texture and mask, store the working
 * decal, then redraw it from the old side's texture mirrored left-right and
 * start the view turning.  The saved dirty flag is copied back either way. */
/* @implements 0x80248B80 tgr BrPaintMirrorSide */
void BrPaintMirrorSide(void)
{
  static unsigned char *D_8028DCA4 = 0; /* the side being mirrored from */
  int x;
  int y;

  if (D_8028DBCC == 0) {
    D_8028DB60 = D_8028DB64;
    return;
  }
  {
    switch (D_8028DB68) {
    case 0:
      D_80369DAB = 1;
      break;
    case 1:
      D_80369DAB = 0;
      break;
    case 2:
      D_80369DAB = 3;
      break;
    case 3:
      D_80369DAB = 2;
      break;
    case 6:
      D_80369DAB = 7;
      break;
    case 7:
      D_80369DAB = 6;
      break;
    }
    D_8028DB6C = D_8028DB68;
    D_8028DCA4 = D_8028AB08->parts[D_8028AB08->decalPart[D_8028DB6C]].a;
    D_8028DB68 = D_80369DAB;
    D_8028DB78 = D_8028AB08->parts[D_8028AB08->decalPart[D_8028DB68]].a;
    D_8028DB7C = D_8028AB08->x11c[D_8028DB68];
    D_8028DB08 = D_8028DB0C[D_8028DB68];
    BrPaintDecalCommit();
    for (y = 0; y < D_8028DB8C; y++) {
      for (x = 0; x < D_8028DB88; x++) {
        BrPaintPlot(D_8028DB88 - x - 1, y, BrPaintPeek(D_8028DCA4, x, y));
      }
    }
    D_8028DBD0 = 1;
    D_8028DBB0 = 0;
    D_8028DCA4 = 0;
  }
  D_8028DB60 = D_8028DB64;
}

/* WHAT IT DOES: Draw a dashed line between two screen points for the paint
 * shop's selection (Bresenham along the longer axis, drawn from the lower
 * end): the dash runs 4 pixels in one colour and 4 in the other, and every
 * 8th call the two colours swap, so the dashes crawl; only every other pixel
 * along the main axis is plotted (BrFillPoint). */
/* @implements 0x80251A54 tgr BrPaintDashLine */
void BrPaintDashLine(int x0, int y0, int x1, int y1)
{
  int dx;
  int dy;
  int err;
  int n;
  int x;
  int y;
  int sx;
  int ex;
  int sy;
  int ey;
  int xstep;
  int ystep;
  int unused;                   /* holds its frame slot */
  unsigned char c;

  n = 0;
  sx = x0;
  sy = y0;
  ex = x1;
  ey = y1;
  dx = x1 - x0 > 0 ? x1 - x0 : -(x1 - x0);
  dy = y1 - y0 > 0 ? y1 - y0 : -(y1 - y0);
  if ((++D_8028DBB0 & 7) == 0) {
    BrSwapBytes((char *)&D_8028DAB8, (char *)&D_8028DABC);
  }
  if ((dx >= dy && x1 < x0) || (dx < dy && y1 < y0)) {
    sx = x1;
    ex = x0;
    sy = y1;
    ey = y0;
  }
  ystep = ey - sy < 0 ? -1 : 1;
  xstep = ex - sx < 0 ? -1 : 1;
  if (dx >= dy) {
    x = sx;
    y = sy;
    err = 0;
    for (; x <= ex; x++) {
      if (err >= dx) {
        err -= dx;
        y += ystep;
      }
      n = (n + 1) % 8;
      c = n < 4 ? D_8028DAB8 : D_8028DABC;
      if (x & 1) {
        BrFillPoint(x, y, c);
      }
      err += dy;
    }
  } else {
    x = sx;
    y = sy;
    err = 0;
    for (; y <= ey; y++) {
      if (err >= dy) {
        err -= dy;
        x += xstep;
      }
      n = (n + 1) % 8;
      c = n < 4 ? D_8028DAB8 : D_8028DABC;
      if (y & 1) {
        BrFillPoint(x, y, c);
      }
      err += dx;
    }
  }
}

/* WHAT IT DOES: Draw the paint shop's text line at (x, y), moved left by
 * half of 0x8028DBAC and up by the font's height (both in texels, 4 screen
 * pixels each): each character's glyph (found by code in the 50-glyph table) is drawn 16x16
 * from the font image scaled to 64x64 in the chosen swatch colour -- with a
 * drop shadow offset (4, 4) in the shadow colour when shadows are on --
 * when it overlaps the paint area; glyphs advance by width minus the
 * smaller of the two facing kerns, plus 2. */
/* @implements 0x80252C9C tgr BrPaintTextDraw */
void BrPaintTextDraw(int x, int y)
{
  int i;
  int k;
  int g;
  int kern;
  int adv;
  int left;

  x -= (D_8028DBAC >> 1) * 4;
  y -= D_8028D2C0.h * 4;
  for (i = 0; i < D_8028DBA8; i++) {
    for (k = 0; k < 50; k++) {
      if (D_80369E68[i]->c == D_8028D540[k].c) {
        g = k;
        break;
      }
    }
    left = D_80369E68[i]->off * 4 + x;
    if (D_8028DB94.x < D_80369E68[i]->w * 4 + left
        && left < D_8028DB94.x + D_8028DB94.w
        && D_8028DB94.y < y + D_8028D2C0.h * 4
        && y < D_8028DB94.y + D_8028DB94.h) {
      if (D_8028CF2C != 0) {
        BrImageDrawPart(&D_8028D2C0, g << 4, 0, 16, 16, x - D_80369E68[i]->off * 4 + 4, y + 4, 64, 64,
                        D_80369B98[D_8028DB5C].r, D_80369B98[D_8028DB5C].g, D_80369B98[D_8028DB5C].b);
      }
      BrImageDrawPart(&D_8028D2C0, g << 4, 0, 16, 16, x - D_80369E68[i]->off * 4, y, 64, 64,
                      D_80369B98[D_8028DB58].r, D_80369B98[D_8028DB58].g, D_80369B98[D_8028DB58].b);
    }
    if (i < D_8028DBA8 - 1) {
      kern = D_80369E68[i]->kernR < D_80369E68[i + 1]->kernL ? D_80369E68[i]->kernR : D_80369E68[i + 1]->kernL;
      adv = D_80369E68[i]->w - kern + 2;
      x += adv * 4;
    }
  }
}
