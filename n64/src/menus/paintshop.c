/* paintshop.c -- the paint shop
 */
#include "tgr/common.h"
#include "tgr/car.h"
#include "tgr/gbi.h"

/* -- declarations -- */
char * memcpy(char *param_1,char *param_2,int param_3);
extern unsigned char D_8028DB68;
extern unsigned char D_8028DB74;
extern unsigned char *D_8028DB78;          /* the decal texture being painted (CI4) */
extern unsigned char *D_8028DB7C;          /* its paint mask, 1 bit a texel, or 0 */
extern unsigned char D_8028DB88;           /* texture width */
extern unsigned char D_8028DB8C;           /* texture height */
extern unsigned char *D_8028DB80;
extern unsigned char D_8028DBB4;
extern unsigned char D_8028DBDC;
extern BrCarModel *D_8028AB08;
typedef struct BrPaintState {   /* 0x8028D110 */
  char pad00[0x1c];
  int x;                        /* 0x1C  cursor */
  int y;                        /* 0x20 */
} BrPaintState;
typedef struct BrPaintBrush {   /* 0x8028D290 */
  char pad00[0x1c];
  int x;                        /* 0x1C */
  int y;                        /* 0x20 */
  int w;                        /* 0x24 */
  int h;                        /* 0x28 */
  unsigned char flash;          /* 0x2C  the key cursor was just moved */
} BrPaintBrush;
typedef struct BrPaintBox {
  int pad[7];
  int x;                        /* 0x1C */
  int y;                        /* 0x20 */
  int w;                        /* 0x24  may be negative (mirrored) */
  int h;                        /* 0x28 */
} BrPaintBox;
extern BrPaintState D_8028D110;
extern BrPaintBrush D_8028D290;
int BrPaintCursorInRect(int *r);
extern int D_8028D12C;
extern int D_8028D130;
extern unsigned char D_8028DBBC;
extern unsigned char D_8028DBC4;
extern unsigned char D_8028DBE8;
extern float D_802AB20C;
extern float D_802AB210;
extern char D_8036A8E0;
extern char D_8036A8F8;
typedef struct BrPaintRect { int x, y, w, h; } BrPaintRect;
typedef struct BrPaintSwatch {  /* 0x14 bytes */
  int x, y, w, h;
  unsigned char r, g, b;
} BrPaintSwatch;
void BrFillRect(int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b);
void BrFillFrame(int x, int y, int w, int h, int t, unsigned char r, unsigned char g, unsigned char b);
extern BrPaintRect D_8028D480;
extern BrPaintRect D_8028D490;
extern BrPaintSwatch D_80369B98[16];
extern unsigned char D_8028DB58;       /* the chosen palette colour */
extern unsigned short *D_8028DB90;     /* the decal palette, RGBA5551 */
typedef struct BrPaintArea { int x, y, w, h; } BrPaintArea;
extern BrPaintArea D_8028DB94;         /* the paint area on screen */
void BrPaintPlot(int x, int y, unsigned char c);
void BrPaintDisc(int x, int y, int r, unsigned char screen);
extern Gfx *D_8028A858;
extern int D_8028A850;
extern int D_8028A898;                 /* the texture filter mode */
extern BrCar *D_8028AAF0;               /* the car shown */
extern BrCarCam *D_8028AAF4;            /* its camera */
extern unsigned char D_8028DBD0;        /* the view is turning */
extern unsigned char D_8028DB6C;        /* the view it turns from */
extern unsigned char D_8028DB70;
extern unsigned char D_8028DAC0;        /* the chosen preset */
extern BrPaintRect D_8028D4A0[];
extern BrPaintRect D_8028DAC4;
extern unsigned short D_8028DBB0;       /* frames into the turn (16 in all) */
extern BrVec3 D_8028DC08[];             /* the preset view directions */
extern float D_8028AAC0;
extern float D_8028AAC8;
extern int D_8028C334;
extern int D_8028AAB0;
extern int D_8028AAB4;
void BrVec3Normalise(BrVec3 *v);
float BrVec3Dot(BrVec3 *a, BrVec3 *b);
void BrVec3Cross(BrVec3 *out, BrVec3 *a, BrVec3 *b);
void BrVec3AddTo(BrVec3 *v, BrVec3 *a);
void BrVec3Scale(BrVec3 *out, BrVec3 *v, float s);
float BrAtan2(float x, float y);
void BrCameraSet(BrCarCam *cam, float a, float b, float c, float d);
void BrViewportSet(int x, int y, int w, int h, int scissor);
void BrGridSpanExtend(int a, int b);
void BrCarPlaceWheels(int n);
void BrCarVisibility(BrCar *car);
void func_80230554(BrCar *car, int);
extern int D_8028DB08;
extern unsigned char D_8028DB54;
extern unsigned char D_8028DB64;
extern unsigned char D_8028DB60;
extern unsigned char D_8028CF5C;
extern unsigned char D_8028CF8C;
extern unsigned char D_8028DBC0;
extern unsigned char D_8028DBC8;
extern unsigned char D_8028DBCC;
extern unsigned char D_8028DBD4;
extern unsigned char D_8028DBD8;
extern unsigned char D_8028DBE0;
extern unsigned char D_8028DBE4;
extern unsigned char D_8028DBEC;
extern int D_8028DB0C[];               /* per decal slot */
void BrPaintDecalCommit(void);
unsigned char BrPaintPeek(unsigned char *tex, int x, int y);
extern unsigned char D_8028CFBC;         /* pending flip: 1 left-right, 2 top-bottom */
extern unsigned char D_80369DAB;        /* the mirrored view */
void BrFillPoint(int x, int y, unsigned char c);
extern unsigned char D_8028DAB8;        /* the two dash colours */
extern unsigned char D_8028DABC;
typedef struct BrImage {
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
typedef struct BrGlyph {
  int x0;
  int x4;
  unsigned char c;
  char pad9[3];
  int off;
  int w;
  int kernL;
  int kernR;
} BrGlyph;
void BrImageDrawPart(BrImage *img, int s, int t, int sw, int th, int x, int y, int w, int h,
                     unsigned char r, unsigned char g, unsigned char b);
extern unsigned char D_8028CF2C;
extern BrImage D_8028D2C0;
extern BrGlyph D_8028D540[50];
extern unsigned char D_8028DB5C;
extern unsigned char D_8028DBA8;
extern unsigned char D_8028DBAC;
extern BrGlyph *D_80369E68[];
void BrFadeTo(float dir, float speed);
void BrBevelPanel();
void BrTextSetFont(int size);
void BrTextAlignLeft(void);
void BrTextHighlightOff(void);
void BrTextSetColours(int r, int g, int b, int a, int x, int y);
void BrTextPrint(char *s, int x, int y);
void BrImageDrawAt(BrImage *img, int x, int y);
void BrPadConsume(unsigned int *pad, unsigned int bits);
extern BrImage D_8028D0B0;              /* the A button */
extern BrImage D_8028D0E0;              /* the B button */
extern float D_802AB260;
extern float D_802AB264;
typedef struct BrPadRec {       /* as tgr/pad.h (0x15C bytes) */
  unsigned int pressed;         /* 0x00 */
  unsigned int held;            /* 0x04 */
  int repeat[4];                /* 0x08 */
  float axis[2];                /* 0x18  stick axes */
  char pad20[0x15c - 0x20];
} BrPadRec;
#define PADS ((BrPadRec *)&D_8036A8E0)
void BrImageDrawRect(BrImage *img, int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b);
void BrTextAlignCentre(void);
void BrPadStickToButtons(BrPadRec *pad);
extern unsigned char D_8028DBB8;       /* the chosen decal style */
extern BrImage *D_8028DB34[4];         /* the four style pictures */
typedef struct { char *n[4]; } BrPaintNames;
extern BrPaintNames D_8028DCF4;        /* the four style names */
extern BrImage *D_8028DB44[4];         /* the four oval style pictures */
extern BrPaintNames D_8028DD04;        /* and their names */
void BrImageDraw(BrImage *img);
extern BrImage D_8028D230;              /* the on-screen keyboard */
extern BrImage D_8028D260;              /* the text box under it */
extern unsigned char D_8028DBA4;        /* the key under the cursor */
extern char D_80369EA8[];               /* the typed text */
void BrImageDrawTinted(BrImage *img, unsigned char r, unsigned char g, unsigned char b);
extern BrImage D_8028D410;              /* the round brush picture */
typedef struct { char *n[2]; } BrPaintNames2;
extern BrPaintNames2 D_8028DCE4;       /* "ROUND BRUSH", "SQUARE BRUSH" */
extern unsigned char D_8028CE9C;        /* the chosen brush shape */
extern unsigned char *D_8028DB84;         /* a scratch copy of the decal */
typedef struct { int y, xl, xr, dy; } BrFillSeg;
/* -- end declarations -- */

/* WHAT IT DOES: Take the chosen preset's rectangle, and make every view
 * slot (previous, current, saved) the current view. */
/* @implements 0x802533F4 tgr BrPaintPresetApply */
void BrPaintPresetApply(void)
{
  D_8028DAC4 = D_8028D4A0[D_8028DAC0];
  D_8028DB6C = D_8028DB68;
  D_8028DB70 = D_8028DB68;
  D_8028DB74 = D_8028DB68;
}

/* WHAT IT DOES: Read the colour index of texel (x, y) of the decal being
 * painted. */
/* @implements 0x8024D7D8 tgr BrPaintGet */
unsigned char BrPaintGet(int x, int y)
{
  int o;
  unsigned char c;

  o = (((y & 1) << 3 ^ x) >> 1) + y * (D_8028DB88 >> 1);
  if (x & 1) {
    c = D_8028DB78[o] & 0xf;
  } else {
    c = D_8028DB78[o] >> 4;
  }
  return c;
}

/* WHAT IT DOES: If the decal has changed, copy the working decal (2 KB)
 * into the car model's texture for the current decal slot, step the edit
 * point back and clear the changed mark. */
/* @implements 0x80244D04 tgr BrPaintDecalApply */
void BrPaintDecalApply(void)
{
  if (D_8028DBDC != 0) {
    memcpy(D_8028AB08->parts[D_8028AB08->decalPart[D_8028DB74]].a, (char *)D_8028DB80, 0x800);
    D_8028DBB4--;
    D_8028DBDC = 0;
  }
}

/* WHAT IT DOES: The Controller Pak decal message box: a framed panel (lower
 * down in the pause layout, mode 1) with the message for code msg, worded
 * for loading (op 9) or saving -- no pak, unreadable or unwritable pak,
 * decals not found or a save error, not enough pages or notes, bad data,
 * a controller error, a nonfunctional pak, or (99999) a general error.
 * The cases are in the source order the strings' .rodata order gives
 * (1/11, 7/8, 3, 5, 99999, 6, 4, 10).
 * RESIDUE (435): register allocation (the ROM keeps mode in s0 and reuses
 * msg's s1 for y; frame 0x68 vs ours 0x50) and this file's .rodata (the
 * jump table) not mapping onto the ROM's. */
/* @t4-pass 0x80244D84 1 2026-10-03 compiles 26 best 434 moved 1  (n64/tools/n64permute.py) */
/* @t4-pass 0x80244D84 2 2026-10-03 compiles 26 best 434 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x80244D84 */
/* @implements 0x80244D84 tgr BrPakMessage */
void BrPakMessage(int msg, char op, char mode)
{
  int x;
  int y;
  int w;

  BrTextSetFont(12);
  if (1 == mode) {
    BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xf5, 0);
  } else {
    BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xca, 0);
  }
  BrTextAlignLeft();
  BrTextHighlightOff();
  switch (msg) {
  case 1:
  case 11:
    y = 0xcc;
    if (mode == 1) {
      y = 0xea;
    }
    BrBevelPanel(0xd5, y, 0xd5, 0x48, 3, 0, 0, 0x80, 0x80, 0x80);
    y = (y + 30) >> 1;
    BrTextPrint("CONTROLLER PAK", 0x72, y);
    BrTextPrint("IS NOT INSERTED", 0x72, y + 14);
    break;
  case 7:
  case 8:
    y = 0x97;
    if (mode == 1) {
      y = 0xc3;
    }
    BrBevelPanel(0xa5, y, 0x135, 0xb2, 3, 0, 0, 0x80, 0x80, 0x80);
    y = (y + 30) >> 1;
    BrTextPrint("INSUFFICIENT FREE PAGES", 0x5a, y);
    BrTextPrint("OR FREE NOTES IN THE", 0x5a, y + 12);
    BrTextPrint("CONTROLLER PAK.", 0x5a, y + 24);
    BrTextPrint("62 PAGES AND ONE NOTE", 0x5a, y + 42);
    BrTextPrint("ARE NEEDED TO SAVE THE", 0x5a, y + 54);
    BrTextPrint("CUSTOM DECALS.", 0x5a, y + 66);
    break;
  case 3:
    w = 0x116;
    if (op == 10) {
      w = 0x106;
    }
    y = 0xcc;
    if (mode == 1) {
      y = 0xe2;
    }
    x = (0x280 - w) >> 1;
    BrBevelPanel(x, y, w, 0x48, 3, 0, 0, 0x80, 0x80, 0x80);
    if (op == 9) {
      y = (y + 30) >> 1;
      x = (x + 16) >> 1;
      BrTextPrint("UNABLE TO READ FROM", x, y);
    } else {
      y = (y + 30) >> 1;
      x = (x + 16) >> 1;
      BrTextPrint("UNABLE TO WRITE TO", x, y);
    }
    BrTextPrint("THE CONTROLLER PAK", x, y + 14);
    break;
  case 5:
    w = 0xf2;
    if (op == 10) {
      w = 0x10c;
    }
    y = 0xcc;
    if (mode == 1) {
      y = 0xe2;
    }
    x = (0x280 - w) >> 1;
    BrBevelPanel(x, y, w, 0x48, 3, 0, 0, 0x80, 0x80, 0x80);
    if (op == 9) {
      y = (y + 30) >> 1;
      x = (x + 16) >> 1;
      BrTextPrint("DECALS NOT FOUND", x, y);
      BrTextPrint("IN CONTROLLER PAK", x, y + 14);
    } else {
      y = (y + 30) >> 1;
      x = (x + 16) >> 1;
      BrTextPrint("ERROR ENCOUNTERED", x, y);
      BrTextPrint("WHILE SAVING DECALS", x, y + 14);
    }
    break;
  case 99999:
    w = 0x114;
    if (op == 10) {
      w = 0x10b;
    }
    y = 0xcc;
    if (mode == 1) {
      y = 0xe2;
    }
    x = (0x280 - w) >> 1;
    BrBevelPanel(x, y, w, 0x48, 3, 0, 0, 0x80, 0x80, 0x80);
    x = (x + 15) >> 1;
    y = (y + 30) >> 1;
    BrTextPrint("ERROR ENCOUNTERED", x, y);
    if (op == 9) {
      BrTextPrint("WHILE LOADING DECALS", x, y + 14);
    } else {
      BrTextPrint("WHILE SAVING DECALS", x, y + 14);
    }
    break;
  case 6:
    y = 0xcc;
    if (mode == 1) {
      y = 0xe4;
    }
    BrBevelPanel(0xad, y, 0x126, 0x48, 3, 0, 0, 0x80, 0x80, 0x80);
    y = (y + 30) >> 1;
    BrTextPrint("BAD DATA ENCOUNTERED", 0x5d, y);
    if (op == 9) {
      BrTextPrint("WHILE LOADING DECALS", 0x5d, y + 14);
    } else {
      BrTextPrint("WHILE SAVING DECALS", 0x5d, y + 14);
    }
    break;
  case 4:
    y = 0xb0;
    if (mode == 1) {
      y = 0xdc;
    }
    BrBevelPanel(0xae, y, 0x124, 0x80, 3, 0, 0, 0x80, 0x80, 0x80);
    y = (y + 30) >> 1;
    BrTextPrint("CONTROLLER ERROR HAS", 0x5e, y);
    BrTextPrint("BEEN DETECTED.", 0x5e, y + 12);
    BrTextPrint("CUSTOM DECALS CANNOT", 0x5e, y + 30);
    if (op == 9) {
      BrTextPrint("BE LOADED.", 0x5e, y + 42);
    } else {
      BrTextPrint("BE SAVED.", 0x5e, y + 42);
    }
    break;
  case 10:
    y = 0xcc;
    if (mode == 1) {
      y = 0xf8;
    }
    BrBevelPanel(0xcd, y, 0xe6, 0x48, 3, 0, 0, 0x80, 0x80, 0x80);
    y = (y + 30) >> 1;
    BrTextPrint("CONTROLLER PAK", 0x6e, y);
    BrTextPrint("IS NONFUNCTIONAL", 0x6e, y + 14);
    break;
  }
}

/* WHAT IT DOES: The character for index i of the name-entry character set
 * (a NUL, blanks, digits, capitals, punctuation; 66 entries, no
 * terminator); a space past its end. */
/* @implements 0x80253460 tgr BrPaintCharset */
char BrPaintCharset(unsigned char i)
{
  char set[0x42] = "\000               0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#'*+,-./:=?@";

  if (i < 0x42) {
    return set[i];
  }
  return ' ';
}

/* WHAT IT DOES: Reset the paint shop for a new session: cursor to the
 * centre, decal slot 2 selected everywhere, the default colour and brush,
 * and every mode and changed flag cleared. */
/* @implements 0x80244BA8 tgr BrPaintReset */
void BrPaintReset(void)
{
  D_8028D110.x = 0xf4;
  D_8028D110.y = 0x110;
  D_8028DB68 = 2;
  D_8028DB6C = D_8028DB68;
  D_8028DB74 = D_8028DB68;
  D_8028DB08 = D_8028DB0C[D_8028DB68];
  D_8028DAC0 = 2;
  D_8028DB54 = 0;
  D_8028DB58 = 3;
  D_8028DB64 = 1;
  D_8028DB60 = 1;
  D_8028DBB0 = 0;
  D_8028CF5C = 0;
  D_8028CF8C = 0;
  D_8028DBB4 = 0;
  D_8028DBC0 = 0;
  D_8028DBC4 = 0;
  D_8028DBC8 = 1;
  D_8028DBCC = 1;
  D_8028DBD0 = 0;
  D_8028DBD4 = 0;
  D_8028DBD8 = 1;
  D_8028DBDC = 0;
  D_8028DBE0 = 0;
  D_8028DBE4 = 0;
  D_8028DBEC = 0;
}

/* WHAT IT DOES: Store the paint shop's working decal (2 KB) into the decal
 * buffer, move the edit point on, and mark the decal as changed so it is
 * redrawn. */
/* @implements 0x80244CA8 tgr BrPaintDecalCommit */
void BrPaintDecalCommit(void)
{
  memcpy((char *)D_8028DB80,(char *)D_8028DB78,0x800);
  D_8028DB74 = D_8028DB68;
  D_8028DBB4 = D_8028DBB4 + '\x01';
  D_8028DBDC = 1;
}

/* WHAT IT DOES: Swap two bytes in place. The paint shop uses it to reorder
 * pixel data. */
/* @implements 0x80252F50 tgr BrSwapBytes */
void BrSwapBytes(char *param_1,char *param_2)
{
  char uVar1;
  
  uVar1 = *param_1;
  *param_1 = *param_2;
  *param_2 = uVar1;
}

/* WHAT IT DOES: Draw the loaded tw by th texture as a w by h rectangle at
 * (x, y), upside down (t starts at the bottom row and steps back), in
 * 320-wide coordinates halved on a low-res screen; perspective correction
 * is off for the rectangle and back on after it.  The rectangle and its two
 * half-words are three statements on three lines: IDO's code for them
 * depends on the line breaks, and a one-line macro puts them out of order. */
/* @implements 0x80252F64 tgr BrTexRectFlipDraw */
void BrTexRectFlipDraw(int tw, int th, int x, int y, int w, int h)
{
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 12, 2, D_8028A898);
  gDPSetCombine(D_8028A858++, 0xffffff, 0xfffcf87c);
  gDPSetRenderMode(D_8028A858++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 19, 1, 0);
  if (D_8028A850 == 0) {
    x >>= 1;
    y >>= 1;
    w >>= 1;
    h >>= 1;
  }
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    _g->words.w0 = (_SHIFTL(G_TEXRECT, 24, 8) | _SHIFTL((x + w) << 2, 12, 12) | _SHIFTL((y + h) << 2, 0, 12));
    _g->words.w1 = (_SHIFTL(0, 24, 3) | _SHIFTL(x << 2, 12, 12) | _SHIFTL(y << 2, 0, 12));
  }
  gImmp1(D_8028A858++, G_RDPHALF_1, (_SHIFTL(0, 16, 16) | _SHIFTL(th << 5, 0, 16)));
  gImmp1(D_8028A858++, G_RDPHALF_2, (_SHIFTL((tw << 10) / w, 16, 16) | _SHIFTL(-(th << 10) / h, 0, 16)));
  gDPPipeSync(D_8028A858++);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 19, 1, 0x80000);
}


/* WHAT IT DOES: The paint shop's leave prompt: with nothing changed, fade
 * out at once; otherwise draw the "CHANGES NOT SAVED. EXIT ANYWAY?" box
 * with its A (yes) and B (no) buttons -- A fades out and leaves, B goes back
 * to the screen it came from. */
/* @implements 0x802531C0 tgr BrPaintExitPrompt */
void BrPaintExitPrompt(void)
{
  char unused[28];             /* declared, never used: the frame holds it */
  unsigned int *pad;
  int y;
  int bx;

  if (D_8028DBE8 == 0) {
    if (D_8028DBB4 <= 0) {
      D_8028DBE8 = 1;
      BrFadeTo(0, D_802AB260);
    } else {
      y = 285 - D_8028D0B0.w;
      bx = 361 - D_8028D0E0.w;
      BrBevelPanel(0xbf, 0xbb, 0x102, 0x6a, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextSetFont(12);
      BrTextAlignLeft();
      BrTextHighlightOff();
      BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xca, 0);
      BrTextPrint("CHANGES NOT SAVED.", 103, 109);
      BrTextPrint("EXIT ANYWAY?", 103, 123);
      BrTextSetFont(10);
      BrTextPrint("%wwYES", (D_8028D0B0.drawW + 249) >> 1, (y + 18) >> 1);
      BrTextPrint("%wwNO", (bx + D_8028D0E0.drawW + 8) >> 1, (y + 18) >> 1);
      BrImageDrawAt(&D_8028D0B0, 243, y);
      BrImageDrawAt(&D_8028D0E0, bx, y);
      pad = (unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c);
      if (*pad & 0x10) {
        BrPadConsume(pad, 0x10);
        D_8028DBE8 = 1;
        BrFadeTo(0, D_802AB264);
      } else if (*pad & 0x20) {
        BrPadConsume(pad, 0x20);
        D_8028DB60 = D_8028DB64;
      }
    }
  }
}

/* WHAT IT DOES: The paint shop's text keyboard: draw it with the text box
 * under it, and while the cursor is over the box, move the key cursor to
 * the key it touches (flashing it for 3 frames when A or Z picks it); a
 * picked key appends its glyph to the text (while the kerned width fits
 * the decal, up to 16), key 47 deletes the last one and key 49 closes the
 * keyboard (starting the text stamp when there is text).  The text is
 * printed centred in the box, in the chosen paint colour.
 * RESIDUE (205): saved-register choice in the key loop (the key pointer and
 * the constant 1 swap s3/s4, the text-length and width globals s0/s1) and
 * the pad address kept in a0 for the consume. */
/* @t4-pass 0x8024AC70 1 2026-10-03 compiles 31 best 205 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8024AC70 2 2026-10-03 compiles 30 best 205 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8024AC70 */
/* @implements 0x8024AC70 tgr BrPaintKeyboard */
void BrPaintKeyboard(void)
{
  int spare;                    /* declared, never used: the frame holds it */
  int x;
  int y;
  int i;
  BrGlyph *k;
  unsigned int n;
  int kern;

  D_8028DBD8 = 0;
  D_8028DBC8 = 0;
  BrImageDrawTinted(&D_8028D260, 0xe0, 0xe0, 0xe0);
  if (D_8028D290.flash != 0) {
    D_8028D290.flash = 0;
    if (D_8028DBB0 > 0) {
      D_8028DBB0--;
    }
    if (D_8028DBB0 == 0) {
      BrImageDrawTinted((BrImage *)&D_8028D290, 0x20, 200, 0xff);
    } else {
      BrImageDrawTinted((BrImage *)&D_8028D290, 0xff, 0xeb, 0);
    }
  }
  BrImageDraw(&D_8028D230);
  x = D_8028D230.x;
  y = D_8028D230.y + D_8028D230.h + 32;
  BrImageDrawRect(&D_8028D260, x, y, 200, 48, 0xe0, 0xe0, 0xe0);
  BrFillFrame(x, y, 200, 48, 2, 0xff, 0xff, 0xff);
  if (BrPaintCursorInBox((int *)&D_8028D260) != 0) {
    for (i = 0, k = D_8028D540; i < 50; i++, k++) {
      if (BrPaintCursorInBrush(&k->x0) != 0) {
        D_8028DBA4 = i;
        D_8028D290.flash = 1;
        D_8028D290.x = k->x0;
        D_8028D290.y = k->x4;
      }
      if (BrPaintCursorInBox((int *)&D_8028D290) != 0 && (PADS[D_8028DBBC].pressed & 0x8010)) {
        BrPadConsume((unsigned int *)&PADS[D_8028DBBC], 0x8010);
        D_8028DBB0 = 3;
        if (D_8028DBA4 == 49) {
          if (D_8028DBA8 > 0) {
            D_8028DBC0 = 1;
            D_8028DBE4 = 1;
          } else {
            D_8028DBC0 = 0;
            D_8028DBC4 = 0;
          }
          D_8028DBD8 = 1;
          D_8028DBC8 = 1;
        } else if (D_8028DBA4 == 47) {
          n = (unsigned char)(D_8028DBA8 - 1);
          if (D_80369E68[D_8028DBA8 - 1] == 0) {
            n = 0;
            D_8028DBA8 = 0;
            D_8028DBAC = 0;
          } else {
            D_8028DBA8 = n;
            if (n == 0) {
              D_8028DBAC -= D_80369E68[0]->w;
            } else {
              kern = D_80369E68[n - 1]->kernR < D_80369E68[n]->kernL ? D_80369E68[n - 1]->kernR : D_80369E68[n]->kernL;
              D_8028DBAC = D_8028DBAC - D_80369E68[n]->w + kern - 2;
            }
          }
          D_80369EA8[n] = 0;
          D_80369E68[n] = 0;
        } else {
          if (D_8028D540[D_8028DBA4].w + D_8028DBAC + 2 < D_8028DB88) {
            n = D_8028DBA8;
            D_80369EA8[n] = D_8028D540[D_8028DBA4].c;
            D_80369E68[n] = &D_8028D540[D_8028DBA4];
            if (n == 0) {
              D_8028DBAC = D_8028D540[D_8028DBA4].w + D_8028DBAC;
            } else {
              kern = D_80369E68[n - 1]->kernR < D_80369E68[n]->kernL ? D_80369E68[n - 1]->kernR : D_80369E68[n]->kernL;
              D_8028DBAC = D_8028D540[D_8028DBA4].w + D_8028DBAC - kern + 2;
            }
            D_80369EA8[(unsigned char)(n + 1)] = 0;
            D_8028DBA8 = n + 1;
            if ((unsigned char)(n + 1) > 16) {
              D_8028DBA8 = 16;
            }
          }
        }
      }
    }
  }
  if (D_8028DBA8 > 0) {
    BrTextSetColours(D_80369B98[D_8028DB58].r, D_80369B98[D_8028DB58].g, D_80369B98[D_8028DB58].b,
                     D_80369B98[D_8028DB58].r, D_80369B98[D_8028DB58].g, D_80369B98[D_8028DB58].b);
    BrTextSetFont(20);
    BrTextAlignCentre();
    BrTextHighlightOff();
    BrTextPrint(D_80369EA8, (x + 100) >> 1, (y + 36) >> 1);
  }
}

/* WHAT IT DOES: Move the paint shop cursor from the stick once it is pushed
 * past the dead zone, unless the cursor is locked. */
/* @t4-pass 0x8024BE78 1 2026-09-26 compiles 17 best 219 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8024BE78 2 2026-09-26 compiles 17 best 219 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8024BE78 3 2026-09-26 compiles 16 best 219 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8024BE78 */
/* @implements 0x8024BE78 tgr BrPaintStickMove */
void BrPaintStickMove(void)
{
  BrPadRec *pad;
  float a;

  if (D_8028DBE8 == 0) {
    pad = &PADS[D_8028DBBC];
    a = pad->axis[0] < 0.0 ? -pad->axis[0] : pad->axis[0];
    if (a >= D_802AB20C) {
      if (BrPaintCursorInRect((int *)&D_8028DB94) != 0 && D_8028DBC4 == 0) {
        pad = &PADS[D_8028DBBC];
        D_8028D110.x += (int)(pad->axis[0] * 6.0f);
      } else {
        pad = &PADS[D_8028DBBC];
        D_8028D110.x += (int)(pad->axis[0] * 12.0f);
      }
    } else if (pad->pressed & 0x201) {
      D_8028D110.x++;
    } else if (pad->pressed & 0x804) {
      D_8028D110.x--;
    }
    a = pad->axis[1] < 0.0 ? -pad->axis[1] : pad->axis[1];
    if (a >= D_802AB210) {
      if (BrPaintCursorInRect((int *)&D_8028DB94) != 0 && D_8028DBC4 == 0) {
        pad = &PADS[D_8028DBBC];
        D_8028D110.y -= (int)(pad->axis[1] * 6.0f);
      } else {
        pad = &PADS[D_8028DBBC];
        D_8028D110.y -= (int)(pad->axis[1] * 12.0f);
      }
    } else if (pad->pressed & 0x402) {
      D_8028D110.y++;
    } else if (pad->pressed & 0x108) {
      D_8028D110.y--;
    }
    if (D_8028D110.x >= 610) {
      D_8028D110.x = 609;
    } else if (D_8028D110.x < 31) {
      D_8028D110.x = 31;
    }
    if (D_8028D110.y < 22) {
      D_8028D110.y = 22;
    } else if (D_8028D110.y > 458) {
      D_8028D110.y = 458;
    }
  }
}

/* WHAT IT DOES: Tell whether the paint-shop cursor is inside a box whose
 * width may be negative (a mirrored decal): x from its left edge to left +
 * |width|, y from its top to top + height. */
/* @implements 0x8024D374 tgr BrPaintCursorInBox */
int BrPaintCursorInBox(BrPaintBox *b)
{
  if (b->x <= D_8028D110.x && D_8028D110.x <= b->x + (b->w > 0 ? b->w : -b->w) && b->y <= D_8028D110.y && D_8028D110.y <= b->y + b->h) {
    return 1;
  }
  return 0;
}

/* WHAT IT DOES: Tell whether the paint-shop cursor is inside the rectangle
 * {x, y, w, h}, edges included. */
/* @implements 0x8024D3F0 tgr BrPaintCursorInRect */
int BrPaintCursorInRect(int *r)
{
  if (r[0] <= D_8028D110.x && D_8028D110.x <= r[2] + r[0] && r[1] <= D_8028D110.y && D_8028D110.y <= r[3] + r[1]) {
    return 1;
  }
  return 0;
}

/* WHAT IT DOES: The same rectangle test as BrPaintCursorInRect (a separate
 * copy in the ROM). */
/* @implements 0x8024D45C tgr BrPaintCursorInRect2 */
int BrPaintCursorInRect2(int *r)
{
  if (r[0] <= D_8028D110.x && D_8028D110.x <= r[2] + r[0] && r[1] <= D_8028D110.y && D_8028D110.y <= r[3] + r[1]) {
    return 1;
  }
  return 0;
}

/* WHAT IT DOES: Tell whether the paint-shop cursor is inside a rectangle at
 * {x, y} the size of the current brush. */
/* @implements 0x8024D4C8 tgr BrPaintCursorInBrush */
int BrPaintCursorInBrush(int *r)
{
  if (r[0] <= D_8028D110.x && D_8028D110.x <= r[0] + D_8028D290.w && r[1] <= D_8028D110.y && D_8028D110.y <= r[1] + D_8028D290.h) {
    return 1;
  }
  return 0;
}

/* WHAT IT DOES: Copy the 16 decal palette colours (RGBA5551) into the
 * palette swatches as 8-bit r, g, b, each 5-bit channel widened by
 * repeating its top bits. */
/* @implements 0x8024D53C tgr BrPaintPaletteLoad */
void BrPaintPaletteLoad(void)
{
  int i;

  for (i = 0; i < 16; i++) {
    D_80369B98[i].r = ((D_8028DB90[i] >> 8) & 0xf8) | ((D_8028DB90[i] >> 13) & 7);
    D_80369B98[i].g = ((D_8028DB90[i] >> 3) & 0xf8) | ((D_8028DB90[i] >> 8) & 7);
    D_80369B98[i].b = ((D_8028DB90[i] << 2) & 0xf8) | ((D_8028DB90[i] >> 3) & 7);
  }
}

/* WHAT IT DOES: Draw the paint shop's palette: its two black panels, the
 * sixteen colour swatches, and a light blue frame round the chosen one. */
/* @implements 0x8024D6B8 tgr BrPaintPaletteDraw */
void BrPaintPaletteDraw(void)
{
  int i;

  BrFillRect(D_8028D480.x, D_8028D480.y, D_8028D480.w, D_8028D480.h, 0, 0, 0);
  BrFillRect(D_8028D490.x, D_8028D490.y, D_8028D490.w, D_8028D490.h, 0, 0, 0);
  for (i = 0; i < 16; i++) {
    BrFillRect(D_80369B98[i].x, D_80369B98[i].y, D_80369B98[i].w, D_80369B98[i].h, D_80369B98[i].r,
               D_80369B98[i].g, D_80369B98[i].b);
  }
  BrFillFrame(D_80369B98[D_8028DB58].x - 2, D_80369B98[D_8028DB58].y - 2, D_80369B98[D_8028DB58].w + 4,
              D_80369B98[D_8028DB58].h + 4, 2, 0x20, 200, 0xff);
}

/* WHAT IT DOES: Read the colour index of texel (x, y) of a 4-bit texture
 * laid out as the decal is (odd rows' 8-texel words swapped). */
/* @implements 0x8024D844 tgr BrPaintPeek */
unsigned char BrPaintPeek(unsigned char *tex, int x, int y)
{
  int o;
  unsigned char c;

  o = (((y & 1) << 3 ^ x) >> 1) + y * (D_8028DB88 >> 1);
  if (x & 1) {
    c = tex[o] & 0xf;
  } else {
    c = tex[o] >> 4;
  }
  return c;
}

/* WHAT IT DOES: The paint shop's brush-shape chooser: a box with the round
 * brush picture and a square, the chosen one outlined, and its name, with
 * A (select) and B (cancel) buttons; left or right swaps the choice, A
 * keeps it and B restores the previous one, both closing the box.  The
 * pad record is re-addressed at every test (no pad or pressed local: the
 * ROM's frame has no slots for them). */
/* @implements 0x8024D89C tgr BrPaintBrushSelect */
void BrPaintBrushSelect(void)
{
  char spare[28];               /* declared, never used: the frame holds it */
  int y;
  int bx;
  BrPaintNames2 names;

  names = D_8028DCE4;
  y = 322 - D_8028D0B0.w;
  bx = 349 - D_8028D0E0.w;
  BrBevelPanel(200, 0x96, 0xf0, 0xb4, 3, 0, 0, 0x80, 0x80, 0x80);
  if (D_8028DBC0 != 0) {
    D_8028DBB8 = D_8028CE9C;
    D_8028DBC0 = 0;
  }
  BrTextSetFont(15);
  BrTextAlignCentre();
  BrTextHighlightOff();
  BrTextPrint("%rySELECT STYLE", 159, 91);
  if (D_8028DBB8 == 0) {
    BrImageDrawRect(&D_8028D410, 250, 200, D_8028D410.drawW, D_8028D410.drawH, 0x20, 200, 0xff);
  } else {
    BrImageDrawRect(&D_8028D410, 250, 200, D_8028D410.drawW, D_8028D410.drawH, 0, 0, 0);
  }
  if (D_8028DBB8 == 1) {
    BrFillRect(D_8028D410.drawW + 276, 202, 38, 38, 0x20, 200, 0xff);
  } else {
    BrFillRect(D_8028D410.drawW + 276, 202, 38, 38, 0, 0, 0);
  }
  BrBevelPanel(0xe0, 0xff, 0xc0, 0x1e, 1, 1, 1, 0x80, 0x80, 0x80);
  BrTextSetFont(11);
  BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xf5, 0);
  BrTextPrint(names.n[D_8028DBB8], 159, 138);
  BrImageDrawAt(&D_8028D0B0, 216, y);
  BrImageDrawAt(&D_8028D0E0, bx, y);
  BrTextSetFont(10);
  BrTextAlignLeft();
  BrTextPrint("%wwSELECT", (unsigned int)(D_8028D0B0.w + 222) >> 1, (y + 18) >> 1);
  BrTextPrint("%wwCANCEL", (unsigned int)(bx + D_8028D0E0.w + 6) >> 1, (y + 18) >> 1);
  BrPadStickToButtons(&PADS[D_8028DBBC]);
  if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 5) {
    BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 5);
    D_8028DBB8 ^= 1;
  }
  if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 0x10) {
    BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 0x10);
    D_8028CE9C = D_8028DBB8;
    D_8028DBE0 = 0;
  } else if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 0x20) {
    BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 0x20);
    D_8028DBE0 = 0;
  }
}

/* WHAT IT DOES: Flood-fill the region of one colour under a screen point
 * with the chosen colour (Heckbert's scanline seed fill, a 400-segment
 * stack): the fill is painted into a scratch copy of the decal with the
 * mask off, then the whole copy is plotted back into the decal so the mask
 * applies.  Nothing when the region is already that colour.
 * RESIDUE (~260): the ROM keeps the stack count in its stack home and
 * recomputes each segment's address from it (dy and x1 homed too); ours
 * walks a pointer. */
/* @t4-pass 0x8024DCA0 1 2026-10-03 compiles 30 best 262 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8024DCA0 2 2026-10-03 compiles 30 best 262 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8024DCA0 */
/* @implements 0x8024DCA0 tgr BrPaintFloodFill */
void BrPaintFloodFill(int sx, int sy)
{
  unsigned char ov;
  int n;
  int x;
  int y;
  int l;
  int x1;
  int x2;
  int dy;
  unsigned char *decal;
  unsigned char *mask;
  BrFillSeg stack[400];

  x = (sx - D_8028DB94.x) >> 2;
  y = (D_8028DB94.y + D_8028DB94.h - sy) >> 2;
  ov = BrPaintGet(x, y);
  if (D_8028DB58 != ov) {
    stack[0].y = y;
    stack[0].xl = x;
    stack[0].xr = x;
    stack[0].dy = 1;
    stack[1].y = y + 1;
    stack[1].xl = x;
    stack[1].xr = x;
    stack[1].dy = -1;
    n = 2;
    mask = D_8028DB7C;
    D_8028DB7C = 0;
    memcpy(D_8028DB84, D_8028DB78, 0x800);
    decal = D_8028DB78;
    D_8028DB78 = D_8028DB84;
    do {
      n--;
      dy = stack[n].dy;
      y = stack[n].y + dy;
      x1 = stack[n].xl;
      x2 = stack[n].xr;
      for (x = x1; x >= 0 && y >= 0 && y < D_8028DB8C && BrPaintGet(x, y) == ov; x--) {
        BrPaintPlot(x, y, D_8028DB58);
      }
      if (x >= x1) {
        goto skip;
      }
      l = x + 1;
      if (l < x1) {
        stack[n].y = y;
        stack[n].xl = l;
        stack[n].xr = x1 - 1;
        stack[n].dy = -dy;
        n++;
      }
      x = x1 + 1;
      do {
        for (; x < D_8028DB88 && y >= 0 && y < D_8028DB8C && BrPaintGet(x, y) == ov; x++) {
          BrPaintPlot(x, y, D_8028DB58);
        }
        stack[n].y = y;
        stack[n].xl = l;
        stack[n].xr = x - 1;
        stack[n].dy = dy;
        n++;
        if (x > x2 + 1) {
          stack[n].y = y;
          stack[n].xl = x2 + 1;
          stack[n].xr = x - 1;
          stack[n].dy = -dy;
          n++;
        }
      skip:
        for (x++; x <= x2 && x < D_8028DB88 && y >= 0 && y < D_8028DB8C && BrPaintGet(x, y) != ov; x++) {
        }
        l = x;
      } while (x <= x2);
    } while (n > 0 && n < 400);
    D_8028DB7C = mask;
    for (y = 0; y < D_8028DB8C; y++) {
      for (x = 0; x < D_8028DB88; x++) {
        ov = BrPaintGet(x, y);
        D_8028DB78 = decal;
        BrPaintPlot(x, y, ov);
        D_8028DB78 = D_8028DB84;
      }
    }
    D_8028DB78 = decal;
  }
}


/* WHAT IT DOES: Stamp the typed text into the decal at a screen point (a
 * quarter scale, y flipped), centred on it: each glyph's full-bright texels
 * from the font image become texels of the chosen colour, with a shadow
 * texel one down and to the right in the shadow colour first when shadows
 * are on; glyphs advance by their width less the tighter of the two kerns,
 * plus 2. */
/* @implements 0x8024EC30 tgr BrPaintTextStamp */
void BrPaintTextStamp(int sx, int sy)
{
  int t0;                       /* declared, never used: the frame holds it */
  int u;
  int i;
  int k;
  int kern;
  int g;
  int row;
  int o;
  int u0;
  int u1;
  int x;
  int px;

  sx = ((sx - D_8028DB94.x) >> 2) + 1;
  sy = (D_8028DB94.y + D_8028DB94.h - sy) >> 2;
  x = sx - (D_8028DBAC >> 1);
  for (i = 0; i < D_8028DBA8; i++) {
    for (k = 0; k < 50; k++) {
      if (D_80369E68[i]->c == D_8028D540[k].c) {
        g = k;
        break;
      }
    }
    u0 = D_80369E68[i]->off + g * 16;
    u1 = D_80369E68[i]->w + u0;
    if (D_8028CF2C != 0) {
      for (row = 0; row < D_8028D2C0.h; row++) {
        for (u = u0; u <= u1; u++) {
          px = u - u0 + x;
          if (px >= 0 && px < D_8028DB88 && row + sy >= 0 && row + sy < D_8028DB8C) {
            o = (u >> 1) + row * ((unsigned int)D_8028D2C0.w >> 1);
            if (u & 1) {
              if ((D_8028D2C0.data[o] & 0xf) == 0xf) {
                BrPaintPlot(px + 1, row + sy - 1, D_8028DB5C);
              }
            } else if (D_8028D2C0.data[o] >> 4 == 0xf) {
              BrPaintPlot(px + 1, row + sy - 1, D_8028DB5C);
            }
          }
        }
      }
    }
    for (row = 0; row < D_8028D2C0.h; row++) {
      for (u = u0; u <= u1; u++) {
        px = u - u0 + x;
        if (px >= 0 && px < D_8028DB88 && row + sy >= 0 && row + sy < D_8028DB8C) {
          o = (u >> 1) + row * ((unsigned int)D_8028D2C0.w >> 1);
          if (u & 1) {
            if ((D_8028D2C0.data[o] & 0xf) == 0xf) {
              BrPaintPlot(px, row + sy, D_8028DB58);
            }
          } else if (D_8028D2C0.data[o] >> 4 == 0xf) {
            BrPaintPlot(px, row + sy, D_8028DB58);
          }
        }
      }
    }
    if (i < D_8028DBA8 - 1) {
      kern = D_80369E68[i]->kernR < D_80369E68[i + 1]->kernL ? D_80369E68[i]->kernR : D_80369E68[i + 1]->kernL;
      x += D_80369E68[i]->w - kern + 2;
    }
  }
}

/* WHAT IT DOES: The paint shop's decal-style chooser: a box with the four
 * style pictures (the chosen one outlined) and the chosen style's name,
 * A (select) and B (cancel) buttons under it; left and right move the
 * choice round, A keeps it and B restores the previous one, both closing
 * the box.  The four names are a local copy of the table at 0x8028DCF4.
 * The pictures' x is written out per call (i * 76 + 175), and the pad
 * record is re-addressed at every test with an unsigned (sizeof) multiply:
 * the ROM keeps 0x15C in a saved register for multu. */
/* @implements 0x8024F39C tgr BrPaintStyleSelect */
void BrPaintStyleSelect(void)
{
  char spare[24];               /* declared, never used: the frame holds it */
  int x;                        /* likewise */
  int i;
  int y;
  int bx;
  BrPaintNames names;

  names = D_8028DCF4;
  y = 323 - D_8028D0B0.w;
  bx = 353 - D_8028D0E0.w;
  BrBevelPanel(0x9f, 0x95, 0x142, 0xb6, 3, 0, 0, 0x80, 0x80, 0x80);
  BrTextSetFont(16);
  BrTextAlignCentre();
  BrTextHighlightOff();
  BrTextPrint("%rySELECT STYLE", 159, 92);
  if (D_8028DBC0 != 0) {
    D_8028DBB8 = D_8028CF5C;
    D_8028DBC0 = 0;
  }
  for (i = 0; i < 4; i++) {
    if (i == D_8028DBB8) {
      BrImageDrawRect(D_8028DB34[i], i * 76 + 175, 203, D_8028DB34[i]->drawW, D_8028DB34[i]->drawH, 0x20, 200, 0xff);
    } else {
      BrImageDrawAt(D_8028DB34[i], i * 76 + 175, 203);
    }
  }
  BrBevelPanel(0xb1, 0x103, 0x11e, 0x1e, 1, 1, 1, 0x80, 0x80, 0x80);
  BrTextSetFont(11);
  BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xf5, 0);
  BrTextPrint(names.n[D_8028DBB8], 159, 140);
  BrTextSetFont(10);
  BrTextAlignLeft();
  BrTextPrint("%wwSELECT", (unsigned int)(D_8028D0B0.w + 219) >> 1, (y + 18) >> 1);
  BrTextPrint("%wwCANCEL", (unsigned int)(bx + D_8028D0E0.w + 6) >> 1, (y + 18) >> 1);
  BrImageDrawAt(&D_8028D0B0, 213, y);
  BrImageDrawAt(&D_8028D0E0, bx, y);
  BrPadStickToButtons(&PADS[D_8028DBBC]);
  if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)) & 4) {
    BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)), 4);
    if (D_8028DBB8 == 0) {
      D_8028DBB8 = 3;
    } else {
      D_8028DBB8--;
    }
  } else if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)) & 1) {
    BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)), 1);
    if (D_8028DBB8 == 3) {
      D_8028DBB8 = 0;
    } else {
      D_8028DBB8++;
    }
  }
  if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)) & 0x10) {
    BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)), 0x10);
    D_8028CF5C = D_8028DBB8;
    D_8028DBE0 = 0;
  } else if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)) & 0x20) {
    BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)), 0x20);
    D_8028DBE0 = 0;
  }
}

/* WHAT IT DOES: Draw a line on the decal between two screen points (a
 * quarter scale, y flipped) by Bresenham's method, stepping along the
 * longer axis from the lower end: each point a texel in the chosen colour,
 * or a disc the brush's size.
 * RESIDUE (92): register priority -- the ROM keeps both steps in s6/s7 and
 * spills the loop end; ours spills the y step.  Hoisting x/y, loops on the
 * parameters, loop-test spellings, declaration order and 396 permuter
 * compiles leave it. */
/* @t4-pass 0x8024F000 1 2026-09-29 compiles 25 best 92 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8024F000 2 2026-09-29 compiles 25 best 92 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8024F000 */
/* @implements 0x8024F000 tgr BrPaintLine */
void BrPaintLine(int x0, int y0, int x1, int y1)
{
  int u0;                       /* u0-u3: declared, never used; the ROM's */
  int u1;                       /* frame has their four slots */
  int u2;
  int u3;
  int dx;
  int dy;
  int r;
  int x;
  int y;
  int stepx;
  int stepy;
  int e;
  int t;

  x0 = (x0 - D_8028DB94.x) >> 2;
  y0 = (D_8028DB94.y + D_8028DB94.h - y0) >> 2;
  x1 = (x1 - D_8028DB94.x) >> 2;
  y1 = (D_8028DB94.y + D_8028DB94.h - y1) >> 2;
  r = D_8028D4A0[D_8028DAC0].w >> 1;
  dx = x1 - x0 < 0 ? -(x1 - x0) : x1 - x0;
  dy = y1 - y0 < 0 ? -(y1 - y0) : y1 - y0;
  stepy = 1;
  if ((dy < dx && x1 < x0) || (dx < dy && y1 < y0)) {
    t = x0;
    x0 = x1;
    x1 = t;
    t = y0;
    y0 = y1;
    y1 = t;
  }
  stepx = 1;
  if (y1 < y0) {
    stepy = -1;
  }
  if (x1 < x0) {
    stepx = -1;
  }
  if (dy < dx) {
    x = x0;
    y = y0;
    e = 0;
    for (; x <= x1; x++) {
      if (e >= dx) {
        e -= dx;
        y += stepy;
      }
      if (D_8028DAC0 == 0) {
        BrPaintPlot(x, y, D_8028DB58);
      } else {
        BrPaintDisc(x, y, r, 0);
      }
      e += dy;
    }
  } else {
    x = x0;
    y = y0;
    e = 0;
    for (; y <= y1; y++) {
      if (e >= dy) {
        e -= dy;
        x += stepx;
      }
      if (D_8028DAC0 == 0) {
        BrPaintPlot(x, y, D_8028DB58);
      } else {
        BrPaintDisc(x, y, r, 0);
      }
      e += dx;
    }
  }
}

/* WHAT IT DOES: Plot one texel of colour c into the 4-bit decal texture at
 * (x, y) -- inside the texture and not masked off -- with the odd rows'
 * 8-texel words swapped as the RDP's TMEM layout wants them.
 * RESIDUE (51): temporaries are numbered one register later than the ROM's
 * from the row-width shift on; the instructions and their order match. */
/* @t4-pass 0x8024F25C 1 2026-09-29 compiles 26 best 51 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8024F25C 2 2026-09-29 compiles 26 best 51 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8024F25C */
/* @implements 0x8024F25C tgr BrPaintPlot */
void BrPaintPlot(int x, int y, unsigned char c)
{
  int o;

  if (x >= 0 && x < D_8028DB88 && y >= 0 && y < D_8028DB8C) {
    o = ((x ^ ((y & 1) << 3)) >> 1) + y * (D_8028DB88 >> 1);
    if (D_8028DB7C == 0) {
      if (x & 1) {
        D_8028DB78[o] = (D_8028DB78[o] & 0xf0) | c;
      } else {
        D_8028DB78[o] = (D_8028DB78[o] & 0xf) | (c << 4);
      }
    } else if (!(D_8028DB7C[(x >> 3) + y * ((D_8028DB88 + 7) >> 3)] & (1 << ((x ^ 7) & 7)))) {
      if (x & 1) {
        D_8028DB78[o] = (D_8028DB78[o] & 0xf0) | c;
      } else {
        D_8028DB78[o] = (D_8028DB78[o] & 0xf) | (c << 4);
      }
    }
  }
}

/* WHAT IT DOES: Fill the decal texels under a screen rectangle (either
 * corner order) with the chosen colour: screen to texel is a quarter, with
 * y turned upside down.  Each corner's two conversions share a source line
 * (IDO orders the copies into the corner registers by line). */
/* @implements 0x8024F7D4 tgr BrPaintFillRect */
void BrPaintFillRect(int sx0, int sy0, int sx1, int sy1)
{
  int x1;
  int x0;
  int y1;
  int x;
  int t;
  int y;

  x0 = (sx0 - D_8028DB94.x) >> 2; sy0 = (D_8028DB94.y + D_8028DB94.h - sy0) >> 2;
  x1 = (sx1 - D_8028DB94.x) >> 2; y1 = (D_8028DB94.y + D_8028DB94.h - sy1) >> 2;
  if (x1 < x0) {
    t = x0;
    x0 = x1;
    x1 = t;
  }
  if (y1 < sy0) {
    t = sy0;
    sy0 = y1;
    y1 = t;
  }
  for (y = sy0; y < y1; y++) {
    for (x = x0; x < x1; x++) {
      BrPaintPlot(x, y, D_8028DB58);
    }
  }
}

/* WHAT IT DOES: Draw a rectangle outline on the decal between two screen
 * corners (either order), in the chosen colour: one texel wide when the
 * brush is size 0 or 1, else a band as wide as the brush centred on each
 * edge. */
/* @implements 0x8024F8CC tgr BrPaintFrameRect */
void BrPaintFrameRect(int sx0, int sy0, int sx1, int sy1)
{
  int x1;
  int y1;
  int r;
  int x0;

  x0 = (sx0 - D_8028DB94.x) >> 2; sy0 = (D_8028DB94.y + D_8028DB94.h - sy0) >> 2;
  x1 = (sx1 - D_8028DB94.x) >> 2; y1 = (D_8028DB94.y + D_8028DB94.h - sy1) >> 2;
  if (x1 < x0) {
    sx1 = x0;
    x0 = x1;
    x1 = sx1;
  }
  if (y1 < sy0) {
    sy1 = sy0;
    sy0 = y1;
    y1 = sy1;
  }
  r = D_8028D4A0[D_8028DAC0].w >> 1;
  if (r == 0) {
    for (sy1 = sy0; sy1 < y1; sy1++) {
      BrPaintPlot(x0, sy1, D_8028DB58);
    }
    for (sx1 = x0; sx1 <= x1; sx1++) {
      BrPaintPlot(sx1, y1, D_8028DB58);
    }
    for (sy1 = sy0; sy1 < y1; sy1++) {
      BrPaintPlot(x1, sy1, D_8028DB58);
    }
    for (sx1 = x0; sx1 < x1; sx1++) {
      BrPaintPlot(sx1, sy0, D_8028DB58);
    }
  } else {
    for (sy1 = sy0 - r; sy1 < y1 + r; sy1++) {
      for (sx1 = x0 - r; sx1 < x0 + r; sx1++) {
        BrPaintPlot(sx1, sy1, D_8028DB58);
      }
    }
    for (sy1 = y1 - r; sy1 < y1 + r; sy1++) {
      for (sx1 = x0; sx1 < x1; sx1++) {
        BrPaintPlot(sx1, sy1, D_8028DB58);
      }
    }
    for (sy1 = sy0 - r; sy1 < y1 + r; sy1++) {
      for (sx1 = x1 - r; sx1 < x1 + r; sx1++) {
        BrPaintPlot(sx1, sy1, D_8028DB58);
      }
    }
    for (sy1 = sy0 - r; sy1 < sy0 + r; sy1++) {
      for (sx1 = x0; sx1 < x1; sx1++) {
        BrPaintPlot(sx1, sy1, D_8028DB58);
      }
    }
  }
}

/* WHAT IT DOES: Fill a rounded rectangle on the decal between two screen
 * corners (either order) in the chosen colour: the corner radius is a
 * quarter of the shorter side; the middle band is filled row by row, then a
 * midpoint circle walk at twice the resolution adds, on every other step,
 * the two spans above the band and the two below it.  The corners are
 * worked in the parameters, inset by the radius after the band.
 * RESIDUE (122): the ROM keeps the half steps in fp/s6 and the right edge in
 * s7; ours homes the half steps on the stack, which moves the span bounds'
 * registers. */
/* @t4-pass 0x8024FBC8 1 2026-10-03 compiles 30 best 122 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8024FBC8 2 2026-10-03 compiles 30 best 122 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8024FBC8 */
/* @implements 0x8024FBC8 tgr BrPaintFillRoundRect */
void BrPaintFillRoundRect(int sx0, int sy0, int sx1, int sy1)
{
  int x1;
  int t;
  int r;
  int y;
  int x;
  int a;
  int b;
  int ha;
  int hb;
  int err;

  sx0 = (sx0 - D_8028DB94.x) >> 2; sy0 = (D_8028DB94.y + D_8028DB94.h - sy0) >> 2;
  x1 = (sx1 - D_8028DB94.x) >> 2; sy1 = (D_8028DB94.y + D_8028DB94.h - sy1) >> 2;
  if (x1 < sx0) {
    t = sx0;
    sx0 = x1;
    x1 = t;
  }
  if (sy1 < sy0) {
    t = sy0;
    sy0 = sy1;
    sy1 = t;
  }
  r = (x1 - sx0 < sy1 - sy0 ? x1 - sx0 : sy1 - sy0) >> 2;
  for (y = sy0 + r; y < sy1 - r; y++) {
    for (x = sx0; x < x1; x++) {
      BrPaintPlot(x, y, D_8028DB58);
    }
  }
  sx0 += r;
  sy0 += r;
  x1 -= r;
  sy1 -= r;
  b = r * 2;
  err = -b;
  for (a = 0; a <= b; a++) {
    if (!(a & 1)) {
      ha = a >> 1;
      hb = b >> 1;
      for (x = sx0 - ha; x < ha + x1; x++) {
        BrPaintPlot(x, sy0 - hb, D_8028DB58);
      }
      for (x = sx0 - hb; x < hb + x1; x++) {
        BrPaintPlot(x, sy0 - ha, D_8028DB58);
      }
      for (x = sx0 - ha; x < ha + x1; x++) {
        BrPaintPlot(x, sy1 + hb, D_8028DB58);
      }
      for (x = sx0 - hb; x < hb + x1; x++) {
        BrPaintPlot(x, sy1 + ha, D_8028DB58);
      }
    }
    err += a;
    if (err >= 0) {
      err -= b;
      b--;
    }
  }
}

/* WHAT IT DOES: Draw a rounded rectangle outline on the decal between two
 * screen corners (either order) in the chosen colour: the corner radius is
 * a quarter of the shorter side plus a quarter of the brush; the straight
 * edges are one texel wide for a brush under 2, else bands as wide as the
 * brush centred on them, and the corners are a midpoint circle walk at
 * twice the resolution, each octant point a run as long as the brush (a
 * texel inward for the larger brushes).  The corners are worked in the
 * parameters, inset by the radius after the edges.
 * RESIDUE (496): the ROM holds the radius twice in its frame and keeps the
 * corners in their stack homes; ours keeps more in saved registers, which
 * reorders the edge loops' setup. */
/* @t4-pass 0x8024FEB8 1 2026-10-03 compiles 31 best 494 moved 2  (n64/tools/n64permute.py) */
/* @t4-pass 0x8024FEB8 2 2026-10-03 compiles 31 best 494 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8024FEB8 */
/* @implements 0x8024FEB8 tgr BrPaintFrameRoundRect */
void BrPaintFrameRoundRect(int sx0, int sy0, int sx1, int sy1)
{
  int x1;
  int t;
  int bw;
  int hw;
  int q;
  int r;
  int x;
  int y;
  int a;
  int b;
  int err;
  int ha;
  int hb;
  int k;

  sx0 = (sx0 - D_8028DB94.x) >> 2;
  sy0 = (D_8028DB94.y + D_8028DB94.h - sy0) >> 2;
  x1 = (sx1 - D_8028DB94.x) >> 2;
  sy1 = (D_8028DB94.y + D_8028DB94.h - sy1) >> 2;
  if (x1 < sx0) {
    t = sx0;
    sx0 = x1;
    x1 = t;
  }
  if (sy1 < sy0) {
    t = sy0;
    sy0 = sy1;
    sy1 = t;
  }
  bw = D_8028D4A0[D_8028DAC0].w;
  r = sy1 - sy0;
  hw = bw >> 1;
  if (x1 - sx0 < r) {
    r = x1 - sx0;
  }
  q = hw >> 1;
  r = (r >> 2) + q;
  if (hw == 0) {
    for (y = sy0 + r; y < sy1 - r; y++) {
      BrPaintPlot(sx0, y, D_8028DB58);
    }
    for (y = sy0 + r; y < sy1 - r; y++) {
      BrPaintPlot(x1, y, D_8028DB58);
    }
    for (x = sx0 + r; x < x1 - r; x++) {
      BrPaintPlot(x, sy1, D_8028DB58);
    }
    for (x = sx0 + r; x < x1 - r; x++) {
      BrPaintPlot(x, sy0, D_8028DB58);
    }
  } else {
    for (y = sy0 + r; y <= sy1 - r; y++) {
      for (x = sx0 - hw; x < sx0 + hw; x++) {
        BrPaintPlot(x, y, D_8028DB58);
      }
    }
    for (y = sy0 + r; y <= sy1 - r; y++) {
      for (x = x1 - hw; x < x1 + hw; x++) {
        BrPaintPlot(x, y, D_8028DB58);
      }
    }
    for (y = sy1 - hw; y < sy1 + hw; y++) {
      for (x = sx0 + r; x <= x1 - r; x++) {
        BrPaintPlot(x, y, D_8028DB58);
      }
    }
    for (y = sy0 - hw; y < sy0 + hw; y++) {
      for (x = sx0 + r; x <= x1 - r; x++) {
        BrPaintPlot(x, y, D_8028DB58);
      }
    }
  }
  sx0 += r;
  x1 -= r;
  sy0 += r;
  sy1 -= r;
  if (D_8028DAC0 > 2) {
    sx0--;
    sy0--;
  }
  r += q;
  b = r * 2;
  err = -b;
  for (a = 0; a <= b;) {
    if (!(a & 1)) {
      ha = a >> 1;
      hb = b >> 1;
      if (hw == 0) {
        BrPaintPlot(sx0 - ha, sy0 - hb, D_8028DB58);
      } else {
        for (k = 0; k < hw * 2; k++) {
          BrPaintPlot(sx0 - ha, sy0 - hb + k, D_8028DB58);
        }
      }
      if (hw == 0) {
        BrPaintPlot(sx0 - ha, hb + sy1, D_8028DB58);
      } else {
        for (k = 0; k < hw * 2; k++) {
          BrPaintPlot(sx0 - ha, hb + sy1 - k, D_8028DB58);
        }
      }
      if (hw == 0) {
        BrPaintPlot(ha + x1, hb + sy1, D_8028DB58);
      } else {
        for (k = 0; k < hw * 2; k++) {
          BrPaintPlot(ha + x1, hb + sy1 - k, D_8028DB58);
        }
      }
      if (hw == 0) {
        BrPaintPlot(ha + x1, sy0 - hb, D_8028DB58);
      } else {
        for (k = 0; k < hw * 2; k++) {
          BrPaintPlot(ha + x1, sy0 - hb + k, D_8028DB58);
        }
      }
      if (hw == 0) {
        BrPaintPlot(hb + x1, ha + sy1, D_8028DB58);
      } else {
        for (k = 0; k < hw * 2; k++) {
          BrPaintPlot(hb + x1 - k, ha + sy1, D_8028DB58);
        }
      }
      if (hw == 0) {
        BrPaintPlot(hb + x1, sy0 - ha, D_8028DB58);
      } else {
        for (k = 0; k < hw * 2; k++) {
          BrPaintPlot(hb + x1 - k, sy0 - ha, D_8028DB58);
        }
      }
      if (hw == 0) {
        BrPaintPlot(sx0 - hb, sy0 - ha, D_8028DB58);
      } else {
        for (k = 0; k < hw * 2; k++) {
          BrPaintPlot(sx0 - hb + k, sy0 - ha, D_8028DB58);
        }
      }
      if (hw == 0) {
        BrPaintPlot(sx0 - hb, ha + sy1, D_8028DB58);
      } else {
        for (k = 0; k < hw * 2; k++) {
          BrPaintPlot(sx0 - hb + k, ha + sy1, D_8028DB58);
        }
      }
    }
    err += a;
    a++;
    if (err >= 0) {
      err -= b;
      b--;
    }
  }
}

/* WHAT IT DOES: The paint shop's oval-style chooser, as the decal-style one:
 * a box with the four pictures (placed once when the box opens: the first
 * two 72 apart, the last two each after its partner's right edge, 52 apart)
 * and the chosen style's name, A (select) and B (cancel) under it; left and
 * right move the choice round, A keeps it and B restores the previous one,
 * both closing the box.  The pad record is re-addressed at every test
 * (no pad or pressed local), as the ROM rebuilds its address each time. */
/* @implements 0x80250698 tgr BrPaintOvalStyleSelect */
void BrPaintOvalStyleSelect(void)
{
  char spare[24];               /* declared, never used: the frame holds it */
  int x;
  int i;
  int y;
  int bx;
  BrPaintNames names;

  names = D_8028DD04;
  y = 323 - D_8028D0B0.w;
  bx = 352 - D_8028D0E0.w;
  BrBevelPanel(0xb8, 0x95, 0x110, 0xb6, 3, 0, 0, 0x80, 0x80, 0x80);
  BrTextSetFont(16);
  BrTextAlignCentre();
  BrTextHighlightOff();
  BrTextPrint("%rySELECT STYLE", 160, 91);
  if (D_8028DBC0 != 0) {
    D_8028DBB8 = D_8028CF8C;
    for (i = 0; i * 72 + 200 < 344; i++) {
      D_8028DB44[i]->x = i * 72 + 200;
      D_8028DB44[i]->y = 203;
    }
    for (i = 0; i < 2; i++) {
      D_8028DB44[i + 2]->x = D_8028DB44[1]->x + D_8028DB44[i]->drawW + i * 52;
      D_8028DB44[i + 2]->y = 203;
    }
    D_8028DBC0 = 0;
  }
  for (i = 0; i < 4; i++) {
    if (i == D_8028DBB8) {
      BrImageDrawRect(D_8028DB44[i], D_8028DB44[i]->x, D_8028DB44[i]->y, D_8028DB44[i]->drawW,
                      D_8028DB44[i]->drawH, 0x20, 200, 0xff);
    } else {
      BrImageDraw(D_8028DB44[i]);
    }
  }
  BrBevelPanel(0xce, 0x101, 0xe4, 0x1e, 1, 1, 1, 0x80, 0x80, 0x80);
  BrTextSetFont(11);
  BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xf5, 0);
  BrTextPrint(names.n[D_8028DBB8], 159, 139);
  BrTextSetFont(10);
  BrTextAlignLeft();
  BrTextPrint("%wwSELECT", (unsigned int)(D_8028D0B0.w + 220) >> 1, (y + 18) >> 1);
  BrTextPrint("%wwCANCEL", (unsigned int)(bx + D_8028D0E0.w + 6) >> 1, (y + 18) >> 1);
  BrImageDrawAt(&D_8028D0B0, 214, y);
  BrImageDrawAt(&D_8028D0E0, bx, y);
  BrPadStickToButtons(&PADS[D_8028DBBC]);
  if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 4) {
    BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 4);
    if (D_8028DBB8 == 0) {
      D_8028DBB8 = 3;
    } else {
      D_8028DBB8--;
    }
  } else if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 1) {
    BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 1);
    if (D_8028DBB8 == 3) {
      D_8028DBB8 = 0;
    } else {
      D_8028DBB8++;
    }
  }
  if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 0x10) {
    BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 0x10);
    D_8028CF8C = D_8028DBB8;
    D_8028DBE0 = 0;
  } else if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 0x20) {
    BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 0x20);
    D_8028DBE0 = 0;
  }
}

/* WHAT IT DOES: Fill an oval on the decal inside two screen corners (either
 * order) in the chosen colour: a midpoint ellipse walk at twice the
 * resolution, radii half the sides (at least 1), and on every other step a
 * span above and below the centre -- first the region where the walk steps
 * across, then the one where it steps up.  rx*rx and ry*ry are each held
 * twice (a2/a2b, b2/b2b), as the ROM's frame does.
 * RESIDUE (250): register priority -- the ROM keeps the corners, the radii
 * and the walk's running sums in their stack homes; ours holds more of them
 * in saved registers. */
/* @t4-pass 0x80250B58 1 2026-10-03 compiles 28 best 250 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80250B58 2 2026-10-03 compiles 31 best 250 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x80250B58 */
/* @implements 0x80250B58 tgr BrPaintFillOval */
void BrPaintFillOval(int sx0, int sy0, int sx1, int sy1)
{
  int ry;
  int t;
  int rx;
  int x;
  int y;
  int err;
  int cx;
  int xs;
  int xe;
  int i;
  int b2b;
  int a2b;
  int b2;
  int a2;
  int ey;
  int ex;
  int d;

  sx0 = (sx0 - D_8028DB94.x) >> 2;
  sx1 = (sx1 - D_8028DB94.x) >> 2;
  sy0 = (D_8028DB94.y + D_8028DB94.h - sy0) >> 2;
  sy1 = (D_8028DB94.y + D_8028DB94.h - sy1) >> 2;
  if (sx1 < sx0) {
    t = sx0;
    sx0 = sx1;
    sx1 = t;
  }
  if (sy1 < sy0) {
    t = sy0;
    sy0 = sy1;
    sy1 = t;
  }
  ry = (sy1 - sy0) >> 1 < 2 ? 1 : (sy1 - sy0) >> 1;
  x = 0;
  rx = (sx1 - sx0) >> 1 < 2 ? 1 : (sx1 - sx0) >> 1;
  y = ry * 2;
  b2 = b2b = ry * ry;
  a2 = a2b = rx * rx;
  err = -a2 * y;
  for (ex = a2b * y, ey = 0, d = 0; ey <= ex;) {
    if (!(x & 1)) {
      cx = (sx0 + sx1) >> 1;
      xe = (x >> 1) + cx;
      xs = cx - (x >> 1);
      for (i = xs; i < xe; i++) {
        BrPaintPlot(i, (y >> 1) + ((sy0 + sy1) >> 1), D_8028DB58);
      }
      for (i = xs; i < xe; i++) {
        BrPaintPlot(i, ((sy0 + sy1) >> 1) - (y >> 1), D_8028DB58);
      }
    }
    err += d;
    ey += b2b;
    x++;
    d += ry * ry;
    err += d;
    if (err > 0) {
      err -= a2 * y;
      y--;
      ex -= a2b;
      err -= a2 * y;
    }
  }
  x = rx * 2;
  y = 0;
  err = -b2 * x;
  for (ey = b2b * x, ex = 0, d = 0; ex <= ey;) {
    if (!(y & 1)) {
      cx = (sx0 + sx1) >> 1;
      xe = (x >> 1) + cx;
      xs = cx - (x >> 1);
      for (i = xs; i < xe; i++) {
        BrPaintPlot(i, (y >> 1) + ((sy0 + sy1) >> 1), D_8028DB58);
      }
      for (i = xs; i < xe; i++) {
        BrPaintPlot(i, ((sy0 + sy1) >> 1) - (y >> 1), D_8028DB58);
      }
    }
    err += d;
    y++;
    ex += a2b;
    d = d + rx * rx;
    err += d;
    if (err > 0) {
      err -= b2 * x;
      x--;
      ey -= b2b;
      err -= b2 * x;
    }
  }
}

/* WHAT IT DOES: Draw an oval outline on the decal inside two screen corners
 * (either order) in the chosen colour: the midpoint ellipse walk of
 * BrPaintFillOval with the radii grown by half the brush, and on every
 * other step each quadrant's point drawn as a run of texels the brush's
 * size inward (up/down in the first region, across in the second).
 * RESIDUE (371): the ROM's frame is 8 smaller and keeps the corners and
 * radii in their stack homes; ours holds them in saved registers first. */
/* @t4-pass 0x80250FCC 1 2026-10-03 compiles 31 best 371 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80250FCC 2 2026-10-03 compiles 31 best 371 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x80250FCC */
/* @implements 0x80250FCC tgr BrPaintFrameOval */
void BrPaintFrameOval(int sx0, int sy0, int sx1, int sy1)
{
  int ry;
  int rx;
  int t;
  int dx;
  int dy;
  int w;
  int x;
  int y;
  int err;
  int k;
  int b2b;
  int a2b;
  int b2;
  int a2;
  int ey;
  int ex;
  int d;

  sx0 = (sx0 - D_8028DB94.x) >> 2;
  sy0 = (D_8028DB94.y + D_8028DB94.h - sy0) >> 2;
  sx1 = (sx1 - D_8028DB94.x) >> 2;
  sy1 = (D_8028DB94.y + D_8028DB94.h - sy1) >> 2;
  dx = sx1 - sx0;
  if (dx < 0) {
    dx = sx0 - sx1;
    t = sx0;
    sx0 = sx1;
    sx1 = t;
  }
  dy = sy1 - sy0;
  if (dy < 0) {
    dy = sy0 - sy1;
    t = sy0;
    sy0 = sy1;
    sy1 = t;
  }
  w = D_8028D4A0[D_8028DAC0].w;
  ry = (dy + w) >> 1 < 2 ? 1 : (dy + w) >> 1;
  rx = (dx + w) >> 1 < 2 ? 1 : (dx + w) >> 1;
  x = 0;
  y = ry * 2;
  b2 = b2b = ry * ry;
  a2 = a2b = rx * rx;
  err = -a2 * y;
  for (ex = a2b * y, ey = 0, d = 0; ey <= ex;) {
    if (!(x & 1)) {
      for (k = 0; k < w; k++) {
        BrPaintPlot((x >> 1) + ((sx0 + sx1) >> 1), (y >> 1) + ((sy0 + sy1) >> 1) - k, D_8028DB58);
      }
      for (k = 0; k < w; k++) {
        BrPaintPlot((x >> 1) + ((sx0 + sx1) >> 1), ((sy0 + sy1) >> 1) - (y >> 1) + k, D_8028DB58);
      }
      for (k = 0; k < w; k++) {
        BrPaintPlot(((sx0 + sx1) >> 1) - (x >> 1), ((sy0 + sy1) >> 1) - (y >> 1) + k, D_8028DB58);
      }
      for (k = 0; k < w; k++) {
        BrPaintPlot(((sx0 + sx1) >> 1) - (x >> 1), (y >> 1) + ((sy0 + sy1) >> 1) - k, D_8028DB58);
      }
    }
    err += d;
    ey += b2b;
    x++;
    d += ry * ry;
    err += d;
    if (err > 0) {
      err -= a2 * y;
      y--;
      ex -= a2b;
      err -= a2 * y;
    }
  }
  x = rx * 2;
  y = 0;
  err = -b2 * x;
  for (ey = b2b * x, ex = 0, d = 0; ex <= ey;) {
    if (!(y & 1)) {
      for (k = 0; k < w; k++) {
        BrPaintPlot((x >> 1) + ((sx0 + sx1) >> 1) - k, (y >> 1) + ((sy0 + sy1) >> 1), D_8028DB58);
      }
      for (k = 0; k < w; k++) {
        BrPaintPlot((x >> 1) + ((sx0 + sx1) >> 1) - k, ((sy0 + sy1) >> 1) - (y >> 1), D_8028DB58);
      }
      for (k = 0; k < w; k++) {
        BrPaintPlot(((sx0 + sx1) >> 1) - (x >> 1) + k, ((sy0 + sy1) >> 1) - (y >> 1), D_8028DB58);
      }
      for (k = 0; k < w; k++) {
        BrPaintPlot(((sx0 + sx1) >> 1) - (x >> 1) + k, (y >> 1) + ((sy0 + sy1) >> 1), D_8028DB58);
      }
    }
    err += d;
    y++;
    ex += a2b;
    d += rx * rx;
    err += d;
    if (err > 0) {
      err -= b2 * x;
      x--;
      ey -= b2b;
      err -= b2 * x;
    }
  }
}



/* WHAT IT DOES: Paint a filled disc of radius r in the chosen colour
 * centred on (x, y) -- in texels, or in screen pixels over the paint area
 * when asked (a quarter scale, y flipped) -- as horizontal spans from a
 * midpoint circle walk.
 * RESIDUE (131): the ROM keeps x and the walk state in stack homes and
 * reuses the span bounds; ours holds x in a saved register. */
/* @t4-pass 0x8025159C 1 2026-10-03 compiles 31 best 131 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8025159C 2 2026-10-03 compiles 30 best 131 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8025159C */
/* @implements 0x8025159C tgr BrPaintDisc */
void BrPaintDisc(int x, int y, int r, unsigned char screen)
{
  int i;
  int ha;
  int hb;
  int j;
  int a;
  int b;
  int err;

  if (screen) {
    x = (x - D_8028DB94.x) >> 2;
    y = (D_8028DB94.y + D_8028DB94.h - y) >> 2;
  }
  b = r * 2;
  err = -b;
  for (a = 0; a <= b; a++) {
    if (!(a & 1)) {
      ha = a >> 1;
      hb = b >> 1;
      for (i = x - ha; i <= x + ha; i++) {
        BrPaintPlot(i, y + hb, D_8028DB58);
      }
      for (i = x - hb; i <= x + hb; i++) {
        BrPaintPlot(i, y + ha, D_8028DB58);
      }
      for (i = x - ha; i <= x + ha; i++) {
        BrPaintPlot(i, y - hb, D_8028DB58);
      }
      for (i = x - hb; i <= x + hb; i++) {
        BrPaintPlot(i, y - ha, D_8028DB58);
      }
    }
    err += a;
    if (err >= 0) {
      err -= b;
      b--;
    }
  }
}


/* WHAT IT DOES: Draw a circle outline of radius r (plus half the brush)
 * on the decal around a screen point (a quarter scale, y flipped): a
 * midpoint circle walk at twice the resolution, and on every other step
 * each of the eight octant points drawn as a run of texels the brush's size
 * inward.  The centre's two conversions share a source line (IDO orders
 * the copies into their registers by line). */
/* @implements 0x802517B4 tgr BrPaintCircle */
void BrPaintCircle(int sx, int sy, int r)
{
  int u0;                      /* declared, never used: the frame holds it */
  int w;
  int cx;
  int cy;
  int y;
  int x;
  int d;
  int k;

  cx = (sx - D_8028DB94.x) >> 2; cy = (D_8028DB94.y + D_8028DB94.h - sy) >> 2;
  w = D_8028D4A0[D_8028DAC0].w;
  r += w >> 1;
  y = r * 2;
  x = 0;
  d = -y;
  while (x <= y) {
    if ((x & 1) == 0) {
      for (k = 0; k < w; k++) {
        BrPaintPlot((x >> 1) + cx, (y >> 1) + cy - k, D_8028DB58);
      }
      for (k = 0; k < w; k++) {
        BrPaintPlot((x >> 1) + cx, cy - (y >> 1) + k, D_8028DB58);
      }
      for (k = 0; k < w; k++) {
        BrPaintPlot(cx - (x >> 1), cy - (y >> 1) + k, D_8028DB58);
      }
      for (k = 0; k < w; k++) {
        BrPaintPlot(cx - (x >> 1), (y >> 1) + cy - k, D_8028DB58);
      }
      for (k = 0; k < w; k++) {
        BrPaintPlot((y >> 1) + cx - k, (x >> 1) + cy, D_8028DB58);
      }
      for (k = 0; k < w; k++) {
        BrPaintPlot((y >> 1) + cx - k, cy - (x >> 1), D_8028DB58);
      }
      for (k = 0; k < w; k++) {
        BrPaintPlot(cx - (y >> 1) + k, cy - (x >> 1), D_8028DB58);
      }
      for (k = 0; k < w; k++) {
        BrPaintPlot(cx - (y >> 1) + k, (x >> 1) + cy, D_8028DB58);
      }
    }
    d += x;
    x++;
    if (d >= 0) {
      d -= y;
      y--;
    }
  }
}

/* WHAT IT DOES: Place the paint-shop car and draw it: while a view change
 * is under way, ease the car's facing and up vectors over 16 frames from
 * the previous preset view to the new one (sidestepping through a
 * perpendicular when they are nearly opposite; view 5 tilts the car),
 * otherwise face the chosen preset; rebuild the body matrix, set the
 * camera and a viewport placed by car kind, and draw the car.  The x1/x2/x3
 * arrays are unused locals that reproduce the ROM's frame (0xD8); `/ 16` is
 * an integer so IDO keeps the divide.
 * The car pointer goes into D_8028AAF0 first and the camera pointer is
 * chained off it; the preset copy goes through a source and a destination
 * pointer (both one load each, as in the ROM).
 * RESIDUE (127, same length): the vector copies and interpolation are
 * scheduled differently (the view-counter store, float registers). */
/* @t4-pass 0x80242BDC 1 2026-10-03 compiles 30 best 125 moved 2  (n64/tools/n64permute.py) */
/* @t4-pass 0x80242BDC 2 2026-10-03 compiles 29 best 125 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x80242BDC */
/* @implements 0x80242BDC tgr BrPaintCarView */
void BrPaintCarView(void)
{
  BrVec3 from;
  BrVec3 to;
  float dx;
  float dy;
  float dz;
  float x1[4];
  BrVec3 side;
  BrVec3 zAxis;
  float x2[3];

  D_8028AAF0 = &D_8031B760[D_8028DBBC];
  D_8028AAF4 = D_8028AAF0->cam = &D_8028AAF0->cams[3];
  if (D_8028DBD0 != 0) {
    from.x = D_8028DC08[D_8028DB6C].x;
    from.y = D_8028DC08[D_8028DB6C].y;
    from.z = D_8028DC08[D_8028DB6C].z;
    to.x = D_8028DC08[D_8028DB68].x;
    to.y = D_8028DC08[D_8028DB68].y;
    to.z = D_8028DC08[D_8028DB68].z;
    D_8028DBB0++;
    BrVec3Normalise(&from);
    BrVec3Normalise(&to);
    if (BrVec3Dot(&to, &from) < -0.9) {
      zAxis.x = 0.0f;
      zAxis.y = 0.0f;
      zAxis.z = 1.0f;
      BrVec3Cross(&side, &zAxis, &from);
      if (D_8028DBB0 < 8) {
        BrVec3AddTo(&to, &side);
      } else {
        BrVec3AddTo(&from, &side);
      }
    }
    dx = (to.x - from.x) / 16;
    dz = (to.z - from.z) / 16;
    dy = (to.y - from.y) / 16;
    ((BrVec3 *)D_8028AAF0->mtx0[0])->x = from.x + D_8028DBB0 * dx;
    ((BrVec3 *)D_8028AAF0->mtx0[0])->y = from.y + D_8028DBB0 * dy;
    ((BrVec3 *)D_8028AAF0->mtx0[0])->z = from.z + D_8028DBB0 * dz;
    BrVec3Normalise((BrVec3 *)D_8028AAF0->mtx0[0]);
    {
      BrVec3 upB = {0.0f, 0.0f, 0.0f};
      BrVec3 upA = {0.0f, 0.0f, 0.0f};

      if (D_8028DB68 == 5) {
        upA.x = 5.0f;
        upA.y = 5.0f;
        upA.z = 1.5f;
      } else {
        upA.z = 1.0f;
        upA.x = 0.0f;
        upA.y = 0.0f;
      }
      if (D_8028DB6C == 5) {
        upB.x = 5.0f;
        upB.y = 5.0f;
        upB.z = 1.5f;
      } else {
        upB.x = 0.0f;
        upB.y = 0.0f;
        upB.z = 1.0f;
      }
      BrVec3Normalise(&upB);
      BrVec3Normalise(&upA);
      dx = (upA.x - upB.x) / 16;
      dy = (upA.y - upB.y) / 16;
      dz = (upA.z - upB.z) / 16;
      ((BrVec3 *)D_8028AAF0->mtx0[2])->x = upB.x + D_8028DBB0 * dx;
      ((BrVec3 *)D_8028AAF0->mtx0[2])->y = upB.y + D_8028DBB0 * dy;
      ((BrVec3 *)D_8028AAF0->mtx0[2])->z = upB.z + D_8028DBB0 * dz;
    }
    BrVec3Normalise((BrVec3 *)D_8028AAF0->mtx0[2]);
    if (D_8028DBB0 == 16) {
      D_8028DBB0 = 0;
      D_8028DBD0 = 0;
    }
  } else {
    float x3[12];
    BrVec3 *s;
    BrVec3 *d;

    d = (BrVec3 *)D_8028AAF0->mtx0[0];
    s = &D_8028DC08[D_8028DB68];
    d->x = s->x;
    d->y = s->y;
    d->z = s->z;
    BrVec3Normalise((BrVec3 *)D_8028AAF0->mtx0[0]);
  }
  BrVec3Cross((BrVec3 *)D_8028AAF0->mtx0[1], (BrVec3 *)D_8028AAF0->mtx0[2], (BrVec3 *)D_8028AAF0->mtx0[0]);
  BrVec3Normalise((BrVec3 *)D_8028AAF0->mtx0[1]);
  BrVec3Cross((BrVec3 *)D_8028AAF0->mtx0[2], (BrVec3 *)D_8028AAF0->mtx0[0], (BrVec3 *)D_8028AAF0->mtx0[1]);
  D_8028AAF0->heading = BrAtan2(D_8028AAF4->mtx[0][0], D_8028AAF4->mtx[0][1]);
  D_8028AAF0->heading = BrAtan2(D_8028AAF4->mtx[0][0], D_8028AAF4->mtx[0][1]);
  BrVec3Scale((BrVec3 *)D_8028AAF0->mtx0[3], (BrVec3 *)D_8028AAF0->mtx0[2], -0.65f);
  BrCameraSet(D_8028AAF4, D_8028AAC0, D_8028AAC8 * 0.2, 320.0f, 200.0f);
  if (D_8031B760[D_8028DBBC].kind == 1) {
    BrViewportSet(0, (D_8028AAB4 >> 1) - 3, (D_8028AAB0 >> 1) - 4, (D_8028AAB4 >> 1) - 3, 1);
  } else if (D_8031B760[D_8028DBBC].kind == 10) {
    BrViewportSet(0, (D_8028AAB4 >> 1) + 2, (D_8028AAB0 >> 1) - 4, (D_8028AAB4 >> 1) - 3, 1);
  } else {
    BrViewportSet(0, (D_8028AAB4 >> 1) - 6, (D_8028AAB0 >> 1) - 4, (D_8028AAB4 >> 1) - 3, 1);
  }
  BrGridSpanExtend(0, 0);
  BrCarPlaceWheels(D_8028DBBC);
  D_8028C334 = 1;
  BrCarVisibility(D_8028AAF0);
  func_80230554(D_8028AAF0, 0);
}

/* WHAT IT DOES: Apply the pending decal flip: store the working decal into
 * the decal buffer, then redraw every texel of the working decal from that
 * stored copy mirrored left-right (flip 1) or top-bottom (flip 2); clear
 * the pending flip and copy the saved dirty flag back. */
/* @implements 0x80248DB4 tgr BrPaintFlipApply */
void BrPaintFlipApply(void)
{
  int x;
  int y;

  BrPaintDecalCommit();
  if (D_8028CFBC == 1) {
    for (y = 0; y < D_8028DB8C; y++) {
      for (x = 0; x < D_8028DB88; x++) {
        BrPaintPlot(D_8028DB88 - x - 1, y, BrPaintPeek(D_8028DB80, x, y));
      }
    }
  } else if (D_8028CFBC == 2) {
    for (y = 0; y < D_8028DB8C; y++) {
      for (x = 0; x < D_8028DB88; x++) {
        BrPaintPlot(x, D_8028DB8C - y - 1, BrPaintPeek(D_8028DB80, x, y));
      }
    }
  }
  D_8028CFBC = 0;
  D_8028DB60 = D_8028DB64;
}

/* WHAT IT DOES: Copy the decal to the car's other side: when the mirror
 * is asked for (0x8028DBCC), switch the paint view to its opposite (0<->1,
 * 2<->3, 6<->7), load that side's decal, texture and mask, store the working
 * decal, then redraw it from the old side's texture mirrored left-right and
 * start the view turning.  The saved dirty flag is copied back either way. */
/* @implements 0x80248B80 tgr BrPaintMirrorSide */
void BrPaintMirrorSide(void)
{
  static unsigned char *D_8028DCA4 = 0; /* the side being mirrored from */
  int x;
  int y;

  if (D_8028DBCC == 0) {
    D_8028DB60 = D_8028DB64;
    return;
  }
  {
    switch (D_8028DB68) {
    case 0:
      D_80369DAB = 1;
      break;
    case 1:
      D_80369DAB = 0;
      break;
    case 2:
      D_80369DAB = 3;
      break;
    case 3:
      D_80369DAB = 2;
      break;
    case 6:
      D_80369DAB = 7;
      break;
    case 7:
      D_80369DAB = 6;
      break;
    }
    D_8028DB6C = D_8028DB68;
    D_8028DCA4 = D_8028AB08->parts[D_8028AB08->decalPart[D_8028DB6C]].a;
    D_8028DB68 = D_80369DAB;
    D_8028DB78 = D_8028AB08->parts[D_8028AB08->decalPart[D_8028DB68]].a;
    D_8028DB7C = D_8028AB08->x11c[D_8028DB68];
    D_8028DB08 = D_8028DB0C[D_8028DB68];
    BrPaintDecalCommit();
    for (y = 0; y < D_8028DB8C; y++) {
      for (x = 0; x < D_8028DB88; x++) {
        BrPaintPlot(D_8028DB88 - x - 1, y, BrPaintPeek(D_8028DCA4, x, y));
      }
    }
    D_8028DBD0 = 1;
    D_8028DBB0 = 0;
    D_8028DCA4 = 0;
  }
  D_8028DB60 = D_8028DB64;
}

/* WHAT IT DOES: Draw a dashed line between two screen points for the paint
 * shop's selection (Bresenham along the longer axis, drawn from the lower
 * end): the dash runs 4 pixels in one colour and 4 in the other, and every
 * 8th call the two colours swap, so the dashes crawl; only every other pixel
 * along the main axis is plotted (BrFillPoint). */
/* @implements 0x80251A54 tgr BrPaintDashLine */
void BrPaintDashLine(int x0, int y0, int x1, int y1)
{
  int dx;
  int dy;
  int err;
  int n;
  int x;
  int y;
  int sx;
  int ex;
  int sy;
  int ey;
  int xstep;
  int ystep;
  int unused;                   /* holds its frame slot */
  unsigned char c;

  n = 0;
  sx = x0;
  sy = y0;
  ex = x1;
  ey = y1;
  dx = x1 - x0 > 0 ? x1 - x0 : -(x1 - x0);
  dy = y1 - y0 > 0 ? y1 - y0 : -(y1 - y0);
  if ((++D_8028DBB0 & 7) == 0) {
    BrSwapBytes((char *)&D_8028DAB8, (char *)&D_8028DABC);
  }
  if ((dx >= dy && x1 < x0) || (dx < dy && y1 < y0)) {
    sx = x1;
    ex = x0;
    sy = y1;
    ey = y0;
  }
  ystep = ey - sy < 0 ? -1 : 1;
  xstep = ex - sx < 0 ? -1 : 1;
  if (dx >= dy) {
    x = sx;
    y = sy;
    err = 0;
    for (; x <= ex; x++) {
      if (err >= dx) {
        err -= dx;
        y += ystep;
      }
      n = (n + 1) % 8;
      c = n < 4 ? D_8028DAB8 : D_8028DABC;
      if (x & 1) {
        BrFillPoint(x, y, c);
      }
      err += dy;
    }
  } else {
    x = sx;
    y = sy;
    err = 0;
    for (; y <= ey; y++) {
      if (err >= dy) {
        err -= dy;
        x += xstep;
      }
      n = (n + 1) % 8;
      c = n < 4 ? D_8028DAB8 : D_8028DABC;
      if (y & 1) {
        BrFillPoint(x, y, c);
      }
      err += dx;
    }
  }
}


/* WHAT IT DOES: Draw a marching-ants rectangle between two screen corners
 * (either order): the edges in 4-pixel dashes alternating between the two
 * dash colours (swapped every 8 frames), top, right, bottom then left, the
 * right and left edges finished with a 2-pixel stub at the bottom.
 * The colour is never set when no dash is drawn before a stub, so the stub
 * takes whatever byte sits in c's home (sp+0x59, after c1 and c2); t holds
 * y1 - 1 for the two stubs.
 * RESIDUE (60): saved-register choice -- the ROM keeps on, x, y in s1, s2,
 * s3 (ours x, y, on) and toggles on as (on + 1) & 1 straight into its
 * register; declaration order and every toggle spelling leave it. */
/* @t4-pass 0x80251CD4 1 2026-09-29 compiles 26 best 60 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80251CD4 2 2026-09-29 compiles 26 best 60 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x80251CD4 */
/* @implements 0x80251CD4 tgr BrPaintDashRect */
void BrPaintDashRect(int x0, int y0, int x1, int y1)
{
  int x;
  int y;
  int on;
  char c1;                      /* c1, c2: declared, never used; */
  char c2;                      /* they put c at sp+0x59 */
  unsigned char c;
  int t;

  on = 0;
  if ((++D_8028DBB0 & 7) == 0) {
    BrSwapBytes((char *)&D_8028DAB8, (char *)&D_8028DABC);
  }
  if (x1 < x0) {
    x = x0;
    x0 = x1;
    x1 = x;
  }
  if (y1 < y0) {
    y = y0;
    y0 = y1;
    y1 = y;
  }
  for (x = x0; x < x1 - 4; x += 4) {
    on ^= 1;
    c = on ? D_8028DAB8 : D_8028DABC;
    BrFillRect(x, y0, 4, 1, c, c, c);
  }
  for (y = y0; y < y1 - 4; y += 4) {
    on ^= 1;
    c = on ? D_8028DAB8 : D_8028DABC;
    BrFillRect(x1, y, 1, 4, c, c, c);
  }
  t = y1 - 1;
  BrFillRect(x1, t, 1, 2, c, c, c);
  for (x = x0; x < x1 - 4; x += 4) {
    on ^= 1;
    c = on ? D_8028DAB8 : D_8028DABC;
    BrFillRect(x, y1, 4, 1, c, c, c);
  }
  for (y = y0; y < y1 - 4; y += 4) {
    on ^= 1;
    c = on ? D_8028DAB8 : D_8028DABC;
    BrFillRect(x0, y, 1, 4, c, c, c);
  }
  BrFillRect(x0, t, 1, 2, c, c, c);
}

/* WHAT IT DOES: Draw a marching-ants rounded rectangle between two screen
 * corners (either order): the corner radius a quarter of the shorter side,
 * the left, right, top and bottom edges in 4-pixel dashes alternating
 * between the two dash colours (swapped every 8 frames), then the corners
 * by a midpoint circle walk at twice the resolution, on every other step
 * the eight octant points on odd pixels only, in dashes of four points.
 * RESIDUE (~270): the ROM keeps the corners in their parameter homes and
 * the radius twice (s6, s7); ours holds the corners in saved registers. */
/* @t4-pass 0x80251F68 1 2026-10-03 compiles 31 best 268 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80251F68 2 2026-10-03 compiles 31 best 268 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x80251F68 */
/* @implements 0x80251F68 tgr BrPaintDashRoundRect */
void BrPaintDashRoundRect(int x0, int y0, int x1, int y1)
{
  int t;
  int r;
  int top;
  int bot;
  int right;
  int x;
  int y;
  int a;
  int b;
  int err;
  int ha;
  int hb;
  unsigned char on;
  unsigned char c;

  on = 0;
  if ((++D_8028DBB0 & 7) == 0) {
    BrSwapBytes((char *)&D_8028DAB8, (char *)&D_8028DABC);
  }
  if (x1 < x0) {
    t = x0;
    x0 = x1;
    x1 = t;
  }
  if (y1 < y0) {
    t = y0;
    y0 = y1;
    y1 = t;
  }
  r = (x1 - x0 < y1 - y0 ? x1 - x0 : y1 - y0) >> 2;
  top = y0 + r;
  bot = y1 - r;
  for (y = top; y <= y1 - r; y += 4) {
    on = (on + 1) & 1;
    c = on ? D_8028DAB8 : D_8028DABC;
    BrFillRect(x0, y, 1, 4, c, c, c);
  }
  for (y = top; y <= y1 - r; y += 4) {
    on = (on + 1) & 1;
    c = on ? D_8028DAB8 : D_8028DABC;
    BrFillRect(x1, y, 1, 4, c, c, c);
  }
  x0 += r;
  right = x1 - r;
  for (x = x0; x <= x1 - r; x += 4) {
    on = (on + 1) & 1;
    c = on ? D_8028DAB8 : D_8028DABC;
    BrFillRect(x, y0, 4, 1, c, c, c);
  }
  for (x = x0; x <= x1 - r; x += 4) {
    on = (on + 1) & 1;
    c = on ? D_8028DAB8 : D_8028DABC;
    BrFillRect(x, y1, 4, 1, c, c, c);
  }
  b = r * 2;
  err = -b;
  for (a = 0; a <= b;) {
    if (!(a & 1)) {
      on = (on + 1) & 7;
      c = on < 4 ? D_8028DAB8 : D_8028DABC;
      ha = a >> 1;
      hb = b >> 1;
      if ((x0 - ha) & 1) {
        BrFillPoint(x0 - ha, top - hb, c);
      }
      if ((x0 - ha) & 1) {
        BrFillPoint(x0 - ha, hb + bot, c);
      }
      if ((ha + right) & 1) {
        BrFillPoint(ha + right, hb + bot, c);
      }
      if ((ha + right) & 1) {
        BrFillPoint(ha + right, top - hb, c);
      }
      if ((ha + bot) & 1) {
        BrFillPoint(hb + right, ha + bot, c);
      }
      if ((top - ha) & 1) {
        BrFillPoint(hb + right, top - ha, c);
      }
      if ((top - ha) & 1) {
        BrFillPoint(x0 - hb, top - ha, c);
      }
      if ((ha + bot) & 1) {
        BrFillPoint(x0 - hb, ha + bot, c);
      }
    }
    err += a;
    a++;
    if (err >= 0) {
      err -= b;
      b--;
    }
  }
}

/* WHAT IT DOES: Draw a marching-ants oval inside two screen corners (either
 * order): the midpoint ellipse walk of BrPaintFillOval at twice the
 * resolution, and on every other step the four quadrant points at odd
 * coordinates only, in dashes of four points of the two dash colours (swapped
 * every 8 frames). */
/* @t4-pass 0x802523CC 1 2026-10-03 compiles 29 best 308 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x802523CC 2 2026-10-03 compiles 31 best 308 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x802523CC */
/* @implements 0x802523CC tgr BrPaintDashOval */
void BrPaintDashOval(int x0, int y0, int x1, int y1)
{
  int ry;
  int rx;
  int t;
  int dx;
  int dy;
  int x;
  int y;
  int err;
  int b2b;
  int a2b;
  int b2;
  int a2;
  int n;
  unsigned char c;

  n = 0;
  dx = x1 - x0;
  if (dx < 0) {
    dx = x0 - x1;
    t = x0;
    x0 = x1;
    x1 = t;
  }
  dy = y1 - y0;
  if (dy < 0) {
    dy = y0 - y1;
    t = y0;
    y0 = y1;
    y1 = t;
  }
  ry = dy >> 1 >= 2 ? dy >> 1 : 1;
  rx = dx >> 1 >= 2 ? dx >> 1 : 1;
  x = 0;
  y = ry * 2;
  b2 = b2b = ry * ry;
  a2 = a2b = rx * rx;
  err = -a2 * y;
  if ((++D_8028DBB0 & 7) == 0) {
    BrSwapBytes((char *)&D_8028DAB8, (char *)&D_8028DABC);
  }
  while (b2b * x <= a2b * y) {
    if (!(x & 1)) {
      n = (n + 1) % 8;
      c = n < 4 ? D_8028DAB8 : D_8028DABC;
      if (((x >> 1) + ((x0 + x1) >> 1)) & 1) {
        BrFillPoint((x >> 1) + ((x0 + x1) >> 1), (y >> 1) + ((y0 + y1) >> 1), c);
      }
      if (((x >> 1) + ((x0 + x1) >> 1)) & 1) {
        BrFillPoint((x >> 1) + ((x0 + x1) >> 1), ((y0 + y1) >> 1) - (y >> 1), c);
      }
      if ((((x0 + x1) >> 1) - (x >> 1)) & 1) {
        BrFillPoint(((x0 + x1) >> 1) - (x >> 1), ((y0 + y1) >> 1) - (y >> 1), c);
      }
      if ((((x0 + x1) >> 1) - (x >> 1)) & 1) {
        BrFillPoint(((x0 + x1) >> 1) - (x >> 1), (y >> 1) + ((y0 + y1) >> 1), c);
      }
    }
    err += b2 * x;
    x++;
    err += b2 * x;
    if (err > 0) {
      err -= a2 * y;
      y--;
      err -= a2 * y;
    }
  }
  x = rx * 2;
  y = 0;
  err = -b2 * x;
  while (a2b * y <= b2b * x) {
    if (!(y & 1)) {
      n = (n + 1) % 8;
      c = n < 4 ? D_8028DAB8 : D_8028DABC;
      if (((y >> 1) + ((y0 + y1) >> 1)) & 1) {
        BrFillPoint((x >> 1) + ((x0 + x1) >> 1), (y >> 1) + ((y0 + y1) >> 1), c);
      }
      if ((((y0 + y1) >> 1) - (y >> 1)) & 1) {
        BrFillPoint((x >> 1) + ((x0 + x1) >> 1), ((y0 + y1) >> 1) - (y >> 1), c);
      }
      if ((((y0 + y1) >> 1) - (y >> 1)) & 1) {
        BrFillPoint(((x0 + x1) >> 1) - (x >> 1), ((y0 + y1) >> 1) - (y >> 1), c);
      }
      if (((y >> 1) + ((y0 + y1) >> 1)) & 1) {
        BrFillPoint(((x0 + x1) >> 1) - (x >> 1), (y >> 1) + ((y0 + y1) >> 1), c);
      }
    }
    err += a2 * y;
    y++;
    err += a2 * y;
    if (err > 0) {
      err -= b2 * x;
      x--;
      err -= b2 * x;
    }
  }
}


/* WHAT IT DOES: Draw a dashed circle outline of radius r around a screen
 * point, clipped to the paint area: a midpoint walk at double resolution,
 * on every other step the eight octant points on odd pixels only, in dashes
 * of the two dash colours (swapped every 8 frames), four points each.
 * RESIDUE (~200): the ROM keeps the dash counter n in a temp register
 * (spilled to its home around the calls) and x in memory; ours keeps both
 * in memory, which shifts every temp after. */
/* @t4-pass 0x802528F8 1 2026-10-03 compiles 31 best 206 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x802528F8 2 2026-10-03 compiles 31 best 206 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x802528F8 */
/* @implements 0x802528F8 tgr BrPaintDashCircle */
void BrPaintDashCircle(int cx, int cy, int r)
{
  int n;
  unsigned char c;
  int y;
  int x;
  int ax;
  int ay;
  int ax2;
  int ay2;
  int u0;                       /* u0, u1: declared, never used; */
  int u1;                       /* the frame holds them */
  int d;

  ax = D_8028DB94.x;
  ay = D_8028DB94.y;
  ax2 = D_8028DB94.x + D_8028DB94.w;
  ay2 = D_8028DB94.y + D_8028DB94.h;
  n = 0;
  if ((++D_8028DBB0 & 7) == 0) {
    BrSwapBytes((char *)&D_8028DAB8, (char *)&D_8028DABC);
  }
  y = r * 2;
  d = -y;
  x = 0;
  while (x <= y) {
    if ((x & 1) == 0) {
      n = (n + 1) % 8;
      if (n < 4) {
        c = D_8028DAB8;
      } else {
        c = D_8028DABC;
      }
      if ((((x >> 1) + cx) & 1) && (x >> 1) + cx >= ax && (x >> 1) + cx < ax2 && (y >> 1) + cy >= ay && (y >> 1) + cy < ay2) {
        BrFillPoint((x >> 1) + cx, (y >> 1) + cy, c);
      }
      if ((((x >> 1) + cx) & 1) && (x >> 1) + cx >= ax && (x >> 1) + cx < ax2 && cy - (y >> 1) >= ay && cy - (y >> 1) < ay2) {
        BrFillPoint((x >> 1) + cx, cy - (y >> 1), c);
      }
      if (((cx - (x >> 1)) & 1) && cx - (x >> 1) >= ax && cx - (x >> 1) < ax2 && cy - (y >> 1) >= ay && cy - (y >> 1) < ay2) {
        BrFillPoint(cx - (x >> 1), cy - (y >> 1), c);
      }
      if (((cx - (x >> 1)) & 1) && cx - (x >> 1) >= ax && cx - (x >> 1) < ax2 && (y >> 1) + cy >= ay && (y >> 1) + cy < ay2) {
        BrFillPoint(cx - (x >> 1), (y >> 1) + cy, c);
      }
      if ((((x >> 1) + cy) & 1) && (y >> 1) + cx >= ax && (y >> 1) + cx < ax2 && (x >> 1) + cy >= ay && (x >> 1) + cy < ay2) {
        BrFillPoint((y >> 1) + cx, (x >> 1) + cy, c);
      }
      if (((cy - (x >> 1)) & 1) && (y >> 1) + cx >= ax && (y >> 1) + cx < ax2 && cy - (x >> 1) >= ay && cy - (x >> 1) < ay2) {
        BrFillPoint((y >> 1) + cx, cy - (x >> 1), c);
      }
      if (((cy - (x >> 1)) & 1) && cx - (y >> 1) >= ax && cx - (y >> 1) < ax2 && cy - (x >> 1) >= ay && cy - (x >> 1) < ay2) {
        BrFillPoint(cx - (y >> 1), cy - (x >> 1), c);
      }
      if ((((x >> 1) + cy) & 1) && cx - (y >> 1) >= ax && cx - (y >> 1) < ax2 && (x >> 1) + cy >= ay && (x >> 1) + cy < ay2) {
        BrFillPoint(cx - (y >> 1), (x >> 1) + cy, c);
      }
    }
    d += x;
    x++;
    if (d >= 0) {
      d -= y;
      y--;
    }
  }
}

/* WHAT IT DOES: Draw the paint shop's text line at (x, y), moved left by
 * half of 0x8028DBAC and up by the font's height (both in texels, 4 screen
 * pixels each): each character's glyph (found by code in the 50-glyph table) is drawn 16x16
 * from the font image scaled to 64x64 in the chosen swatch colour -- with a
 * drop shadow offset (4, 4) in the shadow colour when shadows are on --
 * when it overlaps the paint area; glyphs advance by width minus the
 * smaller of the two facing kerns, plus 2. */
/* @implements 0x80252C9C tgr BrPaintTextDraw */
void BrPaintTextDraw(int x, int y)
{
  int i;
  int k;
  int g;
  int kern;
  int adv;
  int left;

  x -= (D_8028DBAC >> 1) * 4;
  y -= D_8028D2C0.h * 4;
  for (i = 0; i < D_8028DBA8; i++) {
    for (k = 0; k < 50; k++) {
      if (D_80369E68[i]->c == D_8028D540[k].c) {
        g = k;
        break;
      }
    }
    left = D_80369E68[i]->off * 4 + x;
    if (D_8028DB94.x < D_80369E68[i]->w * 4 + left
        && left < D_8028DB94.x + D_8028DB94.w
        && D_8028DB94.y < y + D_8028D2C0.h * 4
        && y < D_8028DB94.y + D_8028DB94.h) {
      if (D_8028CF2C != 0) {
        BrImageDrawPart(&D_8028D2C0, g << 4, 0, 16, 16, x - D_80369E68[i]->off * 4 + 4, y + 4, 64, 64,
                        D_80369B98[D_8028DB5C].r, D_80369B98[D_8028DB5C].g, D_80369B98[D_8028DB5C].b);
      }
      BrImageDrawPart(&D_8028D2C0, g << 4, 0, 16, 16, x - D_80369E68[i]->off * 4, y, 64, 64,
                      D_80369B98[D_8028DB58].r, D_80369B98[D_8028DB58].g, D_80369B98[D_8028DB58].b);
    }
    if (i < D_8028DBA8 - 1) {
      kern = D_80369E68[i]->kernR < D_80369E68[i + 1]->kernL ? D_80369E68[i]->kernR : D_80369E68[i + 1]->kernL;
      adv = D_80369E68[i]->w - kern + 2;
      x += adv * 4;
    }
  }
}
