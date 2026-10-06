/* textstyle.c -- the paint shop's text-style popup
 *
 * Its own object: its strings and initialised name table sit apart from
 * the other popups'.
 */
#include "tgr/common.h"

/* -- declarations -- */
typedef struct BrImage {        /* as drawing/image.c */
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
  unsigned char x2c;            /* 0x2C */
} BrImage;
typedef struct BrPadRec {       /* as tgr/pad.h (0x15C bytes) */
  unsigned int pressed;
  unsigned int held;
  int repeat[4];
  float axis[2];
  char pad20[0x15c - 0x20];
} BrPadRec;
extern char D_8036A8E0;
#define PADS ((BrPadRec *)&D_8036A8E0)
typedef struct BrPaintSwatch {  /* 0x14 bytes */
  int x, y, w, h;
  unsigned char r, g, b;
} BrPaintSwatch;
extern BrPaintSwatch D_80369B98[16];    /* the palette */
extern unsigned char D_8028CF2C;        /* the text style: 0 normal, 1 drop shadow */
extern unsigned char D_8028DB54;        /* the shadow colour before the picker opened */
extern unsigned char D_8028DB58;        /* the chosen palette colour */
extern unsigned char D_8028DB5C;        /* the shadow colour */
extern unsigned char D_8028DBB8;        /* the chosen option */
extern unsigned char D_8028DBBC;        /* the controller */
extern unsigned char D_8028DBC0;        /* the popup has just opened */
extern unsigned char D_8028DBE0;        /* a popup is open */
extern unsigned char D_8028DBE4;        /* the shadow colour picker is open */
extern BrImage D_8028D0B0;              /* the A button */
extern BrImage D_8028D0E0;              /* the B button */
extern BrImage D_8028D2C0;              /* the font sheet */
void BrBevelPanel();
void BrFillFrame(int x, int y, int w, int h, int t, unsigned char r, unsigned char g, unsigned char b);
void BrTextSetFont(int size);
void BrTextAlignLeft(void);
void BrTextAlignCentre(void);
void BrTextHighlightOff(void);
void BrTextSetColours(int r, int g, int b, int a, int x, int y);
void BrTextPrint(char *s, int x, int y);
void BrImageDrawAt(BrImage *img, int x, int y);
void BrImageDrawPart(BrImage *img, int s, int t, int sw, int th, int x, int y, int w, int h,
                     unsigned char r, unsigned char g, unsigned char b);
void BrPadConsume(unsigned int *pad, unsigned int bits);
void BrPadStickToButtons(BrPadRec *pad);
/* -- end declarations -- */

/* WHAT IT DOES: The paint shop's text-style popup: a normal and a
 * drop-shadow sample letter, the chosen style framed in blue and named
 * below; left/right toggle it.  A on drop shadow opens the shadow colour
 * picker (sixteen palette swatches in two rows, the current one framed;
 * left/right walk a row, up/down switch rows, A keeps the colour, B
 * restores it); A on normal closes, B cancels.
 * The A button's x is one char variable: 0xd6 in the style view, stepped
 * by 0x10 for the picker, so the picker keeps it in a register past the
 * draws instead of folding it into the SELECT label's x. */
/* @implements 0x8024E128 tgr BrPaintTextStyleMenu */
void BrPaintTextStyleMenu(void)
{
  int u1[5];
  int bx;
  int u2[4];
  unsigned int i;
  int yb;
  int ty;
  char x;
  int r[2][4];
  int sw[16][5];
  char *names[2] = { "NORMAL TEXT", "DROP-SHADOW TEXT" };

  BrBevelPanel(0xb4, 0x82, 0x118, 0xd0, 3, 0, 0, 0x80, 0x80, 0x80);
  BrTextSetFont(16);
  BrTextAlignCentre();
  BrTextHighlightOff();
  BrTextPrint("%rySELECT OPTION", 0x9f, 0x53);
  if (D_8028DBC0 != 0) {
    D_8028DBB8 = D_8028CF2C;
    D_8028DB54 = D_8028DB5C;
    D_8028DBC0 = 0;
    D_8028DBE4 = 0;
  }
  r[0][0] = 0xf0;
  r[1][0] = 0x150;
  r[1][1] = 0xb8; r[0][1] = 0xb8;
  r[0][2] = r[1][2] = 0x40;
  r[0][3] = r[1][3] = 0x40;
  for (i = 0; (int)i < 2; i++) {
    if (i == D_8028DBB8) {
      BrFillFrame(r[i][0], r[i][1], r[i][2], r[i][2], 1, 0x20, 200, 0xff);
    } else {
      BrFillFrame(r[i][0], r[i][1], r[i][2], r[i][2], 1, 0x20, 0x20, 0x20);
    }
  }
  BrImageDrawPart(&D_8028D2C0, 0xa0, 0, 0x10, 0x10, r[0][0] + 7, r[0][1] + 10, 0x30, 0x30,
                  D_80369B98[D_8028DB58].r, D_80369B98[D_8028DB58].g, D_80369B98[D_8028DB58].b);
  BrImageDrawPart(&D_8028D2C0, 0xa0, 0, 0x10, 0x10, r[1][0] + 9, r[1][1] + 12, 0x30, 0x30,
                  D_80369B98[D_8028DB5C].r, D_80369B98[D_8028DB5C].g, D_80369B98[D_8028DB5C].b);
  BrImageDrawPart(&D_8028D2C0, 0xa0, 0, 0x10, 0x10, r[1][0] + 5, r[1][1] + 8, 0x30, 0x30,
                  D_80369B98[D_8028DB58].r, D_80369B98[D_8028DB58].g, D_80369B98[D_8028DB58].b);
  x = 0xd6;
  if (D_8028DBE4 == 0) {
    BrBevelPanel(0xc6, r[0][1] + r[0][3] + 0x11, 0xf4, 0x1e, 1, 1, 1, 0x80, 0x80, 0x80);
    BrTextSetFont(11);
    BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xf5, 0);
    BrTextPrint(names[D_8028DBB8], 0x9e, (r[0][1] + r[1][3] + 0x26) >> 1);
    BrTextSetFont(10);
    BrTextAlignLeft();
    yb = 0x14a - D_8028D0B0.w;
    ty = (yb + 0x12) >> 1;
    bx = 0x160 - D_8028D0E0.h;
    BrTextPrint("%wwSELECT", (x + (unsigned int)D_8028D0B0.w + 6) >> 1, ty);
    BrTextPrint("%wwCANCEL", (bx + (unsigned int)D_8028D0E0.w + 6) >> 1, ty);
    BrImageDrawAt(&D_8028D0B0, x, yb);
    BrImageDrawAt(&D_8028D0E0, bx, yb);
    BrPadStickToButtons(&PADS[D_8028DBBC]);
    if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 5) {
      BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 5);
      D_8028DBB8 ^= 1;
    }
  } else {
    BrBevelPanel(0xc6, 0x106, 0x116, 0x84, 2, 0, 0, 0x80, 0x80, 0x80);
    BrTextSetFont(12);
    BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xca, 0);
    BrTextAlignCentre();
    BrTextPrint("SELECT SHADOW COLOR", 0xa7, 0x92);
    for (i = 0; (int)i < 16; i++) {
      sw[i][2] = 0x14;
      sw[i][3] = 0x10;
      sw[i][0] = ((int)i % 8) * 0x1e + 0xde;
      sw[i][1] = ((int)i >> 3) * 0x1a + 0x134;
      BrBevelPanel(sw[i][0], sw[i][1], 0x14, 0x10, 1, 0, 1, D_80369B98[i].r, D_80369B98[i].g, D_80369B98[i].b);
    }
    BrFillFrame(sw[D_8028DB5C][0] - 3, sw[D_8028DB5C][1] - 3, sw[D_8028DB5C][2] + 6, sw[D_8028DB5C][3] + 6, 1,
                0x20, 200, 0xff);
    yb = 0x182 - D_8028D0B0.h;
    bx = 0x16e - D_8028D0E0.h;
    {
    int u3[1];
    x += 0x10;
    BrImageDrawAt(&D_8028D0B0, x, yb);
    BrImageDrawAt(&D_8028D0E0, bx, yb);
    BrTextSetFont(10);
    BrTextAlignLeft();
    ty = (yb + 0x12) >> 1;
    BrTextPrint("%wwSELECT", (x + (unsigned int)D_8028D0B0.w + 6) >> 1, ty);
    BrTextPrint("%wwCANCEL", (bx + (unsigned int)D_8028D0E0.w + 6) >> 1, ty);
    }
    BrPadStickToButtons(&PADS[D_8028DBBC]);
    if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 4) {
      BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 4);
      if (D_8028DB5C == 0) {
        D_8028DB5C = 7;
      } else if (D_8028DB5C == 8) {
        D_8028DB5C = 15;
      } else {
        D_8028DB5C--;
      }
    } else if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 1) {
      BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 1);
      if (D_8028DB5C == 7) {
        D_8028DB5C = 0;
      } else if (D_8028DB5C == 15) {
        D_8028DB5C = 8;
      } else {
        D_8028DB5C++;
      }
    } else if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 10) {
      BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 10);
      if (D_8028DB5C < 8) {
        D_8028DB5C += 8;
      } else {
        D_8028DB5C -= 8;
      }
    }
    if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 0x10) {
      BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 0x10);
      D_8028CF2C = D_8028DBB8;
      D_8028DBE4 = 0;
      D_8028DBE0 = 0;
    } else if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 0x20) {
      BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 0x20);
      D_8028DB5C = D_8028DB54;
      D_8028DBE4 = 0;
    }
  }
  if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 0x10) {
    BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 0x10);
    if (D_8028DBB8 != 0) {
      D_8028DBE4 = 1;
    } else {
      D_8028DBE0 = 0;
    }
  } else if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 0x20) {
    BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 0x20);
    D_8028DBE0 = 0;
    D_8028DBE4 = 0;
  }
}
