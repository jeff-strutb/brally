/* textstate.c -- state the text printer reads: alignment, font, highlight and custom colours
 */
#include "tgr/common.h"

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
void func_8022E4E0(unsigned char *param_1);
int BrTextWidth(unsigned char *s, int size);
extern int D_803519D0;
extern int D_803519D4;
extern int D_8028A850;
extern int D_802A187C[];
extern int D_802A17A0[];
extern unsigned char D_802A1740[];
extern int D_8028AAB0;
extern int D_8028AAB4;
void BrTextPrint(char *s, int x, int y);
/* -- end declarations -- */

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
 * puts the converted height first. */
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
    D_803519D0 = x;
    break;
  case 2:
    D_803519D0 = x - (BrTextWidth(s, D_803519D8) >> 1);
    break;
  case 1:
    D_803519D0 = x - BrTextWidth(s, D_803519D8);
    break;
  }
  D_803519D4 = y;
  func_8022E4E0(s);
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

