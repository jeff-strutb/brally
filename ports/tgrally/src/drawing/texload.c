/* texload.c -- preparing RSP/RDP inputs: matrices scaled into the
 * fixed-point range, and texture loads
 */
#include "tgr/common.h"
#include "tgr/gbi.h"

/* -- declarations -- */
void BrFatal(char *msg);
typedef struct BrTex {          /* a texture's load record (0x24 bytes): cartridge data, big-endian */
  be32_t data;                  /* 0x00 */
  be32_t tlut;                  /* 0x04  palette of a 4-bit texture */
  char pad08[4];
  be16_t w;                     /* 0x0C */
  be16_t h;                     /* 0x0E */
  be16_t ult;                   /* 0x10 */
  be16_t uls;                   /* 0x12 */
  be16_t lrt;                   /* 0x14 */
  be16_t lrs;                   /* 0x16 */
  char pad18[8];
  be32_t bits;                  /* 0x20  mirrorS:1 mirrorT:1 clampS:1 clampT:1 type:4 rest:24, from the top */
} BrTex;
#define TEX_MIRRORS(t) ((BE32((t)->bits) >> 31) & 1)
#define TEX_MIRRORT(t) ((BE32((t)->bits) >> 30) & 1)
#define TEX_CLAMPS(t)  ((BE32((t)->bits) >> 29) & 1)
#define TEX_CLAMPT(t)  ((BE32((t)->bits) >> 28) & 1)
#define TEX_TYPE(t)    ((BE32((t)->bits) >> 24) & 0xF)   /* 1: 4-bit colour index, 4: 8-bit intensity, else 16-bit RGBA */
extern Gfx *D_8028A858;
extern int D_8028AB1C;                  /* the texture's TMEM address */
extern int D_8028AB18;                  /* the tile it is drawn with */
void BrTexSizeBits(unsigned int v, int *mask, int *bits);
/* -- end declarations -- */

/* WHAT IT DOES: The largest magnitude among a 4x4 matrix's first twelve
 * elements (its rotation rows). */
/* @implements 0x80217420 tgr BrMat4MaxAbs */
float BrMat4MaxAbs(float m[16])
{
  float *p;
  float max;
  float min;

  min = max = 0.0f;
  for (p = m; p < m + 12; p++) {
    if (*p < 0.0f) {
      if (*p < min) {
        min = *p;
      }
    } else if (max < *p) {
      max = *p;
    }
  }
  min = -min;
  if (max < min) {
    return min;
  }
  return max;
}

/* WHAT IT DOES: Scale a 4x4 matrix so the largest magnitude among its
 * rotation rows fits the RSP's fixed-point range (just under 2). */
/* @implements 0x802174B4 tgr BrMat4FitRange */
void BrMat4FitRange(float m[16])
{
  float *p;
  float max;
  float min;

  min = max = 0.0f;
  for (p = m; p < m + 12; p++) {
    if (*p < 0.0f) {
      if (*p < min) {
        min = *p;
      }
    } else if (max < *p) {
      max = *p;
    }
  }
  min = -min;
  if (max < min) {
    max = min;
  }
  max = 1.9997559f / (max + 0.375f);
  m[0] *= max;
  m[1] *= max;
  m[2] *= max;
  m[3] *= max;
  m[4] *= max;
  m[5] *= max;
  m[6] *= max;
  m[7] *= max;
  m[8] *= max;
  m[9] *= max;
  m[10] *= max;
  m[11] *= max;
  m[12] *= max;
  m[13] *= max;
  m[14] *= max;
  m[15] *= max;
}

/* WHAT IT DOES: For a texture v texels wide (1 to 1024), give the number
 * of address bits the RDP needs to wrap it (log2 rounded up) and a full
 * 16-bit mask; a larger size is a fatal error. */
/* @implements 0x80217614 tgr BrTexSizeBits */
void BrTexSizeBits(unsigned int v, int *mask, int *bits)
{
  char msg[40];

  v--;
  if (v >> 8) {
    if (v >> 10) {
      sprintf(msg, "ERROR: unhandled texture size: %d", v);
      BrFatal(msg);
    } else if (v >> 9) {
      *bits = 10;
    } else {
      *bits = 9;
    }
  } else if (v & 0xf0) {
    if (v & 0xc0) {
      if (v & 0x80) {
        *bits = 8;
      } else {
        *bits = 7;
      }
    } else if (v & 0xe0) {
      *bits = 6;
    } else {
      *bits = 5;
    }
  } else if (v & 0xfc) {
    if (v & 0xf8) {
      *bits = 4;
    } else {
      *bits = 3;
    }
  } else if (v & 0xfe) {
    *bits = 2;
  } else if (v) {
    *bits = 1;
  } else {
    *bits = 0;
  }
  *mask = 0xffff;
}

/* WHAT IT DOES: Load texture n of a table into TMEM and set its render tile:
 * wrap (clamp/mirror) from its flags, a 4-bit colour-index texture with its
 * 16-colour palette, an 8-bit intensity one (clamped), or a 16-bit RGBA
 * one; mask sizes from the texture's width and height, and texturing on
 * with the full scale.  Built from libultra's texture macros (their MIN and
 * block pointers are in the ROM); the wrap flags are bitfields.
 * Every declared local takes a frame slot in order; pal after tmem puts it at
 * the ROM's 0x84, which matters because only the palette texture sets it.
 * RESIDUE (~230 raw): a compiler temporary at 0x24 for the ROM's 0x28, and
 * the TMEM/tile globals load at other points. */
/* @t4-pass 0x80217734 1 2026-10-03 compiles 119 best 232 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80217734 2 2026-10-03 compiles 120 best 232 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x80217734 */
/* @implements 0x80217734 tgr BrTexLoad */
void BrTexLoad(int n, BrTex *tbl)
{
  BrTex *tx;
  int maskS;
  int maskT;
  int bitsS;
  int bitsT;
  int cms;
  int cmt;
  int w;
  int h;
  int fmt;
  int siz;
  int tmem;
  int pal = 0;                  /* set only for the palette texture: the ROM passes
                                   the others whatever its stack slot held, which
                                   the RDP ignores for a non-CI tile (the port
                                   passes 0; tools/streamcmp.py masks the field) */

  tx = &tbl[n];
  w = BE16(tx->w);
  BrTexSizeBits(w, &maskS, &bitsS);
  h = BE16(tx->h);
  BrTexSizeBits(h, &maskT, &bitsT);
  cms = TEX_CLAMPS(tx) ? 2 : 0;
  if (TEX_MIRRORS(tx)) {
    cms |= 1;
  }
  cmt = TEX_CLAMPT(tx) ? 2 : 0;
  if (TEX_MIRRORT(tx)) {
    cmt |= 1;
  }
  gDPTileSync(D_8028A858++);
  gDPSetTextureImage(D_8028A858++, 0, 2, 1, BE32(tx->data));
  tmem = D_8028AB1C & 0x1ff;
  gDPSetTile(D_8028A858++, 0, 2, 0, tmem, 7, 0, 0, bitsT, 0, 0, bitsS, 0);
  gDPLoadSync(D_8028A858++);
  if (TEX_TYPE(tx) == 1) {
    gDPLoadBlock(D_8028A858++, 7, 0, 0, ((w * h + 3) >> 2) - 1, 0);
    pal = 15;
    gDPLoadSync(D_8028A858++);
    gDPSetTextureImage(D_8028A858++, 0, 2, 1, BE32(tx->tlut));
    gDPTileSync(D_8028A858++);
    gDPSetTile(D_8028A858++, 0, 0, 0, 0x1f0, 7, 0, 0, 0, 0, 0, 0, 0);
    gDPLoadSync(D_8028A858++);
    gDPLoadTLUTCmd(D_8028A858++, 7, 15);
    gDPPipeSync(D_8028A858++);
    gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 14, 2, 0x8000);
    fmt = 2;
    siz = 0;
  } else if (TEX_TYPE(tx) == 4) {
    cmt = cms = 2;
    sins(0);
    gDPLoadBlock(D_8028A858++, 7, 0, 0, ((w * h + 1) >> 1) - 1, 0);
    gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 14, 2, 0);
    fmt = 4;
    siz = 1;
    tmem = D_8028AB1C & 0x1ff;
    } else {
    gDPLoadBlock(D_8028A858++, 7, 0, 0, w * h - 1, 0);
    gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 14, 2, 0);
    fmt = 0;
    siz = 2;
  }
  {
    int tile = D_8028AB18;

    gDPSetTile(D_8028A858++, fmt, siz, (w * (4 << siz) + 63)  >> 6, tmem, tile, pal, cmt, bitsT, 0, cms, bitsS, 0);
    gDPSetTileSize(D_8028A858++, tile, BES16(tx->uls) * 4 + 2, BES16(tx->ult) * 4 + 2, BES16(tx->lrs) * 4 + 2, BES16(tx->lrt) * 4 + 2);
    gSPTexture(D_8028A858++, maskS, maskT, 0, tile, 1);
  }
}
