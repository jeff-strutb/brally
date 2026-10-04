/* paintmenus.c -- the paint shop's clear and mirror/flip popups (one
 * object: their strings and initialised tables sit together in .rodata
 * and .data)
 */
#include "tgr/common.h"
#include "tgr/gbi.h"

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
extern unsigned char D_8028DB60;
extern unsigned char D_8028DB64;
extern unsigned char D_8028DB68;        /* the decal being painted */
extern unsigned char D_8028DBB8;        /* the chosen option */
extern unsigned char D_8028DBBC;        /* the controller */
extern unsigned char D_8028DBC0;
extern unsigned char D_8028DBCC;        /* the decal has a mirror side */
extern unsigned char D_8028DBE0;
extern unsigned char D_8028CFBC;        /* pending flip: 1 left-right, 2 top-bottom */
extern BrImage D_8028D0B0;              /* the A button */
extern BrImage D_8028D0E0;              /* the B button */
void BrBevelPanel();
void BrFillFrame(int x, int y, int w, int h, int t, unsigned char r, unsigned char g, unsigned char b);
void BrTextSetFont(int size);
void BrTextAlignLeft(void);
void BrTextAlignCentre(void);
void BrTextHighlightOff(void);
void BrTextSetColours(int r, int g, int b, int a, int x, int y);
void BrTextPrint(char *s, int x, int y);
void BrImageDrawAt(BrImage *img, int x, int y);
void BrPadConsume(unsigned int *pad, unsigned int bits);
void BrPadStickToButtons(BrPadRec *pad);
void BrPaintFlipApply(void);
void BrPaintMirrorSide(void);
typedef struct BrPaintSwatch {  /* 0x14 bytes */
  int x, y, w, h;
  unsigned char r, g, b;
} BrPaintSwatch;
extern BrPaintSwatch D_80369B98[16];    /* the palette */
extern BrImage D_8028CFC0;              /* its 0x2C byte: the clear popup's choice */
extern unsigned char D_8028DB58;        /* the chosen palette colour */
extern unsigned char D_8028DB70;
extern unsigned char *D_8028DB78;       /* the decal texture being painted */
extern void *D_8028DB7C;                /* its paint mask */
extern unsigned char D_8028DB88;        /* texture width */
extern unsigned char D_8028DB8C;        /* texture height */
extern BrImage *D_8028DB0C[];           /* each decal slot's preview image */
extern BrImage D_8028CBA0;              /* the car body preview */
typedef struct BrDecalPart {            /* a model part, as the decal code reads it (0x24) */
  unsigned char *tex;
  char pad04[8];
  unsigned short w;                     /* 0x0C */
  unsigned short h;                     /* 0x0E */
  char pad10[0x24 - 0x10];
} BrDecalPart;
typedef struct BrDecalModel {
  char pad00[0x14];
  BrDecalPart *parts;                   /* 0x14 */
  char pad18[0x110 - 0x18];
  unsigned char decalPart[12];          /* 0x110  each decal slot's part */
  void **masks;                         /* 0x11C  each slot's paint mask */
} BrDecalModel;
extern BrDecalModel *D_8028AB08;        /* the current car's model */
void BrImageDrawRect(BrImage *img, int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b);
void BrPaintDecalCommit(void);
void BrPaintPlot(int x, int y, unsigned char c);
/* -- end declarations -- */


/* WHAT IT DOES: The paint shop's clear popup: four previews (the current
 * decal or the whole car, in the selected colour or the body colour), the
 * chosen one framed in cyan, its description below, and A (select) and B
 * (cancel) buttons.  Left/right move the choice; A fills the current decal
 * or all ten decals with the colour, B cancels.
 * The pad record's index is multiplied unsigned (sizeof), as the ROM keeps
 * 0x15C in a saved register for multu.
 * RESIDUE (383, 7 short): saved-register allocation of the preview offsets
 * (the ROM keeps the unshifted differences in s3/s6/s7 and shifts at each
 * use), and its locals sit 0x1C lower in the frame. */
/* @t4-pass 0x80247B0C 1 2026-10-03 compiles 26 best 325 moved 57  (n64/tools/n64permute.py) */
/* @t4-pass 0x80247B0C 2 2026-10-03 compiles 26 best 325 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x80247B0C */
/* @implements 0x80247B0C tgr BrPaintClearMenu */
void BrPaintClearMenu(void)
{
  int i;
  int unused[8];
  int y;
  int bx;
  int r[4][4];
  char *what[2] = { "CLEAR CURRENT DECAL", "CLEAR ALL DECALS" };
  char *to[2] = { "TO SELECTED COLOR", "TO BODY COLOR" };
  unsigned int dx1;
  unsigned int dy1;
  unsigned int dx2;
  unsigned int dy2;
  int px;
  int py;
  int k;

  y = 0x160 - D_8028D0B0.h;
  bx = 0x161 - D_8028D0E0.w;
  for (i = 0; i < 4; i++) {
    r[i][0] = i * 0x46 + 0xb9;
    r[i][2] = 0x3c;
    r[i][3] = 0x4e;
    r[i][1] = 0xac;
  }
  BrBevelPanel(0xa7, 0x78, 0x132, 0xf0, 3, 0, 0, 0x80, 0x80, 0x80);
  for (i = 0; i < 4; i++) {
    if (i == D_8028CFC0.x2c) {
      BrFillFrame(r[i][0], r[i][1], r[i][2], r[i][3], 1, 0, 0xff, 0xff);
    } else {
      BrFillFrame(r[i][0], r[i][1], r[i][2], r[i][3], 1, 0x20, 0x20, 0x20);
    }
  }
  dx2 = (unsigned int)(r[2][2] - D_8028CBA0.w) >> 1;
  dy1 = (r[0][3] - (D_8028DB0C[D_8028DB68]->h >> 1)) >> 1;
  dy2 = (r[2][3] - D_8028CBA0.h) >> 1;
  dx1 = (r[0][2] - ((unsigned int)D_8028DB0C[D_8028DB68]->w >> 1)) >> 1;
  BrImageDrawRect(D_8028DB0C[D_8028DB68], r[0][0] + dx1, r[0][1] + dy1, D_8028DB0C[D_8028DB68]->drawW >> 1,
                  D_8028DB0C[D_8028DB68]->drawH >> 1, D_80369B98[D_8028DB58].r, D_80369B98[D_8028DB58].g,
                  D_80369B98[D_8028DB58].b);
  BrImageDrawRect(&D_8028CBA0, r[2][0] + dx2, r[2][1] + dy2, D_8028CBA0.drawW, D_8028CBA0.drawH,
                  D_80369B98[D_8028DB58].r, D_80369B98[D_8028DB58].g, D_80369B98[D_8028DB58].b);
  BrImageDrawRect(D_8028DB0C[D_8028DB68], r[1][0] + dx1, r[1][1] + dy1, D_8028DB0C[D_8028DB68]->drawW >> 1,
                  D_8028DB0C[D_8028DB68]->drawH >> 1, D_80369B98[0].r, D_80369B98[0].g, D_80369B98[0].b);
  BrImageDrawRect(&D_8028CBA0, r[3][0] + dx2, r[3][1] + dy2, D_8028CBA0.drawW, D_8028CBA0.drawH,
                  D_80369B98[0].r, D_80369B98[0].g, D_80369B98[0].b);
  BrTextAlignCentre();
  BrTextHighlightOff();
  BrTextSetFont(16);
  BrTextPrint("%rySELECT OPTION", 0x9e, 0x4e);
  BrBevelPanel(0xba, r[0][3] + r[0][1] + 12, 0x10c, 0x3a, 1, 1, 1, 0x80, 0x80, 0x80);
  BrTextSetFont(11);
  BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xf5, 0);
  BrTextPrint(what[D_8028CFC0.x2c >> 1], 0x9e, 0x8e);
  BrTextPrint(to[D_8028CFC0.x2c & 1], 0x9e, 0x9b);
  BrTextSetFont(10);
  BrTextAlignLeft();
  BrTextPrint("%wwSELECT", ((unsigned int)D_8028D0B0.w + 0xd9) >> 1, (y + 18) >> 1);
  BrTextPrint("%wwCANCEL", (bx + (unsigned int)D_8028D0E0.w + 6) >> 1, (y + 18) >> 1);
  BrImageDrawAt(&D_8028D0B0, 0xd3, y);
  BrImageDrawAt(&D_8028D0E0, bx, y);
  BrPadStickToButtons(&PADS[D_8028DBBC]);
  if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)) & 4) {
    BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)), 4);
    if (D_8028CFC0.x2c == 0) {
      D_8028CFC0.x2c = 3;
    } else {
      D_8028CFC0.x2c--;
    }
  } else if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)) & 1) {
    BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)), 1);
    if (D_8028CFC0.x2c == 3) {
      D_8028CFC0.x2c = 0;
    } else {
      D_8028CFC0.x2c++;
    }
  }
  if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)) & 0x10) {
    BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)), 0x10);
    if (D_8028CFC0.x2c == 0 || D_8028CFC0.x2c == 1) {
      BrPaintDecalCommit();
      for (py = 0; py < D_8028DB8C; py++) {
        for (px = 0; px < D_8028DB88; px++) {
          if (D_8028CFC0.x2c == 0) {
            BrPaintPlot(px, py, D_8028DB58);
          } else {
            BrPaintPlot(px, py, 0);
          }
        }
      }
    } else if (D_8028CFC0.x2c == 2 || D_8028CFC0.x2c == 3) {
      D_8028DB70 = D_8028DB68;
      for (k = 0; k < 10; k++) {
        D_8028DB78 = D_8028AB08->parts[D_8028AB08->decalPart[k]].tex;
        D_8028DB7C = D_8028AB08->masks[k];
        D_8028DB88 = D_8028AB08->parts[D_8028AB08->decalPart[k]].w;
        D_8028DB8C = D_8028AB08->parts[D_8028AB08->decalPart[k]].h;
        for (py = 0; py < D_8028DB8C; py++) {
          for (px = 0; px < D_8028DB88; px++) {
            if (D_8028CFC0.x2c == 2) {
              BrPaintPlot(px, py, D_8028DB58);
            } else {
              BrPaintPlot(px, py, 0);
            }
          }
        }
      }
      D_8028DB68 = D_8028DB70;
      D_8028DB78 = D_8028AB08->parts[D_8028AB08->decalPart[D_8028DB70]].tex;
      D_8028DB7C = D_8028AB08->masks[D_8028DB70];
      D_8028DB88 = D_8028AB08->parts[D_8028AB08->decalPart[D_8028DB70]].w;
      D_8028DB8C = D_8028AB08->parts[D_8028AB08->decalPart[D_8028DB70]].h;
      BrPaintDecalCommit();
      D_8028CFC0.x2c = 0;
    } else {
      D_8028CFC0.x2c = 0;
    }
    D_8028DB60 = D_8028DB64;
  } else if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)) & 0x20) {
    BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)), 0x20);
    D_8028CFC0.x2c = 0;
    D_8028DB60 = D_8028DB64;
  }
}

/* WHAT IT DOES: The paint shop's mirror/flip popup: three option boxes
 * (MIRROR, greyed when the decal has no mirror side; FLIP H; FLIP V) with
 * the chosen one framed in blue, a description of the choice below (which
 * side a mirror goes to, by the decal), and A (select) and B (cancel)
 * buttons.  Left/right move the choice; A applies it (mirror or flip); B
 * cancels back to the previous screen. */
/* @implements 0x8024843C tgr BrPaintMirrorMenu */
void BrPaintMirrorMenu(void)
{
  int i;
  int unused[8];
  int y;
  int bx;
  int r[3][4];
  char *flip1[2] = { "FLIP CURRENT DECAL", "FLIP CURRENT DECAL" };
  char *flip2[2] = { "HORIZONTALLY", "VERTICALLY" };

  y = 0x155 - D_8028D0B0.w;
  bx = 0x163 - D_8028D0E0.w;
  if (D_8028DBC0 != 0) {
    D_8028DBB8 = D_8028CFBC;
    D_8028DBC0 = 0;
  }
  r[0][2] = 0x66;
  r[0][3] = 0x2c;
  r[0][0] = 0xab;
  r[0][1] = 0xbf;
  for (i = 1; i < 3; i++) {
    r[i][2] = 0x5a;
    r[i][3] = r[0][3];
    r[i][0] = r[0][0] + r[0][2] + 8 + (i - 1) * (r[i][2] + 8);
    r[i][1] = r[0][1];
  }
  BrBevelPanel(0x99, 0x83, 0x14e, 0xda, 3, 0, 0, 0x80, 0x80, 0x80);
  BrTextSetFont(16);
  BrTextAlignCentre();
  BrTextHighlightOff();
  BrTextPrint("%rySELECT OPTION", 0x9f, 0x55);
  BrTextSetFont(14);
  if (D_8028DBCC != 0) {
    BrTextPrint("%wwMIRROR", 0x6e, 0x6e);
  } else {
    BrTextPrint("%55MIRROR", 0x6e, 0x6e);
  }
  BrTextPrint("%wwFLIP H", 0xa2, 0x6e);
  BrTextPrint("%wwFLIP V", 0xd3, 0x6e);
  for (i = 0; i < 3; i++) {
    if (i == D_8028DBB8) {
      BrFillFrame(r[i][0], r[i][1], r[i][2], r[i][3], 1, 0x20, 200, 0xff);
    } else {
      BrFillFrame(r[i][0], r[i][1], r[i][2], r[i][3], 1, 0x20, 0x20, 0x20);
    }
  }
  BrBevelPanel(0xac, 0xf8, 0x128, 0x3a, 1, 1, 1, 0x80, 0x80, 0x80);
  BrTextSetFont(11);
  BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xf5, 0);
  if (D_8028DBB8 == 0) {
    if (D_8028DBCC != 0) {
      switch (D_8028DB68) {
      case 0:
        BrTextPrint("MIRROR CURRENT DECAL", 0x9f, 0x87);
        BrTextPrint("TO THE RIGHT DOOR", 0x9f, 0x94);
        break;
      case 1:
        BrTextPrint("MIRROR CURRENT DECAL", 0x9f, 0x87);
        BrTextPrint("TO THE LEFT DOOR", 0x9f, 0x94);
        break;
      case 2:
        BrTextPrint("MIRROR CURRENT DECAL TO", 0x9f, 0x87);
        BrTextPrint("THE RIGHT FRONT FENDER", 0x9f, 0x94);
        break;
      case 3:
        BrTextPrint("MIRROR CURRENT DECAL TO", 0x9f, 0x87);
        BrTextPrint("THE LEFT FRONT FENDER", 0x9f, 0x94);
        break;
      case 6:
        BrTextPrint("MIRROR CURRENT DECAL TO", 0x9f, 0x87);
        BrTextPrint("THE RIGHT REAR FENDER", 0x9f, 0x94);
        break;
      case 7:
        BrTextPrint("MIRROR CURRENT DECAL TO", 0x9f, 0x87);
        BrTextPrint("THE LEFT REAR FENDER", 0x9f, 0x94);
        break;
      }
    } else {
      BrTextPrint("MIRROR IS NOT APPLICABLE", 0x9f, 0x87);
      BrTextPrint("TO THE CURRENT DECAL", 0x9f, 0x94);
    }
  } else {
    BrTextPrint(flip1[D_8028DBB8 - 1], 0x9f, 0x87);
    BrTextPrint(flip2[D_8028DBB8 - 1], 0x9f, 0x94);
  }
  BrTextSetFont(10);
  BrTextAlignLeft();
  BrTextPrint("%wwSELECT", ((unsigned int)D_8028D0B0.w + 0xd9) >> 1, (y + 18) >> 1);
  BrTextPrint("%wwCANCEL", (bx + (unsigned int)D_8028D0E0.w + 6) >> 1, (y + 18) >> 1);
  BrImageDrawAt(&D_8028D0B0, 0xd3, y);
  BrImageDrawAt(&D_8028D0E0, bx, y);
  BrPadStickToButtons(&PADS[D_8028DBBC]);
  if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 1) {
    BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 1);
    if (D_8028DBB8 == 2) {
      D_8028DBB8 = 0;
    } else {
      D_8028DBB8++;
    }
  } else if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 4) {
    BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 4);
    if (D_8028DBB8 == 0) {
      D_8028DBB8 = 2;
    } else {
      D_8028DBB8--;
    }
  }
  if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 0x10) {
    BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 0x10);
    D_8028CFBC = D_8028DBB8;
    if (D_8028DBB8 == 0) {
      if (D_8028DBCC != 0) {
        BrPaintMirrorSide();
      }
    } else {
      BrPaintFlipApply();
    }
  } else if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 0x20) {
    BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 0x20);
    D_8028CFBC = 0;
    D_8028DBE0 = 0;
    D_8028DB60 = D_8028DB64;
  }
}
