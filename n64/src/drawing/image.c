/* image.c -- drawing 2D images (the front end's pictures) in horizontal strips
 */
#include "tgr/common.h"
#include "tgr/gbi.h"

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
 * sequences written out (the DXT and line arithmetic as in its macros).
 * RESIDUE (693): the ROM keeps no saved register (frame 0xF8) and reloads
 * w from its home at every use; ours holds w in s0. */
/* @t4-pass 0x80245B00 1 2026-10-03 compiles 26 best 693 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80245B00 2 2026-10-03 compiles 26 best 693 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x80245B00 */
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
    flip = 1;
    dw = -dw;
  }
  BrTexSizeBits(w, &maskS, &bitsS);
  BrTexSizeBits(h, &maskT, &bitsT);
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
  gSPTexture(D_8028A858++, maskS, maskT, 0, 0, 1);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 12, 2, D_8028A898);
  switch (img->siz) {
  case 4:
    if (img == &D_8028CB40) {
      gDPSetCombine(D_8028A858++, 0x309861, 0x5532ff7f);
      gDPSetRenderMode(D_8028A858++, 0x0f0a4000, 0);
    } else {
      gDPSetCombine(D_8028A858++, 0xffffff, 0xfffdf2f9);
      gDPSetRenderMode(D_8028A858++, 0x00504240, 0);
    }
    gDPSetTextureImage(D_8028A858++, 4, 2, 1, data);
    gDPSetTile(D_8028A858++, 4, 2, 0, 0, 7, 0, 0, bitsT, 0, 0, bitsS, 0);
    gDPLoadSync(D_8028A858++);
    gDPLoadBlock(D_8028A858++, 7, 0, 0, ((w * h + 3) >> 2) - 1, CALC_DXT_4b(w));
    gDPPipeSync(D_8028A858++);
    gDPSetTile(D_8028A858++, 4, 0, ((w >> 1) + 7) >> 3, 0, 0, 0, 0, bitsT, 0, 0, bitsS, 0);
    gDPSetTileSize(D_8028A858++, 0, 0, 0, (w - 1) << 2, (h - 1) << 2);
    break;
  case 8:
    if (img->kind == 'c') {
      gDPSetCombine(D_8028A858++, 0x11fe23, 0xfffff3f9);
    } else {
      gDPSetCombine(D_8028A858++, 0xffffff, 0xfffcf279);
    }
    gDPSetRenderMode(D_8028A858++, 0x00504240, 0);
    gDPSetTextureImage(D_8028A858++, 3, 2, 1, data);
    gDPSetTile(D_8028A858++, 3, 2, 0, 0, 7, 0, 0, bitsT, 0, 0, bitsS, 0);
    gDPLoadSync(D_8028A858++);
    gDPLoadBlock(D_8028A858++, 7, 0, 0, ((w * h + 1) >> 1) - 1, CALC_DXT(w, 1));
    gDPPipeSync(D_8028A858++);
    gDPSetTile(D_8028A858++, 3, 1, (w + 7) >> 3, 0, 0, 0, 0, bitsT, 0, 0, bitsS, 0);
    gDPSetTileSize(D_8028A858++, 0, 0, 0, (w - 1) << 2, (h - 1) << 2);
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
    gDPSetTileSize(D_8028A858++, 0, 0, 0, (w - 1) << 2, (h - 1) << 2);
    break;
  }
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 14, 2, 0);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 19, 1, 0);
  if (D_8028A850 == 0) {
    x >>= 1;
    y >>= 1;
    dh >>= 1;
    dw >>= 1;
  }
  gDPSetPrimColor(D_8028A858++, 0xff, 0xff, r, g, b, 0xff);
  gDPSetEnvColor(D_8028A858++, 0, 0, 0, 0xff);
  gSPTextureRectangle(D_8028A858++, x << 2, y << 2, (x + dw) << 2, (y + dh) << 2, 0,
                      flip ? (w - 1) << 5 : 0, (h - 1) << 5,
                      flip ? ((1 - w) << 10) / dw : ((w - 1) << 10) / dw, ((1 - h) << 10) / dh);
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
