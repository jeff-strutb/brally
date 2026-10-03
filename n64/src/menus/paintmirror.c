/* paintmirror.c -- the paint shop's mirror/flip popup (its own object:
 * its strings and initialised tables sit together in .rodata and .data)
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
/* -- end declarations -- */

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
