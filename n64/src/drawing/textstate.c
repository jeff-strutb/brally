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
int func_8022F720(unsigned char *param_1,int param_2);
extern int D_803519D0;
extern int D_803519D4;
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
    D_803519D0 = x - (func_8022F720(s, D_803519D8) >> 1);
    break;
  case 1:
    D_803519D0 = x - func_8022F720(s, D_803519D8);
    break;
  }
  D_803519D4 = y;
  func_8022E4E0(s);
}

