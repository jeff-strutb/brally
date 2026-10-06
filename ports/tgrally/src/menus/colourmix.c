/* colourmix.c -- the paint shop's colour mixer popup
 *
 * Its own object here: its strings and initialised tables sit apart from
 * the other paint-shop functions'.
 */
#include "tgr/common.h"
#include "tgr/pad.h"

/* -- declarations -- */
#include "tgr/image.h"
typedef struct BrMixSwatch {    /* a palette entry, as the mixer reads it (0x14) */
  int x, y, w, h;
  unsigned char c[3];           /* 0x10  red, green, blue */
} BrMixSwatch;
extern BrMixSwatch D_80369B98[16];      /* the palette: 0 the body colour, 1 the shadow (half scale) */
extern unsigned char D_80369DA8[3];     /* the colour before the mixer opened */
typedef struct BrMixPart {      /* a model part (0x24) */
  be32_t x0;  /* void * */
  be32_t tlut;         /* unsigned short * -- 0x04 */
  char pad08[0x24 - 8];
} BrMixPart;
typedef struct BrMixModel {
  char pad00[0x14];
  be32_t parts;             /* BrMixPart * -- 0x14 */
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
extern unsigned char D_8028DCE0;         /* the channel being mixed (the arena: tools/globals.py) */
extern BrImage D_8028D0B0;              /* the A button */
extern BrImage D_8028D0E0;              /* the B button */
void BrBevelPanel(int, int, int, int, int, char, char, unsigned char, unsigned char, unsigned char);
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
 * The channel is tested by name and stepped through chan, so the step
 * reads it again rather than reusing the tested value. */
/* @implements 0x8024B144 tgr BrPaintColourMix */
void BrPaintColourMix(void)
{
  unsigned int i;
  int y;
  int ty;
  int v;
  char **name;
  float mag;
  unsigned char *chan;
  int u0[3];
  int bx;
  int yb;
  int u1[6];
  int step;
  char buf[12];
  unsigned char bar[3][3] = { { 0xff, 0, 0 }, { 0, 0xff, 0 }, { 0, 0, 0xff } };
  char *names[3] = { "R", "G", "B" };
  int u2[2];
  unsigned int ch;

  chan = &D_8028DCE0;
  bx = 0x151 - D_8028D0E0.w;
  step = 0;
  yb = 0x122 - D_8028D0B0.h;
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
  for (i = 0, name = names, ty = 0x68; (int)i < 3; i++, name++, ty += 0xb) {
    if (i == D_8028DCE0) {
      BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xff, 0);
    } else {
      BrTextSetColours(200, 200, 200, 0x80, 0x80, 0x80);
    }
    BrTextSetFont(11);
    BrTextAlignLeft();
    BrTextPrint(*name, 0x83, ty);
    BrTextSetFont(10);
    BrTextAlignRight();
    sprintf(buf, "%d", D_80369B98[D_8028DB58].c[i]);
    BrTextPrint(buf, 0xe2, ty);
  }
  BrImageDrawAt(&D_8028D0B0, 0xe5, yb);
  BrImageDrawAt(&D_8028D0E0, bx, yb);
  BrTextAlignLeft();
  BrTextSetFont(10);
  ty = (yb + 0x12) >> 1;
  BrTextPrint("%wwOK", ((unsigned int)D_8028D0B0.w + 0xeb) >> 1, ty);
  BrTextPrint("%wwCANCEL", (bx + (unsigned int)D_8028D0E0.w + 6) >> 1, ty);
  BrPadStickRepeatPos(&D_8036A8E0[D_8028DBBC].pressed, &D_8036A8E0[D_8028DBBC].repeat[0], &D_8036A8E0[D_8028DBBC].axis[1], 8);
  BrPadStickRepeatNeg(&D_8036A8E0[D_8028DBBC].pressed, &D_8036A8E0[D_8028DBBC].repeat[1], &D_8036A8E0[D_8028DBBC].axis[1], 2);
  mag = D_8036A8E0[D_8028DBBC].axis[0] < 0.0 ? -D_8036A8E0[D_8028DBBC].axis[0] : D_8036A8E0[D_8028DBBC].axis[0];
  if (mag >= 0.1f) {
    step = D_8036A8E0[D_8028DBBC].axis[0] * 10.0f;
  } else if (D_8036A8E0[D_8028DBBC].pressed & 1) {
    step = 4;
  } else if (D_8036A8E0[D_8028DBBC].pressed & 4) {
    step = -4;
  } else if (D_8036A8E0[D_8028DBBC].pressed & 0x200) {
    step = 1;
  } else if (D_8036A8E0[D_8028DBBC].pressed & 0x800) {
    step = -1;
  }
  if (D_8036A8E0[D_8028DBBC].pressed & 0x402) {
    BrPadConsume(&D_8036A8E0[D_8028DBBC].pressed, 0x402);
    if (D_8028DCE0 == 2) {
      *chan = 0;
    } else {
      (*chan)++;
    }
  } else if (D_8036A8E0[D_8028DBBC].pressed & 0x108) {
    BrPadConsume(&D_8036A8E0[D_8028DBBC].pressed, 0x108);
    if (D_8028DCE0 == 0) {
      *chan = 2;
    } else {
      (*chan)--;
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
    tgr_wr16(&D_8028DB90[D_8028DB58], ((D_80369B98[D_8028DB58].c[0] & 0xf8) << 8) | ((D_80369B98[D_8028DB58].c[1] & 0xf8) << 3) | ((D_80369B98[D_8028DB58].c[2] & 0xf8) >> 2) | (tgr_rd16(&D_8028DB90[D_8028DB58]) & 1));
    if (D_8028DB58 == 0) {
      tgr_wr16(&D_8028DB90[1], (((D_80369B98[0].c[0] >> 1) & 0xf8) << 8) | (((D_80369B98[0].c[1] >> 1) & 0xf8) << 3) | (((D_80369B98[0].c[2] >> 1) & 0xf8) >> 2) | (tgr_rd16(&D_8028DB90[1]) & 1));
      for (i = 0; i < 3; i++) {
        if (D_80369B98[0].c[i] == 0xff) {
          D_80369B98[1].c[i] = 0x7f;
        } else {
          D_80369B98[1].c[i] = D_80369B98[0].c[i] >> 1;
        }
      }
    } else if (D_8028DB58 == 1) {
      tgr_wr16(&D_8028DB90[0], (((D_80369B98[1].c[0] << 1) & 0xf8) << 8) | (((D_80369B98[1].c[1] << 1) & 0xf8) << 3) | (((D_80369B98[1].c[2] << 1) & 0xf8) >> 2) | (tgr_rd16(&D_8028DB90[0]) & 1));
      for (i = 0; i < 3; i++) {
        if (D_80369B98[1].c[i] == 0x7f) {
          D_80369B98[0].c[i] = 0xff;
        } else {
          D_80369B98[0].c[i] = D_80369B98[1].c[i] << 1;
        }
      }
    }
    if (D_8028DB58 == 0 || D_8028DB58 == 1) {
      BEPTR(unsigned short *, BEPTR(BrMixPart *, D_8028AB08->parts)[D_8028AB08->bodyPart].tlut)[0] = D_8028DB90[0];   /* a raw copy between palettes */
      BEPTR(unsigned short *, BEPTR(BrMixPart *, D_8028AB08->parts)[D_8028AB08->bodyPart].tlut)[1] = D_8028DB90[1];   /* a raw copy between palettes */
      BEPTR(unsigned short *, BEPTR(BrMixPart *, D_8028AB08->parts)[D_8028AB08->shadePart].tlut)[0] = D_8028DB90[0];   /* a raw copy between palettes */
      BEPTR(unsigned short *, BEPTR(BrMixPart *, D_8028AB08->parts)[D_8028AB08->shadePart].tlut)[0] = D_8028DB90[0];   /* a raw copy between palettes */
    }
  }
  if (D_8036A8E0[D_8028DBBC].pressed & 0x10) {
    BrPadConsume(&D_8036A8E0[D_8028DBBC].pressed, 0x10);
    D_8028DCE0 = 0;
    D_8028DBD4 = 0;
    D_8028DBB4++;
  } else if (D_8036A8E0[D_8028DBBC].pressed & 0x20) {
    BrPadConsume(&D_8036A8E0[D_8028DBBC].pressed, 0x20);
    tgr_wr16(&D_8028DB90[D_8028DB58], ((D_80369DA8[0] & 0xf8) << 8) | ((D_80369DA8[1] & 0xf8) << 3) | ((D_80369DA8[2] & 0xf8) >> 2) | (tgr_rd16(&D_8028DB90[D_8028DB58]) & 1));
    for (i = 0; i < 3; i++) {
      D_80369B98[D_8028DB58].c[i] = D_80369DA8[i];
    }
    if (D_8028DB58 == 0) {
      tgr_wr16(&D_8028DB90[1], (((D_80369DA8[0] >> 1) & 0xf8) << 8) | (((D_80369DA8[1] >> 1) & 0xf8) << 3) | (((D_80369DA8[2] >> 1) & 0xf8) >> 2) | (tgr_rd16(&D_8028DB90[1]) & 1));
      for (i = 0; i < 3; i++) {
        if (D_80369B98[0].c[i] == 0xff) {
          D_80369B98[1].c[i] = 0x7f;
        } else {
          D_80369B98[1].c[i] = D_80369DA8[i] >> 1;
        }
      }
    } else if (D_8028DB58 == 1) {
      tgr_wr16(&D_8028DB90[0], (((D_80369DA8[0] << 1) & 0xf8) << 8) | (((D_80369DA8[1] << 1) & 0xf8) << 3) | (((D_80369DA8[2] << 1) & 0xf8) >> 2) | (tgr_rd16(&D_8028DB90[0]) & 1));
      for (i = 0; i < 3; i++) {
        if (D_80369B98[1].c[i] == 0x7f) {
          D_80369B98[0].c[i] = 0xff;
        } else {
          D_80369B98[0].c[i] = D_80369DA8[i] << 1;
        }
      }
    }
    if (D_8028DB58 == 0 || D_8028DB58 == 1) {
      BEPTR(unsigned short *, BEPTR(BrMixPart *, D_8028AB08->parts)[D_8028AB08->bodyPart].tlut)[0] = D_8028DB90[0];   /* a raw copy between palettes */
      BEPTR(unsigned short *, BEPTR(BrMixPart *, D_8028AB08->parts)[D_8028AB08->bodyPart].tlut)[1] = D_8028DB90[1];   /* a raw copy between palettes */
      BEPTR(unsigned short *, BEPTR(BrMixPart *, D_8028AB08->parts)[D_8028AB08->shadePart].tlut)[0] = D_8028DB90[0];   /* a raw copy between palettes */
      BEPTR(unsigned short *, BEPTR(BrMixPart *, D_8028AB08->parts)[D_8028AB08->shadePart].tlut)[0] = D_8028DB90[0];   /* a raw copy between palettes */
    }
    D_8028DCE0 = 0;
    D_8028DBD4 = 0;
  }
}
