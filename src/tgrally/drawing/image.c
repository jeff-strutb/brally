/* image.c -- drawing 2D images (the front end's pictures) in horizontal strips
 */
#include "tgr/common.h"
#include "tgr/gbi.h"
#include "tgr/pad.h"

/* -- declarations -- */
typedef struct BrImage {
  unsigned char *data;          /* 0x00  pixels, top row first */
  int x4;
  int x8;
  unsigned char siz;            /* 0x0C  bits per pixel */
  char pad0d[3];
  int w;                        /* 0x10  pixels */
  unsigned int h;               /* 0x14  rows */
  unsigned int stripH;          /* 0x18  rows per strip (one texture load) */
  int x;                        /* 0x1C  where it is drawn */
  int y;                        /* 0x20 */
  int drawW;                    /* 0x24  drawn size */
  int drawH;                    /* 0x28 */
  char kind;                    /* 0x2C  'c': drawn with the car-select combiner */
} BrImage;
void BrImageStrip(BrImage *img, unsigned char *data, int w, int h, int x, int y, int dw, int dh,
                  unsigned char r, unsigned char g, unsigned char b);
extern Gfx *D_8028A858;
extern int D_8028A850;
extern int D_8028A898;                  /* the texture filter mode */
void BrSwapBytes(unsigned char *a, unsigned char *b);
extern BrImage D_8028CB40;               /* the 4-bit image drawn with its own combiner */
extern BrImage D_8028CB70;               /* the 16-bit image loaded as IA */
void BrTexSizeBits(unsigned int v, int *mask, int *bits);
#define TXL2WORDS(txls, b_txl)  MAX(1, ((txls) * (b_txl) / 8))
#define CALC_DXT(width, b_txl)  (((1 << 11) + TXL2WORDS(width, b_txl) - 1) / TXL2WORDS(width, b_txl))
#define TXL2WORDS_4b(txls)      MAX(1, ((txls) / 16))
#define CALC_DXT_4b(width)      (((1 << 11) + TXL2WORDS_4b(width) - 1) / TXL2WORDS_4b(width))
/* -- end declarations -- */

/* WHAT IT DOES: Draw the part (s, t, sw by th texels) of a 4-bit image as a
 * w by h rectangle at (x, y), upside down, tinted r, g, b: load the texels
 * as a 4-bit tile, then one textured rectangle stepping back up the rows.
 * Coordinates are halved on a low-res screen.  The two tile commands, the
 * rectangle and its first half-word are multi-line blocks: IDO schedules a
 * command's stores by its line layout. */
/* @implements 0x802465F0 tgr BrImageDrawPart */
void BrImageDrawPart(BrImage *img, int s, int t, int sw, int th, int x, int y, int w, int h,
                     unsigned char r, unsigned char g, unsigned char b)
{
  int lrs;
  int lrt;

  lrs = s + sw;
  lrt = t + th;
  gRaw(D_8028A858++, 0xe7000000, 0);
  gRaw(D_8028A858++, 0xba001402, 0);
  gRaw(D_8028A858++, 0xba001001, 0);
  gRaw(D_8028A858++, 0xba000c02, D_8028A898);
  gRaw(D_8028A858++, 0xfcffffff, 0xfffdf2f9);
  gRaw(D_8028A858++, 0xb900031d, 0x504240);
  gRaw(D_8028A858++, 0xfd880000 | _SHIFTL(((unsigned int)img->w >> 1) - 1, 0, 12), img->data);
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    _g->words.w0 = 0xf5880000 | _SHIFTL((((lrs - s + 1) >> 1) + 7) >> 3, 9, 9);
    _g->words.w1 = 0x7080200;
  }
  gRaw(D_8028A858++, 0xe6000000, 0);
  gRaw(D_8028A858++, 0xf4000000 | _SHIFTL(s << 1, 12, 12) | _SHIFTL(t << 2, 0, 12),
       0x7000000 | _SHIFTL(lrs << 1, 12, 12) | _SHIFTL(lrt << 2, 0, 12));
  gRaw(D_8028A858++, 0xe7000000, 0);
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    _g->words.w0 = 0xf5800000 | _SHIFTL((((lrs - s + 1) >> 1) + 7) >> 3, 9, 9);
    _g->words.w1 = 0x80200;
  }
  gRaw(D_8028A858++, 0xf2000000 | _SHIFTL(s << 2, 12, 12) | _SHIFTL(t << 2, 0, 12),
       _SHIFTL(lrs << 2, 12, 12) | _SHIFTL(lrt << 2, 0, 12));
  gRaw(D_8028A858++, 0xba000e02, 0);
  gRaw(D_8028A858++, 0xba001301, 0);
  if (D_8028A850 == 0) {
    x >>= 1;
    y >>= 1;
    w >>= 1;
    h >>= 1;
  }
  gDPSetPrimColor(D_8028A858++, 0xff, 0xff, r, g, b, 0xff);
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    _g->words.w0 = 0xe4000000 | _SHIFTL((x + w) << 2, 12, 12) | _SHIFTL((y + h) << 2, 0, 12);
    _g->words.w1 = _SHIFTL(x << 2, 12, 12) | _SHIFTL(y << 2, 0, 12);
  }
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    _g->words.w0 = _SHIFTL(G_RDPHALF_1, 24, 8);
    _g->words.w1 = _SHIFTL((s - 1) << 5, 16, 16) | _SHIFTL((lrt - 1) << 5, 0, 16);
  }
  gImmp1(D_8028A858++, G_RDPHALF_2, _SHIFTL(((sw << 10) - 0x400) / w, 16, 16) | _SHIFTL((1024 - th * 1024) / h, 0, 16));
  gRaw(D_8028A858++, 0xe7000000, 0);
  gRaw(D_8028A858++, 0xba001301, 0x80000);
}

/* WHAT IT DOES: Draw an image at its own position and size, strip by
 * strip from the bottom of its pixel data. */
/* @implements 0x80245470 tgr BrImageDraw */
void BrImageDraw(BrImage *img)
{
  unsigned int total;
  unsigned int strip;
  int n;
  int step;
  int i;

  if (img->siz == 4) {
    total = img->w * img->h >> 1;
    strip = img->w * img->stripH >> 1;
  } else {
    total = img->w * img->h * (img->siz >> 3);
    strip = img->w * img->stripH * (img->siz >> 3);
  }
  n = img->h / img->stripH;
  step = img->drawH / n;
  for (i = 1; i <= n; i++) {
    BrImageStrip(img, img->data + total - strip * i, img->w, img->stripH, img->x,
                 img->y + step * (i - 1), img->drawW, step, 0, 0, 0);
  }
}

/* WHAT IT DOES: Draw an image at (x, y) at its own size, strip by strip. */
/* @implements 0x80245604 tgr BrImageDrawAt */
void BrImageDrawAt(BrImage *img, int x, int y)
{
  unsigned int total;
  unsigned int strip;
  int n;
  int step;
  int i;

  if (img->siz == 4) {
    total = img->w * img->h >> 1;
    strip = img->w * img->stripH >> 1;
  } else {
    total = img->w * img->h * (img->siz >> 3);
    strip = img->w * img->stripH * (img->siz >> 3);
  }
  n = img->h / img->stripH;
  step = img->drawH / n;
  for (i = 1; i <= n; i++) {
    BrImageStrip(img, img->data + total - strip * i, img->w, img->stripH, x, y + step * (i - 1),
                 img->drawW, step, 0, 0, 0);
  }
}

/* WHAT IT DOES: Draw an image at its own position and size, tinted by the
 * given colour. */
/* @implements 0x80245798 tgr BrImageDrawTinted */
void BrImageDrawTinted(BrImage *img, unsigned char r, unsigned char g, unsigned char b)
{
  unsigned int total;
  unsigned int strip;
  int n;
  int step;
  int i;

  if (img->siz == 4) {
    total = img->w * img->h >> 1;
    strip = img->w * img->stripH >> 1;
  } else {
    total = img->w * img->h * (img->siz >> 3);
    strip = img->w * img->stripH * (img->siz >> 3);
  }
  n = img->h / img->stripH;
  step = img->drawH / n;
  for (i = 1; i <= n; i++) {
    BrImageStrip(img, img->data + total - strip * i, img->w, img->stripH, img->x,
                 img->y + step * (i - 1), img->drawW, step, r, g, b);
  }
}

/* WHAT IT DOES: Draw an image at (x, y), w by h on screen, tinted by the
 * given colour. */
/* @implements 0x8024594C tgr BrImageDrawRect */
void BrImageDrawRect(BrImage *img, int x, int y, int w, int h, unsigned char r, unsigned char g,
                     unsigned char b)
{
  int step;
  unsigned int total;
  unsigned int strip;
  int n;
  int i;

  if (img->siz == 4) {
    total = img->w * img->h >> 1;
    strip = img->w * img->stripH >> 1;
  } else {
    total = img->w * img->h * (img->siz >> 3);
    strip = img->w * img->stripH * (img->siz >> 3);
  }
  n = img->h / img->stripH;
  step = h / n;
  for (i = 1; i <= n; i++) {
    BrImageStrip(img, img->data + total - strip * i, img->w, img->stripH, x, y + step * (i - 1),
                 w, step, r, g, b);
  }
}

/* WHAT IT DOES: Draw one strip of an image: load its w by h texels (4-bit
 * intensity, 8-bit intensity-alpha or 16-bit RGBA, the last as IA for
 * D_8028CB70) as a texture block with the image's combiner, then draw it
 * as a dw by dh textured rectangle at (x, y) tinted r, g, b -- mirrored
 * left to right when dw is negative, upside down always.  Coordinates are
 * halved on a low-res screen.  The loads are libultra's LoadTextureBlock
 * sequences written out (the DXT and line arithmetic as in its macros,
 * the 8-bit line as width * 1), each through fmt; the tile-size command
 * is a block, one statement per line, as the other commands here. */
/* @implements 0x80245B00 tgr BrImageStrip */
void BrImageStrip(BrImage *img, unsigned char *data, int w, int h, int x, int y, int dw, int dh,
                  unsigned char r, unsigned char g, unsigned char b)
{
  int bitsS;
  int bitsT;
  int maskS;
  int maskT;
  int flip;
  int fmt;

  flip = 0;
  if (dw < 0) {
    dw = -dw;
    flip = 1;
  }
  BrTexSizeBits(w, &maskS, &bitsS);
  BrTexSizeBits(h, &maskT, &bitsT);
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
  gSPTexture(D_8028A858++, maskS, maskT, 0, 0, 1);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 12, 2, D_8028A898);
  switch (img->siz) {
  case 4:
    fmt = 4;
    if (img == &D_8028CB40) {
      gDPSetCombine(D_8028A858++, 0x309861, 0x5532ff7f);
      gDPSetRenderMode(D_8028A858++, 0x0f0a4000, 0);
    } else {
      gDPSetCombine(D_8028A858++, 0xffffff, 0xfffdf2f9);
      gDPSetRenderMode(D_8028A858++, 0x00504240, 0);
    }
    gDPSetTextureImage(D_8028A858++, fmt, 2, 1, data);
    gDPSetTile(D_8028A858++, fmt, 2, 0, 0, 7, 0, 0, bitsT, 0, 0, bitsS, 0);
    gDPLoadSync(D_8028A858++);
    gDPLoadBlock(D_8028A858++, 7, 0, 0, ((w * h + 3) >> 2) - 1, CALC_DXT_4b(w));
    gDPPipeSync(D_8028A858++);
    gDPSetTile(D_8028A858++, fmt, 0, ((w >> 1) + 7) >> 3, 0, 0, 0, 0, bitsT, 0, 0, bitsS, 0);
    {
      Gfx *_g = (Gfx *)(D_8028A858++);

      _g->words.w0 = _SHIFTL(G_SETTILESIZE, 24, 8);
      _g->words.w1 = _SHIFTL((w - 1) << 2, 12, 12) | _SHIFTL((h - 1) << 2, 0, 12);
    }
    break;
  case 8:
    fmt = 3;
    if (img->kind == 'c') {
      gDPSetCombine(D_8028A858++, 0x11fe23, 0xfffff3f9);
    } else {
      gDPSetCombine(D_8028A858++, 0xffffff, 0xfffcf279);
    }
    gDPSetRenderMode(D_8028A858++, 0x00504240, 0);
    gDPSetTextureImage(D_8028A858++, fmt, 2, 1, data);
    gDPSetTile(D_8028A858++, fmt, 2, 0, 0, 7, 0, 0, bitsT, 0, 0, bitsS, 0);
    gDPLoadSync(D_8028A858++);
    gDPLoadBlock(D_8028A858++, 7, 0, 0, ((w * h + 1) >> 1) - 1, CALC_DXT(w, 1));
    gDPPipeSync(D_8028A858++);
    gDPSetTile(D_8028A858++, fmt, 1, (w * 1 + 7) >> 3, 0, 0, 0, 0, bitsT, 0, 0, bitsS, 0);
    {
      Gfx *_g = (Gfx *)(D_8028A858++);

      _g->words.w0 = _SHIFTL(G_SETTILESIZE, 24, 8);
      _g->words.w1 = _SHIFTL((w - 1) << 2, 12, 12) | _SHIFTL((h - 1) << 2, 0, 12);
    }
    break;
  case 16:
    if (img == &D_8028CB70) {
      fmt = 3;
    } else {
      fmt = 0;
    }
    gDPSetCombine(D_8028A858++, 0xffffff, 0xfffcf279);
    gDPSetRenderMode(D_8028A858++, 0x00504240, 0);
    gDPSetTextureImage(D_8028A858++, fmt, 2, 1, data);
    gDPSetTile(D_8028A858++, fmt, 2, 0, 0, 7, 0, 0, bitsT, 0, 0, bitsS, 0);
    gDPLoadSync(D_8028A858++);
    gDPLoadBlock(D_8028A858++, 7, 0, 0, w * h - 1, CALC_DXT(w, 2));
    gDPPipeSync(D_8028A858++);
    gDPSetTile(D_8028A858++, fmt, 2, (w * 2 + 7) >> 3, 0, 0, 0, 0, bitsT, 0, 0, bitsS, 0);
    {
      Gfx *_g = (Gfx *)(D_8028A858++);

      _g->words.w0 = _SHIFTL(G_SETTILESIZE, 24, 8);
      _g->words.w1 = _SHIFTL((w - 1) << 2, 12, 12) | _SHIFTL((h - 1) << 2, 0, 12);
    }
    break;
  }
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 14, 2, 0);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 19, 1, 0);
  if (D_8028A850 == 0) {
    x >>= 1;
    y >>= 1;
    dw >>= 1;
    dh >>= 1;
  }
  gDPSetPrimColor(D_8028A858++, 0xff, 0xff, r, g, b, 0xff);
  gDPSetEnvColor(D_8028A858++, 0, 0, 0, 0xff);
  gSPTextureRectangle(D_8028A858++, x << 2, y << 2, (x + dw) << 2, (y + dh) << 2, 0,
                      flip ? (w - 1) << 5 : 0, (h - 1) << 5,
                      flip ? (0x400 - (w << 10)) / dw : ((w << 10) - 0x400) / dw, (0x400 - (h << 10)) / dh);
  gDPPipeSync(D_8028A858++);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 19, 1, 0x80000);
}

/* WHAT IT DOES: Plot one grey pixel at (x, y) (fill mode, RGBA5551), in
 * 640-wide coordinates halved on a low-res screen. */
/* @implements 0x80246A80 tgr BrFillPoint */
void BrFillPoint(int x, int y, unsigned char v)
{
  if (D_8028A850 == 0) {
    x >>= 1;
    y >>= 1;
  }
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_FILL);
  gDPSetRenderMode(D_8028A858++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  gDPSetFillColor(D_8028A858++, GPACK_RGBA5551(v, v, v, 1) << 16 | GPACK_RGBA5551(v, v, v, 1));
  gDPFillRectangle(D_8028A858++, x, y, x + 1, y + 1);
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
}

/* WHAT IT DOES: Draw the outline of a w by h box at (x, y), t pixels thick,
 * in one colour: top, bottom, left and right edges as fill rectangles. */
/* @implements 0x80246BCC tgr BrFillFrame */
void BrFillFrame(int x, int y, int w, int h, int t, unsigned char r, unsigned char g,
                 unsigned char b)
{
  if (D_8028A850 == 0) {
    x >>= 1;
    y >>= 1;
    w >>= 1;
    h >>= 1;
    t >>= 1;
  }
  if (t < 2) {
    t = 1;
  }
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_FILL);
  gDPSetRenderMode(D_8028A858++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  gDPSetFillColor(D_8028A858++, GPACK_RGBA5551(r, g, b, 1) << 16 | GPACK_RGBA5551(r, g, b, 1));
  gDPFillRectangle(D_8028A858++, x, y, x + w, y + t);
  gDPFillRectangle(D_8028A858++, x, y + h - t, x + w, y + h);
  gDPFillRectangle(D_8028A858++, x, y, x + t, y + h);
  gDPFillRectangle(D_8028A858++, x + w - t, y, x + w, y + h);
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
}

/* WHAT IT DOES: Fill a w by h rectangle at (x, y) in one colour (fill mode,
 * RGBA5551), in 640-wide coordinates halved on a low-res screen; at least
 * one pixel each way. */
/* @implements 0x80246E10 tgr BrFillRect */
void BrFillRect(int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b)
{
  if (D_8028A850 == 0) {
    x >>= 1;
    y >>= 1;
    w >>= 1;
    h >>= 1;
  }
  if (w < 2) {
    w = 1;
  }
  if (h < 2) {
    h = 1;
  }
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_FILL);
  gDPSetRenderMode(D_8028A858++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  gDPSetFillColor(D_8028A858++, GPACK_RGBA5551(r, g, b, 1) << 16 | GPACK_RGBA5551(r, g, b, 1));
  gDPFillRectangle(D_8028A858++, x, y, x + w, y + h);
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
}

/* WHAT IT DOES: Draw a bevelled panel: unless asked not to, a w by h fill
 * in r, g, b; then a bevel `bevel` pixels deep around it -- the left and
 * top edges light grey (0xE0), the right and bottom dark (0x30), the two
 * swapped for a sunken panel (sunken == 1).  In 640-wide coordinates,
 * halved on a low-res screen.  The loop index is declared first (its slot
 * sits above the two shades). */
/* @implements 0x80246F90 tgr BrBevelPanel */
void BrBevelPanel(int x, int y, int w, int h, int bevel, char noFill, char sunken, unsigned char r,
                  unsigned char g, unsigned char b)
{
  int i;
  unsigned char hi;
  unsigned char lo;

  hi = 0xe0;
  lo = 0x30;
  if (D_8028A850 == 0) {
    x >>= 1;
    y >>= 1;
    w >>= 1;
    h >>= 1;
    bevel >>= 1;
  }
  if (bevel < 2) {
    bevel = 1;
  }
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_FILL);
  gDPSetRenderMode(D_8028A858++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  if (noFill == 0) {
    gDPSetFillColor(D_8028A858++, GPACK_RGBA5551(r, g, b, 1) << 16 | GPACK_RGBA5551(r, g, b, 1));
    gDPFillRectangle(D_8028A858++, x, y, x + w, y + h);
    gDPPipeSync(D_8028A858++);
  }
  if (sunken == 1) {
    BrSwapBytes(&hi, &lo);
  }
  gDPSetFillColor(D_8028A858++, GPACK_RGBA5551(hi, hi, hi, 1) << 16 | GPACK_RGBA5551(hi, hi, hi, 1));
  for (i = 1; i <= bevel; i++) {
    gDPFillRectangle(D_8028A858++, x - i, y - i, x - i + 1, y + h + i);
  }
  for (i = 1; i <= bevel; i++) {
    gDPFillRectangle(D_8028A858++, x - i, y - i, x + w + i, y - i + 1);
  }
  gDPPipeSync(D_8028A858++);
  gDPSetFillColor(D_8028A858++, GPACK_RGBA5551(lo, lo, lo, 1) << 16 | GPACK_RGBA5551(lo, lo, lo, 1));
  for (i = 1; i <= bevel; i++) {
    gDPFillRectangle(D_8028A858++, x + w + i - 1, y - i, x + w + i, y + h + i);
  }
  for (i = 1; i <= bevel; i++) {
    gDPFillRectangle(D_8028A858++, x - i, y + h + i - 1, x + w + i, y + h + i);
  }
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
}

/* -- saving and loading the paint shop's decals and colour palette on the
 * Controller Pak (the same translation unit in the ROM) -- */

extern BrImage D_8028D0B0;      /* the A button */
extern BrImage D_8028D0E0;      /* the B button */

typedef struct BrPaintPart {    /* a model part the paint shop edits (0x24 bytes) */
  unsigned char *tex;           /* 0x00  its texture, 4 bits a pixel */
  unsigned short *pal;          /* 0x04  its palette */
  char pad08[4];
  unsigned short w;             /* 0x0C */
  unsigned short h;             /* 0x0E */
  char pad10[0x24 - 0x10];
} BrPaintPart;
typedef struct BrPaintModel {   /* the car model being painted */
  char pad00[0x14];
  BrPaintPart *parts;           /* 0x14 */
  char pad18[0x110 - 0x18];
  unsigned char decal[10];      /* 0x110  the parts holding the ten decals */
  unsigned char paint[2];       /* 0x11A  the parts that share the palette */
  unsigned char **mask;         /* 0x11C  per decal: the pixels it keeps, 0 for all */
} BrPaintModel;

typedef struct OSPfs {          /* libultra's Controller Pak handle (0x68 bytes) */
  int status;
  void *queue;
  int channel;
  unsigned char id[32];         /* 0x0C */
  char pad2c[0x68 - 0x2C];
} OSPfs;
extern OSPfs D_80369EC0[4];
extern OSPfs D_8031A3F8[4];
extern unsigned char D_803163E0[4][32];
extern unsigned char D_80316420[4];
extern unsigned char D_8031B1E8[4];
extern char D_80272D48[];
extern int D_80271FA8;

extern short D_802A4BE8;
extern unsigned char D_8028DB60;
extern unsigned char D_8028DB64;
extern unsigned char *D_8028DB80;  /* the transfer buffer */
extern unsigned char D_8028DBB4;

void osSyncPrintf(char *fmt, ...);
void *memcpy(void *dst, void *src, unsigned int n);
int bcmp(void *a, void *b, unsigned int n);
int osPfsIsPlug(void *mq, unsigned char *pattern);
int osPfsInitPak(void *mq, OSPfs *pfs, int channel);
int osPfsRepairId(OSPfs *pfs);
int osMotorInit(void *mq, OSPfs *pfs, int channel);
int osMotorStop(OSPfs *pfs);
int osPfsFindFile(OSPfs *pfs, unsigned short company, unsigned int game, unsigned char *name,
                  unsigned char *ext, int *file);
int osPfsChecker(OSPfs *pfs);
int osPfsReadWriteFile(OSPfs *pfs, int file, unsigned char flag, int offset, int size,
                       unsigned char *buf);
int osPfsFreeBlocks(OSPfs *pfs, int *bytes);
int osPfsAllocateFile(OSPfs *pfs, unsigned short company, unsigned int game, unsigned char *name,
                      unsigned char *ext, int size, int *file);
void BrSfxFadeTo(float level, float seconds);
void BrMusicFadeTo(float level, float seconds);
int BrSfxFadeDone(void);
void BrTextHighlightOff(void);
void BrTextAlignLeft(void);
void BrTextSetColours(int a, int b, int c, int d, int e, int f);
void BrTextSetFont(int font);
void BrTextPrint(char *s, int x, int y);
void BrImageDrawAt(BrImage *img, int x, int y);
void BrFillRect(int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b);
void BrPaintPaletteLoad(void);
void BrPaintDecalCommit(void);
void BrPadConsume(BrPadRec *pad, unsigned int bits);
/* -- end declarations -- */

#define COMPANY 0x3544          /* "5D" */
#define GAME 0x4E475245         /* "NGRE" */
#define OP_LOAD 9
#define PAD (&D_8036A8E0[port])
#define PFS (&D_80369EC0[port])

/* WHAT IT DOES: One frame of moving the paint shop's ten decals and its
 * colour palette to or from the Controller Pak in `port` (op 9 loads, any
 * other saves; fromMenu 1 when called from the car-select menus, which skip
 * the confirmation).  A state machine: confirm; fade the sound; probe the
 * pak (swap a Rumble Pak, remember a new pak's id); find the decal file (on a
 * save, confirm an overwrite or allocate it); move one decal a frame, each
 * masked by its keep-mask on a load, with a progress bar; move the palette
 * and share it with the other painted parts; offer the Rumble Pak back; then
 * report.  Sets *done when the caller may leave; returns the pak status on
 * an error.  (Run by the box with an empty pak: save, load, overwrite; the
 * pak errors, a new pak and the Rumble Pak swap are not reached.) */
/* @implements 0x80248F88 tgr BrDecalPakTransfer */
int BrDecalPakTransfer(BrPaintModel *m, unsigned char port, char op, char fromMenu,
                       unsigned char *done)
{
  static unsigned char D_8028DCBC = 0;  /* started */
  static unsigned char D_8028DCC0 = 0;  /* the file still has to be allocated */
  static int D_8028DCC4 = 0;            /* bytes transferred so far */
  static unsigned char D_80369E50;      /* the Rumble Pak was taken out for this */
  static unsigned char D_80369E51;      /* a new pak turned up mid-transfer */
  static int D_80369E54;                /* the last Controller Pak status */
  static int D_80369E58;                /* the file */
  static int D_80369E5C;                /* the size of the decal being moved */
  static unsigned char D_80369E60;      /* the state */
  static unsigned char D_80369E61;      /* the decal being moved */
  static unsigned char D_80369E62;      /* a frame counter */
  BrPaintPart *part;
  OSPfs *pfs;
  unsigned char *tex;
  int top;
  unsigned char *mask;
  int stride;
  int half;
  int ty;
  int row;
  int y;
  int col;
  int freeBytes;
  int w;
  int h;
  int k;
  int flip;
  unsigned short *pal;
  int n;
  int x2;
  int tx;
  unsigned char pakBits;
  unsigned char name[16] = {
    0x2D, 0x28, 0x29, 0x0F, 0x20, 0x1E, 0x1A, 0x2B, 0x0F, 0x1D, 0x1E, 0x1C, 0x1A, 0x25, 0x2C, 0x00
  };
  unsigned char ext[4] = { 0, 0, 0, 0 };

  D_802A4BE8 = 0;
  if (D_8028DCBC == 0) {
    D_8028DCBC = 1;
    D_80369E50 = 0;
    D_80369E51 = 0;
    D_80369E54 = 0;
    D_80369E61 = 0;
    D_8028DCC4 = 0;
    D_80369E5C = 0;
    D_80369E62 = 0;
    if (fromMenu == 1) {
      D_80369E60 = 0;
    } else {
      D_80369E60 = 2;
    }
  }
  *done = 0;
  switch (D_80369E60) {
  case 0:
    BrSfxFadeTo(0.0f, 0.2f);
    BrMusicFadeTo(0.0f, 0.2f);
    D_80369E60 = 1;
    break;
  case 1:
    if (BrSfxFadeDone() != 0 && D_80369E62++ == 3) {
      D_80369E60 = 3;
      D_80369E62 = 0;
    }
    break;
  case 2:                       /* load/save? */
    y = 0x110 - D_8028D0B0.w;
    BrBevelPanel(0xDC, 200, 200, 0x50, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextAlignLeft();
    BrTextHighlightOff();
    BrTextSetFont(14);
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
    if (op == OP_LOAD) {
      BrTextPrint("LOAD DECALS?", 119, 116);
    } else {
      BrTextPrint("SAVE DECALS?", 119, 116);
    }
    BrTextSetFont(10);
    BrTextPrint("%wwOK", (D_8028D0B0.w + 0xF2U) >> 1, (y + 18) >> 1);
    BrTextPrint("%wwCANCEL", (D_8028D0E0.w + 0x136U) >> 1, (y + 18) >> 1);
    BrImageDrawAt(&D_8028D0B0, 0xEC, y);
    BrImageDrawAt(&D_8028D0E0, 0x130, y);
    if (PAD->pressed & 0x10) {
      BrPadConsume(PAD, 0x10);
      D_80369E60 = 0;
    } else {
      if (PAD->pressed & 0x20) {
        BrPadConsume(PAD, 0x20);
        *done = 1;
        D_8028DB60 = D_8028DB64;
        return 0;
      }
    }
    break;
  case 3:                       /* probe the pak */
    osPfsIsPlug(D_80272D48, &pakBits);
    if ((pakBits & (1 << port)) == 0) {
      D_80369E54 = 1;
      D_80369E60 = 12;
    } else {
      osSyncPrintf("\nInitializing controller pak...\n");
      D_80369E54 = osPfsInitPak(D_80272D48, PFS, port);
      if (D_80369E54 == 0) {
        D_8031B1E8[D_80271FA8] = 0;
        if (D_80316420[port] == 0) {
          D_80316420[port] = 1;
          memcpy(D_803163E0[port], D_80369EC0[port].id, 32);
          D_80369E60 = 6;
        } else if (bcmp(D_803163E0[port], D_80369EC0[port].id, 32) != 0) {
          memcpy(D_803163E0[port], D_80369EC0[port].id, 32);
          D_80369E60 = 9;
        } else {
          D_80369E60 = 6;
        }
      } else if (D_80369E54 == 10) {
        if (osMotorInit(D_80272D48, &D_8031A3F8[port], port) == 0) {
          osMotorStop(&D_8031A3F8[port]);
          D_80369E54 = 9999;
          D_80369E50 = 1;
          D_80369E60 = 10;
        } else {
          D_8031B1E8[D_80271FA8] = 0;
          D_80369E54 = 0;
          D_80369E60 = 4;
        }
      } else {
        D_8031B1E8[D_80271FA8] = 0;
        D_80369E60 = 12;
      }
    }
    break;
  case 4:
    D_80369E54 = osPfsRepairId(PFS);
    if (D_80369E54 != 0) {
      D_80369E60 = 10;
    } else {
      D_80369E60 = 3;
    }
    break;
  case 6:                       /* find the file */
    D_8028DCC4 = 0;
    D_80369E5C = 0;
    D_80369E61 = 0;
    D_8028DCC0 = 0;
    pfs = PFS;
    D_80369E62 = 0;
    D_80369E54 = osPfsFindFile(pfs, COMPANY, GAME, name, ext, &D_80369E58);
    if (D_80369E54 == 0) {
      if (op == OP_LOAD) {
        top = 0xC5;
        if (fromMenu == 1) {
          top = 0xDB;
        }
        BrBevelPanel(0xD4, top, 0xD8, 0x56, 3, 0, 0, 0x80, 0x80, 0x80);
        BrTextAlignLeft();
        BrTextHighlightOff();
        BrTextSetFont(13);
        BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
        BrTextPrint("LOADING DECALS...", 114, (top + 30) >> 1);
        BrBevelPanel(0xE6, top + 0x34, 0xB4, 0x14, 1, 1, 1, 0x80, 0x80, 0x80);
        D_80369E60 = 7;
      } else {
        D_80369E60 = 5;
      }
    } else if (D_80369E54 == 5) {
      if (op == OP_LOAD) {
        D_80369E60 = 12;
      } else if (osPfsFreeBlocks(pfs, &freeBytes) == 0) {
        if (freeBytes < 0x3E00) {
          D_80369E54 = 7;
          D_80369E60 = 12;
        } else {
          BrBevelPanel(0xD4, 0xC5, 0xD8, 0x56, 3, 0, 0, 0x80, 0x80, 0x80);
          BrTextAlignLeft();
          BrTextHighlightOff();
          BrTextSetFont(13);
          BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
          BrTextPrint("SAVING DECALS...", 115, 113);
          BrBevelPanel(0xE6, 0xF9, 0xB4, 0x14, 1, 1, 1, 0x80, 0x80, 0x80);
          D_8028DCC0 = 1;
          D_80369E60 = 7;
          D_80369E62 = 0;
        }
      } else {
        D_80369E54 = 4;
        D_80369E60 = 12;
      }
    } else if (D_80369E54 == 3) {
      D_80369E54 = osPfsChecker(pfs);
      if (D_80369E54 == 0) {
        D_80369E60 = 6;
      } else {
        D_80369E60 = 12;
      }
    } else if (D_80369E54 == 2) {
      D_80369E60 = 3;
      D_80369E54 = 0;
    } else {
      D_80369E60 = 12;
    }
    break;
  case 5:                       /* overwrite? */
    y = 0x11D - D_8028D0B0.w;
    BrBevelPanel(0xD6, 0xBB, 0xD4, 0x6A, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextAlignLeft();
    BrTextHighlightOff();
    BrTextSetFont(13);
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
    BrTextPrint("OK TO OVERWRITE", 114, 108);
    BrTextPrint("SAVED DECALS?", 114, 122);
    BrTextSetFont(10);
    BrTextPrint("%wwOK", (D_8028D0B0.w + 0xEEU) >> 1, (y + 18) >> 1);
    BrTextPrint("%wwCANCEL", (D_8028D0E0.h + 0x13AU) >> 1, (y + 18) >> 1);
    BrImageDrawAt(&D_8028D0B0, 0xE8, y);
    BrImageDrawAt(&D_8028D0E0, 0x134, y);
    if (PAD->pressed & 0x10) {
      BrPadConsume(PAD, 0x10);
      D_80369E60 = 7;
    } else if (PAD->pressed & 0x20) {
      BrPadConsume(PAD, 0x20);
      D_80369E60 = 12;
    }
    break;
  case 7:                       /* one decal a frame */
    top = 0xC5;
    ty = 0x71;
    if (fromMenu == 1) {
      ty = 0x7C; if (ty); top = 0xDB;
    }
    BrBevelPanel(0xD4, top, 0xD8, 0x56, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextAlignLeft();
    BrTextHighlightOff();
    BrTextSetFont(13);
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
    tx = 114;
    if (op == OP_LOAD) {
      BrTextPrint("LOADING DECALS...", tx, ty);
    } else {
      tx++;
      BrTextPrint("SAVING DECALS...", tx, ty);
    }
    BrBevelPanel(0xE6, top + 0x34, 0xB4, 0x14, 1, 1, 1, 0x80, 0x80, 0x80);
    if (D_80369E62++ > 1) {
      if (D_8028DCC0 != 0) {
        pfs = PFS;
        D_8028DCC0 = 0;
        D_80369E54 = osPfsAllocateFile(pfs, COMPANY, GAME, name, ext, 0x3E00, &D_80369E58);
        switch (D_80369E54) {
        case 3:
          D_80369E54 = osPfsChecker(pfs);
          if (D_80369E54 == 0) {
            D_80369E60 = 3;
            return 0;
          }
          D_80369E60 = 2;
          return D_80369E54;
        case 5:
        case 9:
          D_80369E54 = 3;
          break;
        case 2:
          D_80369E60 = 3;
          break;
        case 0:
          break;
        }
        if (D_80369E54 != 0) {
          if (D_80369E54 == 2) {
            D_80369E54 = 0;
            D_80369E60 = 3;
            *done = 0;
            return 0;
          }
          D_80369E60 = 12;
          break;
        }
      }
      if (D_80369E61 < 10) {
        n = D_80369E61;
        w = m->parts[m->decal[D_80369E61]].w;
        h = m->parts[m->decal[D_80369E61]].h;
        pfs = PFS;
        if (n == 0 || n == 1) {
          h++;
        }
        D_80369E5C = (int)(w * h) >> 1;
        if (op == OP_LOAD) {
          D_80369E54 = osPfsReadWriteFile(pfs, D_80369E58, 0, D_8028DCC4, D_80369E5C, D_8028DB80);
        } else {
          D_80369E54 = osPfsReadWriteFile(pfs, D_80369E58, 1, D_8028DCC4, D_80369E5C, m->parts[m->decal[D_80369E61]].tex);
        }
        switch (D_80369E54) {
        case 0:
          if (op == OP_LOAD) {
            osSyncPrintf("Loading decal %d...\n", D_80369E61);
            tex = m->parts[m->decal[D_80369E61]].tex;
            mask = m->mask[D_80369E61];
            if (mask == 0) {
              memcpy(tex, D_8028DB80, D_80369E5C);
            } else {
              /* keep the pixels the mask marks, take the rest from the pak */
              for (row = 0; row < h; row++) {
                stride = (int)(w + 7) >> 3;
                half = (int)w >> 1;
                flip = (row & 1) << 3;
                for (col = 0; col < w; col++) {
                  x2 = ((int)col >> 3) + row * stride;
                  if ((mask[x2] & (1 << ((col ^ 7) & 7))) == 0) {
                    k = ((int)(flip ^ col) >> 1) + row * half;
                    tex[k] = D_8028DB80[k];
                  }
                }
              }
            }
          } else {
            osSyncPrintf("Saving decal %d...\n", D_80369E61);
          }
          D_8028DCC4 += D_80369E5C;
          break;
        case 5:
          D_80369E54 = 99999;
          break;
        case 3:
          D_80369E54 = osPfsChecker(pfs);
          if (D_80369E54 == 1 || D_80369E54 == 4 || D_80369E54 == 3) {
            D_80369E61 = 0;
            D_8028DCC4 = 0;
            D_80369E5C = 0;
            D_80369E60 = 12;
          } else {
            D_80369E61--;
            D_8028DCC4 -= D_80369E5C;
            D_80369E54 = 0;
          }
          break;
        }
        if (D_80369E54 != 0) {
          if (D_80369E54 == 2) {
            D_80369E54 = 0;
            D_80369E60 = 9;
            D_80369E51 = 1;
            *done = 0;
            return 0;
          }
          D_80369E60 = 12;
        } else {
          BrFillRect(0xE7, top + 0x35, D_8028DCC4 * 0xB2 / 0x3D00, 0x12, 0, 0, 0xC0);
          D_80369E61++;
        }
      } else {
        D_80369E61 = 0;
        BrFillRect(0xE7, top + 0x35, 0xB2, 0x12, 0, 0, 0xC0);
        D_80369E60 = 8;
      }
    }
    break;
  case 8:                       /* the palette */
    if (op == OP_LOAD) {
      osSyncPrintf("Loading color palette...\n");
      pfs = PFS;
      D_80369E54 = osPfsReadWriteFile(pfs, D_80369E58, 0, D_8028DCC4, 0x20,
                                      (unsigned char *)m->parts[m->decal[2]].pal);
      if (D_80369E54 == 0) {
        pal = m->parts[m->decal[2]].pal;
        m->parts[m->paint[0]].pal[0] = pal[0];
        m->parts[m->paint[0]].pal[1] = pal[1];
        m->parts[m->paint[1]].pal[0] = pal[0];
        m->parts[m->paint[1]].pal[1] = pal[1];
        if (fromMenu == 0) {
          BrPaintPaletteLoad();
          BrPaintDecalCommit();
        }
      }
    } else {
      osSyncPrintf("Saving color palette...\n");
      pfs = PFS;
      D_80369E54 = osPfsReadWriteFile(pfs, D_80369E58, 1, D_8028DCC4, 0x20,
                                      (unsigned char *)m->parts[m->decal[2]].pal);
    }
    switch (D_80369E54) {
    case 0:
      if (op == OP_LOAD) {
        osSyncPrintf("Color palette loaded successfully!\n\n");
      } else {
        osSyncPrintf("Color palette saved successfully!\n\n");
      }
      D_80369E60 = 11;
      if (fromMenu == 0) {
        D_8028DBB4 = 0;
      }
      break;
    case 3:
      D_80369E54 = osPfsChecker(pfs);
      if (D_80369E54 == 1 || D_80369E54 == 4 || D_80369E54 == 3) {
        D_80369E60 = 12;
      } else {
        D_8028DCC4 -= D_80369E5C;
        D_80369E54 = 0;
        D_80369E60 = 8;
      }
      break;
    }
    if (D_80369E54 != 0) {
      osSyncPrintf("err_code = %d\n\n", D_80369E54);
      if (D_80369E54 == 2) {
        D_80369E60 = 9;
        D_80369E51 = 1;
        *done = 0;
        D_80369E54 = 0;
        return 0;
      }
      D_80369E60 = 12;
      *done = 0;
    }
    break;
  case 9:                       /* a different pak */
    top = 0xAB;
    if (fromMenu == 1) {
      top = 0xD3;
    }
    BrTextHighlightOff();
    BrTextAlignLeft();
    BrTextSetFont(12);
    BrBevelPanel(0xAF, top, 0x122, 0x89, 3, 0, 0, 0x80, 0x80, 0x80);
    if (fromMenu == 1) {
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
    } else {
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
    }
    BrTextPrint("A NEW CONTROLLER PAK", 95, (top + 32) >> 1);
    BrTextPrint("WAS INSERTED.", 95, ((top + 32) >> 1) + 13);
    BrTextPrint("THIS CONTROLLER PAK", 95, ((top + 32) >> 1) + 32);
    BrTextPrint("WILL BE USED.", 95, ((top + 32) >> 1) + 45);
    if (PAD->pressed & 0x8030) {
      BrPadConsume(PAD, 0x8030);
      if (D_80369E51 != 0) {
        D_80369E51 = 0;
        D_80369E60 = 3;
      } else {
        D_80369E60 = 6;
      }
    }
    break;
  case 10:                      /* a Rumble Pak is in the way, or an error */
    BrTextHighlightOff();
    BrTextAlignLeft();
    BrTextSetFont(12);
    if (fromMenu == 1) {
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
    } else {
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
    }
    if (D_80369E54 == 9999) {
      top = 0xA0;
      if (fromMenu == 1) {
        top = 200;
      }
      y = top - D_8028D0B0.w + 0x98;
      BrBevelPanel(0x7A, top, 0x18C, 0xA0, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("PLEASE REMOVE THE RUMBLE PAK", 69, (top + 32) >> 1);
      BrTextPrint("AND INSERT THE CONTROLLER PAK", 69, ((top + 32) >> 1) + 13);
      BrTextPrint("INTO CONTROLLER.  PRESS THE A", 69, ((top + 32) >> 1) + 26);
      BrTextPrint("BUTTON WHEN READY.", 69, ((top + 32) >> 1) + 39);
      BrTextSetFont(10);
      BrTextPrint("%wwOK", (D_8028D0B0.w + 0xDEU) >> 1, (y + 18) >> 1);
      BrTextPrint("%wwCANCEL", (D_8028D0E0.w + 0x14AU) >> 1, (y + 18) >> 1);
      BrImageDrawAt(&D_8028D0B0, 0xD8, y);
      BrImageDrawAt(&D_8028D0E0, 0x144, y);
    }
    if (D_80369E54 != 9999) {
      if (PAD->pressed & 0x8030) {
        BrPadConsume(PAD, 0x8030);
        D_80369E60 = 12;
      }
    } else if (PAD->pressed & 0x10) {
      BrPadConsume(PAD, 0x10);
      D_80369E54 = 0;
      D_80369E60 = 3;
    } else if (PAD->pressed & 0x20) {
      BrPadConsume(PAD, 0x20);
      D_80369E54 = 0;
      D_80369E60 = 12;
      if (fromMenu == 1) {
        D_80369E54 = 999;
      }
    }
    break;
  case 11:                      /* put the Rumble Pak back */
    D_80369E61 = 0;
    D_8028DCC4 = 0;
    D_80369E5C = 0;
    D_80369E62 = 0;
    if (fromMenu == 0) {
      D_80369E60 = 12;
    } else if (D_80369E50 != 0) {
      BrBevelPanel(0x73, 0xCC, 0x19A, 0x98, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextHighlightOff();
      BrTextAlignLeft();
      BrTextSetFont(12);
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
      BrTextPrint("IF THE RUMBLE PAK IS TO BE USED,", 65, 118);
      BrTextPrint("REMOVE THE CONTROLLER PAK AND", 65, 131);
      BrTextPrint("INSERT THE RUMBLE PAK INTO THE", 65, 144);
      BrTextPrint("CONTROLLER.  PRESS THE A BUTTON", 65, 157);
      BrTextPrint("TO CONTINUE.", 65, 170);
      if (PAD->pressed & 0x8010) {
        BrPadConsume(PAD, 0x8010);
        D_80369E50 = 0;
        D_80369E54 = 0;
        if (osMotorInit(D_80272D48, &D_8031A3F8[port], port) == 0) {
          D_8031B1E8[port] = 1;
        }
        D_80369E60 = 12;
      }
    } else {
      D_80369E60 = 12;
    }
    break;
  case 12:                      /* done */
    D_8028DCBC = 0;
    BrSfxFadeTo(1.0f, 0.2f);
    BrMusicFadeTo(1.0f, 0.2f);
    D_802A4BE8 = 1;
    if (D_80369E54 == 0) {
      *done = 1;
      if (fromMenu == 0) {
        D_8028DB60 = D_8028DB64;
      }
      return 0;
    }
    if (fromMenu == 0 && (PAD->pressed & 0x8030) != 0) {
      BrPadConsume(PAD, 0x8030);
      D_8028DB60 = D_8028DB64;
    }
    *done = 0;
    return D_80369E54;
  }
  return 0;
}
