/* romfile.c -- loading ROM files into memory someone else allocates; drawing image files
 */
#include "tgr/common.h"
#include "tgr/gbi.h"

/* -- declarations -- */
typedef struct BrRomFile {
  int start;                    /* 0x00  ROM range */
  int end;                      /* 0x04 */
  void *data;                   /* 0x08  where it was loaded */
} BrRomFile;
void BrRomRead(void *dst, int rom, int len);
unsigned int BrRomReadSize(int rom);
void BrRomUnpack(void *dst, int rom, void *stream);
typedef struct BrRomImage {     /* a ROM file holding an image (0x14 bytes) */
  int start;                    /* 0x00  ROM range */
  int end;                      /* 0x04 */
  unsigned char *data;          /* 0x08  where it was loaded */
  unsigned short w;             /* 0x0C */
  unsigned short h;             /* 0x0E */
  unsigned char fmt;            /* 0x10  G_IM_FMT_*; 2 = colour-indexed */
  unsigned char siz;            /* 0x11  0 4-bit, 1 8-bit, 2 16-bit */
} BrRomImage;
extern Gfx *D_8028A858;
extern int D_8028A850;                  /* hi-res screen */
extern int D_8028A898;                  /* the texture filter mode */
void BrTexSizeBits(unsigned int v, int *mask, int *bits);
#define TXL2WORDS(txls, b_txl)  MAX(1, ((txls) * (b_txl) / 8))
#define CALC_DXT(width, b_txl)  (((1 << 11) + TXL2WORDS(width, b_txl) - 1) / (unsigned)TXL2WORDS(width, b_txl))
#define TXL2WORDS_4b(txls)      MAX(1, ((txls) / 16))
#define CALC_DXT_4b(width)      (((1 << 11) + TXL2WORDS_4b(width) - 1) / (unsigned)TXL2WORDS_4b(width))
/* -- end declarations -- */

/* WHAT IT DOES: Load a ROM file as it is: allocate its length with the
 * given allocator and copy it in. */
/* @implements 0x8023DF00 tgr BrRomFileLoad */
void BrRomFileLoad(BrRomFile *f, void *(*alloc)(int size))
{
  f->data = alloc(f->end - f->start);
  BrRomRead(f->data, f->start, f->end - f->start);
}

/* WHAT IT DOES: Load a compressed ROM file: allocate its unpacked size with
 * the given allocator and unpack it there. */
/* @implements 0x8023DF4C tgr BrRomFileUnpack */
void BrRomFileUnpack(BrRomFile *f, void *(*alloc)(int size))
{
  f->data = alloc(BrRomReadSize(f->start));
  BrRomUnpack(f->data, f->start, 0);
}

/* WHAT IT DOES: Draw a loaded ROM image as a w by h rectangle at (x, y)
 * (w taken positive), in strips that fit the texture memory (half of it for
 * a colour-indexed image): each strip loads one row more than it advances,
 * as libultra's LoadTextureBlock sequences written out for the image's
 * 4, 8 or 16-bit texels.  The mode picks the combiner: 1 tinted by the
 * primitive colour, 3 tinted and blended with the environment colour, 4
 * the same over the other render mode, else the texture as it is.
 * Coordinates are doubled on a hi-res screen.  The DXT divisions are
 * unsigned here (the ROM uses divu), unlike image.c's.  The strip row is
 * zeroed just before the loop (its short live range ranks it above the
 * rectangle's bottom for a saved register), and the texture and rectangle
 * commands are written out as blocks, one word per line, so the stores
 * keep the ROM's order. */
/* @implements 0x8023DF9C tgr BrRomImageDraw */
void BrRomImageDraw(BrRomImage *img, int x, int y, int w, int h, int pr, int pg, int pb, int pa,
                    int er, int eg, int eb, int ea, int mode)
{
  int bitsS;
  int bitsT;
  int maskS;
  int maskT;
  int u0[1];                    /* unused: the frame has it */
  int tpw;
  int dsdx;
  int dtdy;
  int lines;
  int left;
  int line;
  int row;
  int step;
  int yb;

  if (w < 0) {
    w = -w;
  }
  BrTexSizeBits(img->w, &maskS, &bitsS);
  BrTexSizeBits(img->h, &maskT, &bitsT);
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
  {
    Gfx *_g = (Gfx *)(D_8028A858++);
    _g->words.w0 = 0xbb000001;
    _g->words.w1 = 0xffffffff;
  }
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 12, 2, D_8028A898);
  switch (mode) {
  case 1:
    gDPSetCombine(D_8028A858++, 0xffffff, 0xfffdf2f9);
    gDPSetPrimColor(D_8028A858++, 0xff, 0xff, pr, pg, pb, pa);
    gDPSetRenderMode(D_8028A858++, 0x00504240, 0);
    break;
  case 2:
    gDPSetCombine(D_8028A858++, 0xffffff, 0xfffcf279);
    gDPSetRenderMode(D_8028A858++, 0x00504240, 0);
    break;
  case 3:
    gDPSetCombine(D_8028A858++, 0x50fea1, 0x33fdf2f9);
    gDPSetPrimColor(D_8028A858++, 0xff, 0xff, pr, pg, pb, pa);
    gDPSetEnvColor(D_8028A858++, er, eg, eb, ea);
    gDPSetRenderMode(D_8028A858++, 0x00504240, 0);
    break;
  case 4:
    gDPSetCombine(D_8028A858++, 0x5098a1, 0x3331feff);
    gDPSetPrimColor(D_8028A858++, 0xff, 0xff, pr, pg, pb, pa);
    gDPSetEnvColor(D_8028A858++, er, eg, eb, ea);
    gDPSetRenderMode(D_8028A858++, 0x0f0a4000, 0);
    break;
  case 0:
  default:
    gDPSetCombine(D_8028A858++, 0xffffff, 0xfffcf279);
    gDPSetRenderMode(D_8028A858++, 0x00504240, 0);
    break;
  }
  if (img->fmt == 2) {
    gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 14, 2, 0x8000);
  } else {
    gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 14, 2, 0);
  }
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 19, 1, 0);
  if (D_8028A850 != 0) {
    x <<= 1;
    y <<= 1;
    w <<= 1;
    h <<= 1;
  }
  switch (img->siz) {
  case 0:
    tpw = 16;
    break;
  case 1:
    tpw = 8;
    break;
  case 2:
    tpw = 4;
    break;
  }
  line = (img->w + tpw - 1) / tpw * 8;
  if (img->fmt == 2) {
    lines = 0x800 / line;
  } else {
    lines = 0x1000 / line;
  }
  left = img->h;
  dsdx = (img->w << 10) / w;
  dtdy = (img->h << 10) / h;
  lines -= 1;
  yb = ((y + h) << 2) << 8;
  row = 0;
  while (left != 0) {
    if (left < lines) {
      lines = left;
    }
    left -= lines;
    step = ((lines << 10) / dtdy) << 10;
    yb -= step;
    switch (img->siz) {
    case 0:
      gDPSetTextureImage(D_8028A858++, img->fmt, G_IM_SIZ_16b, 1, img->data + line * row);
      gDPSetTile(D_8028A858++, img->fmt, G_IM_SIZ_16b, 0, 0, 7, 0, 2, bitsT, 0, 2, bitsS, 0);
      gDPLoadSync(D_8028A858++);
      gDPLoadBlock(D_8028A858++, 7, 0, 0, ((img->w * (lines + 1) + 3) >> 2) - 1, CALC_DXT_4b(img->w));
      gDPPipeSync(D_8028A858++);
      gDPSetTile(D_8028A858++, img->fmt, 0, ((img->w >> 1) + 7) >> 3, 0, 0, 0, 2, bitsT, 0, 2, bitsS, 0);
      gDPSetTileSize(D_8028A858++, 0, 0, 0, (img->w - 1) << 2, lines << 2);
      break;
    case 1:
      gDPSetTextureImage(D_8028A858++, img->fmt, G_IM_SIZ_16b, 1, img->data + line * row);
      gDPSetTile(D_8028A858++, img->fmt, G_IM_SIZ_16b, 0, 0, 7, 0, 2, bitsT, 0, 2, bitsS, 0);
      gDPLoadSync(D_8028A858++);
      gDPLoadBlock(D_8028A858++, 7, 0, 0, ((img->w * (lines + 1) + 1) >> 1) - 1, CALC_DXT(img->w, 1));
      gDPPipeSync(D_8028A858++);
      gDPSetTile(D_8028A858++, img->fmt, 1, (img->w + 7) >> 3, 0, 0, 0, 2, bitsT, 0, 2, bitsS, 0);
      gDPSetTileSize(D_8028A858++, 0, 0, 0, (img->w - 1) << 2, lines << 2);
      break;
    case 2:
      gDPSetTextureImage(D_8028A858++, img->fmt, G_IM_SIZ_16b, 1, img->data + line * row);
      gDPSetTile(D_8028A858++, img->fmt, G_IM_SIZ_16b, 0, 0, 7, 0, 2, bitsT, 0, 2, bitsS, 0);
      gDPLoadSync(D_8028A858++);
      gDPLoadBlock(D_8028A858++, 7, 0, 0, img->w * (lines + 1) - 1, CALC_DXT(img->w, 2));
      gDPPipeSync(D_8028A858++);
      gDPSetTile(D_8028A858++, img->fmt, 2, (img->w * 2 + 7) >> 3, 0, 0, 0, 2, bitsT, 0, 2, bitsS, 0);
      gDPSetTileSize(D_8028A858++, 0, 0, 0, (img->w - 1) << 2, lines << 2);
      break;
    }
    {
      Gfx *_g = (Gfx *)(D_8028A858++);

      _g->words.w0 = (_SHIFTL(G_TEXRECT, 24, 8) | _SHIFTL((x + w) << 2, 12, 12) | _SHIFTL((yb + step) >> 8, 0, 12));
      _g->words.w1 = (_SHIFTL(0, 24, 3) | _SHIFTL(x << 2, 12, 12) | _SHIFTL(yb >> 8, 0, 12));
      gImmp1(D_8028A858++, G_RDPHALF_1, (_SHIFTL(0, 16, 16) | _SHIFTL((lines - 1) << 5, 0, 16)));
      gImmp1(D_8028A858++, G_RDPHALF_2, (_SHIFTL(dsdx, 16, 16) | _SHIFTL(-dtdy, 0, 16)));
    }
    row += lines;
  }
  gDPPipeSync(D_8028A858++);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 19, 1, 0x80000);
}
