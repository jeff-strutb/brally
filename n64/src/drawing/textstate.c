/* textstate.c -- state the text printer reads: alignment, font, highlight and custom colours
 */
#include "tgr/common.h"
#include "tgr/gbi.h"

/* -- declarations -- */
extern char D_8028BDC0;
extern char D_8028BDC8;
extern unsigned char D_8028BDC4;
extern int D_8028BDCC;
extern int D_8028BDD0;
extern int D_8028BDD4;
extern int D_8028BDD8;
extern int D_8028BDDC;
extern int D_8028BDE0;
extern int D_8028BDE4;
extern int D_803519D8;
void BrTextEmitString(unsigned char *s);
int BrTextWidth(unsigned char *s, int size);
extern int D_803519D0[2];              /* the text position (x, y) */
extern int D_8028A850;
extern int D_802A187C[];
extern int D_802A17A0[];
extern unsigned char D_802A1740[];
extern int D_8028AAB0;
extern int D_8028AAB4;
void BrTextPrint(char *s, int x, int y);
extern Gfx *D_8028A858;
extern unsigned int D_8028A898;         /* the texture filter */
extern unsigned char D_8028BDF0[];      /* the large font's shading ramps */
extern unsigned char D_8028BF30[];
extern unsigned char D_8028C070[];      /* the small font's */
extern unsigned char D_8028C1B0[];
extern unsigned char D_8028FE00[];      /* the large font's glyph page */
extern unsigned char D_8029DA00[];      /* the small font's */
/* -- end declarations -- */

/* WHAT IT DOES: Draw a string at the text position in the current font size
 * and colours: set up the two-cycle shaded-glyph combine and load the
 * shading ramp, then walk the characters. A space advances by 12/40 of the
 * size plus one; %% prints a percent sign; %i and %n are skipped; a percent
 * and two letters sets the gradient's primitive then environment colour;
 * every other printable loads its glyph's column of the font page and draws
 * it as a texture rectangle, clamped at the top-left screen edges when it
 * runs off them. The frame (0x1C0, the four homed locals at 0x184..0x190)
 * is set by the declaration order and the two unused ints; the loop walks a
 * copy of the argument, which leaves the argument itself homed.
 * RESIDUE (549): the display-list pointer's address lives in t1 where the
 * ROM has a3, which renames the temporaries of every command after it. */
/* @t3 0x8022E4E0 */
/* @t4-pass 0x8022E4E0 1 2026-09-29 compiles 150 best 549 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8022E4E0 2 2026-09-29 compiles 150 best 549 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8022E4E0 tgr BrTextEmitString */
void BrTextEmitString(unsigned char *s)
{
  int size;
  int x;
  int y;
  int cell;
  unsigned char *rampA;
  unsigned char *rampB;
  unsigned char *p;
  int c;
  int n;
  int u0;
  int u1;
  int pad;
  int *tbl;
  int texw;
  unsigned char *tex;
  unsigned char g;
  int w;
  int uls;
  int lrs;
  int row;

  size = D_803519D8;
  x = D_803519D0[0];
  y = D_803519D0[1];
  y -= size * 30 / 40;
  if (D_8028A850 != 0) {
    x <<= 1;
    y <<= 1;
    size <<= 1;
  }
  if (size < 25) {
    cell = 20;
    pad = 4;
    tbl = D_802A187C;
    texw = 392;
    tex = D_8029DA00;
    rampA = D_8028C070;
    rampB = D_8028C1B0;
  } else {
    cell = 40;
    pad = 7;
    tbl = D_802A17A0;
    texw = 704;
    tex = D_8028FE00;
    rampA = D_8028BDF0;
    rampB = D_8028BF30;
  }
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_2CYCLE);
  if (D_8028BDC8) {
    gDPSetCombine(D_8028A858++, 0x317E02, 0x51FEF3FA);
  } else {
    gDPSetCombine(D_8028A858++, 0x317E02, 0x5FFEF3FA);
  }
  gDPSetRenderMode(D_8028A858++, G_RM_PASS, G_RM_XLU_SURF2);
  gDPSetTextureFilter(D_8028A858++, D_8028A898);
  gDPSetTextureLUT(D_8028A858++, 0);
  gDPSetTexturePersp(D_8028A858++, G_TP_NONE);
  gDPSetTextureLOD(D_8028A858++, 0);
  gSPTexture(D_8028A858++, 0xffff, 0xffff, 0, 0, 1);
  gDPTileSync(D_8028A858++);
  gDPLoadSync(D_8028A858++);
  gDPPipeSync(D_8028A858++);
  gDPSetTile(D_8028A858++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0x1B0, 7, 0, 0, 0, 0, 0, 0, 0);
  gDPSetTextureImage(D_8028A858++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, D_8028BDC8 ? rampB : rampA);
  gDPLoadBlock(D_8028A858++, 7, 0, 0, 319, 0);
  gDPSetTile(D_8028A858++, G_IM_FMT_IA, G_IM_SIZ_8b, 1, 0x1B0, 1, 0, 2, 6, 0, 0, 3, 0);
  gDPSetTileSize(D_8028A858++, 1, 2, 2, 0x1E, 0x9E);
  if (D_8028BDCC != 0) {
    gDPSetEnvColor(D_8028A858++, D_8028BDD0, D_8028BDD4, D_8028BDD8, 0xff);
    gDPSetPrimColor(D_8028A858++, 0xff, 0xff, D_8028BDDC, D_8028BDE0, D_8028BDE4, 0xff);
  } else if (D_8028BDC0) {
    gDPSetEnvColor(D_8028A858++, 0xff, 0x7f, 0, 0xff);
    gDPSetPrimColor(D_8028A858++, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff);
  } else {
    gDPSetEnvColor(D_8028A858++, 0xc8, 0, 0, 0xff);
    gDPSetPrimColor(D_8028A858++, 0xff, 0xff, 0xe6, 0xe6, 0, 0xff);
  }
  p = s;
  while (*p != 0) {
    c = *p;
    if (c != ' ') {
      if (c == '%' && (n = p[1]) != 0) {
        if (n == '%') {
          p++;
          c = *p;
        } else if (n == 'i') {
          p++;
          goto next;
        } else if (n == 'n') {
          p++;
          goto next;
        } else if (p[2] != 0) {
          gDPPipeSync(D_8028A858++);
          switch (p[1]) {
          case 'r':
            gDPSetPrimColor(D_8028A858++, 0xff, 0xff, 0xbe, 0, 0, 0xff);
            break;
          case 'o':
            gDPSetPrimColor(D_8028A858++, 0xff, 0xff, 0xcd, 0x5f, 0, 0xff);
            break;
          case 'O':
            gDPSetPrimColor(D_8028A858++, 0xff, 0xff, 0xff, 0x78, 0, 0xff);
            break;
          case 'y':
            gDPSetPrimColor(D_8028A858++, 0xff, 0xff, 0xff, 0xf5, 0, 0xff);
            break;
          case 'Y':
            gDPSetPrimColor(D_8028A858++, 0xff, 0xff, 0xff, 0xfa, 0x80, 0xff);
            break;
          case 'g':
            gDPSetPrimColor(D_8028A858++, 0xff, 0xff, 0, 0x96, 0, 0xff);
            break;
          case 'b':
            gDPSetPrimColor(D_8028A858++, 0xff, 0xff, 0, 0, 0xc8, 0xff);
            break;
          case 'p':
            gDPSetPrimColor(D_8028A858++, 0xff, 0xff, 0xc8, 0, 0xc8, 0xff);
            break;
          case '1':
          case 'w':
            gDPSetPrimColor(D_8028A858++, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
            break;
          case '5':
            gDPSetPrimColor(D_8028A858++, 0xff, 0xff, 0x80, 0x80, 0x80, 0xff);
            break;
          case '0':
            gDPSetPrimColor(D_8028A858++, 0xff, 0xff, 0, 0, 0, 0xff);
            break;
          }
          switch (p[2]) {
          case 'r':
            gDPSetEnvColor(D_8028A858++, 0xc8, 0, 0, 0xff);
            break;
          case 'o':
            gDPSetEnvColor(D_8028A858++, 0xcd, 0x5f, 0, 0xff);
            break;
          case 'O':
            gDPSetEnvColor(D_8028A858++, 0xff, 0x78, 0, 0xff);
            break;
          case 'y':
            gDPSetEnvColor(D_8028A858++, 0xd2, 0xbe, 0, 0xff);
            break;
          case 'Y':
            gDPSetEnvColor(D_8028A858++, 0xd2, 0xc8, 0x69, 0xff);
            break;
          case 'g':
            gDPSetEnvColor(D_8028A858++, 0, 0x96, 0, 0xff);
            break;
          case 'b':
            gDPSetEnvColor(D_8028A858++, 0, 0, 0xc8, 0xff);
            break;
          case 'p':
            gDPSetEnvColor(D_8028A858++, 0xc8, 0, 0xc8, 0xff);
            break;
          case '1':
          case 'w':
            gDPSetEnvColor(D_8028A858++, 0xff, 0xff, 0xff, 0xff);
            break;
          case '5':
            gDPSetEnvColor(D_8028A858++, 0x80, 0x80, 0x80, 0xff);
            break;
          case '0':
            gDPSetEnvColor(D_8028A858++, 0, 0, 0, 0xff);
            break;
          }
          p += 2;
          goto next;
        }
      }
      if (c >= 0x21 && c < 0x80) {
        g = D_802A1740[c - 0x21];
        row = g < 27 ? cell : 0;
        uls = tbl[g];
        w = tbl[g + 1] - uls + 1;
        lrs = uls + w;
        gDPSetTextureImage(D_8028A858++, G_IM_FMT_IA, G_IM_SIZ_8b, texw, tex);
        gDPSetTile(D_8028A858++, G_IM_FMT_IA, G_IM_SIZ_8b, ((lrs - uls + 1) + 7) >> 3, 0, 7, 0, 2, 6, 0, 2, 6, 0);
        gDPLoadSync(D_8028A858++);
        gDPLoadTile(D_8028A858++, 7, uls << 2, row << 2, lrs << 2, (row + cell) << 2);
        gDPTileSync(D_8028A858++);
        gDPSetTile(D_8028A858++, G_IM_FMT_IA, G_IM_SIZ_8b, ((w + 1) + 7) >> 3, 0, 0, 0, 2, 6, 0, 2, 6, 0);
        gDPSetTileSize(D_8028A858++, 0, 2, 2, ((w - 1) << 2) + 2, ((cell - 1) << 2) + 2);
        if (x < 0 || x + w * size / cell > 320 || y < 0 || y + size > 240) {
          gSPScisTextureRectangle(D_8028A858++, x << 2, y << 2, (x + w * size / cell) << 2, (y + size) << 2, 0,
                                  16, ((cell - 1) << 5) + 16, (w << 10) / (w * size / cell),
                                  -(((cell << 10) - (1 << 10)) / size));
        } else {
          gSPTextureRectangle(D_8028A858++, x << 2, y << 2, (x + w * size / cell) << 2, (y + size) << 2, 0,
                              16, ((cell - 1) << 5) + 16, (w << 10) / (w * size / cell),
                              -(((cell << 10) - (1 << 10)) / size));
        }
        x += (w - pad) * size / cell;
      }
    } else {
      x += size * 12 / 40 + 1;
    }
  next:
    p++;
  }
  gDPPipeSync(D_8028A858++);
  gDPSetTexturePersp(D_8028A858++, G_TP_PERSP);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
}

/* WHAT IT DOES: Draw the following text in the highlight colours (orange
 * shading to pale yellow) instead of the normal red-to-yellow. */
/* @implements 0x8022F4CC tgr BrTextHighlightOn */
void BrTextHighlightOn(void)
{
  D_8028BDC0 = 1;
}

/* WHAT IT DOES: Switch the text printer to its alternative colour-combine
 * setting for the following text. */
/* @implements 0x8022F4DC tgr BrTextAltBlendOn */
void BrTextAltBlendOn(void)
{
  D_8028BDC8 = 1;
}

/* WHAT IT DOES: Switch the text printer back to its normal colour-combine
 * setting. */
/* @implements 0x8022F4EC tgr BrTextAltBlendOff */
void BrTextAltBlendOff(void)
{
  D_8028BDC8 = 0;
}

/* WHAT IT DOES: Draw the following text in the normal colours again (red
 * shading to yellow). */
/* @implements 0x8022F4F8 tgr BrTextHighlightOff */
void BrTextHighlightOff(void)
{
  D_8028BDC0 = 0;
}

/* WHAT IT DOES: Centre the following text on the x position it is printed
 * at. */
/* @implements 0x8022F504 tgr BrTextAlignCentre */
void BrTextAlignCentre(void)
{
  D_8028BDC4 = 2;
}

/* WHAT IT DOES: Start the following text at the x position it is printed at
 * (left aligned). */
/* @implements 0x8022F514 tgr BrTextAlignLeft */
void BrTextAlignLeft(void)
{
  D_8028BDC4 = 0;
}

/* WHAT IT DOES: End the following text at the x position it is printed at
 * (right aligned). */
/* @implements 0x8022F520 tgr BrTextAlignRight */
void BrTextAlignRight(void)
{
  D_8028BDC4 = 1;
}

/* WHAT IT DOES: Give the following text custom colours: the shading colour
 * (r1, g1, b1) and the main colour (r2, g2, b2), overriding the normal and
 * highlight palettes. */
/* @implements 0x8022F530 tgr BrTextSetColours */
void BrTextSetColours(int param_1,int param_2,int param_3,int param_4,
                 int param_5,int param_6)
{
  D_8028BDCC = 1;
  D_8028BDD0 = param_1;
  D_8028BDD4 = param_2;
  D_8028BDD8 = param_3;
  D_8028BDDC = param_4;
  D_8028BDE0 = param_5;
  D_8028BDE4 = param_6;
}

/* WHAT IT DOES: Give the following text custom colours taken from two
 * 3-byte RGB triples: the shading colour and the main colour. */
/* @implements 0x8022F578 tgr BrTextSetColoursRGB */
void BrTextSetColoursRGB(unsigned char *param_1,unsigned char *param_2)
{
  D_8028BDCC = 1;
  D_8028BDD0 = (unsigned int)*param_1;
  D_8028BDD4 = (unsigned int)param_1[1];
  D_8028BDD8 = (unsigned int)param_1[2];
  D_8028BDDC = (unsigned int)*param_2;
  D_8028BDE0 = (unsigned int)param_2[1];
  D_8028BDE4 = (unsigned int)param_2[2];
}

/* WHAT IT DOES: Choose the font the text printer uses for the following
 * text. */
/* @implements 0x8022F5D0 tgr BrTextSetFont */
void BrTextSetFont(int param_1)
{
  D_803519D8 = param_1;
}

/* WHAT IT DOES: Print a string at a position given in fractions of the
 * screen (y measured up from the bottom), nudged in from the edges.
 * RESIDUE (1): the ROM multiplies y * height with y as the first operand;
 * every spelling here (operand order, casts, locals, 80 permuter compiles)
 * puts the converted height first.
 *
 * NEVER RUN IN THE RETAIL GAME: nothing in the ROM refers to 0x8022F694 -- no
 * jal to it, no lui/addiu pair forming its address (n64rom xref: none), and
 * no data word holding it (the whole ROM searched for the value). */
/* @t4-pass 0x8022F694 1 2026-10-03 compiles 121 best 1 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8022F694 2 2026-10-03 compiles 121 best 1 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8022F694 tgr BrTextPrintAt */
void BrTextPrintAt(char *str, float x, float y, float unused)
{
  BrTextPrint(str, (int)(D_8028AAB0 * x * 0.0009267578134313226f) + 8,
              D_8028AAB4 - (int)(D_8028AAB4 * y * 0.0012148438254371285f) - 9);
}

/* WHAT IT DOES: Print a string at (x, y) in the current font, honouring the
 * alignment: left as given, right-aligned so it ends at x, or centred on x. */
/* @implements 0x8022F5DC tgr BrTextPrint */
void BrTextPrint(unsigned char *s, int x, int y)
{
  switch (D_8028BDC4) {
  case 0:
    D_803519D0[0] = x;
    break;
  case 2:
    D_803519D0[0] = x - (BrTextWidth(s, D_803519D8) >> 1);
    break;
  case 1:
    D_803519D0[0] = x - BrTextWidth(s, D_803519D8);
    break;
  }
  D_803519D0[1] = y;
  BrTextEmitString(s);
}

/* WHAT IT DOES: Measure how wide a string prints at the given size: each
 * glyph's width from the small or large font's table, spaces and other
 * unprintables as 12/40 of the size, %% as a percent sign, and the %i, %n and
 * two-letter colour codes as nothing. Halved back when the hi-res flag doubled
 * the size. */
/* @t4-pass 0x8022F720 1 2026-09-29 compiles 13 best 1 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8022F720 2 2026-09-29 compiles 13 best 1 moved 0  (n64/tools/n64permute.py) */
/* RESIDUE (1): the ROM compares the percent sign with the constant register
 * first (bnel t5, t1); IDO orders this compare itself, and every spelling of
 * it and the byte types tried gives c first. */
/* @t3 0x8022F720 */
/* @implements 0x8022F720 tgr BrTextWidth */
int BrTextWidth(unsigned char *s, int size)
{
  int w;
  int div;
  int pad;
  int *tbl;
  int c;
  unsigned char g;
  unsigned char n;

  w = 0;
  if (D_8028A850 != 0) {
    size <<= 1;
  }
  div = 40;
  if (size < 25) {
    div = 20;
    pad = 4;
    tbl = D_802A187C;
  } else {
    pad = 7;
    tbl = D_802A17A0;
  }
  while (*s != 0) {
    c = *s;
    if (c < 0x21 || c >= 0x80) {
      w += size * 12 / 40;
    } else {
      if (c == '%' && (n = s[1])) {
        if (n == '%') {
          s++;
        } else if (n == 'i' || n == 'n') {
          s++;
          goto next;
        } else if (s[2] != 0) {
          s += 2;
          goto next;
        }
      }
      g = D_802A1740[c - 0x21];
      w += (tbl[g + 1] - tbl[g] - pad + 1) * size / div;
    }
next:
    s++;
  }
  if (D_8028A850 != 0) {
    w >>= 1;
  }
  return w;
}

