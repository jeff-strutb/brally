/* colourmix.c -- the paint shop's colour mixer popup
 *
 * Its own object here: its strings and initialised tables sit apart from
 * the other paint-shop functions'.
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
typedef struct BrMixSwatch {    /* a palette entry, as the mixer reads it (0x14) */
  int x, y, w, h;
  unsigned char c[3];           /* 0x10  red, green, blue */
} BrMixSwatch;
extern BrMixSwatch D_80369B98[16];      /* the palette: 0 the body colour, 1 the shadow (half scale) */
extern unsigned char D_80369DA8[3];     /* the colour before the mixer opened */
typedef struct BrMixPart {      /* a model part (0x24) */
  void *x0;
  unsigned short *tlut;         /* 0x04 */
  char pad08[0x24 - 8];
} BrMixPart;
typedef struct BrMixModel {
  char pad00[0x14];
  BrMixPart *parts;             /* 0x14 */
  char pad18[0x11a - 0x18];
  unsigned char bodyPart;       /* 0x11A */
  unsigned char shadePart;      /* 0x11B */
} BrMixModel;
extern BrMixModel *D_8028AB08;          /* the current car's model */
extern unsigned short *D_8028DB90;      /* the decal palette, RGBA5551 */
extern unsigned char D_8028DB58;        /* the chosen palette colour */
extern unsigned char D_8028DBB4;        /* the paint shop's step */
extern unsigned char D_8028DBBC;        /* the controller */
extern unsigned char D_8028DBD4;        /* the mixer is open */
extern unsigned char D_8028DCE0;        /* the channel being mixed */
extern BrImage D_8028D0B0;              /* the A button */
extern BrImage D_8028D0E0;              /* the B button */
void BrBevelPanel();
void BrFillRect(int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b);
void BrTextSetFont(int size);
void BrTextAlignLeft(void);
void BrTextAlignRight(void);
void BrTextSetColours(int r, int g, int b, int a, int x, int y);
void BrTextPrint(char *s, int x, int y);
void BrImageDrawAt(BrImage *img, int x, int y);
void BrPadConsume(unsigned int *pad, unsigned int bits);
void BrPadStickRepeatPos(unsigned int *pressed, int *timer, float *axis, unsigned int bit);
void BrPadStickRepeatNeg(unsigned int *pressed, int *timer, float *axis, unsigned int bit);
int sprintf(char *s, const char *fmt, ...);
/* -- end declarations -- */

/* WHAT IT DOES: The paint shop's colour mixer: a preview of the chosen
 * palette colour and a red, green and blue bar for it (the shadow colour,
 * entry 1, is held at half scale and drawn full width), each named and
 * valued, the current channel lit.  Up/down pick the channel; the stick
 * (tenths), left/right (4) and the C buttons (1) change it, clamped.  A
 * change re-packs the colour into the decal palette; the body colour and
 * the shadow track each other (one is half the other) and both go to the
 * car's body and shade parts.  A keeps the colour, B restores it.
 * The shade part's palette gets its first entry written twice and its
 * second never, as in the ROM.
 * RESIDUE (gap 96, 4 short): the ROM keeps the pad pointer for the two
 * stick-repeat calls in s0 and re-reads the channel byte before stepping
 * it up or down; the locals sit 0x10 lower in the frame. */
/* @t4-pass 0x8024B144 1 2026-10-03 compiles 26 best 527 moved 1  (n64/tools/n64permute.py) */
/* @t4-pass 0x8024B144 2 2026-10-03 compiles 26 best 527 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8024B144 */
/* @implements 0x8024B144 tgr BrPaintColourMix */
void BrPaintColourMix(void)
{
  int u0[6];
  int bx;
  int yb;
  int u1[6];
  int step;
  char buf[12];
  unsigned char bar[3][3] = { { 0xff, 0, 0 }, { 0, 0xff, 0 }, { 0, 0, 0xff } };
  char *names[3] = { "R", "G", "B" };
  unsigned int i;
  int y;
  int v;
  BrPadRec *pad;
  unsigned int ch;

  bx = 0x151 - D_8028D0E0.w;
  yb = 0x122 - D_8028D0B0.h;
  step = 0;
  BrBevelPanel(0xad, 0xb6, 0x126, 0x74, 3, 0, 0, 0x80, 0x80, 0x80);
  BrBevelPanel(0xbb, 0xc2, 0x3c, 0x3c, 1, 0, 1, D_80369B98[D_8028DB58].c[0], D_80369B98[D_8028DB58].c[1],
               D_80369B98[D_8028DB58].c[2]);
  ch = D_8028DCE0;
  for (i = 0, y = 0xc2; (int)i < 3; i++, y += 0x16) {
    if (i == ch) {
      BrBevelPanel(0x11d, y, 0x80, 0x10, 1, 0, 1, 0xb4, 0xb4, 0xb4);
    } else {
      BrBevelPanel(0x11d, y, 0x80, 0x10, 1, 1, 1, 0x80, 0x80, 0x80);
    }
    if ((v = D_80369B98[D_8028DB58].c[i]) > 0) {
      if (D_8028DB58 == 1) {
        BrFillRect(0x11d, y, D_80369B98[D_8028DB58].c[i], 0x10, bar[i][0], bar[i][1], bar[i][2]);
        if (i == ch) {
          BrBevelPanel(0x11d, y, D_80369B98[D_8028DB58].c[i], 0x10, 1, 0, 0, bar[i][0], bar[i][1], bar[i][2]);
        }
      } else {
        BrFillRect(0x11d, y, D_80369B98[D_8028DB58].c[i] >> 1, 0x10, bar[i][0], bar[i][1], bar[i][2]);
        if (i == ch) {
          BrBevelPanel(0x11d, y, D_80369B98[D_8028DB58].c[i] >> 1, 0x10, 1, 0, 0, bar[i][0], bar[i][1],
                       bar[i][2]);
        }
      }
    }
  }
  for (i = 0, y = 0x68; (int)i < 3; i++, y += 0xb) {
    if (i == D_8028DCE0) {
      BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xff, 0);
    } else {
      BrTextSetColours(200, 200, 200, 0x80, 0x80, 0x80);
    }
    BrTextSetFont(11);
    BrTextAlignLeft();
    BrTextPrint(names[i], 0x83, y);
    BrTextSetFont(10);
    BrTextAlignRight();
    sprintf(buf, "%d", D_80369B98[D_8028DB58].c[i]);
    BrTextPrint(buf, 0xe2, y);
  }
  BrImageDrawAt(&D_8028D0B0, 0xe5, yb);
  BrImageDrawAt(&D_8028D0E0, bx, yb);
  BrTextAlignLeft();
  BrTextSetFont(10);
  y = (yb + 0x12) >> 1;
  BrTextPrint("%wwOK", ((unsigned int)D_8028D0B0.w + 0xeb) >> 1, y);
  BrTextPrint("%wwCANCEL", (bx + (unsigned int)D_8028D0E0.w + 6) >> 1, y);
  pad = &PADS[D_8028DBBC];
  BrPadStickRepeatPos(&pad->pressed, &pad->repeat[0], &pad->axis[1], 8);
  pad = &PADS[D_8028DBBC];
  BrPadStickRepeatNeg(&pad->pressed, &pad->repeat[1], &pad->axis[1], 2);
  pad = &PADS[D_8028DBBC];
  if ((pad->axis[0] < 0.0 ? -pad->axis[0] : pad->axis[0]) >= 0.1f) {
    step = pad->axis[0] * 10.0f;
  } else if (pad->pressed & 1) {
    step = 4;
  } else if (pad->pressed & 4) {
    step = -4;
  } else if (pad->pressed & 0x200) {
    step = 1;
  } else if (pad->pressed & 0x800) {
    step = -1;
  }
  if (pad->pressed & 0x402) {
    BrPadConsume(&pad->pressed, 0x402);
    if (D_8028DCE0 == 2) {
      D_8028DCE0 = 0;
    } else {
      D_8028DCE0++;
    }
  } else if (pad->pressed & 0x108) {
    BrPadConsume(&pad->pressed, 0x108);
    if (D_8028DCE0 == 0) {
      D_8028DCE0 = 2;
    } else {
      D_8028DCE0--;
    }
  }
  if (D_8028DB58 == 1) {
    v = D_80369B98[D_8028DB58].c[D_8028DCE0] + step;
    if (v < 0) {
      D_80369B98[D_8028DB58].c[D_8028DCE0] = 0;
    } else if (v >= 0x80) {
      D_80369B98[D_8028DB58].c[D_8028DCE0] = 0x7f;
    } else {
      D_80369B98[D_8028DB58].c[D_8028DCE0] = v;
    }
  } else {
    v = D_80369B98[D_8028DB58].c[D_8028DCE0] + step;
    if (v < 0) {
      D_80369B98[D_8028DB58].c[D_8028DCE0] = 0;
    } else if (v >= 0x100) {
      D_80369B98[D_8028DB58].c[D_8028DCE0] = 0xff;
    } else {
      D_80369B98[D_8028DB58].c[D_8028DCE0] = v;
    }
  }
  if (step != 0) {
    D_8028DB90[D_8028DB58] = ((D_80369B98[D_8028DB58].c[0] & 0xf8) << 8) | ((D_80369B98[D_8028DB58].c[1] & 0xf8) << 3) | ((D_80369B98[D_8028DB58].c[2] & 0xf8) >> 2) | (D_8028DB90[D_8028DB58] & 1);
    if (D_8028DB58 == 0) {
      D_8028DB90[1] = (((D_80369B98[0].c[0] >> 1) & 0xf8) << 8) | (((D_80369B98[0].c[1] >> 1) & 0xf8) << 3) | (((D_80369B98[0].c[2] >> 1) & 0xf8) >> 2) | (D_8028DB90[1] & 1);
      for (i = 0; i < 3; i++) {
        if (D_80369B98[0].c[i] == 0xff) {
          D_80369B98[1].c[i] = 0x7f;
        } else {
          D_80369B98[1].c[i] = D_80369B98[0].c[i] >> 1;
        }
      }
    } else if (D_8028DB58 == 1) {
      D_8028DB90[0] = (((D_80369B98[1].c[0] << 1) & 0xf8) << 8) | (((D_80369B98[1].c[1] << 1) & 0xf8) << 3) | (((D_80369B98[1].c[2] << 1) & 0xf8) >> 2) | (D_8028DB90[0] & 1);
      for (i = 0; i < 3; i++) {
        if (D_80369B98[1].c[i] == 0x7f) {
          D_80369B98[0].c[i] = 0xff;
        } else {
          D_80369B98[0].c[i] = D_80369B98[1].c[i] << 1;
        }
      }
    }
    if (D_8028DB58 == 0 || D_8028DB58 == 1) {
      D_8028AB08->parts[D_8028AB08->bodyPart].tlut[0] = D_8028DB90[0];
      D_8028AB08->parts[D_8028AB08->bodyPart].tlut[1] = D_8028DB90[1];
      D_8028AB08->parts[D_8028AB08->shadePart].tlut[0] = D_8028DB90[0];
      D_8028AB08->parts[D_8028AB08->shadePart].tlut[0] = D_8028DB90[0];
    }
  }
  pad = &PADS[D_8028DBBC];
  if (pad->pressed & 0x10) {
    BrPadConsume(&pad->pressed, 0x10);
    D_8028DBD4 = 0;
    D_8028DCE0 = 0;
    D_8028DBB4++;
  } else if (pad->pressed & 0x20) {
    BrPadConsume(&pad->pressed, 0x20);
    D_8028DB90[D_8028DB58] = ((D_80369DA8[0] & 0xf8) << 8) | ((D_80369DA8[1] & 0xf8) << 3) | ((D_80369DA8[2] & 0xf8) >> 2) | (D_8028DB90[D_8028DB58] & 1);
    for (i = 0; i < 3; i++) {
      D_80369B98[D_8028DB58].c[i] = D_80369DA8[i];
    }
    if (D_8028DB58 == 0) {
      D_8028DB90[1] = (((D_80369DA8[0] >> 1) & 0xf8) << 8) | (((D_80369DA8[1] >> 1) & 0xf8) << 3) | (((D_80369DA8[2] >> 1) & 0xf8) >> 2) | (D_8028DB90[1] & 1);
      for (i = 0; i < 3; i++) {
        if (D_80369B98[0].c[i] == 0xff) {
          D_80369B98[1].c[i] = 0x7f;
        } else {
          D_80369B98[1].c[i] = D_80369DA8[i] >> 1;
        }
      }
    } else if (D_8028DB58 == 1) {
      D_8028DB90[0] = (((D_80369DA8[0] << 1) & 0xf8) << 8) | (((D_80369DA8[1] << 1) & 0xf8) << 3) | (((D_80369DA8[2] << 1) & 0xf8) >> 2) | (D_8028DB90[0] & 1);
      for (i = 0; i < 3; i++) {
        if (D_80369B98[1].c[i] == 0x7f) {
          D_80369B98[0].c[i] = 0xff;
        } else {
          D_80369B98[0].c[i] = D_80369DA8[i] << 1;
        }
      }
    }
    if (D_8028DB58 == 0 || D_8028DB58 == 1) {
      D_8028AB08->parts[D_8028AB08->bodyPart].tlut[0] = D_8028DB90[0];
      D_8028AB08->parts[D_8028AB08->bodyPart].tlut[1] = D_8028DB90[1];
      D_8028AB08->parts[D_8028AB08->shadePart].tlut[0] = D_8028DB90[0];
      D_8028AB08->parts[D_8028AB08->shadePart].tlut[0] = D_8028DB90[0];
    }
    D_8028DCE0 = 0;
    D_8028DBD4 = 0;
  }
}
