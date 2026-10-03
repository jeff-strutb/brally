/* paintscreen.c -- the paint shop screen
 *
 * Its own object here: its strings and switch tables sit apart from the
 * other paint-shop functions'.
 */
#include "tgr/common.h"
#include "tgr/car.h"
#include "tgr/gbi.h"

/* -- declarations -- */
typedef struct BrImage {        /* as drawing/image.c (0x30 bytes) */
  unsigned char *data;
  int x4;
  int x8;
  unsigned char siz;            /* 0x0C */
  char pad0d[3];
  int w;                        /* 0x10 */
  int h;                        /* 0x14 */
  int stripH;                   /* 0x18 */
  int x;                        /* 0x1C */
  int y;                        /* 0x20 */
  int drawW;                    /* 0x24 */
  int drawH;                    /* 0x28 */
  unsigned char kind;           /* 0x2C */
  char pad2d[3];
} BrImage;
typedef struct BrPaintRect { int x, y, w, h; } BrPaintRect;
typedef struct BrGlyph {        /* a keyboard key (0x20) */
  int x0;
  int x4;
  unsigned char c;
  char pad9[3];
  int off;
  int w;
  int x14;
  int x18;
  int x1c;
} BrGlyph;
typedef struct BrPaintSwatch {  /* a palette entry (0x14) */
  int x, y, w, h;
  unsigned char r, g, b;
} BrPaintSwatch;
typedef struct BrPaintPart {    /* a model part (0x24) */
  unsigned char *tex;
  unsigned short *tlut;         /* 0x04 */
  char pad08[4];
  unsigned short w;             /* 0x0C */
  unsigned short h;             /* 0x0E */
  char pad10[0x24 - 0x10];
} BrPaintPart;
typedef struct BrPaintModel {   /* the car model being painted */
  char pad00[0x14];
  BrPaintPart *parts;           /* 0x14 */
  char pad18[0x110 - 0x18];
  unsigned char decal[10];      /* 0x110  the parts holding the ten decals */
  unsigned char paint[2];       /* 0x11A */
  unsigned char **mask;         /* 0x11C */
} BrPaintModel;
typedef struct BrPadRec {       /* as tgr/pad.h (0x15C bytes) */
  unsigned int pressed;
  unsigned int held;
  int repeat[4];
  float axis[2];
  char pad20[0x15c - 0x20];
} BrPadRec;
extern char D_8036A8E0;
extern BrCar D_8031B760[];
extern int D_80271FA8;                  /* the player's controller */
extern int D_8028B7F4;
extern int D_8028C328;
extern int D_8028C334;
extern short D_802A4BE8;
extern unsigned char D_8028DC80;        /* the screen has been set up */
extern int D_802724F0;                  /* the Controller Pak transfer under way */
extern BrPaintModel *D_8028AB08;        /* the car model being painted */
extern Gfx *D_8028A858;
extern int D_8028AA80;                  /* night */
extern int D_8028AA98;                  /* the performance meter */
extern int D_8028AAB0;                  /* the screen size */
extern int D_8028AAB4;
extern unsigned char *D_8028DB80;       /* the two decal buffers */
extern unsigned char *D_8028DB84;
extern BrImage D_8028CB40;              /* the background */
extern BrImage D_8028CB70;              /* the title bar */
extern BrImage D_8028CBA0;
extern BrImage D_8028CBD0;
extern BrImage D_8028CC00;
extern BrImage D_8028CC30;
extern BrImage D_8028CC60;
extern BrImage D_8028CC90;
extern BrImage D_8028CCC0;
extern BrImage D_8028CCF0;
extern BrImage D_8028CD20;
extern BrImage D_8028CD50;
extern BrImage D_8028CD80;
extern BrImage D_8028CDB0;              /* the slot frame */
extern BrImage D_8028CDE0;              /* the slot highlight */
extern BrImage D_8028CE10;              /* the round brush sizes */
extern BrImage D_8028CE40;              /* the square brush sizes */
extern BrImage D_8028D0B0;              /* the A button */
extern BrImage D_8028D0E0;              /* the B button */
extern BrImage D_8028D110;              /* the cursor */
extern BrImage D_8028D140;              /* the tool cursors */
extern BrImage D_8028D170;
extern BrImage D_8028D1A0;
extern BrImage D_8028D1D0;
extern BrImage D_8028D200;
extern BrImage D_8028D230;              /* the on-screen keyboard */
extern BrImage D_8028D260;              /* the text box under it */
extern BrImage D_8028D290;              /* the keyboard cursor */
extern BrImage D_8028D2C0;              /* the font sheet */
extern BrImage *D_8028DAD8[12];         /* the tool button pictures */
extern BrImage *D_8028DB0C[10];         /* each decal slot's preview image */
extern BrImage *D_8028DB08;             /* the slot's preview image */
extern BrImage *D_8028DB34[4];          /* the four style pictures */
extern BrImage *D_8028DB44[4];          /* the four oval style pictures */
extern BrPaintRect D_8028D470;          /* the paint area's frame */
extern BrPaintRect D_8028D480;          /* the palette's two halves */
extern BrPaintRect D_8028D490;
extern BrPaintRect D_8028D4F0[5];       /* the brush size buttons */
extern BrGlyph D_8028D540[50];          /* the keyboard */
extern BrPaintRect D_8028DB94;          /* the paint area on screen */
extern BrPaintRect D_80369CD8[13];      /* the tool buttons */
extern BrPaintRect D_80369DB0[10];      /* the decal slot buttons */
extern BrPaintSwatch D_80369B98[16];    /* the palette */
extern float D_802AB0D0;
extern float D_802AB0FC;
extern unsigned char D_8028CE9C;        /* the brush shape: 0 round */
extern unsigned char D_8028DAC0;        /* the chosen brush size */
extern unsigned char D_8028DB58;        /* the chosen palette colour */
extern unsigned char D_8028DB60;        /* the tool */
extern unsigned char D_8028DB64;        /* the tool before */
extern unsigned char D_8028DB68;        /* the decal being painted */
extern unsigned char *D_8028DB78;       /* the decal texture */
extern unsigned char *D_8028DB7C;       /* its paint mask */
extern unsigned char D_8028DB88;        /* texture width */
extern unsigned char D_8028DB8C;        /* texture height */
extern unsigned short *D_8028DB90;      /* the decal palette */
extern unsigned char D_8028DBA8;
extern unsigned char D_8028DBAC;
extern unsigned char D_8028DBBC;        /* the controller */
extern unsigned char D_8028DBC0;        /* a stroke is under way */
extern unsigned char D_8028DBC4;        /* the keyboard is up */
extern unsigned char D_8028DBC8;        /* the palette is live */
extern unsigned char D_8028DBD4;        /* the colour mixer is open */
extern unsigned char D_8028DBD8;        /* the car view needs redrawing */
extern unsigned char D_8028DBE0;        /* a popup is open */
extern unsigned char D_8028DBE8;        /* leaving */
extern unsigned char D_8028DBEC;        /* the eyedropper is held */
void BrCarDefaultColour(BrCar *car);
void BrCarCamStep(void);
void BrEntLoadRecord(BrCar *car, int slot, int kind);
void BrVec3Negate(BrVec3 *out, BrVec3 *v);
void BrVec3Normalise(BrVec3 *v);
void BrVec3Cross(BrVec3 *out, BrVec3 *a, BrVec3 *b);
void BrIfaceMemReset(void);
unsigned char *BrIfaceMemAlloc(int size);
void BrAllocPaintShopGfxMem(BrImage *img);
void osSyncPrintf(char *fmt, ...);
void BrPaintPaletteLoad(void);
void BrPaintPaletteDraw(void);
void BrFadeTo(float level, float seconds);
int BrFadeOutDone(void);
void BrFadeStep(void);
void BrFadeBarsDraw(void);
void BrClockTick(void);
void BrFrameBeginLayout1(void);
void BrFrameEnd(void);
void BrMenuCameraSet(float w, float h);
void BrViewportSet(int x, int y, int w, int h, int scissor);
void BrZBufferClear(void);
void BrScreenClear(int r, int g, int b);
void BrFogSetup(void);
void BrFrameTintSetup(void);
void BrPerfMeterDraw(void);
void BrModeSet(void (*mode)(void));
void BrFrontReturnToTitle(void);
void BrImageDraw(BrImage *img);
void BrImageDrawAt(BrImage *img, int x, int y);
void BrImageDrawTinted(BrImage *img, unsigned char r, unsigned char g, unsigned char b);
void BrImageDrawPart(BrImage *img, int s, int t, int sw, int th, int x, int y, int w, int h,
                     unsigned char r, unsigned char g, unsigned char b);
void BrFillRect(int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b);
void BrTextSetFont(int size);
void BrTextAlignCentre(void);
void BrTextHighlightOff(void);
void BrTextPrint(char *s, int x, int y);
void BrTexLoad(int n, BrPaintPart *tbl);
void BrTexRectFlipDraw(int tw, int th, int x, int y, int w, int h);
void BrPaintCarView(void);
void BrPaintReset(void);
int BrPaintCursorInRect(BrPaintRect *r);
unsigned char BrPaintGet(int x, int y);
void BrPadConsume(unsigned int *pad, unsigned int bits);
void BrPakMessage(int msg, char op, char mode);
int BrDecalPakTransfer(BrPaintModel *m, unsigned char port, char op, char fromMenu, unsigned char *done);
void BrPaintClick(void);
void BrPaintStickMove(void);
void BrPaintColourMix(void);
void BrPaintBrushSelect(void);
void BrPaintTextStyleMenu(void);
void BrPaintStyleSelect(void);
void BrPaintOvalStyleSelect(void);
void BrPaintMirrorMenu(void);
void BrPaintClearMenu(void);
void BrPaintExitPrompt(void);
/* -- end declarations -- */

/* WHAT IT DOES: The paint shop screen, one frame.  The first time in: put
 * the player's car on the stand facing the camera (body colours, the
 * close camera at 6.5, 6.5, 2 looking at it, 4.5 out for the kind 10 car),
 * reset the interface memory and take two 2 KB decal buffers from it, load
 * every picture, and lay out the decal slots, the paint area, the palette,
 * the twelve tool buttons, the brush sizes and the on-screen keyboard; then
 * fade in.  Every frame: draw the backdrop and title, the ten decal slots
 * (the current one lit), the decal being painted at 4x, the palette, the
 * tools (the current one lit) and the brush size; redraw the car when it
 * changed.  Off the paint area the tool's popup runs (mirror, clear, Pak
 * load or save, the exit prompt, or the option popups); on it the click and
 * stick handling run and the tool's cursor is drawn.  B on the paint area
 * picks up the colour under the cursor (an eyedropper) or cancels a stroke
 * or the keyboard.  Once the exit fade is done, clear both screens, reset
 * the paint shop and go back to the front end.
 * RESIDUE (gap 545, 2 short): the ROM indexes the car array with multu by
 * 0x2090 held in s2, where ours gets shift sequences, and keeps the tool
 * grid's inner loop rolled with its index in s0, where ours unrolls it;
 * the other differences follow from those registers. */
/* @implements 0x80243260 tgr BrPaintShopScreen */
void BrPaintShopScreen(void)
{
  unsigned char done;
  int i;
  int k;
  int r;
  int x;
  int y;

  done = 0;
  D_802A4BE8 = 0;
  if (D_8028DC80 == 0) {
    D_8028C328 = 0;
    D_8028DBBC = D_80271FA8;
    D_8028B7F4 = 1;
    BrCarDefaultColour(&D_8031B760[D_8028DBBC]);
    D_8031B760[D_8028DBBC].xed8 = (int)BrCarCamStep;
    BrEntLoadRecord(&D_8031B760[D_8028DBBC], D_8028DBBC, D_8031B760[D_8028DBBC].kind = D_8031B760[0].kind);
    D_8028AB08 = (BrPaintModel *)D_8031B760[D_8028DBBC].model;
    D_8031B760[D_8028DBBC].mtx0[3][0] = D_8031B760[D_8028DBBC].mtx0[3][1] = D_8031B760[D_8028DBBC].mtx0[3][2] = 0.0f;
    D_8031B760[D_8028DBBC].mtx0[2][2] = 1.0f;
    D_8031B760[D_8028DBBC].mtx0[2][0] = D_8031B760[D_8028DBBC].mtx0[2][1] = 0.0f;
    D_8031B760[D_8028DBBC].mtx0[0][1] = 1.0f;
    D_8031B760[D_8028DBBC].mtx0[0][2] = D_8031B760[D_8028DBBC].mtx0[0][0] = 0.0f;
    D_8031B760[D_8028DBBC].mtx0[1][1] = D_8031B760[D_8028DBBC].mtx0[1][2] = 0.0f;
    D_8031B760[D_8028DBBC].mtx0[1][0] = -1.0f;
    if (D_8031B760[D_8028DBBC].kind == 10) {
      D_8031B760[D_8028DBBC].cams[3].mtx[3][0] = D_802AB0D0;
      D_8031B760[D_8028DBBC].cams[3].mtx[3][1] = D_802AB0D0;
      D_8031B760[D_8028DBBC].cams[3].mtx[3][2] = 2.0f;
    } else {
      D_8031B760[D_8028DBBC].cams[3].mtx[3][0] = 6.5f;
      D_8031B760[D_8028DBBC].cams[3].mtx[3][1] = 6.5f;
      D_8031B760[D_8028DBBC].cams[3].mtx[3][2] = 2.0f;
    }
    BrVec3Negate((BrVec3 *)D_8031B760[D_8028DBBC].cams[3].mtx[0], (BrVec3 *)D_8031B760[D_8028DBBC].cams[3].mtx[3]);
    BrVec3Normalise((BrVec3 *)D_8031B760[D_8028DBBC].cams[3].mtx[0]);
    D_8031B760[D_8028DBBC].cams[3].mtx[2][1] = D_8031B760[D_8028DBBC].cams[3].mtx[2][0] = 0.0f;
    D_8031B760[D_8028DBBC].cams[3].mtx[2][2] = 1.0f;
    BrVec3Cross((BrVec3 *)D_8031B760[D_8028DBBC].cams[3].mtx[1], (BrVec3 *)D_8031B760[D_8028DBBC].cams[3].mtx[2],
                (BrVec3 *)D_8031B760[D_8028DBBC].cams[3].mtx[0]);
    BrVec3Cross((BrVec3 *)D_8031B760[D_8028DBBC].cams[3].mtx[2], (BrVec3 *)D_8031B760[D_8028DBBC].cams[3].mtx[0],
                (BrVec3 *)D_8031B760[D_8028DBBC].cams[3].mtx[1]);
    D_8031B760[D_8028DBBC].x2000 = 0;
    D_8031B760[D_8028DBBC].x2044 = 0;
    BrIfaceMemReset();
    osSyncPrintf("\nAllocating %d bytes (x2) for decal buffers...\n\n", 0x800);
    D_8028DB80 = BrIfaceMemAlloc(0x800);
    D_8028DB84 = BrIfaceMemAlloc(0x800);
    BrAllocPaintShopGfxMem(&D_8028CB40);
    BrAllocPaintShopGfxMem(&D_8028CB70);
    BrAllocPaintShopGfxMem(&D_8028CBA0);
    D_8028CBA0.kind = 99;
    BrAllocPaintShopGfxMem(&D_8028CBD0);
    D_8028CC00.data = D_8028CBD0.data;
    BrAllocPaintShopGfxMem(&D_8028CC30);
    D_8028CC60.data = D_8028CC30.data;
    BrAllocPaintShopGfxMem(&D_8028CC90);
    BrAllocPaintShopGfxMem(&D_8028CCC0);
    BrAllocPaintShopGfxMem(&D_8028CCF0);
    D_8028CD20.data = D_8028CCF0.data;
    BrAllocPaintShopGfxMem(&D_8028CD50);
    BrAllocPaintShopGfxMem(&D_8028CD80);
    BrAllocPaintShopGfxMem(&D_8028CDB0);
    D_8028CDB0.x = D_8028CD50.x;
    D_8028CDB0.y = D_8028CD50.y;
    BrAllocPaintShopGfxMem(&D_8028CDE0);
    D_8028CDE0.y = D_8028CD80.y + 8;
    D_8028CDE0.x = D_8028CD80.x;
    for (i = 0; i < 10; i++) {
      switch (i) {
      case 0:
      case 1:
        D_80369DB0[i].x = D_8028DB0C[i]->x + 5;
        D_80369DB0[i].y = D_8028DB0C[i]->y + 2;
        D_80369DB0[i].w = D_8028DB0C[i]->w - 10;
        D_80369DB0[i].h = D_8028DB0C[i]->h - 4;
        break;
      case 2:
      case 3:
        D_80369DB0[i].x = D_8028DB0C[i]->x + 4;
        D_80369DB0[i].y = D_8028DB0C[i]->y + 1;
        D_80369DB0[i].w = D_8028DB0C[i]->w - 9;
        D_80369DB0[i].h = D_8028DB0C[i]->h - 4;
        break;
      case 4:
        D_80369DB0[i].x = D_8028DB0C[i]->x + 6;
        D_80369DB0[i].y = D_8028DB0C[i]->y + 4;
        D_80369DB0[i].w = D_8028DB0C[i]->w - 12;
        D_80369DB0[i].h = D_8028DB0C[i]->h - 8;
        break;
      case 5:
        D_80369DB0[i].x = D_8028DB0C[i]->x + 4;
        D_80369DB0[i].y = D_8028DB0C[i]->y + 2;
        D_80369DB0[i].w = D_8028DB0C[i]->w - 8;
        D_80369DB0[i].h = D_8028DB0C[i]->h - 4;
        break;
      case 6:
      case 7:
        D_80369DB0[i].x = D_8028DB0C[i]->x + 2;
        D_80369DB0[i].y = D_8028DB0C[i]->y + 2;
        D_80369DB0[i].w = D_8028DB0C[i]->w - 6;
        D_80369DB0[i].h = D_8028DB0C[i]->h - 4;
        break;
      case 8:
        D_80369DB0[i].x = D_8028DB0C[i]->x + 2;
        D_80369DB0[i].y = D_8028DB0C[i]->y + 2;
        D_80369DB0[i].w = D_8028DB0C[i]->w - 4;
        D_80369DB0[i].h = D_8028DB0C[i]->h - 4;
        break;
      case 9:
        D_80369DB0[i].x = D_8028DB0C[i]->x + 4;
        D_80369DB0[i].y = D_8028DB0C[i]->y + 2;
        D_80369DB0[i].w = D_8028DB0C[i]->w - 8;
        D_80369DB0[i].h = D_8028DB0C[i]->h - 4;
        break;
      }
      D_8028DB0C[i]->kind = 99;
    }
    D_80369CD8[0].x = D_8028D470.x;
    D_80369CD8[0].y = D_8028D470.y + D_8028D470.h + 6;
    D_80369CD8[0].w = 0x20;
    D_8028D480.x = D_80369CD8[0].x + D_80369CD8[0].w + 2;
    D_80369CD8[0].h = 0x5e;
    D_8028D490.x = D_8028D480.x + D_8028D480.w + 0xe;
    D_8028D480.y = D_80369CD8[0].y;
    D_8028D490.y = D_80369CD8[0].y;
    y = D_80369CD8[0].y + D_8028D480.h + 2;
    for (r = 0; r < 3; r++) {
      x = D_8028D480.x;
      for (k = 0; k < 4; k++) {
        D_80369CD8[r * 4 + k + 1].x = x;
        x += 0x4a;
        D_80369CD8[r * 4 + k + 1].w = 0x48;
        D_80369CD8[r * 4 + k + 1].h = 0x16;
        D_80369CD8[r * 4 + k + 1].y = y;
      }
      y += 0x18;
    }
    D_8028CE40.y = D_8028CE10.y = D_80369CD8[0].y + 4;
    D_8028CE40.x = D_8028CE10.x = D_80369CD8[0].x;
    BrAllocPaintShopGfxMem(&D_8028CE10);
    BrAllocPaintShopGfxMem(&D_8028CE40);
    for (i = 0; i < 12; i++) {
      D_8028DAD8[i]->w = 0x40;
      D_8028DAD8[i]->h = 0xe;
      D_8028DAD8[i]->stripH = D_8028DAD8[i]->h;
      D_8028DAD8[i]->drawW = D_8028DAD8[i]->w;
      D_8028DAD8[i]->drawH = D_8028DAD8[i]->h;
      D_8028DAD8[i]->x = D_80369CD8[1 + i].x + 4;
      D_8028DAD8[i]->y = D_80369CD8[1 + i].y + 4;
      D_8028DAD8[i]->kind = 0;
      BrAllocPaintShopGfxMem(D_8028DAD8[i]);
    }
    BrAllocPaintShopGfxMem(&D_8028D110);
    BrAllocPaintShopGfxMem(&D_8028D140);
    BrAllocPaintShopGfxMem(&D_8028D170);
    BrAllocPaintShopGfxMem(&D_8028D1A0);
    BrAllocPaintShopGfxMem(&D_8028D1D0);
    BrAllocPaintShopGfxMem(&D_8028D200);
    BrAllocPaintShopGfxMem(&D_8028D230);
    BrAllocPaintShopGfxMem(&D_8028D260);
    BrAllocPaintShopGfxMem(&D_8028D290);
    BrAllocPaintShopGfxMem(&D_8028D0B0);
    BrAllocPaintShopGfxMem(&D_8028D0E0);
    BrAllocPaintShopGfxMem(&D_8028D2C0);
    D_8028D260.x = D_8028D230.x + 8;
    D_8028D260.y = D_8028D230.y + 8;
    D_8028D260.drawW = D_8028D230.drawW - 16;
    D_8028D260.drawH = D_8028D230.drawH - 16;
    for (i = 0; i < 4; i++) {
      D_8028DB34[i]->siz = D_8028DB44[i]->siz = 4;
      D_8028DB34[i]->w = D_8028DB44[i]->w = 0x40;
      D_8028DB34[i]->h = D_8028DB44[i]->h = 0x2a;
      D_8028DB34[i]->stripH = D_8028DB44[i]->stripH = D_8028DB34[i]->h;
      D_8028DB34[i]->drawW = D_8028DB44[i]->drawW = D_8028DB34[i]->w;
      D_8028DB34[i]->drawH = D_8028DB44[i]->drawH = D_8028DB34[i]->h;
      BrAllocPaintShopGfxMem(D_8028DB34[i]);
      BrAllocPaintShopGfxMem(D_8028DB44[i]);
    }
    D_8028DB78 = D_8028AB08->parts[D_8028AB08->decal[D_8028DB68]].tex;
    D_8028DB7C = D_8028AB08->mask[D_8028DB68];
    D_8028DB88 = D_8028AB08->parts[D_8028AB08->decal[D_8028DB68]].w;
    D_8028DB8C = D_8028AB08->parts[D_8028AB08->decal[D_8028DB68]].h;
    D_8028DB08 = D_8028DB0C[D_8028DB68];
    D_8028DB90 = D_8028AB08->parts[D_8028AB08->decal[D_8028DB68]].tlut;
    D_8028D4F0[0].x = D_8028CE10.x;
    D_8028D4F0[0].y = D_8028CE10.y;
    for (i = 1; i < 5; i++) {
      D_8028D4F0[i].x = D_8028CE10.x;
      D_8028D4F0[i].y = D_8028D4F0[i - 1].y + D_8028D4F0[i - 1].h;
    }
    for (i = 0; i < 2; i++) {
      D_80369B98[i].x = D_8028D480.x + 4 + i * 0x14;
      D_80369B98[i].w = 0x14;
      D_80369B98[i].h = 0xe;
      D_80369B98[i].y = D_8028D480.y + 4;
    }
    D_80369B98[2].w = 0x10;
    D_80369B98[2].h = 0xe;
    D_80369B98[2].x = D_8028D490.x + 4;
    D_80369B98[3].w = 0x10;
    D_80369B98[3].h = 0xe;
    D_80369B98[3].x = D_8028D490.x + 0x14;
    D_80369B98[2].y = D_80369B98[3].y = D_8028D490.y + 4;
    for (i = 4; i < 16; i++) {
      D_80369B98[i].x = D_8028D490.x + 0x24 + (i - 4) * 0x10;
      D_80369B98[i].w = 0x10;
      D_80369B98[i].h = 0xe;
      D_80369B98[i].y = D_8028D490.y + 4;
    }
    BrPaintPaletteLoad();
    y = D_8028D260.y;
    for (r = 0; r < 5; r++) {
      for (k = 0; k < 10; k++) {
        D_8028D540[r * 10 + k].x0 = D_8028D260.x + (D_8028D290.drawW + 4) * k;
        D_8028D540[r * 10 + k].x4 = y;
      }
      y += D_8028D290.drawH + 4;
    }
    D_8028DB94.w = D_8028DB88 * 4;
    D_8028DB94.h = D_8028DB8C * 4;
    D_8028DB94.x = D_8028D470.x + ((D_8028D470.w - D_8028DB88 * 4) >> 1);
    D_8028DB94.y = D_8028D470.y + ((D_8028D470.h - D_8028DB8C * 4) >> 1);
    D_8028DC80 = 1;
    D_802724F0 = 0;
    BrFadeTo(1.0f, D_802AB0FC);
  }
  BrClockTick();
  BrFadeStep();
  BrFrameBeginLayout1();
  BrMenuCameraSet((float)D_8028AAB0, (float)D_8028AAB4);
  BrViewportSet(0, 0, D_8028AAB0, D_8028AAB4, 1);
  BrZBufferClear();
  BrScreenClear(0, 0, 0);
  BrImageDrawTinted(&D_8028CB40, 0x71, 0xb, 0x40);
  BrImageDraw(&D_8028CB70);
  BrTextSetFont(0x22);
  BrTextAlignCentre();
  BrTextHighlightOff();
  BrTextPrint("%ryPAINT SHOP", 0xa0, 0x26);
  for (i = 0; i < 10; i++) {
    if (i == D_8028DB68) {
      BrImageDrawTinted(D_8028DB0C[i], 0x20, 200, 0xff);
    } else {
      BrImageDrawTinted(D_8028DB0C[i], 0x20, 0x10, 0xff);
    }
  }
  BrImageDrawTinted(&D_8028CDB0, 0xff, 0xff, 0xff);
  BrImageDrawTinted(&D_8028CDE0, 0xc0, 0, 0);
  BrFillRect(D_8028D470.x, D_8028D470.y, D_8028D470.w, D_8028D470.h, 0, 0, 0);
  BrTexLoad(D_8028AB08->decal[D_8028DB68], D_8028AB08->parts);
  BrTexRectFlipDraw(D_8028DB88, D_8028DB8C, D_8028DB94.x, D_8028DB94.y, D_8028DB94.w, D_8028DB94.h);
  BrPaintPaletteDraw();
  BrFillRect(D_80369CD8[0].x, D_80369CD8[0].y, D_80369CD8[0].w, D_80369CD8[0].h, 0, 0, 0);
  BrFillRect(D_80369CD8[0].x + 4, D_80369CD8[0].y + 4, D_80369CD8[0].w - 8, D_80369CD8[0].h - 8, 0xff, 0xca, 0);
  if (D_8028CE9C == 0) {
    BrImageDraw(&D_8028CE10);
  } else {
    BrImageDraw(&D_8028CE40);
  }
  BrFillRect(D_8028D4F0[D_8028DAC0].x, D_8028D4F0[D_8028DAC0].y, D_8028D4F0[D_8028DAC0].w,
             D_8028D4F0[D_8028DAC0].h, 0, 0, 0);
  if (D_8028CE9C == 0) {
    BrImageDrawPart(&D_8028CE10, 0,
                    D_8028CE10.y + D_8028CE10.h - D_8028D4F0[D_8028DAC0].y - D_8028D4F0[D_8028DAC0].h,
                    D_8028CE10.w, D_8028D4F0[D_8028DAC0].h, D_8028CE10.x - 1, D_8028D4F0[D_8028DAC0].y,
                    D_8028CE10.w, D_8028D4F0[D_8028DAC0].h, 0xff, 0xca, 0);
  } else {
    BrImageDrawPart(&D_8028CE40, 0,
                    D_8028CE40.y + D_8028CE40.h - D_8028D4F0[D_8028DAC0].y - D_8028D4F0[D_8028DAC0].h,
                    D_8028CE40.w, D_8028D4F0[D_8028DAC0].h, D_8028CE40.x - 1, D_8028D4F0[D_8028DAC0].y,
                    D_8028CE40.w, D_8028D4F0[D_8028DAC0].h, 0xff, 0xca, 0);
  }
  for (i = 1; i < 13; i++) {
    if (i == D_8028DB60) {
      BrFillRect(D_80369CD8[i].x, D_80369CD8[i].y, D_80369CD8[i].w, D_80369CD8[i].h, 0x20, 200, 0xff);
    } else {
      BrFillRect(D_80369CD8[i].x, D_80369CD8[i].y, D_80369CD8[i].w, D_80369CD8[i].h, 0, 0, 0);
    }
    BrFillRect(D_80369CD8[i].x + 4, D_80369CD8[i].y + 4, D_80369CD8[i].w - 8, D_80369CD8[i].h - 8, 0xff, 0xca, 0);
    BrImageDraw(D_8028DAD8[i - 1]);
  }
  if (D_8028DBD8 != 0) {
    D_8028AA80 = 0;
    BrFogSetup();
    BrFrameTintSetup();
    BrPaintCarView();
    BrMenuCameraSet((float)D_8028AAB0, (float)D_8028AAB4);
    BrViewportSet(0, 0, D_8028AAB0, D_8028AAB4, 1);
  }
  if (BrPaintCursorInRect(&D_8028DB94)) {
    BrPaintClick();
    BrPaintStickMove();
    switch (D_8028DB60) {
    case 1:
      if (D_8028DBEC != 0) {
        BrImageDrawAt(&D_8028D200, D_8028D110.x + 1, D_8028D110.y - 0x18);
      } else {
        BrImageDrawAt(&D_8028D170, D_8028D110.x + 1, D_8028D110.y - 0x18);
      }
      break;
    case 3:
    case 5:
    case 6:
      if (D_8028DBEC != 0) {
        BrImageDrawAt(&D_8028D200, D_8028D110.x, D_8028D110.y - 0x18);
      } else {
        BrImageDrawAt(&D_8028D1A0, D_8028D110.x - 8, D_8028D110.y - 8);
      }
      break;
    case 4:
      if (D_8028DBC4 == 0) {
        if (D_8028DBEC != 0) {
          BrImageDrawAt(&D_8028D200, D_8028D110.x, D_8028D110.y - 0x18);
        } else {
          BrImageDrawAt(&D_8028D1D0, D_8028D110.x - (D_8028D1D0.w >> 1), D_8028D110.y - D_8028D1D0.h);
        }
      } else {
        BrImageDraw(&D_8028D110);
      }
      break;
    case 2:
      if (D_8028DBEC != 0) {
        BrImageDrawAt(&D_8028D200, D_8028D110.x, D_8028D110.y - 0x18);
      } else {
        BrImageDrawAt(&D_8028D140, D_8028D110.x - 6, D_8028D110.y - 0x17);
      }
      break;
    default:
      BrImageDraw(&D_8028D110);
      break;
    }
    } else {
    switch (D_8028DB60) {
    case 7:
      BrPaintMirrorMenu();
      break;
    case 8:
      BrPaintClearMenu();
      break;
    case 9:
      if (D_802724F0 == 0) {
        D_802724F0 = BrDecalPakTransfer(D_8028AB08, D_8028DBBC, 9, 0, &done);
      } else {
        BrPakMessage(D_802724F0, 9, 0);
        if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 0x30) {
          BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 0x30);
          D_802724F0 = 0;
          D_8028DB60 = D_8028DB64;
        }
      }
      break;
    case 10:
      if (D_802724F0 == 0) {
        D_802724F0 = BrDecalPakTransfer(D_8028AB08, D_8028DBBC, 10, 0, &done);
      } else {
        BrPakMessage(D_802724F0, 10, 0);
        if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 0x30) {
          BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 0x30);
          D_802724F0 = 0;
          D_8028DB60 = D_8028DB64;
        }
      }
      break;
    case 12:
      BrPaintExitPrompt();
      break;
    default:
      if (D_8028DBE0 != 0) {
        if (D_8028DB60 == 1) {
          BrPaintBrushSelect();
        } else if (D_8028DB60 == 4) {
          BrPaintTextStyleMenu();
        } else if (D_8028DB60 == 5) {
          BrPaintStyleSelect();
        } else if (D_8028DB60 == 6) {
          BrPaintOvalStyleSelect();
        }
      } else if (D_8028DBD4 != 0) {
        BrPaintColourMix();
      } else {
        BrPaintClick();
        BrPaintStickMove();
        BrImageDraw(&D_8028D110);
      }
      break;
    }
  }
  gDPPipeSync(D_8028A858++);
  BrFadeBarsDraw();
  if (D_8028AA98 > 1) {
    BrViewportSet(0, 0, D_8028AAB0, D_8028AAB4, 1);
    BrPerfMeterDraw();
  }
  BrFrameEnd();
  if (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 0x20) {
    if (BrPaintCursorInRect(&D_8028DB94) && D_8028DBC0 == 0 && D_8028DBC4 == 0) {
      D_8028DB58 = BrPaintGet((D_8028D110.x - D_8028DB94.x) >> 2, (D_8028DB94.y + D_8028DB94.h - D_8028D110.y) >> 2);
      D_8028DBEC = 1;
    } else if (D_8028DBC0 != 0 || D_8028DBC4 != 0) {
      BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 0x20);
      D_8028DBC4 = 0;
      D_8028DBC0 = 0;
      D_8028DBD8 = 1;
      D_8028DBC8 = 1;
      D_8028DBA8 = 0;
      D_8028DBAC = 0;
    }
  } else {
    D_8028DBEC = 0;
  }
  if (D_8028DBE8 != 0 && BrFadeOutDone()) {
    D_8028DBE8 = 0;
    BrFrameBeginLayout1();
    BrScreenClear(0, 0, 0);
    BrFrameEnd();
    BrFrameBeginLayout1();
    BrScreenClear(0, 0, 0);
    BrFrameEnd();
    BrPaintReset();
    D_8028DC80 = 0;
    D_802724F0 = 0;
    D_8028C334 = 0;
    D_802A4BE8 = 1;
    BrModeSet(BrFrontReturnToTitle);
  }
}
