/* paint.c -- the paint shop: the screen, its menus, the decal pak transfer,
 * the image drawing it shares, and every paint tool.  One object in the ROM,
 * from BrAllocPaintShopGfxMem (padding precedes it) to BrPaintPresetApply
 * (BrPaintCharset, padded too, starts cpakmenu.c's object).  The functions
 * are in ROM order; the initialised data is defined in its ROM .data order.
 *
 * The .bss order.  IDO sorts an object's .bss by each variable's declaration
 * index mod 256.  The index advances by one for each data definition, each
 * extern variable at its first reference, each string literal, each called
 * function at its first reference, and two for each function definition
 * (three for one written `(void)`, so definitions are written `()`).  The
 * ROM's order needs BrPaintClick's flag at index 512 in Japan (flag first,
 * the four anchors last) and its first anchor at 512 + 3 in the USA and
 * Europe (all five first), with the palette group above 256 + 7 in every
 * region.  As SM64 does with prevent_bss_reordering.h, the source carries
 * the difference explicitly: a dead block at the end of BrPaintCarView,
 * just before the palette group, takes the first reference of fifteen
 * externs the screen code uses later and of 22 index pads.  It compiles to
 * nothing.  The descriptors' ROM ranges are per-asset start and end linker
 * symbols, as the game's code reads its assets.
 */
#include "tgr/common.h"
#include "tgr/car.h"
#include "tgr/gbi.h"
#include "tgr/pad.h"

typedef struct BrPaintRect { int x, y, w, h; } BrPaintRect;
typedef struct BrImage {        /* a front-end image (0x30 bytes) */
  unsigned char *data;
  int x4;
  int x8;
  unsigned char siz;            /* 0x0C */
  char pad0d[3];
  int w;                        /* 0x10 */
  unsigned int h;               /* 0x14 */
  unsigned int stripH;          /* 0x18 */
  int x;                        /* 0x1C */
  int y;                        /* 0x20 */
  int drawW;                    /* 0x24 */
  int drawH;                    /* 0x28 */
  unsigned char kind;           /* 0x2C */
  char pad2d[3];
} BrImage;
typedef struct BrIfaceImage {   /* the same record as the interface pool sees it */
  unsigned int data;            /* 0x00  RAM copy, from the interface pool */
  int rom;                      /* 0x04  compressed image in ROM */
  int x8;
  unsigned char depth;          /* 0x0C  bits per pixel */
  char pad0d[3];
  unsigned int w;               /* 0x10 */
  unsigned int h;               /* 0x14 */
} BrIfaceImage;
typedef struct BrPaintSwatch {  /* a palette entry (0x14) */
  int x, y, w, h;
  unsigned char r, g, b;
} BrPaintSwatch;
typedef struct BrGlyph {        /* a keyboard key (0x1C) */
  int x0;
  int x4;
  unsigned char c;
  char pad9[3];
  int off;
  int w;
  int kernL;
  int kernR;
} BrGlyph;
typedef struct BrPaintPart {    /* a model part the paint shop edits (0x24 bytes) */
  unsigned char *tex;           /* 0x00  its texture, 4 bits a pixel */
  unsigned short *tlut;         /* 0x04  its palette */
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
  unsigned char paint[2];       /* 0x11A  the parts that share the palette */
  unsigned char **mask;         /* 0x11C  per decal: the pixels it keeps, 0 for all */
} BrPaintModel;
typedef struct BrDecalPart {    /* a model part, as the decal code reads it (0x24) */
  unsigned char *tex;
  char pad04[8];
  unsigned short w;             /* 0x0C */
  unsigned short h;             /* 0x0E */
  char pad10[0x24 - 0x10];
} BrDecalPart;
typedef struct BrDecalModel {
  char pad00[0x14];
  BrDecalPart *parts;           /* 0x14 */
  char pad18[0x110 - 0x18];
  unsigned char decalPart[12];  /* 0x110  each decal slot's part */
  void **masks;                 /* 0x11C  each slot's paint mask */
} BrDecalModel;

extern BrPaintModel *D_8028AB08;        /* the car model being painted */











unsigned int osGetCount(void);
void osSyncPrintf(char *fmt, ...);
void *memcpy(void *dst, void *src, unsigned int n);
int sprintf(char *buf, const char *fmt, ...);
unsigned char *BrIfaceMemAlloc(int size);
void BrAllocPaintShopGfxMem(BrIfaceImage *img);
int BrPaintCursorInRect(int *r);
void BrPadConsume(unsigned int *pad, unsigned int bits);
void BrFadeTo(float level, float seconds);
void BrTextSetColours(int r, int g, int b, int a, int x, int y);
void BrTextSetFont(int size);
#if TGR_REGION_EUR || TGR_REGION_JPN
void BrImageStrip(BrImage *img, unsigned char *data, void *tlut, int w, int h, int x, int y, int dw,
                  int dh, unsigned char r, unsigned char g, unsigned char b);
#else
void BrImageStrip(BrImage *img, unsigned char *data, int w, int h, int x, int y, int dw, int dh,
                  unsigned char r, unsigned char g, unsigned char b);
#endif
void BrBevelPanel(int x, int y, int w, int h, int bevel, char noFill, char sunken, unsigned char r, unsigned char g, unsigned char b);
extern BrGlyph *D_80369E68[16];         /* the typed text's keys */
extern char D_80369EA8[16];             /* the typed text */
extern BrPaintSwatch D_80369B98[16];
extern BrPaintRect D_80369CD8[13];
extern unsigned char D_80369DA8[3];
extern unsigned char D_80369DAB;
extern BrPaintRect D_80369DB0[10];
#define PADS D_8036A8E0
#define BR_MSEC() ((unsigned int)((((u64)osGetCount() * 1000000) / osClockRate) / 1000))

/* ---- declarations: the frame helpers ---- */
void BrFrameBegin(int hires);
void BrScissorSet(int x, int y, int w, int h);
void BrFrameStatsReset(void);
void BrScreenClear(int r, int g, int b);
void BrFrameBeginLayout1(void);
void BrFrameEnd(void);
void osViBlack(char param_1);
extern int D_8028A884;
void BrFrameBeginLayout0(void);
extern int D_8028AA08;
extern int D_8028AA0C;
extern int D_8028AA10;
extern int D_8028AA2C;
extern int D_8028AA30;
extern int D_8028AA34;
extern int D_8028AA38;
extern int D_8028AA3C;
extern Gfx *D_8028A858;
extern int D_8028A850;
extern int D_8028B740;
extern int D_8028B744;
extern int D_8028B748;
extern int D_8028B74C;
extern int D_8028AAB0;
extern int D_8028AAB4;
extern int D_8028A85C;                 /* the frame buffer being drawn */
extern unsigned int D_8031AA28[];      /* the frame buffers */
extern char D_00000400[];              /* the Z-buffer (a link-time address) */
void BrPerfMark(int bar, int r, int g, int b, int a);
void BrFramePoolsReset(void);
void osViSetSpecialFeatures(unsigned int func);
typedef struct BrViewRect { int x; int y; int w; int h; int x10; } BrViewRect;
extern BrViewRect D_8031B2C8[2];        /* the players' views */
extern int D_8028AB0C;                  /* number of players */
extern int D_8028A84C;                  /* resolution changed */
extern Gfx (*D_8028A848)[6000];         /* the two display-list buffers */
extern int D_8028AA68;                  /* texture filtering on */
extern int D_8028A898;                  /* the texture filter mode */
extern int D_8028A8A0;                  /* the colour dither mode */
extern int D_8028A89C;                  /* the alpha dither mode */
extern int D_8028AA50;                  /* z-buffering on */
extern int D_8028AA4C;                  /* gamma off */
extern int D_8028A8A4;                  /* the viewport being used */
extern char D_8028A900[];               /* the viewports (0x28 bytes each) */
extern char D_801B5000[];               /* the low-res frame buffers */
extern char D_801DA800[];
extern char D_0028A8C0[];               /* the identity matrix, as a physical address */
typedef struct BrTask {         /* an OSTask (0x40 bytes) */
  unsigned int type;
  unsigned int flags;
  void *ucode_boot;
  unsigned int ucode_boot_size;
  void *ucode;
  unsigned int ucode_size;
  void *ucode_data;
  unsigned int ucode_data_size;
  void *dram_stack;
  unsigned int dram_stack_size;
  void *output_buff;
  void *output_buff_size;
  void *data_ptr;
  unsigned int data_size;
  void *yield_data_ptr;
  unsigned int yield_data_size;
} BrTask;
extern BrTask D_8031A9A8[2];            /* one graphics task per display list */
extern char rspbootTextStart[];
extern char rspbootTextEnd[];
extern char gspF3DEX_fifoTextStart[];
extern char gspF3DEX_fifoDataStart[];
extern char D_8031A598[];               /* the RSP's DRAM stack */
extern long long *D_8028A860[2];        /* the RDP FIFO: start and end */
extern int D_8028AB84;                  /* FIFO words kept back */
extern int D_8028AB70;                  /* display-list commands this frame */
extern int D_8028AB7C;                  /* the most commands in one frame */
extern int *D_8028C75C;                 /* matrix pool: next and start */
extern int *D_8028C760;
extern int D_8028AB74;                  /* the most matrices in one frame */
typedef struct { char x[16]; } BrVtx16;
extern BrVtx16 *D_8028C764;             /* vertex pool: next and start */
extern BrVtx16 *D_8028C768;
extern int D_8028AB78;                  /* the most vertices in one frame */
extern char D_8031A320[];               /* RSP done queue */
extern char D_8031A358[];               /* RDP done queue */
extern char D_8031A390[];               /* retrace queue */
extern void (*D_8031B31C)(void);        /* run once the RSP has finished */
extern int D_8031B320;
extern int D_8028AA94;                  /* debug: copy the low-memory frame into the display */
extern int D_80000400[];
extern int D_8028AAE0;                  /* CPU time of the last frame */
extern int D_8028AAE4;
extern int D_8028AAE8;
extern int D_8028A854;                  /* resolution the VI is set to */
extern int osTvType;
extern char D_802A5D70[];               /* VI modes: low res MPAL, NTSC; high res MPAL, NTSC */
extern char D_802A54B0[];
extern char D_802A6040[];
extern char D_802A5780[];
extern int D_8028A888;                  /* frames the screen stays blank */
extern int D_8028AA20;
extern int D_8028AA24;
void BrFatal(char *msg);
void osWritebackDCacheAll(void);
void osRecvMesg(void *mq, void *msg, int flag);
void osViSetMode(void *mode);
void osViSwapBuffer(void *fb);
void *osViGetCurrentFramebuffer(void);
void BrPerfFrameStart(void);
void osSpTaskLoad(BrTask *t);
void osSpTaskStartGo(BrTask *t);
#if TGR_REGION_JPN
void BrScreenBlank(void);
#define BLANK_SCREEN() BrScreenBlank()
#elif TGR_REGION_EUR
void BrVideoBlank(void);
#define BLANK_SCREEN() BrVideoBlank()
#else
#define BLANK_SCREEN() osViBlack(1)
#endif
/* ---- declarations: the interface pool ---- */
extern unsigned int D_80369B70;
int BrRomReadSize(int rom);
unsigned int BrRomUnpack(unsigned int dst, int rom, unsigned int *size);
extern int D_80272500;
/* ---- declarations: the paint tools ---- */







typedef struct BrPaintBox {
  int pad[7];
  int x;                        /* 0x1C */
  int y;                        /* 0x20 */
  int w;                        /* 0x24  may be negative (mirrored) */
  int h;                        /* 0x28 */
} BrPaintBox;





extern char D_8036A8F8;




typedef struct BrPaintArea { int x, y, w, h; } BrPaintArea;
extern BrCar *D_8028AAF0;               /* the car shown */
extern BrCarCam *D_8028AAF4;            /* its camera */






extern BrVec3 D_8028DC08[];             /* the preset view directions */
extern float D_8028AAC0;
extern float D_8028AAC8;
extern int D_8028C334;
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






















void BrTextAlignLeft(void);
void BrTextHighlightOff(void);
void BrTextPrint(char *s, int x, int y);
void BrTextAlignCentre(void);
void BrPadStickToButtons(BrPadRec *pad);


typedef struct { char *n[4]; } BrPaintNames;
extern BrPaintNames D_8028DCF4;        /* the four style names */

extern BrPaintNames D_8028DD04;        /* and their names */
#if TGR_REGION_EUR || TGR_REGION_JPN
void BrImageDraw(BrImage *img, void *tlut);
#define BR_IMAGE_DRAW(img) BrImageDraw(img, 0)
#else
void BrImageDraw(BrImage *img);
#define BR_IMAGE_DRAW(img) BrImageDraw(img)
#endif




typedef struct { char *n[2]; } BrPaintNames2;
extern BrPaintNames2 D_8028DCE4;       /* "ROUND BRUSH", "SQUARE BRUSH" */


typedef struct { int y, xl, xr, dy; } BrFillSeg;
/* ---- declarations: the paint shop screen ---- */
extern BrCar D_8031B760[];
extern int D_80271FA8;                  /* the player's controller */
extern int D_8028B7F4;
extern int D_8028C328;
extern short D_802A4BE8;
extern signed char D_8028DC80;        /* the screen has been set up */
extern int D_802724F0;                  /* the Controller Pak transfer under way */
extern int D_8028AA80;                  /* night */
extern int D_8028AA98;                  /* the performance meter */



























int BrCarDefaultColour(BrCar *car);
void BrCarCamStep(void);
void BrEntLoadRecord(BrCar *car, int slot, int kind);
void BrVec3Negate(BrVec3 *out, BrVec3 *v);
void BrIfaceMemReset(void);
int BrFadeOutDone(void);
void BrFadeStep(void);
void BrFadeBarsDraw(void);
void BrClockTick(void);
void BrMenuCameraSet(float w, float h);
void BrZBufferClear(void);
void BrFogSetup(void);
void BrFrameTintSetup(void);
void BrPerfMeterDraw(void);
void BrModeSet(void (*mode)(void));
void BrFrontReturnToTitle(void);
#if TGR_REGION_EUR || TGR_REGION_JPN
#else
#endif
void BrTexLoad(int n, BrPaintPart *tbl);
void BrPakMessage(int msg, char op, char mode);
int BrDecalPakTransfer(BrPaintModel *m, unsigned char port, char op, char fromMenu, unsigned char *done);
void BrPaintClick(void);
void BrPaintBrushSelect(void);
void BrPaintTextStyleMenu(void);
void BrPaintStyleSelect(void);
void BrPaintOvalStyleSelect(void);
void BrPaintMirrorMenu(void);
void BrPaintClearMenu(void);
void BrPaintExitPrompt(void);
#if TGR_REGION_JPN

extern char jpn_D_001DC050[];
extern char jpn_D_001DC070[];
void BrRomRead(void *dst, void *rom, int len);
#endif
/* ---- declarations: the image drawing ---- */
#if TGR_REGION_EUR || TGR_REGION_JPN
#else
#endif
void BrTexSizeBits(unsigned int v, int *mask, int *bits);
#define TXL2WORDS(txls, b_txl)  MAX(1, ((txls) * (b_txl) / 8))
#define CALC_DXT(width, b_txl)  (((1 << 11) + TXL2WORDS(width, b_txl) - 1) / TXL2WORDS(width, b_txl))
#define TXL2WORDS_4b(txls)      MAX(1, ((txls) / 16))
#define CALC_DXT_4b(width)      (((1 << 11) + TXL2WORDS_4b(width) - 1) / TXL2WORDS_4b(width))
#if TGR_REGION_EUR || TGR_REGION_JPN
#define STRIP_TLUT(t) t,
#else
#define STRIP_TLUT(t)
#endif
typedef struct OSPfs {          /* libultra's Controller Pak handle (0x68 bytes) */
  int status;
  void *queue;
  int channel;
  unsigned char id[32];         /* 0x0C */
  char pad2c[0x68 - 0x2C];
} OSPfs;
extern OSPfs D_80369EC0[4];
extern OSPfs D_8031A3F8[4];
extern unsigned char D_803163E0[4][32];
extern unsigned char D_80316420[4];
extern unsigned char D_8031B1E8[4];
extern char D_80272D48[];
int bcmp(void *a, void *b, unsigned int n);
int osPfsIsPlug(void *mq, unsigned char *pattern);
int osPfsInitPak(void *mq, OSPfs *pfs, int channel);
int osPfsRepairId(OSPfs *pfs);
int osMotorInit(void *mq, OSPfs *pfs, int channel);
int osMotorStop(OSPfs *pfs);
int osPfsFindFile(OSPfs *pfs, unsigned short company, unsigned int game, unsigned char *name,
                  unsigned char *ext, int *file);
int osPfsChecker(OSPfs *pfs);
int osPfsReadWriteFile(OSPfs *pfs, int file, unsigned char flag, int offset, int size,
                       unsigned char *buf);
int osPfsFreeBlocks(OSPfs *pfs, int *bytes);
int osPfsAllocateFile(OSPfs *pfs, unsigned short company, unsigned int game, unsigned char *name,
                      unsigned char *ext, int size, int *file);
void BrSfxFadeTo(float level, float seconds);
void BrMusicFadeTo(float level, float seconds);
int BrSfxFadeDone(void);
#if TGR_REGION_JPN
#endif
#if TGR_REGION_JPN
#define COMPANY 0x3238          /* "28" */
#define GAME 0x4E54524A         /* "NTRJ" */
#elif TGR_REGION_EUR
#define COMPANY 0x3746          /* "7F" */
#define GAME 0x4E545250         /* "NTRP" */
#else
#define COMPANY 0x3544          /* "5D" */
#define GAME 0x4E475245         /* "NGRE" */
#endif
#define OP_LOAD 9
#define PAD (&D_8036A8E0[port])
#define PFS (&D_80369EC0[port])
/* ---- declarations: the clear and mirror popups ---- */

/* ---- declarations: the colour mixer ---- */
typedef struct BrMixSwatch {    /* a palette entry, as the mixer reads it (0x14) */
  int x, y, w, h;
  unsigned char c[3];           /* 0x10  red, green, blue */
} BrMixSwatch;
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
extern unsigned char D_8028DCE0;        /* the channel being mixed */
void BrTextAlignRight(void);
void BrPadStickRepeatPos(unsigned int *pressed, int *timer, float *axis, unsigned int bit);
void BrPadStickRepeatNeg(unsigned int *pressed, int *timer, float *axis, unsigned int bit);
/* ---- declarations: the click handler ---- */
typedef unsigned long long u64;
extern u64 osClockRate;
float sqrtf(float x);
typedef struct BrClickSwatch {  /* a palette entry (0x14) */
  int x, y, w, h;
  unsigned char c[3];           /* 0x10 */
} BrClickSwatch;

/* ---- declarations: the text-style popup ---- */

/* the functions defined below */
void BrScreenFlush3Layout1(void);
void BrScreenFlush3Layout0(void);
void BrPaintCarView(void);
void BrPaintReset(void);
void BrPaintDecalCommit(void);
void BrPaintDecalApply(void);
void BrImageDrawAt(BrImage *img, int x, int y);
void BrImageDrawTinted(BrImage *img, unsigned char r, unsigned char g, unsigned char b);
void BrImageDrawRect(BrImage *img, int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b);
void BrImageDrawPart(BrImage *img, int s, int t, int sw, int th, int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b);
void BrFillPoint(int x, int y, unsigned char v);
void BrFillFrame(int x, int y, int w, int h, int t, unsigned char r, unsigned char g, unsigned char b);
void BrFillRect(int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b);
void BrBevelPanel(int x, int y, int w, int h, int bevel, char noFill, char sunken, unsigned char r, unsigned char g, unsigned char b);
void BrPaintMirrorSide(void);
void BrPaintFlipApply(void);
void BrPaintKeyboard(void);
void BrPaintColourMix(void);
void BrPaintStickMove(void);
int BrPaintCursorInBox(BrPaintBox *b);
int BrPaintCursorInRect(int *r);
int BrPaintCursorInRect2(int *r);
int BrPaintCursorInBrush(int *r);
void BrPaintPaletteLoad(void);
void BrPaintPaletteDraw(void);
unsigned char BrPaintGet(int x, int y);
unsigned char BrPaintPeek(unsigned char *tex, int x, int y);
void BrPaintFloodFill(int x, int y);
void BrPaintTextStamp(int sx, int sy);
void BrPaintLine(int x0, int y0, int x1, int y1);
void BrPaintPlot(int x, int y, unsigned char c);
void BrPaintFillRect(int sx0, int sy0, int sx1, int sy1);
void BrPaintFrameRect(int sx0, int sy0, int sx1, int sy1);
void BrPaintFillRoundRect(int sx0, int sy0, int sx1, int sy1);
void BrPaintFrameRoundRect(int sx0, int sy0, int sx1, int sy1);
void BrPaintFillOval(int sx0, int sy0, int sx1, int sy1);
void BrPaintFrameOval(int sx0, int sy0, int sx1, int sy1);
void BrPaintDisc(int x, int y, int r, unsigned char screen);
void BrPaintCircle(int sx, int sy, int r);
void BrPaintDashLine(int x0, int y0, int x1, int y1);
void BrPaintDashRect(int x0, int y0, int x1, int y1);
void BrPaintDashRoundRect(int x0, int y0, int x1, int y1);
void BrPaintDashOval(int x0, int y0, int x1, int y1);
void BrPaintDashCircle(int cx, int cy, int r);
void BrPaintTextDraw(int x, int y);
void BrSwapBytes(char *param_1,char *param_2);
void BrTexRectFlipDraw(int tw, int th, int x, int y, int w, int h);
void BrPaintPresetApply(void);


/* ---- the initialised data, in its ROM order ---- */
#if TGR_REGION_JPN
extern char D_001D7140_ROM_START[], D_001D7140_ROM_END[];
extern char D_000F8B60_ROM_START[], D_000F8B60_ROM_END[];
extern char D_001DAEE0_ROM_START[], D_001DAEE0_ROM_END[];
extern char D_001DB9B0_ROM_START[], D_001DB9B0_ROM_END[];
extern char D_001DB800_ROM_START[], D_001DB800_ROM_END[];
extern char D_001DB330_ROM_START[], D_001DB330_ROM_END[];
extern char D_001DB4A0_ROM_START[], D_001DB4A0_ROM_END[];
extern char D_001DBA80_ROM_START[], D_001DBA80_ROM_END[];
extern char D_001DBC30_ROM_START[], D_001DBC30_ROM_END[];
extern char D_001DBD60_ROM_START[], D_001DBD60_ROM_END[];
extern char D_001DBEA0_ROM_START[], D_001DBEA0_ROM_END[];
extern char D_001DBF00_ROM_START[], D_001DBF00_ROM_END[];
extern char D_001DBFA0_ROM_START[], D_001DBFA0_ROM_END[];
extern char D_001DC010_ROM_START[], D_001DC010_ROM_END[];
extern char D_001DC070_ROM_START[], D_001DC070_ROM_END[];
extern char D_001DC0F0_ROM_START[], D_001DC0F0_ROM_END[];
extern char D_001DC130_ROM_START[], D_001DC130_ROM_END[];
extern char D_001DC1B0_ROM_START[], D_001DC1B0_ROM_END[];
extern char D_001DC210_ROM_START[], D_001DC210_ROM_END[];
extern char D_001DC250_ROM_START[], D_001DC250_ROM_END[];
extern char D_001DC2B0_ROM_START[], D_001DC2B0_ROM_END[];
extern char D_001DC340_ROM_START[], D_001DC340_ROM_END[];
extern char D_001DC3C0_ROM_START[], D_001DC3C0_ROM_END[];
extern char D_001DC460_ROM_START[], D_001DC460_ROM_END[];
extern char D_001DC4F0_ROM_START[], D_001DC4F0_ROM_END[];
extern char D_001DC540_ROM_START[], D_001DC540_ROM_END[];
extern char D_001DC5A0_ROM_START[], D_001DC5A0_ROM_END[];
extern char D_001DC7B0_ROM_START[], D_001DC7B0_ROM_END[];
extern char D_001DCBC0_ROM_START[], D_001DCBC0_ROM_END[];
extern char D_001DCC90_ROM_START[], D_001DCC90_ROM_END[];
extern char D_001DCD20_ROM_START[], D_001DCD20_ROM_END[];
extern char D_001DCD80_ROM_START[], D_001DCD80_ROM_END[];
extern char D_001DCDC0_ROM_START[], D_001DCDC0_ROM_END[];
extern char D_001DCE00_ROM_START[], D_001DCE00_ROM_END[];
extern char D_001DCE70_ROM_START[], D_001DCE70_ROM_END[];
extern char D_001DF910_ROM_START[], D_001DF910_ROM_END[];
extern char D_001DF930_ROM_START[], D_001DF930_ROM_END[];
extern char D_001DF950_ROM_START[], D_001DF950_ROM_END[];
extern char D_001DFCA0_ROM_START[], D_001DFCA0_ROM_END[];
extern char D_001DFCD0_ROM_START[], D_001DFCD0_ROM_END[];
extern char D_001DFD00_ROM_START[], D_001DFD00_ROM_END[];
extern char D_001DFD70_ROM_START[], D_001DFD70_ROM_END[];
extern char D_001DFE10_ROM_START[], D_001DFE10_ROM_END[];
extern char D_001DFEF0_ROM_START[], D_001DFEF0_ROM_END[];
extern char D_001E0030_ROM_START[], D_001E0030_ROM_END[];
extern char D_001E0100_ROM_START[], D_001E0100_ROM_END[];
BrImage D_8028CB40 = { 0, (int)D_001D7140_ROM_START, (int)D_001D7140_ROM_END, 4, { 0 }, 0x140, 0xF0, 0x18, 0x0, 0x0, 0x280, 0x1E0, 0 };
BrImage D_8028CB70 = { 0, (int)D_000F8B60_ROM_START, (int)D_000F8B60_ROM_END, 16, { 0 }, 0x118, 0x22, 0x2, 0x2E, 0x16, 0x230, 0x44, 0 };
BrImage D_8028CBA0 = { 0, (int)D_001DAEE0_ROM_START, (int)D_001DAEE0_ROM_END, 8, { 0 }, 0x30, 0x46, 0x46, 0x0, 0x0, 0x30, 0x46, 0 };
BrImage D_8028CBD0 = { 0, (int)D_001DB9B0_ROM_START, (int)D_001DB9B0_ROM_END, 8, { 0 }, 0x20, 0x2E, 0x2E, 0xBA, 0xA2, 0x20, 0x2E, 0 };
BrImage D_8028CC00 = { 0, (int)D_001DB9B0_ROM_START, (int)D_001DB9B0_ROM_END, 8, { 0 }, 0x20, 0x2E, 0x2E, 0x5E, 0xA2, -32, 0x2E, 0 };
BrImage D_8028CC30 = { 0, (int)D_001DB800_ROM_START, (int)D_001DB800_ROM_END, 8, { 0 }, 0x20, 0x2A, 0x2A, 0xBA, 0xD0, 0x20, 0x2A, 0 };
BrImage D_8028CC60 = { 0, (int)D_001DB800_ROM_START, (int)D_001DB800_ROM_END, 8, { 0 }, 0x20, 0x2A, 0x2A, 0x5E, 0xD0, -32, 0x2A, 0 };
BrImage D_8028CC90 = { 0, (int)D_001DB330_ROM_START, (int)D_001DB330_ROM_END, 8, { 0 }, 0x30, 0x2A, 0x2A, 0x84, 0xD0, 0x30, 0x2A, 0 };
BrImage D_8028CCC0 = { 0, (int)D_001DB4A0_ROM_START, (int)D_001DB4A0_ROM_END, 8, { 0 }, 0x30, 0x5C, 0x2E, 0x84, 0x78, 0x30, 0x5C, 0 };
BrImage D_8028CCF0 = { 0, (int)D_001DBA80_ROM_START, (int)D_001DBA80_ROM_END, 8, { 0 }, 0x20, 0x30, 0x30, 0xB8, 0x74, 0x20, 0x30, 0 };
BrImage D_8028CD20 = { 0, (int)D_001DBA80_ROM_START, (int)D_001DBA80_ROM_END, 8, { 0 }, 0x20, 0x30, 0x30, 0x60, 0x74, -32, 0x30, 0 };
BrImage D_8028CD50 = { 0, (int)D_001DBC30_ROM_START, (int)D_001DBC30_ROM_END, 8, { 0 }, 0x30, 0x18, 0x18, 0x84, 0xF8, 0x30, 0x18, 0 };
BrImage D_8028CD80 = { 0, (int)D_001DBD60_ROM_START, (int)D_001DBD60_ROM_END, 8, { 0 }, 0x30, 0x18, 0x18, 0x84, 0x60, 0x30, 0x18, 0 };
BrImage D_8028CDB0 = { 0, (int)D_001DBEA0_ROM_START, (int)D_001DBEA0_ROM_END, 4, { 0 }, 0x30, 0xA, 0xA, 0x0, 0x0, 0x30, 0xA, 0 };
BrImage D_8028CDE0 = { 0, (int)D_001DBF00_ROM_START, (int)D_001DBF00_ROM_END, 4, { 0 }, 0x30, 0xE, 0xE, 0x0, 0x0, 0x30, 0xE, 0 };
BrImage D_8028CE10 = { 0, (int)D_001DBFA0_ROM_START, (int)D_001DBFA0_ROM_END, 4, { 0 }, 0x20, 0x56, 0x56, 0x0, 0x0, 0x20, 0x56, 0 };
BrImage D_8028CE40 = { 0, (int)D_001DC010_ROM_START, (int)D_001DC010_ROM_END, 4, { 0 }, 0x20, 0x56, 0x56, 0x0, 0x0, 0x20, 0x56, 0 };
unsigned char jpn_D_8028F020[0x20] = { 0 };
BrImage D_8028CE70 = { 0, (int)D_001DC070_ROM_START, (int)D_001DC070_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CEA0 = { 0, (int)D_001DC0F0_ROM_START, (int)D_001DC0F0_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CED0 = { 0, (int)D_001DC130_ROM_START, (int)D_001DC130_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CF00 = { 0, (int)D_001DC1B0_ROM_START, (int)D_001DC1B0_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CF30 = { 0, (int)D_001DC210_ROM_START, (int)D_001DC210_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CF60 = { 0, (int)D_001DC250_ROM_START, (int)D_001DC250_ROM_END, 4, { 0 }, 0x30, 0xC, 0xC, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CF90 = { 0, (int)D_001DC2B0_ROM_START, (int)D_001DC2B0_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CFC0 = { 0, (int)D_001DC340_ROM_START, (int)D_001DC340_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CFF0 = { 0, (int)D_001DC3C0_ROM_START, (int)D_001DC3C0_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D020 = { 0, (int)D_001DC460_ROM_START, (int)D_001DC460_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D050 = { 0, (int)D_001DC4F0_ROM_START, (int)D_001DC4F0_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D080 = { 0, (int)D_001DC540_ROM_START, (int)D_001DC540_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D0B0 = { 0, (int)D_001DC5A0_ROM_START, (int)D_001DC5A0_ROM_END, 16, { 0 }, 0x18, 0x18, 0x18, 0x6C, 0xDC, 0x18, 0x18, 0 };
BrImage D_8028D0E0 = { 0, (int)D_001DC7B0_ROM_START, (int)D_001DC7B0_ROM_END, 16, { 0 }, 0x18, 0x18, 0x18, 0xAE, 0xDC, 0x18, 0x18, 0 };
BrImage D_8028D110 = { 0, (int)D_001DCBC0_ROM_START, (int)D_001DCBC0_ROM_END, 8, { 0 }, 0x18, 0x1A, 0x1A, 0xF4, 0x110, 0x18, 0x1A, 0 };
BrImage D_8028D140 = { 0, (int)D_001DCC90_ROM_START, (int)D_001DCC90_ROM_END, 8, { 0 }, 0x18, 0x18, 0x18, 0x96, 0x78, 0x18, 0x18, 0 };
BrImage D_8028D170 = { 0, (int)D_001DCD20_ROM_START, (int)D_001DCD20_ROM_END, 8, { 0 }, 0x18, 0x1A, 0x1A, 0x96, 0x78, 0x18, 0x1A, 0 };
BrImage D_8028D1A0 = { 0, (int)D_001DCD80_ROM_START, (int)D_001DCD80_ROM_END, 8, { 0 }, 0x10, 0x10, 0x10, 0x96, 0x78, 0x10, 0x10, 0 };
BrImage D_8028D1D0 = { 0, (int)D_001DCDC0_ROM_START, (int)D_001DCDC0_ROM_END, 8, { 0 }, 0x18, 0x18, 0x18, 0x96, 0x78, 0x18, 0x18, 0 };
BrImage D_8028D200 = { 0, (int)D_001DCE00_ROM_START, (int)D_001DCE00_ROM_END, 8, { 0 }, 0x18, 0x1A, 0x1A, 0x96, 0x78, 0x18, 0x1A, 0 };
BrImage D_8028D230 = { 0, (int)D_001DCE70_ROM_START, (int)D_001DCE70_ROM_END, 8, { 0 }, 0x200, 0xD4, 0x4, 0x42, 0x76, 0x200, 0xD4, 0 };
BrImage D_8028D260 = { 0, (int)D_001DF910_ROM_START, (int)D_001DF910_ROM_END, 4, { 0 }, 0x10, 0x2, 0x2, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D290 = { 0, (int)D_001DF930_ROM_START, (int)D_001DF930_ROM_END, 4, { 0 }, 0x10, 0x2, 0x2, 0x4A, 0x72, 0x2E, 0x24, 0 };
BrImage D_8028D2C0 = { 0, (int)D_001DF950_ROM_START, (int)D_001DF950_ROM_END, 4, { 0 }, 0x320, 0x10, 0x8, 0x0, 0x0, 0x320, 0x10, 0 };
BrImage D_8028D2F0 = { 0, (int)D_001DFCA0_ROM_START, (int)D_001DFCA0_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D320 = { 0, (int)D_001DFCD0_ROM_START, (int)D_001DFCD0_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D350 = { 0, (int)D_001DFD00_ROM_START, (int)D_001DFD00_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D380 = { 0, (int)D_001DFD70_ROM_START, (int)D_001DFD70_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D3B0 = { 0, (int)D_001DFE10_ROM_START, (int)D_001DFE10_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D3E0 = { 0, (int)D_001DFEF0_ROM_START, (int)D_001DFEF0_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D410 = { 0, (int)D_001E0030_ROM_START, (int)D_001E0030_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D440 = { 0, (int)D_001E0100_ROM_START, (int)D_001E0100_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrPaintRect D_8028D470 = { 280, 94, 328, 260 };
BrPaintRect D_8028D480 = { 0, 0, 48, 22 };
BrPaintRect D_8028D490 = { 0, 0, 232, 22 };
BrPaintRect D_8028D4A0[5] = { { 0, 0, 1, 1 }, { 0, 0, 2, 2 }, { 0, 0, 4, 4 }, { 0, 0, 6, 6 }, { 0, 0, 8, 8 } };
BrPaintRect D_8028D4F0[5] = { { 0, 0, 32, 12 }, { 0, 0, 32, 12 }, { 0, 0, 32, 18 }, { 0, 0, 32, 20 }, { 0, 0, 32, 24 } };
BrGlyph D_8028D540[50] = {
  { 0, 0, 49, { 0 }, 4, 8, 0, 2 },
  { 0, 0, 50, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 51, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 52, { 0 }, 3, 9, 1, 2 },
  { 0, 0, 53, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 54, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 55, { 0 }, 4, 8, 0, 2 },
  { 0, 0, 56, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 57, { 0 }, 3, 10, 1, 2 },
  { 0, 0, 48, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 65, { 0 }, 2, 11, 6, 0 },
  { 0, 0, 66, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 67, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 68, { 0 }, 3, 11, 2, 2 },
  { 0, 0, 69, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 70, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 71, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 72, { 0 }, 2, 11, 2, 2 },
  { 0, 0, 73, { 0 }, 6, 5, 2, 2 },
  { 0, 0, 74, { 0 }, 4, 7, 2, 2 },
  { 0, 0, 75, { 0 }, 2, 11, 2, 2 },
  { 0, 0, 76, { 0 }, 4, 8, 2, 0 },
  { 0, 0, 77, { 0 }, 1, 14, 2, 2 },
  { 0, 0, 78, { 0 }, 2, 11, 2, 2 },
  { 0, 0, 79, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 80, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 81, { 0 }, 3, 10, 2, 1 },
  { 0, 0, 82, { 0 }, 2, 11, 2, 2 },
  { 0, 0, 83, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 84, { 0 }, 4, 8, 0, 2 },
  { 0, 0, 85, { 0 }, 2, 11, 2, 2 },
  { 0, 0, 86, { 0 }, 3, 11, 0, 5 },
  { 0, 0, 87, { 0 }, 1, 14, 0, 4 },
  { 0, 0, 88, { 0 }, 2, 13, 3, 2 },
  { 0, 0, 89, { 0 }, 4, 10, 0, 6 },
  { 0, 0, 90, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 44, { 0 }, 6, 4, 2, 0 },
  { 0, 0, 46, { 0 }, 6, 4, 2, 0 },
  { 0, 0, 63, { 0 }, 4, 8, 1, 1 },
  { 0, 0, 33, { 0 }, 5, 6, 2, 2 },
  { 0, 0, 35, { 0 }, 2, 13, 2, 1 },
  { 0, 0, 36, { 0 }, 3, 10, 1, 1 },
  { 0, 0, 38, { 0 }, 3, 10, 1, 0 },
  { 0, 0, 45, { 0 }, 5, 6, 0, 0 },
  { 0, 0, 43, { 0 }, 4, 7, 0, 0 },
  { 0, 0, 59, { 0 }, 6, 4, 1, 1 },
  { 0, 0, 58, { 0 }, 6, 4, 1, 1 },
  { 0, 0, 48, { 0 }, 4, 8, 0, 0 },
  { 0, 0, 32, { 0 }, 4, 3, 0, 0 },
  { 0, 0, 48, { 0 }, 4, 8, 0, 0 }
};
unsigned char D_8028DAB8 = 0;
unsigned char D_8028DABC = 255;
unsigned char D_8028DAC0 = 2;
BrPaintRect D_8028DAC4 = { 0, 0, 0, 0 };
BrImage * D_8028DAD4[13] = { &D_8028CE10, &D_8028CE70, &D_8028CED0, &D_8028CEA0, &D_8028CF00, &D_8028CF30, &D_8028CF60, &D_8028CF90, &D_8028CFC0, &D_8028CFF0, &D_8028D020, &D_8028D050, &D_8028D080 };
BrImage * D_8028DB08 = 0;
BrImage * D_8028DB0C[10] = { &D_8028CBD0, &D_8028CC00, &D_8028CC30, &D_8028CC60, &D_8028CC90, &D_8028CCC0, &D_8028CCF0, &D_8028CD20, &D_8028CD50, &D_8028CD80 };
BrImage * D_8028DB34[4] = { &D_8028D2F0, &D_8028D320, &D_8028D350, &D_8028D380 };
BrImage * D_8028DB44[4] = { &D_8028D3B0, &D_8028D3E0, &D_8028D410, &D_8028D440 };
unsigned char D_8028DB54 = 0;
unsigned char D_8028DB58 = 3;
unsigned char D_8028DB5C = 15;
unsigned char D_8028DB60 = 1;
unsigned char D_8028DB64 = 1;
unsigned char D_8028DB68 = 2;
unsigned char D_8028DB6C = 0;
unsigned char D_8028DB70 = 0;
unsigned char D_8028DB74 = 0;
unsigned char * D_8028DB78 = 0;
unsigned char * D_8028DB7C = 0;
unsigned char * D_8028DB80 = 0;
unsigned char * D_8028DB84 = 0;
unsigned char D_8028DB88 = 0;
unsigned char D_8028DB8C = 0;
unsigned short * D_8028DB90 = 0;
BrPaintRect D_8028DB94 = { 0, 0, 0, 0 };
unsigned char D_8028DBA4 = 0;
unsigned char D_8028DBA8 = 0;
unsigned char D_8028DBAC = 0;
unsigned short D_8028DBB0 = 0;
unsigned char D_8028DBB4 = 0;
unsigned char D_8028DBB8 = 0;
unsigned char D_8028DBBC = 0;
unsigned char D_8028DBC0 = 0;
unsigned char D_8028DBC4 = 0;
unsigned char D_8028DBC8 = 1;
unsigned char D_8028DBCC = 1;
unsigned char D_8028DBD0 = 0;
unsigned char D_8028DBD4 = 0;
unsigned char D_8028DBD8 = 1;
unsigned char D_8028DBDC = 0;
unsigned char D_8028DBE0 = 0;
unsigned char D_8028DBE4 = 0;
unsigned char D_8028DBE8 = 0;
unsigned char D_8028DBEC = 0;
#elif TGR_REGION_EUR
extern char D_001BF900_ROM_START[], D_001BF900_ROM_END[];
extern char D_001C36A0_ROM_START[], D_001C36A0_ROM_END[];
extern char D_001C4C40_ROM_START[], D_001C4C40_ROM_END[];
extern char D_001C5710_ROM_START[], D_001C5710_ROM_END[];
extern char D_001C5560_ROM_START[], D_001C5560_ROM_END[];
extern char D_001C5090_ROM_START[], D_001C5090_ROM_END[];
extern char D_001C5200_ROM_START[], D_001C5200_ROM_END[];
extern char D_001C57E0_ROM_START[], D_001C57E0_ROM_END[];
extern char D_001C5990_ROM_START[], D_001C5990_ROM_END[];
extern char D_001C5AC0_ROM_START[], D_001C5AC0_ROM_END[];
extern char D_001C5C00_ROM_START[], D_001C5C00_ROM_END[];
extern char D_001C5C60_ROM_START[], D_001C5C60_ROM_END[];
extern char D_001C5D00_ROM_START[], D_001C5D00_ROM_END[];
extern char D_001C5D70_ROM_START[], D_001C5D70_ROM_END[];
extern char D_001C5DB0_ROM_START[], D_001C5DB0_ROM_END[];
extern char D_001C5EB0_ROM_START[], D_001C5EB0_ROM_END[];
extern char D_001C5F80_ROM_START[], D_001C5F80_ROM_END[];
extern char D_001C6030_ROM_START[], D_001C6030_ROM_END[];
extern char D_001C6110_ROM_START[], D_001C6110_ROM_END[];
extern char D_001C6200_ROM_START[], D_001C6200_ROM_END[];
extern char D_001C62F0_ROM_START[], D_001C62F0_ROM_END[];
extern char D_001C6410_ROM_START[], D_001C6410_ROM_END[];
extern char D_001C6510_ROM_START[], D_001C6510_ROM_END[];
extern char D_001C6600_ROM_START[], D_001C6600_ROM_END[];
extern char D_001C66F0_ROM_START[], D_001C66F0_ROM_END[];
extern char D_001C67F0_ROM_START[], D_001C67F0_ROM_END[];
extern char D_001C68E0_ROM_START[], D_001C68E0_ROM_END[];
extern char D_001C6AF0_ROM_START[], D_001C6AF0_ROM_END[];
extern char D_001C6F00_ROM_START[], D_001C6F00_ROM_END[];
extern char D_001C6FD0_ROM_START[], D_001C6FD0_ROM_END[];
extern char D_001C7060_ROM_START[], D_001C7060_ROM_END[];
extern char D_001C70C0_ROM_START[], D_001C70C0_ROM_END[];
extern char D_001C7100_ROM_START[], D_001C7100_ROM_END[];
extern char D_001C7140_ROM_START[], D_001C7140_ROM_END[];
extern char D_001C71B0_ROM_START[], D_001C71B0_ROM_END[];
extern char D_001C9C50_ROM_START[], D_001C9C50_ROM_END[];
extern char D_001C9C70_ROM_START[], D_001C9C70_ROM_END[];
extern char D_001C9C90_ROM_START[], D_001C9C90_ROM_END[];
extern char D_001C9FE0_ROM_START[], D_001C9FE0_ROM_END[];
extern char D_001CA010_ROM_START[], D_001CA010_ROM_END[];
extern char D_001CA040_ROM_START[], D_001CA040_ROM_END[];
extern char D_001CA0B0_ROM_START[], D_001CA0B0_ROM_END[];
extern char D_001CA150_ROM_START[], D_001CA150_ROM_END[];
extern char D_001CA230_ROM_START[], D_001CA230_ROM_END[];
extern char D_001CA370_ROM_START[], D_001CA370_ROM_END[];
extern char D_001CA440_ROM_START[], D_001CA440_ROM_END[];
BrImage D_8028CB40 = { 0, (int)D_001BF900_ROM_START, (int)D_001BF900_ROM_END, 4, { 0 }, 0x140, 0xF0, 0x18, 0x0, 0x0, 0x280, 0x1E0, 0 };
BrImage D_8028CB70 = { 0, (int)D_001C36A0_ROM_START, (int)D_001C36A0_ROM_END, 16, { 0 }, 0x11C, 0x28, 0x5, 0x2A, 0xE, 0x238, 0x50, 0 };
BrImage D_8028CBA0 = { 0, (int)D_001C4C40_ROM_START, (int)D_001C4C40_ROM_END, 8, { 0 }, 0x30, 0x46, 0x46, 0x0, 0x0, 0x30, 0x46, 0 };
BrImage D_8028CBD0 = { 0, (int)D_001C5710_ROM_START, (int)D_001C5710_ROM_END, 8, { 0 }, 0x20, 0x2E, 0x2E, 0xBA, 0xA2, 0x20, 0x2E, 0 };
BrImage D_8028CC00 = { 0, (int)D_001C5710_ROM_START, (int)D_001C5710_ROM_END, 8, { 0 }, 0x20, 0x2E, 0x2E, 0x5E, 0xA2, -32, 0x2E, 0 };
BrImage D_8028CC30 = { 0, (int)D_001C5560_ROM_START, (int)D_001C5560_ROM_END, 8, { 0 }, 0x20, 0x2A, 0x2A, 0xBA, 0xD0, 0x20, 0x2A, 0 };
BrImage D_8028CC60 = { 0, (int)D_001C5560_ROM_START, (int)D_001C5560_ROM_END, 8, { 0 }, 0x20, 0x2A, 0x2A, 0x5E, 0xD0, -32, 0x2A, 0 };
BrImage D_8028CC90 = { 0, (int)D_001C5090_ROM_START, (int)D_001C5090_ROM_END, 8, { 0 }, 0x30, 0x2A, 0x2A, 0x84, 0xD0, 0x30, 0x2A, 0 };
BrImage D_8028CCC0 = { 0, (int)D_001C5200_ROM_START, (int)D_001C5200_ROM_END, 8, { 0 }, 0x30, 0x5C, 0x2E, 0x84, 0x78, 0x30, 0x5C, 0 };
BrImage D_8028CCF0 = { 0, (int)D_001C57E0_ROM_START, (int)D_001C57E0_ROM_END, 8, { 0 }, 0x20, 0x30, 0x30, 0xB8, 0x74, 0x20, 0x30, 0 };
BrImage D_8028CD20 = { 0, (int)D_001C57E0_ROM_START, (int)D_001C57E0_ROM_END, 8, { 0 }, 0x20, 0x30, 0x30, 0x60, 0x74, -32, 0x30, 0 };
BrImage D_8028CD50 = { 0, (int)D_001C5990_ROM_START, (int)D_001C5990_ROM_END, 8, { 0 }, 0x30, 0x18, 0x18, 0x84, 0xF8, 0x30, 0x18, 0 };
BrImage D_8028CD80 = { 0, (int)D_001C5AC0_ROM_START, (int)D_001C5AC0_ROM_END, 8, { 0 }, 0x30, 0x18, 0x18, 0x84, 0x60, 0x30, 0x18, 0 };
BrImage D_8028CDB0 = { 0, (int)D_001C5C00_ROM_START, (int)D_001C5C00_ROM_END, 4, { 0 }, 0x30, 0xA, 0xA, 0x0, 0x0, 0x30, 0xA, 0 };
BrImage D_8028CDE0 = { 0, (int)D_001C5C60_ROM_START, (int)D_001C5C60_ROM_END, 4, { 0 }, 0x30, 0xE, 0xE, 0x0, 0x0, 0x30, 0xE, 0 };
BrImage D_8028CE10 = { 0, (int)D_001C5D00_ROM_START, (int)D_001C5D00_ROM_END, 4, { 0 }, 0x20, 0x56, 0x56, 0x0, 0x0, 0x20, 0x56, 0 };
BrImage D_8028CE40 = { 0, (int)D_001C5D70_ROM_START, (int)D_001C5D70_ROM_END, 4, { 0 }, 0x20, 0x56, 0x56, 0x0, 0x0, 0x20, 0x56, 0 };
BrImage D_8028CE70 = { 0, (int)D_001C5DB0_ROM_START, (int)D_001C5DB0_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CEA0 = { 0, (int)D_001C5EB0_ROM_START, (int)D_001C5EB0_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CED0 = { 0, (int)D_001C5F80_ROM_START, (int)D_001C5F80_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CF00 = { 0, (int)D_001C6030_ROM_START, (int)D_001C6030_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CF30 = { 0, (int)D_001C6110_ROM_START, (int)D_001C6110_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CF60 = { 0, (int)D_001C6200_ROM_START, (int)D_001C6200_ROM_END, 4, { 0 }, 0x30, 0xC, 0xC, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CF90 = { 0, (int)D_001C62F0_ROM_START, (int)D_001C62F0_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CFC0 = { 0, (int)D_001C6410_ROM_START, (int)D_001C6410_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CFF0 = { 0, (int)D_001C6510_ROM_START, (int)D_001C6510_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D020 = { 0, (int)D_001C6600_ROM_START, (int)D_001C6600_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D050 = { 0, (int)D_001C66F0_ROM_START, (int)D_001C66F0_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D080 = { 0, (int)D_001C67F0_ROM_START, (int)D_001C67F0_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D0B0 = { 0, (int)D_001C68E0_ROM_START, (int)D_001C68E0_ROM_END, 16, { 0 }, 0x18, 0x18, 0x18, 0x6C, 0xDC, 0x18, 0x18, 0 };
BrImage D_8028D0E0 = { 0, (int)D_001C6AF0_ROM_START, (int)D_001C6AF0_ROM_END, 16, { 0 }, 0x18, 0x18, 0x18, 0xAE, 0xDC, 0x18, 0x18, 0 };
BrImage D_8028D110 = { 0, (int)D_001C6F00_ROM_START, (int)D_001C6F00_ROM_END, 8, { 0 }, 0x18, 0x1A, 0x1A, 0xF4, 0x110, 0x18, 0x1A, 0 };
BrImage D_8028D140 = { 0, (int)D_001C6FD0_ROM_START, (int)D_001C6FD0_ROM_END, 8, { 0 }, 0x18, 0x18, 0x18, 0x96, 0x78, 0x18, 0x18, 0 };
BrImage D_8028D170 = { 0, (int)D_001C7060_ROM_START, (int)D_001C7060_ROM_END, 8, { 0 }, 0x18, 0x1A, 0x1A, 0x96, 0x78, 0x18, 0x1A, 0 };
BrImage D_8028D1A0 = { 0, (int)D_001C70C0_ROM_START, (int)D_001C70C0_ROM_END, 8, { 0 }, 0x10, 0x10, 0x10, 0x96, 0x78, 0x10, 0x10, 0 };
BrImage D_8028D1D0 = { 0, (int)D_001C7100_ROM_START, (int)D_001C7100_ROM_END, 8, { 0 }, 0x18, 0x18, 0x18, 0x96, 0x78, 0x18, 0x18, 0 };
BrImage D_8028D200 = { 0, (int)D_001C7140_ROM_START, (int)D_001C7140_ROM_END, 8, { 0 }, 0x18, 0x1A, 0x1A, 0x96, 0x78, 0x18, 0x1A, 0 };
BrImage D_8028D230 = { 0, (int)D_001C71B0_ROM_START, (int)D_001C71B0_ROM_END, 8, { 0 }, 0x200, 0xD4, 0x4, 0x42, 0x76, 0x200, 0xD4, 0 };
BrImage D_8028D260 = { 0, (int)D_001C9C50_ROM_START, (int)D_001C9C50_ROM_END, 4, { 0 }, 0x10, 0x2, 0x2, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D290 = { 0, (int)D_001C9C70_ROM_START, (int)D_001C9C70_ROM_END, 4, { 0 }, 0x10, 0x2, 0x2, 0x4A, 0x72, 0x2E, 0x24, 0 };
BrImage D_8028D2C0 = { 0, (int)D_001C9C90_ROM_START, (int)D_001C9C90_ROM_END, 4, { 0 }, 0x320, 0x10, 0x8, 0x0, 0x0, 0x320, 0x10, 0 };
BrImage D_8028D2F0 = { 0, (int)D_001C9FE0_ROM_START, (int)D_001C9FE0_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D320 = { 0, (int)D_001CA010_ROM_START, (int)D_001CA010_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D350 = { 0, (int)D_001CA040_ROM_START, (int)D_001CA040_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D380 = { 0, (int)D_001CA0B0_ROM_START, (int)D_001CA0B0_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D3B0 = { 0, (int)D_001CA150_ROM_START, (int)D_001CA150_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D3E0 = { 0, (int)D_001CA230_ROM_START, (int)D_001CA230_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D410 = { 0, (int)D_001CA370_ROM_START, (int)D_001CA370_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D440 = { 0, (int)D_001CA440_ROM_START, (int)D_001CA440_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrPaintRect D_8028D470 = { 280, 94, 328, 260 };
BrPaintRect D_8028D480 = { 0, 0, 48, 22 };
BrPaintRect D_8028D490 = { 0, 0, 232, 22 };
BrPaintRect D_8028D4A0[5] = { { 0, 0, 1, 1 }, { 0, 0, 2, 2 }, { 0, 0, 4, 4 }, { 0, 0, 6, 6 }, { 0, 0, 8, 8 } };
BrPaintRect D_8028D4F0[5] = { { 0, 0, 32, 12 }, { 0, 0, 32, 12 }, { 0, 0, 32, 18 }, { 0, 0, 32, 20 }, { 0, 0, 32, 24 } };
BrGlyph D_8028D540[50] = {
  { 0, 0, 49, { 0 }, 4, 8, 0, 2 },
  { 0, 0, 50, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 51, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 52, { 0 }, 3, 9, 1, 2 },
  { 0, 0, 53, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 54, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 55, { 0 }, 4, 8, 0, 2 },
  { 0, 0, 56, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 57, { 0 }, 3, 10, 1, 2 },
  { 0, 0, 48, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 65, { 0 }, 2, 11, 6, 0 },
  { 0, 0, 66, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 67, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 68, { 0 }, 3, 11, 2, 2 },
  { 0, 0, 69, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 70, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 71, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 72, { 0 }, 2, 11, 2, 2 },
  { 0, 0, 73, { 0 }, 6, 5, 2, 2 },
  { 0, 0, 74, { 0 }, 4, 7, 2, 2 },
  { 0, 0, 75, { 0 }, 2, 11, 2, 2 },
  { 0, 0, 76, { 0 }, 4, 8, 2, 0 },
  { 0, 0, 77, { 0 }, 1, 14, 2, 2 },
  { 0, 0, 78, { 0 }, 2, 11, 2, 2 },
  { 0, 0, 79, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 80, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 81, { 0 }, 3, 10, 2, 1 },
  { 0, 0, 82, { 0 }, 2, 11, 2, 2 },
  { 0, 0, 83, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 84, { 0 }, 4, 8, 0, 2 },
  { 0, 0, 85, { 0 }, 2, 11, 2, 2 },
  { 0, 0, 86, { 0 }, 3, 11, 0, 5 },
  { 0, 0, 87, { 0 }, 1, 14, 0, 4 },
  { 0, 0, 88, { 0 }, 2, 13, 3, 2 },
  { 0, 0, 89, { 0 }, 4, 10, 0, 6 },
  { 0, 0, 90, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 44, { 0 }, 6, 4, 2, 0 },
  { 0, 0, 46, { 0 }, 6, 4, 2, 0 },
  { 0, 0, 63, { 0 }, 4, 8, 1, 1 },
  { 0, 0, 33, { 0 }, 5, 6, 2, 2 },
  { 0, 0, 35, { 0 }, 2, 13, 2, 1 },
  { 0, 0, 36, { 0 }, 3, 10, 1, 1 },
  { 0, 0, 38, { 0 }, 3, 10, 1, 0 },
  { 0, 0, 45, { 0 }, 5, 6, 0, 0 },
  { 0, 0, 43, { 0 }, 4, 7, 0, 0 },
  { 0, 0, 59, { 0 }, 6, 4, 1, 1 },
  { 0, 0, 58, { 0 }, 6, 4, 1, 1 },
  { 0, 0, 48, { 0 }, 4, 8, 0, 0 },
  { 0, 0, 32, { 0 }, 4, 3, 0, 0 },
  { 0, 0, 48, { 0 }, 4, 8, 0, 0 }
};
unsigned char D_8028DAB8 = 0;
unsigned char D_8028DABC = 255;
unsigned char D_8028DAC0 = 2;
BrPaintRect D_8028DAC4 = { 0, 0, 0, 0 };
BrImage * D_8028DAD4[13] = { &D_8028CE10, &D_8028CE70, &D_8028CED0, &D_8028CEA0, &D_8028CF00, &D_8028CF30, &D_8028CF60, &D_8028CF90, &D_8028CFC0, &D_8028CFF0, &D_8028D020, &D_8028D050, &D_8028D080 };
BrImage * D_8028DB08 = 0;
BrImage * D_8028DB0C[10] = { &D_8028CBD0, &D_8028CC00, &D_8028CC30, &D_8028CC60, &D_8028CC90, &D_8028CCC0, &D_8028CCF0, &D_8028CD20, &D_8028CD50, &D_8028CD80 };
BrImage * D_8028DB34[4] = { &D_8028D2F0, &D_8028D320, &D_8028D350, &D_8028D380 };
BrImage * D_8028DB44[4] = { &D_8028D3B0, &D_8028D3E0, &D_8028D410, &D_8028D440 };
unsigned char D_8028DB54 = 0;
unsigned char D_8028DB58 = 3;
unsigned char D_8028DB5C = 15;
unsigned char D_8028DB60 = 1;
unsigned char D_8028DB64 = 1;
unsigned char D_8028DB68 = 2;
unsigned char D_8028DB6C = 0;
unsigned char D_8028DB70 = 0;
unsigned char D_8028DB74 = 0;
unsigned char * D_8028DB78 = 0;
unsigned char * D_8028DB7C = 0;
unsigned char * D_8028DB80 = 0;
unsigned char * D_8028DB84 = 0;
unsigned char D_8028DB88 = 0;
unsigned char D_8028DB8C = 0;
unsigned short * D_8028DB90 = 0;
BrPaintRect D_8028DB94 = { 0, 0, 0, 0 };
unsigned char D_8028DBA4 = 0;
unsigned char D_8028DBA8 = 0;
unsigned char D_8028DBAC = 0;
unsigned short D_8028DBB0 = 0;
unsigned char D_8028DBB4 = 0;
unsigned char D_8028DBB8 = 0;
unsigned char D_8028DBBC = 0;
unsigned char D_8028DBC0 = 0;
unsigned char D_8028DBC4 = 0;
unsigned char D_8028DBC8 = 1;
unsigned char D_8028DBCC = 1;
unsigned char D_8028DBD0 = 0;
unsigned char D_8028DBD4 = 0;
unsigned char D_8028DBD8 = 1;
unsigned char D_8028DBDC = 0;
unsigned char D_8028DBE0 = 0;
unsigned char D_8028DBE4 = 0;
unsigned char D_8028DBE8 = 0;
unsigned char D_8028DBEC = 0;
#else
extern char D_001C07C0_ROM_START[], D_001C07C0_ROM_END[];
extern char D_001C4560_ROM_START[], D_001C4560_ROM_END[];
extern char D_001C5B00_ROM_START[], D_001C5B00_ROM_END[];
extern char D_001C65D0_ROM_START[], D_001C65D0_ROM_END[];
extern char D_001C6420_ROM_START[], D_001C6420_ROM_END[];
extern char D_001C5F50_ROM_START[], D_001C5F50_ROM_END[];
extern char D_001C60C0_ROM_START[], D_001C60C0_ROM_END[];
extern char D_001C66A0_ROM_START[], D_001C66A0_ROM_END[];
extern char D_001C6850_ROM_START[], D_001C6850_ROM_END[];
extern char D_001C6980_ROM_START[], D_001C6980_ROM_END[];
extern char D_001C6AC0_ROM_START[], D_001C6AC0_ROM_END[];
extern char D_001C6B20_ROM_START[], D_001C6B20_ROM_END[];
extern char D_001C6BC0_ROM_START[], D_001C6BC0_ROM_END[];
extern char D_001C6C30_ROM_START[], D_001C6C30_ROM_END[];
extern char D_001C6C70_ROM_START[], D_001C6C70_ROM_END[];
extern char D_001C6D70_ROM_START[], D_001C6D70_ROM_END[];
extern char D_001C6E40_ROM_START[], D_001C6E40_ROM_END[];
extern char D_001C6EF0_ROM_START[], D_001C6EF0_ROM_END[];
extern char D_001C6FD0_ROM_START[], D_001C6FD0_ROM_END[];
extern char D_001C70C0_ROM_START[], D_001C70C0_ROM_END[];
extern char D_001C71B0_ROM_START[], D_001C71B0_ROM_END[];
extern char D_001C72D0_ROM_START[], D_001C72D0_ROM_END[];
extern char D_001C73D0_ROM_START[], D_001C73D0_ROM_END[];
extern char D_001C74C0_ROM_START[], D_001C74C0_ROM_END[];
extern char D_001C75B0_ROM_START[], D_001C75B0_ROM_END[];
extern char D_001C76B0_ROM_START[], D_001C76B0_ROM_END[];
extern char D_001C77A0_ROM_START[], D_001C77A0_ROM_END[];
extern char D_001C79B0_ROM_START[], D_001C79B0_ROM_END[];
extern char D_001C7DC0_ROM_START[], D_001C7DC0_ROM_END[];
extern char D_001C7E90_ROM_START[], D_001C7E90_ROM_END[];
extern char D_001C7F20_ROM_START[], D_001C7F20_ROM_END[];
extern char D_001C7F80_ROM_START[], D_001C7F80_ROM_END[];
extern char D_001C7FC0_ROM_START[], D_001C7FC0_ROM_END[];
extern char D_001C8000_ROM_START[], D_001C8000_ROM_END[];
extern char D_001C8070_ROM_START[], D_001C8070_ROM_END[];
extern char D_001CAB10_ROM_START[], D_001CAB10_ROM_END[];
extern char D_001CAB30_ROM_START[], D_001CAB30_ROM_END[];
extern char D_001CAB50_ROM_START[], D_001CAB50_ROM_END[];
extern char D_001CAEA0_ROM_START[], D_001CAEA0_ROM_END[];
extern char D_001CAED0_ROM_START[], D_001CAED0_ROM_END[];
extern char D_001CAF00_ROM_START[], D_001CAF00_ROM_END[];
extern char D_001CAF70_ROM_START[], D_001CAF70_ROM_END[];
extern char D_001CB010_ROM_START[], D_001CB010_ROM_END[];
extern char D_001CB0F0_ROM_START[], D_001CB0F0_ROM_END[];
extern char D_001CB230_ROM_START[], D_001CB230_ROM_END[];
extern char D_001CB300_ROM_START[], D_001CB300_ROM_END[];
BrImage D_8028CB40 = { 0, (int)D_001C07C0_ROM_START, (int)D_001C07C0_ROM_END, 4, { 0 }, 0x140, 0xF0, 0x18, 0x0, 0x0, 0x280, 0x1E0, 0 };
BrImage D_8028CB70 = { 0, (int)D_001C4560_ROM_START, (int)D_001C4560_ROM_END, 16, { 0 }, 0x11C, 0x28, 0x5, 0x2A, 0xE, 0x238, 0x50, 0 };
BrImage D_8028CBA0 = { 0, (int)D_001C5B00_ROM_START, (int)D_001C5B00_ROM_END, 8, { 0 }, 0x30, 0x46, 0x46, 0x0, 0x0, 0x30, 0x46, 0 };
BrImage D_8028CBD0 = { 0, (int)D_001C65D0_ROM_START, (int)D_001C65D0_ROM_END, 8, { 0 }, 0x20, 0x2E, 0x2E, 0xBA, 0xA2, 0x20, 0x2E, 0 };
BrImage D_8028CC00 = { 0, (int)D_001C65D0_ROM_START, (int)D_001C65D0_ROM_END, 8, { 0 }, 0x20, 0x2E, 0x2E, 0x5E, 0xA2, -32, 0x2E, 0 };
BrImage D_8028CC30 = { 0, (int)D_001C6420_ROM_START, (int)D_001C6420_ROM_END, 8, { 0 }, 0x20, 0x2A, 0x2A, 0xBA, 0xD0, 0x20, 0x2A, 0 };
BrImage D_8028CC60 = { 0, (int)D_001C6420_ROM_START, (int)D_001C6420_ROM_END, 8, { 0 }, 0x20, 0x2A, 0x2A, 0x5E, 0xD0, -32, 0x2A, 0 };
BrImage D_8028CC90 = { 0, (int)D_001C5F50_ROM_START, (int)D_001C5F50_ROM_END, 8, { 0 }, 0x30, 0x2A, 0x2A, 0x84, 0xD0, 0x30, 0x2A, 0 };
BrImage D_8028CCC0 = { 0, (int)D_001C60C0_ROM_START, (int)D_001C60C0_ROM_END, 8, { 0 }, 0x30, 0x5C, 0x2E, 0x84, 0x78, 0x30, 0x5C, 0 };
BrImage D_8028CCF0 = { 0, (int)D_001C66A0_ROM_START, (int)D_001C66A0_ROM_END, 8, { 0 }, 0x20, 0x30, 0x30, 0xB8, 0x74, 0x20, 0x30, 0 };
BrImage D_8028CD20 = { 0, (int)D_001C66A0_ROM_START, (int)D_001C66A0_ROM_END, 8, { 0 }, 0x20, 0x30, 0x30, 0x60, 0x74, -32, 0x30, 0 };
BrImage D_8028CD50 = { 0, (int)D_001C6850_ROM_START, (int)D_001C6850_ROM_END, 8, { 0 }, 0x30, 0x18, 0x18, 0x84, 0xF8, 0x30, 0x18, 0 };
BrImage D_8028CD80 = { 0, (int)D_001C6980_ROM_START, (int)D_001C6980_ROM_END, 8, { 0 }, 0x30, 0x18, 0x18, 0x84, 0x60, 0x30, 0x18, 0 };
BrImage D_8028CDB0 = { 0, (int)D_001C6AC0_ROM_START, (int)D_001C6AC0_ROM_END, 4, { 0 }, 0x30, 0xA, 0xA, 0x0, 0x0, 0x30, 0xA, 0 };
BrImage D_8028CDE0 = { 0, (int)D_001C6B20_ROM_START, (int)D_001C6B20_ROM_END, 4, { 0 }, 0x30, 0xE, 0xE, 0x0, 0x0, 0x30, 0xE, 0 };
BrImage D_8028CE10 = { 0, (int)D_001C6BC0_ROM_START, (int)D_001C6BC0_ROM_END, 4, { 0 }, 0x20, 0x56, 0x56, 0x0, 0x0, 0x20, 0x56, 0 };
BrImage D_8028CE40 = { 0, (int)D_001C6C30_ROM_START, (int)D_001C6C30_ROM_END, 4, { 0 }, 0x20, 0x56, 0x56, 0x0, 0x0, 0x20, 0x56, 0 };
BrImage D_8028CE70 = { 0, (int)D_001C6C70_ROM_START, (int)D_001C6C70_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CEA0 = { 0, (int)D_001C6D70_ROM_START, (int)D_001C6D70_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CED0 = { 0, (int)D_001C6E40_ROM_START, (int)D_001C6E40_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CF00 = { 0, (int)D_001C6EF0_ROM_START, (int)D_001C6EF0_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CF30 = { 0, (int)D_001C6FD0_ROM_START, (int)D_001C6FD0_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CF60 = { 0, (int)D_001C70C0_ROM_START, (int)D_001C70C0_ROM_END, 4, { 0 }, 0x30, 0xC, 0xC, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CF90 = { 0, (int)D_001C71B0_ROM_START, (int)D_001C71B0_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CFC0 = { 0, (int)D_001C72D0_ROM_START, (int)D_001C72D0_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028CFF0 = { 0, (int)D_001C73D0_ROM_START, (int)D_001C73D0_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D020 = { 0, (int)D_001C74C0_ROM_START, (int)D_001C74C0_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D050 = { 0, (int)D_001C75B0_ROM_START, (int)D_001C75B0_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D080 = { 0, (int)D_001C76B0_ROM_START, (int)D_001C76B0_ROM_END, 4, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D0B0 = { 0, (int)D_001C77A0_ROM_START, (int)D_001C77A0_ROM_END, 16, { 0 }, 0x18, 0x18, 0x18, 0x6C, 0xDC, 0x18, 0x18, 0 };
BrImage D_8028D0E0 = { 0, (int)D_001C79B0_ROM_START, (int)D_001C79B0_ROM_END, 16, { 0 }, 0x18, 0x18, 0x18, 0xAE, 0xDC, 0x18, 0x18, 0 };
BrImage D_8028D110 = { 0, (int)D_001C7DC0_ROM_START, (int)D_001C7DC0_ROM_END, 8, { 0 }, 0x18, 0x1A, 0x1A, 0xF4, 0x110, 0x18, 0x1A, 0 };
BrImage D_8028D140 = { 0, (int)D_001C7E90_ROM_START, (int)D_001C7E90_ROM_END, 8, { 0 }, 0x18, 0x18, 0x18, 0x96, 0x78, 0x18, 0x18, 0 };
BrImage D_8028D170 = { 0, (int)D_001C7F20_ROM_START, (int)D_001C7F20_ROM_END, 8, { 0 }, 0x18, 0x1A, 0x1A, 0x96, 0x78, 0x18, 0x1A, 0 };
BrImage D_8028D1A0 = { 0, (int)D_001C7F80_ROM_START, (int)D_001C7F80_ROM_END, 8, { 0 }, 0x10, 0x10, 0x10, 0x96, 0x78, 0x10, 0x10, 0 };
BrImage D_8028D1D0 = { 0, (int)D_001C7FC0_ROM_START, (int)D_001C7FC0_ROM_END, 8, { 0 }, 0x18, 0x18, 0x18, 0x96, 0x78, 0x18, 0x18, 0 };
BrImage D_8028D200 = { 0, (int)D_001C8000_ROM_START, (int)D_001C8000_ROM_END, 8, { 0 }, 0x18, 0x1A, 0x1A, 0x96, 0x78, 0x18, 0x1A, 0 };
BrImage D_8028D230 = { 0, (int)D_001C8070_ROM_START, (int)D_001C8070_ROM_END, 8, { 0 }, 0x200, 0xD4, 0x4, 0x42, 0x76, 0x200, 0xD4, 0 };
BrImage D_8028D260 = { 0, (int)D_001CAB10_ROM_START, (int)D_001CAB10_ROM_END, 4, { 0 }, 0x10, 0x2, 0x2, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D290 = { 0, (int)D_001CAB30_ROM_START, (int)D_001CAB30_ROM_END, 4, { 0 }, 0x10, 0x2, 0x2, 0x4A, 0x72, 0x2E, 0x24, 0 };
BrImage D_8028D2C0 = { 0, (int)D_001CAB50_ROM_START, (int)D_001CAB50_ROM_END, 4, { 0 }, 0x320, 0x10, 0x8, 0x0, 0x0, 0x320, 0x10, 0 };
BrImage D_8028D2F0 = { 0, (int)D_001CAEA0_ROM_START, (int)D_001CAEA0_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D320 = { 0, (int)D_001CAED0_ROM_START, (int)D_001CAED0_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D350 = { 0, (int)D_001CAF00_ROM_START, (int)D_001CAF00_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D380 = { 0, (int)D_001CAF70_ROM_START, (int)D_001CAF70_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D3B0 = { 0, (int)D_001CB010_ROM_START, (int)D_001CB010_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D3E0 = { 0, (int)D_001CB0F0_ROM_START, (int)D_001CB0F0_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D410 = { 0, (int)D_001CB230_ROM_START, (int)D_001CB230_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrImage D_8028D440 = { 0, (int)D_001CB300_ROM_START, (int)D_001CB300_ROM_END, 0, { 0 }, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0 };
BrPaintRect D_8028D470 = { 280, 94, 328, 260 };
BrPaintRect D_8028D480 = { 0, 0, 48, 22 };
BrPaintRect D_8028D490 = { 0, 0, 232, 22 };
BrPaintRect D_8028D4A0[5] = { { 0, 0, 1, 1 }, { 0, 0, 2, 2 }, { 0, 0, 4, 4 }, { 0, 0, 6, 6 }, { 0, 0, 8, 8 } };
BrPaintRect D_8028D4F0[5] = { { 0, 0, 32, 12 }, { 0, 0, 32, 12 }, { 0, 0, 32, 18 }, { 0, 0, 32, 20 }, { 0, 0, 32, 24 } };
BrGlyph D_8028D540[50] = {
  { 0, 0, 49, { 0 }, 4, 8, 0, 2 },
  { 0, 0, 50, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 51, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 52, { 0 }, 3, 9, 1, 2 },
  { 0, 0, 53, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 54, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 55, { 0 }, 4, 8, 0, 2 },
  { 0, 0, 56, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 57, { 0 }, 3, 10, 1, 2 },
  { 0, 0, 48, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 65, { 0 }, 2, 11, 6, 0 },
  { 0, 0, 66, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 67, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 68, { 0 }, 3, 11, 2, 2 },
  { 0, 0, 69, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 70, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 71, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 72, { 0 }, 2, 11, 2, 2 },
  { 0, 0, 73, { 0 }, 6, 5, 2, 2 },
  { 0, 0, 74, { 0 }, 4, 7, 2, 2 },
  { 0, 0, 75, { 0 }, 2, 11, 2, 2 },
  { 0, 0, 76, { 0 }, 4, 8, 2, 0 },
  { 0, 0, 77, { 0 }, 1, 14, 2, 2 },
  { 0, 0, 78, { 0 }, 2, 11, 2, 2 },
  { 0, 0, 79, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 80, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 81, { 0 }, 3, 10, 2, 1 },
  { 0, 0, 82, { 0 }, 2, 11, 2, 2 },
  { 0, 0, 83, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 84, { 0 }, 4, 8, 0, 2 },
  { 0, 0, 85, { 0 }, 2, 11, 2, 2 },
  { 0, 0, 86, { 0 }, 3, 11, 0, 5 },
  { 0, 0, 87, { 0 }, 1, 14, 0, 4 },
  { 0, 0, 88, { 0 }, 2, 13, 3, 2 },
  { 0, 0, 89, { 0 }, 4, 10, 0, 6 },
  { 0, 0, 90, { 0 }, 3, 10, 2, 2 },
  { 0, 0, 44, { 0 }, 6, 4, 2, 0 },
  { 0, 0, 46, { 0 }, 6, 4, 2, 0 },
  { 0, 0, 63, { 0 }, 4, 8, 1, 1 },
  { 0, 0, 33, { 0 }, 5, 6, 2, 2 },
  { 0, 0, 35, { 0 }, 2, 13, 2, 1 },
  { 0, 0, 36, { 0 }, 3, 10, 1, 1 },
  { 0, 0, 38, { 0 }, 3, 10, 1, 0 },
  { 0, 0, 45, { 0 }, 5, 6, 0, 0 },
  { 0, 0, 43, { 0 }, 4, 7, 0, 0 },
  { 0, 0, 59, { 0 }, 6, 4, 1, 1 },
  { 0, 0, 58, { 0 }, 6, 4, 1, 1 },
  { 0, 0, 48, { 0 }, 4, 8, 0, 0 },
  { 0, 0, 32, { 0 }, 4, 3, 0, 0 },
  { 0, 0, 48, { 0 }, 4, 8, 0, 0 }
};
unsigned char D_8028DAB8 = 0;
unsigned char D_8028DABC = 255;
unsigned char D_8028DAC0 = 2;
BrPaintRect D_8028DAC4 = { 0, 0, 0, 0 };
BrImage * D_8028DAD4[13] = { &D_8028CE10, &D_8028CE70, &D_8028CED0, &D_8028CEA0, &D_8028CF00, &D_8028CF30, &D_8028CF60, &D_8028CF90, &D_8028CFC0, &D_8028CFF0, &D_8028D020, &D_8028D050, &D_8028D080 };
BrImage * D_8028DB08 = 0;
BrImage * D_8028DB0C[10] = { &D_8028CBD0, &D_8028CC00, &D_8028CC30, &D_8028CC60, &D_8028CC90, &D_8028CCC0, &D_8028CCF0, &D_8028CD20, &D_8028CD50, &D_8028CD80 };
BrImage * D_8028DB34[4] = { &D_8028D2F0, &D_8028D320, &D_8028D350, &D_8028D380 };
BrImage * D_8028DB44[4] = { &D_8028D3B0, &D_8028D3E0, &D_8028D410, &D_8028D440 };
unsigned char D_8028DB54 = 0;
unsigned char D_8028DB58 = 3;
unsigned char D_8028DB5C = 15;
unsigned char D_8028DB60 = 1;
unsigned char D_8028DB64 = 1;
unsigned char D_8028DB68 = 2;
unsigned char D_8028DB6C = 0;
unsigned char D_8028DB70 = 0;
unsigned char D_8028DB74 = 0;
unsigned char * D_8028DB78 = 0;
unsigned char * D_8028DB7C = 0;
unsigned char * D_8028DB80 = 0;
unsigned char * D_8028DB84 = 0;
unsigned char D_8028DB88 = 0;
unsigned char D_8028DB8C = 0;
unsigned short * D_8028DB90 = 0;
BrPaintRect D_8028DB94 = { 0, 0, 0, 0 };
unsigned char D_8028DBA4 = 0;
unsigned char D_8028DBA8 = 0;
unsigned char D_8028DBAC = 0;
unsigned short D_8028DBB0 = 0;
unsigned char D_8028DBB4 = 0;
unsigned char D_8028DBB8 = 0;
unsigned char D_8028DBBC = 0;
unsigned char D_8028DBC0 = 0;
unsigned char D_8028DBC4 = 0;
unsigned char D_8028DBC8 = 1;
unsigned char D_8028DBCC = 1;
unsigned char D_8028DBD0 = 0;
unsigned char D_8028DBD4 = 0;
unsigned char D_8028DBD8 = 1;
unsigned char D_8028DBDC = 0;
unsigned char D_8028DBE0 = 0;
unsigned char D_8028DBE4 = 0;
unsigned char D_8028DBE8 = 0;
unsigned char D_8028DBEC = 0;
#endif
/* Index pads for the dead block in BrPaintCarView: declared, never defined
 * or linked (the block is compiled away). */
extern char bss_index_pad_00;
extern char bss_index_pad_01;
extern char bss_index_pad_02;
extern char bss_index_pad_03;
extern char bss_index_pad_04;
extern char bss_index_pad_05;
extern char bss_index_pad_06;
extern char bss_index_pad_07;
extern char bss_index_pad_08;
extern char bss_index_pad_09;
extern char bss_index_pad_10;
extern char bss_index_pad_11;
extern char bss_index_pad_12;
extern char bss_index_pad_13;
extern char bss_index_pad_14;
extern char bss_index_pad_15;
extern char bss_index_pad_16;
extern char bss_index_pad_17;
extern char bss_index_pad_18;
extern char bss_index_pad_19;
extern char bss_index_pad_20;
extern char bss_index_pad_21;

/* ---- the functions, in ROM order ---- */
/* WHAT IT DOES: Give an interface image its RAM copy: take width x height x
 * depth / 8 bytes from the interface pool and decompress the ROM image into
 * it. A ROM image of any other size is reported and the game stops. The
 * original name is in its error message.  Japan prints none of this
 * file's messages. */
/* @implements 0x80242B10 tgr BrAllocPaintShopGfxMem */
void BrAllocPaintShopGfxMem(BrIfaceImage *img)
{
  img->data = BrIfaceMemAlloc(img->w * img->h * img->depth >> 3);
  if (BrRomReadSize(img->rom) != img->w * img->h * img->depth >> 3) {
#if !TGR_REGION_JPN
    osSyncPrintf("ERROR: AllocPaintShopGfxMem given image %dx%dx%d = %d bytes but uncompressed rom image is %d bytes\n",
                 img->w, img->h, img->depth, BrRomReadSize(img->rom));
#endif
    for (;;) {
    }
  }
  BrRomUnpack(img->data, img->rom, 0);
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
 * chained off it.  The view vectors are float arrays (cfe loads indexed
 * elements ahead of the product, so each sum reads the start vector
 * first) and the preset copies read through two pointers, one per vector,
 * which keeps the ROM's interleaved loads and stores. */
/* @implements 0x80242BDC tgr BrPaintCarView */
void BrPaintCarView()
{
  float from[3];
  float to[3];
  float dx;
  float dy;
  float dz;
  float x1[4];
  BrVec3 side;
  BrVec3 zAxis;
  float x2[1];
  BrVec3 *p;
  BrVec3 *q;

  D_8028AAF0 = &D_8031B760[D_8028DBBC];
  D_8028AAF4 = D_8028AAF0->cam = &D_8028AAF0->cams[3];
  if (D_8028DBD0 != 0) {
    p = &D_8028DC08[D_8028DB6C];
    from[0] = p->x;
    from[1] = p->y;
    from[2] = p->z;
    q = &D_8028DC08[D_8028DB68];
    to[0] = q->x;
    to[1] = q->y;
    to[2] = q->z;
    D_8028DBB0++;
    BrVec3Normalise((BrVec3 *)from);
    BrVec3Normalise((BrVec3 *)to);
    if (BrVec3Dot((BrVec3 *)to, (BrVec3 *)from) < -0.9) {
      zAxis.x = 0.0f;
      zAxis.y = 0.0f;
      zAxis.z = 1.0f;
      BrVec3Cross(&side, &zAxis, (BrVec3 *)from);
      if (D_8028DBB0 < 8) {
        BrVec3AddTo((BrVec3 *)to, &side);
      } else {
        BrVec3AddTo((BrVec3 *)from, &side);
      }
    }
    dx = (to[0] - from[0]) / 16;
    dy = (to[1] - from[1]) / 16;
    dz = (to[2] - from[2]) / 16;
    ((BrVec3 *)D_8028AAF0->mtx0[0])->x = D_8028DBB0 * dx + from[0];
    ((BrVec3 *)D_8028AAF0->mtx0[0])->y = D_8028DBB0 * dy + from[1];
    ((BrVec3 *)D_8028AAF0->mtx0[0])->z = D_8028DBB0 * dz + from[2];
    BrVec3Normalise((BrVec3 *)D_8028AAF0->mtx0[0]);
    {
      float upB[3] = {0.0f, 0.0f, 0.0f};
      float upA[3] = {0.0f, 0.0f, 0.0f};

      if (D_8028DB68 == 5) {
        upA[0] = 5.0f;
        upA[1] = 5.0f;
        upA[2] = 1.5f;
      } else {
        upA[0] = 0.0f;
        upA[1] = 0.0f;
        upA[2] = 1.0f;
      }
      if (D_8028DB6C == 5) {
        upB[0] = 5.0f;
        upB[1] = 5.0f;
        upB[2] = 1.5f;
      } else {
        upB[0] = 0.0f;
        upB[1] = 0.0f;
        upB[2] = 1.0f;
      }
      BrVec3Normalise((BrVec3 *)upB);
      BrVec3Normalise((BrVec3 *)upA);
      dx = (upA[0] - upB[0]) / 16;
      dy = (upA[1] - upB[1]) / 16;
      dz = (upA[2] - upB[2]) / 16;
      ((BrVec3 *)D_8028AAF0->mtx0[2])->x = D_8028DBB0 * dx + upB[0];
      ((BrVec3 *)D_8028AAF0->mtx0[2])->y = D_8028DBB0 * dy + upB[1];
      ((BrVec3 *)D_8028AAF0->mtx0[2])->z = D_8028DBB0 * dz + upB[2];
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
  /* The .bss index (see the file comment): never runs and emits no code. */
  if (0) {
    (void)&D_80271FA8;
    (void)&D_802724F0;
    (void)&D_80272D48;
    (void)&D_8028A850;
    (void)&D_8028A858;
    (void)&D_8028A898;
    (void)&D_8028AA80;
    (void)&D_8028AA98;
    (void)&D_8028B7F4;
    (void)&D_8028C328;
    (void)&D_802A4BE8;
    (void)&D_803163E0;
    (void)&D_80316420;
    (void)&D_8031A3F8;
    (void)&D_8031B1E8;
    (void)&bss_index_pad_00;
    (void)&bss_index_pad_01;
    (void)&bss_index_pad_02;
    (void)&bss_index_pad_03;
    (void)&bss_index_pad_04;
    (void)&bss_index_pad_05;
    (void)&bss_index_pad_06;
    (void)&bss_index_pad_07;
    (void)&bss_index_pad_08;
    (void)&bss_index_pad_09;
    (void)&bss_index_pad_10;
    (void)&bss_index_pad_11;
    (void)&bss_index_pad_12;
    (void)&bss_index_pad_13;
    (void)&bss_index_pad_14;
    (void)&bss_index_pad_15;
    (void)&bss_index_pad_16;
    (void)&bss_index_pad_17;
    (void)&bss_index_pad_18;
    (void)&bss_index_pad_19;
    (void)&bss_index_pad_20;
    (void)&bss_index_pad_21;
  }
}

BrPaintSwatch D_80369B98[16];           /* the palette */
BrPaintRect D_80369CD8[13];             /* the tool buttons */
unsigned char D_80369DA8[3];            /* the colour before the mixer opened */
unsigned char D_80369DAB;               /* the mirrored view */
BrPaintRect D_80369DB0[10];             /* the decal slot buttons */



BrVec3 D_8028DC08[10] = {         /* the preset view directions */
  { 1.0f, -1.0f, 0.0f }, { -1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
  { 3.0f, 3.0f, -1.0f }, { -1.0f, 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f },
  { 16.0f, 16.0f, 1.0f }, { -64.0f, -64.0f, 1.0f }
};

/* WHAT IT DOES: The paint shop screen, one frame.  The first time in: put
 * the player's car on the stand facing the camera (body colours, the
 * close camera at 6.5, 6.5, 2 looking at it, 7.3 out for the kind 10 car),
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
 * The car's rows are set through a row pointer and the grid index goes
 * through an unsigned temporary, as the ROM's register use shows; i and r
 * are the counters of every loop.  The paint area is centred through
 * half-differences (dx, dy) and the eyedropper's cell is shifted into i and
 * r before the call, as the ROM's temporaries show; the screen flag is a
 * signed byte tested as unsigned, which keeps its 1 apart from the
 * eyedropper's flags; the empty test of r after the second palette half's
 * y lets the shared y value take v0 ahead of the first half's address. */
/* @implements 0x80243260 tgr BrPaintShopScreen */
void BrPaintShopScreen()
{
  int u0[5];                    /* u0, u1: unused; they place done at sp+0xF3 as in */
  unsigned char done;           /* the ROM (the Pak transfer keeps a pointer to it) */
  int u1[13];
  int i;
  int k;
  int r;
  int x;
  int y;
  float *v;
  BrCar *car;
  unsigned int n;
  int dx;
  int dy;
  BrPaintRect *area;

  area = &D_8028D470;
  done = 0;
  D_802A4BE8 = 0;
  if ((unsigned char)D_8028DC80 == 0) {
    D_8028C328 = 0;
    D_8028DBBC = D_80271FA8;
    D_8028B7F4 = 1;
    BrCarDefaultColour(&D_8031B760[D_8028DBBC]);
    car = &D_8031B760[D_8028DBBC];
    car->xed8 = (int)BrCarCamStep;
    n = D_8028DBBC;
    D_8031B760[n].kind = D_8031B760[0].kind;
    BrEntLoadRecord(&D_8031B760[n], n, D_8031B760[n].kind);
    D_8028AB08 = (BrPaintModel *)D_8031B760[D_8028DBBC].model;
    {
      float *p = D_8031B760[D_8028DBBC].mtx0[3];

      p[2] = p[1] = p[0] = 0.0f;
    }
    {
      float *p = D_8031B760[D_8028DBBC].mtx0[2];

      p[2] = 1.0f;
      p[1] = p[0] = 0.0f;
    }
    {
      float *p = D_8031B760[D_8028DBBC].mtx0[0];

      p[1] = 1.0f;
      p[2] = p[0] = 0.0f;
    }
    {
      float *p = D_8031B760[D_8028DBBC].mtx0[1];

      p[2] = p[1] = 0.0f;
      p[0] = -1.0f;
    }
    if (D_8031B760[D_8028DBBC].kind == 10) {
      v = D_8031B760[D_8028DBBC].cams[3].mtx[3];
      v[1] = v[0] = 7.3f;
      v[2] = 2.0f;
    } else {
      v = D_8031B760[D_8028DBBC].cams[3].mtx[3];
      v[1] = v[0] = 6.5f;
      v[2] = 2.0f;
    }
    BrVec3Negate((BrVec3 *)D_8031B760[D_8028DBBC].cams[3].mtx[0], (BrVec3 *)D_8031B760[D_8028DBBC].cams[3].mtx[3]);
    BrVec3Normalise((BrVec3 *)D_8031B760[D_8028DBBC].cams[3].mtx[0]);
    v = D_8031B760[D_8028DBBC].cams[3].mtx[2];
    v[1] = v[0] = 0.0f;
    v[2] = 1.0f;
    BrVec3Cross((BrVec3 *)D_8031B760[D_8028DBBC].cams[3].mtx[1], (BrVec3 *)D_8031B760[D_8028DBBC].cams[3].mtx[2],
                (BrVec3 *)D_8031B760[D_8028DBBC].cams[3].mtx[0]);
    BrVec3Cross((BrVec3 *)D_8031B760[D_8028DBBC].cams[3].mtx[2], (BrVec3 *)D_8031B760[D_8028DBBC].cams[3].mtx[0],
                (BrVec3 *)D_8031B760[D_8028DBBC].cams[3].mtx[1]);
    D_8031B760[D_8028DBBC].x2000 = 0;
    D_8031B760[D_8028DBBC].x2044 = 0;
    BrIfaceMemReset();
#if !TGR_REGION_JPN
    osSyncPrintf("\nAllocating %d bytes (x2) for decal buffers...\n\n", 0x800);
#endif
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
    D_8028CDE0.x = D_8028CD80.x;
    D_8028CDE0.y = D_8028CD80.y + 8;
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
      default:
        break;
      }
      D_8028DB0C[i]->kind = 99;
    }
    D_80369CD8[0].x = D_8028D470.x;
    D_80369CD8[0].y = D_8028D470.y + D_8028D470.h + 6;
    D_80369CD8[0].w = 0x20;
    D_80369CD8[0].h = 0x5e;
    D_8028D480.x = D_80369CD8[0].w + D_80369CD8->x + 2;
    D_8028D480.y = D_80369CD8[0].y;
    D_8028D490.x = D_8028D480.x + D_8028D480.w + 0xe;
    D_8028D490.y = D_80369CD8[0].y;
    if (r) {
    }
    y = D_8028D490.y + D_8028D480.h + 2;
    for (r = 0; r < 3; r++) {
      for (i = 0; i < 4; i++) {
        n = (r << 2) + i;
        D_80369CD8[n + 1].x = D_8028D480.x + i * 0x4a;
        D_80369CD8[n + 1].w = 0x48;
        D_80369CD8[n + 1].h = 0x16;
        D_80369CD8[n + 1].y = y;
      }
      y += 0x18;
    }
    D_8028CE10.y = D_80369CD8[0].y + 4;
    D_8028CE40.y = D_80369CD8[0].y + 4;
    D_8028CE10.x = D_80369CD8[0].x;
    D_8028CE40.x = D_80369CD8[0].x;
    BrAllocPaintShopGfxMem(&D_8028CE10);
    BrAllocPaintShopGfxMem(&D_8028CE40);
    for (i = 1; i < 13; i++) {
      D_8028DAD4[i]->w = 0x40;
      D_8028DAD4[i]->h = 0xe;
      D_8028DAD4[i]->stripH = D_8028DAD4[i]->h;
      D_8028DAD4[i]->drawW = D_8028DAD4[i]->w;
      D_8028DAD4[i]->drawH = D_8028DAD4[i]->h;
      D_8028DAD4[i]->x = D_80369CD8[i].x + 4;
      D_8028DAD4[i]->y = D_80369CD8[i].y + 4;
      D_8028DAD4[i]->kind = 0;
      BrAllocPaintShopGfxMem(D_8028DAD4[i]);
    }
#if TGR_REGION_JPN
    BrRomRead(jpn_D_8028F020, jpn_D_001DC050, jpn_D_001DC070 - jpn_D_001DC050);
#endif
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
    D_8028DB90 = D_8028AB08->parts[D_8028AB08->decal[D_8028DB68]].tlut;
    D_8028DB08 = D_8028DB0C[D_8028DB68];
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
    for (i = 2; i < 16; i++) {
      D_80369B98[i].w = 0x10;
      D_80369B98[i].h = 0xe;
      D_80369B98[i].x = D_8028D490.x + 4 + (i - 2) * 0x10;
      D_80369B98[i].y = (&D_8028D470)[2].y + 4;
    }
    BrPaintPaletteLoad();
    for (r = 0; r < 5; r++) {
      for (i = 0; i < 10; i++) {
        k = r * 10 + i;
        D_8028D540[k].x0 = D_8028D260.x + (D_8028D290.drawW + 4) * i;
        D_8028D540[k].x4 = D_8028D260.y + (D_8028D290.drawH + 4) * r;
      }
    }
    D_8028DB94.w = D_8028DB88 * 4;
    D_8028DB94.h = D_8028DB8C * 4;
    dx = (D_8028D470.w - D_8028DB94.w) >> 1;
    dy = (D_8028D470.h - D_8028DB94.h) >> 1;
    D_8028DB94.x = D_8028D470.x + dx;
    D_8028DB94.y = D_8028D470.y + dy;
    D_8028DC80 = 1;
    D_802724F0 = 0;
    BrFadeTo(1.0f, 0.2f);
  }
  BrClockTick();
  BrFadeStep();
  BrFrameBeginLayout1();
  BrMenuCameraSet((float)D_8028AAB0, (float)D_8028AAB4);
  BrViewportSet(0, 0, D_8028AAB0, D_8028AAB4, 1);
  BrZBufferClear();
  BrScreenClear(0, 0, 0);
  BrImageDrawTinted(&D_8028CB40, 0x71, 0xb, 0x40);
  BR_IMAGE_DRAW(&D_8028CB70);
#if TGR_REGION_JPN
  BrTextSetFont(0x1e);
#else
  BrTextSetFont(0x22);
#endif
  BrTextAlignCentre();
  BrTextHighlightOff();
#if TGR_REGION_JPN
  BrTextPrint("%ryPAINT SHOP", 0xa0, 0x25);
#else
  BrTextPrint("%ryPAINT SHOP", 0xa0, 0x26);
#endif
  for (i = 0; i < 10; i++) {
    if (i == D_8028DB68) {
      BrImageDrawTinted(D_8028DB0C[i], 0x20, 200, 0xff);
    } else {
      BrImageDrawTinted(D_8028DB0C[i], 0x20, 0x10, 0xff);
    }
  }
  BrImageDrawTinted(&D_8028CDB0, 0xff, 0xff, 0xff);
  BrImageDrawTinted(&D_8028CDE0, 0xc0, 0, 0);
  BrFillRect(area->x, area->y, area->w, area->h, 0, 0, 0);
  BrTexLoad(D_8028AB08->decal[D_8028DB68], D_8028AB08->parts);
  BrTexRectFlipDraw(D_8028DB88, D_8028DB8C, D_8028DB94.x, D_8028DB94.y, D_8028DB94.w, D_8028DB94.h);
  BrPaintPaletteDraw();
  BrFillRect(D_80369CD8[0].x, D_80369CD8[0].y, D_80369CD8[0].w, D_80369CD8[0].h, 0, 0, 0);
  BrFillRect(D_80369CD8[0].x + 4, D_80369CD8[0].y + 4, D_80369CD8[0].w - 8, D_80369CD8[0].h - 8, 0xff, 0xca, 0);
  if (D_8028CE70.kind == 0) {
    BR_IMAGE_DRAW(&D_8028CE10);
  } else {
    BR_IMAGE_DRAW(&D_8028CE40);
  }
  BrFillRect(D_8028D4F0[D_8028DAC0].x, D_8028D4F0[D_8028DAC0].y, D_8028D4F0[D_8028DAC0].w,
             D_8028D4F0[D_8028DAC0].h, 0, 0, 0);
  if (D_8028CE70.kind == 0) {
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
#if TGR_REGION_JPN
    BrImageDraw(D_8028DAD4[i], jpn_D_8028F020);
#else
    BR_IMAGE_DRAW(D_8028DAD4[i]);
#endif
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
          BrImageDrawAt(&D_8028D1D0, D_8028D110.x - (D_8028D1D0.drawW >> 1), D_8028D110.y - D_8028D1D0.drawH);
        }
      } else {
        BR_IMAGE_DRAW(&D_8028D110);
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
      BR_IMAGE_DRAW(&D_8028D110);
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
        if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 0x30) {
          BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 0x30);
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
        if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 0x30) {
          BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 0x30);
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
        BR_IMAGE_DRAW(&D_8028D110);
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
  if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 0x20) {
    if (BrPaintCursorInRect(&D_8028DB94) && D_8028DBC0 == 0 && D_8028DBC4 == 0) {
      i = D_8028D110.x;
      r = D_8028D110.y;
      i = (i - D_8028DB94.x) >> 2;
      r = (D_8028DB94.y + D_8028DB94.h - r) >> 2;
      D_8028DB58 = BrPaintGet(i, r);
      D_8028DBEC = 1;
    } else if (D_8028DBC0 != 0 || D_8028DBC4 != 0) {
      BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 0x20);
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


signed char D_8028DC80 = 0;             /* the screen has been set up */

/* WHAT IT DOES: Reset the paint shop for a new session: cursor to the
 * centre, decal slot 2 selected everywhere, the default colour and brush,
 * and every mode and changed flag cleared. */
/* @implements 0x80244BA8 tgr BrPaintReset */
void BrPaintReset()
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
  D_8028CF30.kind = 0;
  D_8028CF60.kind = 0;
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
void BrPaintDecalCommit()
{
  memcpy((char *)D_8028DB80,(char *)D_8028DB78,0x800);
  D_8028DB74 = D_8028DB68;
  D_8028DBB4 = D_8028DBB4 + '\x01';
  D_8028DBDC = 1;
}

/* WHAT IT DOES: If the decal has changed, copy the working decal (2 KB)
 * into the car model's texture for the current decal slot, step the edit
 * point back and clear the changed mark. */
/* @implements 0x80244D04 tgr BrPaintDecalApply */
void BrPaintDecalApply()
{
  if (D_8028DBDC != 0) {
    memcpy(((BrCarModel *)D_8028AB08)->parts[((BrCarModel *)D_8028AB08)->decalPart[D_8028DB74]].a, (char *)D_8028DB80, 0x800);
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
 * Each arm's text line uses its own variable (ty) for the panel's y, and
 * the text x is written inline as (x + 16) >> 1, so mode and ty share s0
 * and msg and y share s1 as in the ROM; the unused pad[2] gives its 0x68
 * frame. */
/* @t4-pass 0x80244D84 1 2026-10-03 compiles 26 best 434 moved 1  (tools/tgrally/n64permute.py) */
/* @t4-pass 0x80244D84 2 2026-10-03 compiles 26 best 434 moved 0  (tools/tgrally/n64permute.py)
 * Japan's messages are Japanese, in its own panel sizes. */
/* @implements 0x80244D84 tgr BrPakMessage */
void BrPakMessage(int msg, char op, char mode)
{
#if TGR_REGION_JPN
  int x;
  int y;
  int w;
  int ty;
  int pad[2];                   /* declared, never used: the frame is the ROM's 0x68 */

  BrTextSetFont(14);
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
    BrBevelPanel(0xcc, y, 0xe8, 0x48, 3, 0, 0, 0x80, 0x80, 0x80);
    ty = (y + 30) >> 1;
    BrTextPrint("%j\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1" "9", 0x6e, ty);
    BrTextPrint("%j0H6b\x7F&", 0x6e, ty + 14);
    break;
  case 7:
  case 8:
    y = 0xaf;
    if (mode == 1) {
      y = 0xdb;
    }
    BrBevelPanel(0xb9, y, 0x10e, 0x82, 3, 0, 0, 0x80, 0x80, 0x80);
    ty = (y + 30) >> 1;
    BrTextPrint("%j\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1" "D", 100, ty);
    BrTextPrint("%j\xE1" "A\xBD(\x9A" "9\xE2H6b\x7F&", 100, ty + 12);
    BrTextPrint("%j\xBB\x81(\xA8t\xB2(\xAC" "D5", 100, ty + 30);
    BrTextPrint("62%j\xBD(\x9A\xD6\xD7kR&", 100, ty + 42);
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
      ty = (y + 30) >> 1;
      BrTextPrint("%j\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1", (x + 16) >> 1, ty);
      BrTextPrint("%j18\xE3" "F\xE5" "F9kA6b\x7F&", (x + 16) >> 1, ty + 14);
    } else {
      ty = (y + 30) >> 1;
      BrTextPrint("%j\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1", (x + 16) >> 1, ty);
      BrTextPrint("%je\xE4" "A\xE5" "F9kA6b\x7F&", (x + 16) >> 1, ty + 14);
    }
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
      ty = (y + 30) >> 1;
      BrTextPrint("%j\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1" "D", (x + 16) >> 1, ty);
      BrTextPrint("%j\xBB\x81(\xA8" "90H6b\x7F&", (x + 16) >> 1, ty + 14);
    } else {
      ty = (y + 30) >> 1;
      BrTextPrint("%j\xBB\x81(\xA8\xB2(\xAC\xE6", (x + 16) >> 1, ty);
      BrTextPrint("%jD\xB0\x88(9\xED\xEE" "B6B3&", (x + 16) >> 1, ty + 14);
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
    if (op == 9) {
      ty = (y + 30) >> 1;
      BrTextPrint("%j\xBB\x81(\xA8\xC8(\xCB\xE6", (x + 15) >> 1, ty);
      BrTextPrint("%jD\xB0\x88(9\xED\xEE" "B6B3&", (x + 15) >> 1, ty + 14);
    } else {
      ty = (y + 30) >> 1;
      BrTextPrint("%j\xBB\x81(\xA8\xB2(\xAC\xE6", (x + 15) >> 1, ty);
      BrTextPrint("%jD\xB0\x88(9\xED\xEE" "B6B3&", (x + 15) >> 1, ty + 14);
    }
    break;
  case 6:
    y = 0xcc;
    if (mode == 1) {
      y = 0xe4;
    }
    BrBevelPanel(0xb6, y, 0x114, 0x48, 3, 0, 0, 0x80, 0x80, 0x80);
    if (op == 9) {
      ty = (y + 30) >> 1;
      BrTextPrint("%j\xBB\x81(\xA8\xC8(\xCB\xE6" "D\xD0\xD1", 0x62, ty);
      BrTextPrint("%j\xBB(\x83" "9\xED\xEE" "B6B3&", 0x62, ty + 14);
    } else {
      ty = (y + 30) >> 1;
      BrTextPrint("%j\xBB\x81(\xA8\xB2(\xAC\xE6" "D\xD0\xD1", 0x62, ty);
      BrTextPrint("%j\xBB(\x83" "9\xED\xEE" "B6B3&", 0x62, ty + 14);
    }
    break;
  case 4:
    y = 0xb0;
    if (mode == 1) {
      y = 0xdc;
    }
    BrBevelPanel(0xcd, y, 0xe6, 0x80, 3, 0, 0, 0x80, 0x80, 0x80);
    ty = (y + 30) >> 1;
    BrTextPrint("%j\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1" "9", 0x6d, ty);
    BrTextPrint("%j\xD0\xD1kR&", 0x6d, ty + 12);
    if (op == 9) {
      BrTextPrint("%j\xBB\x81(\xA8w", 0x6d, ty + 30);
      BrTextPrint("%j\xC8(\xCBkA6b\x7F&", 0x6d, ty + 42);
    } else {
      BrTextPrint("%j\xBB\x81(\xA8w", 0x6d, ty + 30);
      BrTextPrint("%j\xB2(\xACkA6b\x7F&", 0x6d, ty + 42);
    }
    break;
  case 10:
    y = 0xcc;
    if (mode == 1) {
      y = 0xf8;
    }
    BrBevelPanel(0x50, y, 0x1e0, 0x48, 3, 0, 0, 0x80, 0x80, 0x80);
    ty = (y + 30) >> 1;
    BrTextPrint("%j\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1" "9'qghc@6R&", 0x30, ty);
    BrTextPrint("%j038B@\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1w'2Bc\xDB" "2@&", 0x30, ty + 14);
    break;
  }
#else
  int x;
  int y;
  int w;
  int ty;
  int pad[2];                   /* declared, never used: the frame is the ROM's 0x68 */

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
    ty = (y + 30) >> 1;
    BrTextPrint("CONTROLLER PAK", 0x72, ty);
    BrTextPrint("IS NOT INSERTED", 0x72, ty + 14);
    break;
  case 7:
  case 8:
    y = 0x97;
    if (mode == 1) {
      y = 0xc3;
    }
    BrBevelPanel(0xa5, y, 0x135, 0xb2, 3, 0, 0, 0x80, 0x80, 0x80);
    ty = (y + 30) >> 1;
    BrTextPrint("INSUFFICIENT FREE PAGES", 0x5a, ty);
    BrTextPrint("OR FREE NOTES IN THE", 0x5a, ty + 12);
    BrTextPrint("CONTROLLER PAK.", 0x5a, ty + 24);
    BrTextPrint("62 PAGES AND ONE NOTE", 0x5a, ty + 42);
    BrTextPrint("ARE NEEDED TO SAVE THE", 0x5a, ty + 54);
    BrTextPrint("CUSTOM DECALS.", 0x5a, ty + 66);
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
      ty = (y + 30) >> 1;
      BrTextPrint("UNABLE TO READ FROM", (x + 16) >> 1, ty);
    } else {
      ty = (y + 30) >> 1;
      BrTextPrint("UNABLE TO WRITE TO", (x + 16) >> 1, ty);
    }
    BrTextPrint("THE CONTROLLER PAK", (x + 16) >> 1, ty + 14);
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
      ty = (y + 30) >> 1;
      BrTextPrint("DECALS NOT FOUND", (x + 16) >> 1, ty);
      BrTextPrint("IN CONTROLLER PAK", (x + 16) >> 1, ty + 14);
    } else {
      ty = (y + 30) >> 1;
      BrTextPrint("ERROR ENCOUNTERED", (x + 16) >> 1, ty);
      BrTextPrint("WHILE SAVING DECALS", (x + 16) >> 1, ty + 14);
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
    ty = (y + 30) >> 1;
    BrTextPrint("ERROR ENCOUNTERED", (x + 15) >> 1, ty);
    if (op == 9) {
      BrTextPrint("WHILE LOADING DECALS", (x + 15) >> 1, ty + 14);
    } else {
      BrTextPrint("WHILE SAVING DECALS", (x + 15) >> 1, ty + 14);
    }
    break;
  case 6:
    y = 0xcc;
    if (mode == 1) {
      y = 0xe4;
    }
    BrBevelPanel(0xad, y, 0x126, 0x48, 3, 0, 0, 0x80, 0x80, 0x80);
    ty = (y + 30) >> 1;
    BrTextPrint("BAD DATA ENCOUNTERED", 0x5d, ty);
    if (op == 9) {
      BrTextPrint("WHILE LOADING DECALS", 0x5d, ty + 14);
    } else {
      BrTextPrint("WHILE SAVING DECALS", 0x5d, ty + 14);
    }
    break;
  case 4:
#if TGR_REGION_EUR
    y = 0xaf;
    if (mode == 1) {
      y = 0xdb;
    }
    BrBevelPanel(0xae, y, 0x124, 0x82, 3, 0, 0, 0x80, 0x80, 0x80);
#else
    y = 0xb0;
    if (mode == 1) {
      y = 0xdc;
    }
    BrBevelPanel(0xae, y, 0x124, 0x80, 3, 0, 0, 0x80, 0x80, 0x80);
#endif
    ty = (y + 30) >> 1;
    BrTextPrint("CONTROLLER ERROR HAS", 0x5e, ty);
    BrTextPrint("BEEN DETECTED.", 0x5e, ty + 12);
    BrTextPrint("CUSTOM DECALS CANNOT", 0x5e, ty + 30);
    if (op == 9) {
      BrTextPrint("BE LOADED.", 0x5e, ty + 42);
    } else {
      BrTextPrint("BE SAVED.", 0x5e, ty + 42);
    }
    break;
  case 10:
    y = 0xcc;
    if (mode == 1) {
      y = 0xf8;
    }
    BrBevelPanel(0xcd, y, 0xe6, 0x48, 3, 0, 0, 0x80, 0x80, 0x80);
    ty = (y + 30) >> 1;
    BrTextPrint("CONTROLLER PAK", 0x6e, ty);
    BrTextPrint("IS NONFUNCTIONAL", 0x6e, ty + 14);
    break;
  }
#endif
}

/* WHAT IT DOES: Draw an image at its own position and size, strip by
 * strip from the bottom of its pixel data (Europe: with a palette for a
 * 4-bit image, or 0). */
/* @implements 0x80245470 tgr BrImageDraw */
#if TGR_REGION_EUR || TGR_REGION_JPN
void BrImageDraw(BrImage *img, void *tlut)
#else
void BrImageDraw(BrImage *img)
#endif
{
  unsigned int total;
  unsigned int strip;
  int n;
  int step;
  int i;

  if (img->siz == 4) {
    total = img->w * img->h >> 1;
    strip = img->w * img->stripH >> 1;
  } else {
    total = img->w * img->h * (img->siz >> 3);
    strip = img->w * img->stripH * (img->siz >> 3);
  }
  n = img->h / img->stripH;
  step = img->drawH / n;
  for (i = 1; i <= n; i++) {
    BrImageStrip(img, img->data + total - strip * i, STRIP_TLUT(tlut) img->w, img->stripH, img->x,
                 img->y + step * (i - 1), img->drawW, step, 0, 0, 0);
  }
}

/* WHAT IT DOES: Draw an image at (x, y) at its own size, strip by strip. */
/* @implements 0x80245604 tgr BrImageDrawAt */
void BrImageDrawAt(BrImage *img, int x, int y)
{
  unsigned int total;
  unsigned int strip;
  int n;
  int step;
  int i;

  if (img->siz == 4) {
    total = img->w * img->h >> 1;
    strip = img->w * img->stripH >> 1;
  } else {
    total = img->w * img->h * (img->siz >> 3);
    strip = img->w * img->stripH * (img->siz >> 3);
  }
  n = img->h / img->stripH;
  step = img->drawH / n;
  for (i = 1; i <= n; i++) {
    BrImageStrip(img, img->data + total - strip * i, STRIP_TLUT(0) img->w, img->stripH, x,
                 y + step * (i - 1), img->drawW, step, 0, 0, 0);
  }
}

/* WHAT IT DOES: Draw an image at its own position and size, tinted by the
 * given colour. */
/* @implements 0x80245798 tgr BrImageDrawTinted */
void BrImageDrawTinted(BrImage *img, unsigned char r, unsigned char g, unsigned char b)
{
  unsigned int total;
  unsigned int strip;
  int n;
  int step;
  int i;

  if (img->siz == 4) {
    total = img->w * img->h >> 1;
    strip = img->w * img->stripH >> 1;
  } else {
    total = img->w * img->h * (img->siz >> 3);
    strip = img->w * img->stripH * (img->siz >> 3);
  }
  n = img->h / img->stripH;
  step = img->drawH / n;
  for (i = 1; i <= n; i++) {
    BrImageStrip(img, img->data + total - strip * i, STRIP_TLUT(0) img->w, img->stripH, img->x,
                 img->y + step * (i - 1), img->drawW, step, r, g, b);
  }
}

/* WHAT IT DOES: Draw an image at (x, y), w by h on screen, tinted by the
 * given colour. */
/* @implements 0x8024594C tgr BrImageDrawRect */
void BrImageDrawRect(BrImage *img, int x, int y, int w, int h, unsigned char r, unsigned char g,
                     unsigned char b)
{
  int step;
  unsigned int total;
  unsigned int strip;
  int n;
  int i;

  if (img->siz == 4) {
    total = img->w * img->h >> 1;
    strip = img->w * img->stripH >> 1;
  } else {
    total = img->w * img->h * (img->siz >> 3);
    strip = img->w * img->stripH * (img->siz >> 3);
  }
  n = img->h / img->stripH;
  step = h / n;
  for (i = 1; i <= n; i++) {
    BrImageStrip(img, img->data + total - strip * i, STRIP_TLUT(0) img->w, img->stripH, x,
                 y + step * (i - 1), w, step, r, g, b);
  }
}

/* WHAT IT DOES: Draw one strip of an image: load its w by h texels (4-bit
 * intensity, 8-bit intensity-alpha or 16-bit RGBA, the last as IA for
 * D_8028CB70) as a texture block with the image's combiner, then draw it
 * as a dw by dh textured rectangle at (x, y) tinted r, g, b -- mirrored
 * left to right when dw is negative, upside down always.  Coordinates are
 * halved on a low-res screen.  The loads are libultra's LoadTextureBlock
 * sequences written out (the DXT and line arithmetic as in its macros,
 * the 8-bit line as width * 1), each through fmt; the tile-size command
 * is a block, one statement per line, as the other commands here. */
/* @implements 0x80245B00 tgr BrImageStrip */
#if TGR_REGION_EUR || TGR_REGION_JPN
void BrImageStrip(BrImage *img, unsigned char *data, void *tlut, int w, int h, int x, int y, int dw,
                  int dh, unsigned char r, unsigned char g, unsigned char b)
#else
void BrImageStrip(BrImage *img, unsigned char *data, int w, int h, int x, int y, int dw, int dh,
                  unsigned char r, unsigned char g, unsigned char b)
#endif
{
  int bitsS;
  int bitsT;
  int maskS;
  int maskT;
  int flip;
  int fmt;

  flip = 0;
  if (dw < 0) {
    dw = -dw;
    flip = 1;
  }
  BrTexSizeBits(w, &maskS, &bitsS);
  BrTexSizeBits(h, &maskT, &bitsT);
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
  gSPTexture(D_8028A858++, maskS, maskT, 0, 0, 1);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 12, 2, D_8028A898);
  switch (img->siz) {
  case 4:
#if TGR_REGION_EUR || TGR_REGION_JPN
    if (tlut != 0) {
      fmt = 2;
      gDPSetCombine(D_8028A858++, 0xffffff, 0xfffcf279);
      gDPSetRenderMode(D_8028A858++, 0x0f0a4000, 0);
    } else {
#endif
    fmt = 4;
    if (img == &D_8028CB40) {
      gDPSetCombine(D_8028A858++, 0x309861, 0x5532ff7f);
      gDPSetRenderMode(D_8028A858++, 0x0f0a4000, 0);
    } else {
      gDPSetCombine(D_8028A858++, 0xffffff, 0xfffdf2f9);
      gDPSetRenderMode(D_8028A858++, 0x00504240, 0);
    }
#if TGR_REGION_EUR || TGR_REGION_JPN
    }
#endif
    gDPSetTextureImage(D_8028A858++, fmt, 2, 1, data);
    gDPSetTile(D_8028A858++, fmt, 2, 0, 0, 7, 0, 0, bitsT, 0, 0, bitsS, 0);
    gDPLoadSync(D_8028A858++);
    gDPLoadBlock(D_8028A858++, 7, 0, 0, ((w * h + 3) >> 2) - 1, CALC_DXT_4b(w));
    gDPPipeSync(D_8028A858++);
    gDPSetTile(D_8028A858++, fmt, 0, ((w >> 1) + 7) >> 3, 0, 0, 0, 0, bitsT, 0, 0, bitsS, 0);
    {
      Gfx *_g = (Gfx *)(D_8028A858++);

      _g->words.w0 = _SHIFTL(G_SETTILESIZE, 24, 8);
      _g->words.w1 = _SHIFTL((w - 1) << 2, 12, 12) | _SHIFTL((h - 1) << 2, 0, 12);
    }
    break;
  case 8:
    fmt = 3;
    if (img->kind == 'c') {
      gDPSetCombine(D_8028A858++, 0x11fe23, 0xfffff3f9);
    } else {
      gDPSetCombine(D_8028A858++, 0xffffff, 0xfffcf279);
    }
    gDPSetRenderMode(D_8028A858++, 0x00504240, 0);
    gDPSetTextureImage(D_8028A858++, fmt, 2, 1, data);
    gDPSetTile(D_8028A858++, fmt, 2, 0, 0, 7, 0, 0, bitsT, 0, 0, bitsS, 0);
    gDPLoadSync(D_8028A858++);
    gDPLoadBlock(D_8028A858++, 7, 0, 0, ((w * h + 1) >> 1) - 1, CALC_DXT(w, 1));
    gDPPipeSync(D_8028A858++);
    gDPSetTile(D_8028A858++, fmt, 1, (w * 1 + 7) >> 3, 0, 0, 0, 0, bitsT, 0, 0, bitsS, 0);
    {
      Gfx *_g = (Gfx *)(D_8028A858++);

      _g->words.w0 = _SHIFTL(G_SETTILESIZE, 24, 8);
      _g->words.w1 = _SHIFTL((w - 1) << 2, 12, 12) | _SHIFTL((h - 1) << 2, 0, 12);
    }
    break;
  case 16:
    if (img == &D_8028CB70) {
      fmt = 3;
    } else {
      fmt = 0;
    }
    gDPSetCombine(D_8028A858++, 0xffffff, 0xfffcf279);
    gDPSetRenderMode(D_8028A858++, 0x00504240, 0);
    gDPSetTextureImage(D_8028A858++, fmt, 2, 1, data);
    gDPSetTile(D_8028A858++, fmt, 2, 0, 0, 7, 0, 0, bitsT, 0, 0, bitsS, 0);
    gDPLoadSync(D_8028A858++);
    gDPLoadBlock(D_8028A858++, 7, 0, 0, w * h - 1, CALC_DXT(w, 2));
    gDPPipeSync(D_8028A858++);
    gDPSetTile(D_8028A858++, fmt, 2, (w * 2 + 7) >> 3, 0, 0, 0, 0, bitsT, 0, 0, bitsS, 0);
    {
      Gfx *_g = (Gfx *)(D_8028A858++);

      _g->words.w0 = _SHIFTL(G_SETTILESIZE, 24, 8);
      _g->words.w1 = _SHIFTL((w - 1) << 2, 12, 12) | _SHIFTL((h - 1) << 2, 0, 12);
    }
    break;
  }
#if TGR_REGION_EUR || TGR_REGION_JPN
  if (fmt == 2) {
    /* a 4-bit image with a palette: load its 16 colours (RGBA16) */
    gDPSetTextureImage(D_8028A858++, 0, 2, 1, tlut);
    gDPTileSync(D_8028A858++);
    gDPSetTile(D_8028A858++, 0, 0, 0, 0x100, 7, 0, 0, 0, 0, 0, 0, 0);
    gDPLoadSync(D_8028A858++);
    gDPLoadTLUTCmd(D_8028A858++, 7, 15);
    gDPPipeSync(D_8028A858++);
    gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 14, 2, 0x8000);
  } else {
    gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 14, 2, 0);
  }
#else
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 14, 2, 0);
#endif
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 19, 1, 0);
  if (D_8028A850 == 0) {
    x >>= 1;
    y >>= 1;
    dw >>= 1;
    dh >>= 1;
  }
  gDPSetPrimColor(D_8028A858++, 0xff, 0xff, r, g, b, 0xff);
  gDPSetEnvColor(D_8028A858++, 0, 0, 0, 0xff);
  gSPTextureRectangle(D_8028A858++, x << 2, y << 2, (x + dw) << 2, (y + dh) << 2, 0,
                      flip ? (w - 1) << 5 : 0, (h - 1) << 5,
                      flip ? (0x400 - (w << 10)) / dw : ((w << 10) - 0x400) / dw, (0x400 - (h << 10)) / dh);
  gDPPipeSync(D_8028A858++);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 19, 1, 0x80000);
}

/* WHAT IT DOES: Draw the part (s, t, sw by th texels) of a 4-bit image as a
 * w by h rectangle at (x, y), upside down, tinted r, g, b: load the texels
 * as a 4-bit tile, then one textured rectangle stepping back up the rows.
 * Coordinates are halved on a low-res screen.  The two tile commands, the
 * rectangle and its first half-word are multi-line blocks: IDO schedules a
 * command's stores by its line layout. */
/* @implements 0x802465F0 tgr BrImageDrawPart */
void BrImageDrawPart(BrImage *img, int s, int t, int sw, int th, int x, int y, int w, int h,
                     unsigned char r, unsigned char g, unsigned char b)
{
  int lrs;
  int lrt;

  lrs = s + sw;
  lrt = t + th;
  gRaw(D_8028A858++, 0xe7000000, 0);
  gRaw(D_8028A858++, 0xba001402, 0);
  gRaw(D_8028A858++, 0xba001001, 0);
  gRaw(D_8028A858++, 0xba000c02, D_8028A898);
  gRaw(D_8028A858++, 0xfcffffff, 0xfffdf2f9);
  gRaw(D_8028A858++, 0xb900031d, 0x504240);
  gRaw(D_8028A858++, 0xfd880000 | _SHIFTL(((unsigned int)img->w >> 1) - 1, 0, 12), img->data);
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    _g->words.w0 = 0xf5880000 | _SHIFTL((((lrs - s + 1) >> 1) + 7) >> 3, 9, 9);
    _g->words.w1 = 0x7080200;
  }
  gRaw(D_8028A858++, 0xe6000000, 0);
  gRaw(D_8028A858++, 0xf4000000 | _SHIFTL(s << 1, 12, 12) | _SHIFTL(t << 2, 0, 12),
       0x7000000 | _SHIFTL(lrs << 1, 12, 12) | _SHIFTL(lrt << 2, 0, 12));
  gRaw(D_8028A858++, 0xe7000000, 0);
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    _g->words.w0 = 0xf5800000 | _SHIFTL((((lrs - s + 1) >> 1) + 7) >> 3, 9, 9);
    _g->words.w1 = 0x80200;
  }
  gRaw(D_8028A858++, 0xf2000000 | _SHIFTL(s << 2, 12, 12) | _SHIFTL(t << 2, 0, 12),
       _SHIFTL(lrs << 2, 12, 12) | _SHIFTL(lrt << 2, 0, 12));
  gRaw(D_8028A858++, 0xba000e02, 0);
  gRaw(D_8028A858++, 0xba001301, 0);
  if (D_8028A850 == 0) {
    x >>= 1;
    y >>= 1;
    w >>= 1;
    h >>= 1;
  }
  gDPSetPrimColor(D_8028A858++, 0xff, 0xff, r, g, b, 0xff);
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    _g->words.w0 = 0xe4000000 | _SHIFTL((x + w) << 2, 12, 12) | _SHIFTL((y + h) << 2, 0, 12);
    _g->words.w1 = _SHIFTL(x << 2, 12, 12) | _SHIFTL(y << 2, 0, 12);
  }
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    _g->words.w0 = _SHIFTL(G_RDPHALF_1, 24, 8);
    _g->words.w1 = _SHIFTL((s - 1) << 5, 16, 16) | _SHIFTL((lrt - 1) << 5, 0, 16);
  }
  gImmp1(D_8028A858++, G_RDPHALF_2, _SHIFTL(((sw << 10) - 0x400) / w, 16, 16) | _SHIFTL((1024 - th * 1024) / h, 0, 16));
  gRaw(D_8028A858++, 0xe7000000, 0);
  gRaw(D_8028A858++, 0xba001301, 0x80000);
}

/* WHAT IT DOES: Plot one grey pixel at (x, y) (fill mode, RGBA5551), in
 * 640-wide coordinates halved on a low-res screen. */
/* @implements 0x80246A80 tgr BrFillPoint */
void BrFillPoint(int x, int y, unsigned char v)
{
  if (D_8028A850 == 0) {
    x >>= 1;
    y >>= 1;
  }
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_FILL);
  gDPSetRenderMode(D_8028A858++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  gDPSetFillColor(D_8028A858++, GPACK_RGBA5551(v, v, v, 1) << 16 | GPACK_RGBA5551(v, v, v, 1));
  gDPFillRectangle(D_8028A858++, x, y, x + 1, y + 1);
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
}

/* WHAT IT DOES: Draw the outline of a w by h box at (x, y), t pixels thick,
 * in one colour: top, bottom, left and right edges as fill rectangles. */
/* @implements 0x80246BCC tgr BrFillFrame */
void BrFillFrame(int x, int y, int w, int h, int t, unsigned char r, unsigned char g,
                 unsigned char b)
{
  if (D_8028A850 == 0) {
    x >>= 1;
    y >>= 1;
    w >>= 1;
    h >>= 1;
    t >>= 1;
  }
  if (t < 2) {
    t = 1;
  }
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_FILL);
  gDPSetRenderMode(D_8028A858++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  gDPSetFillColor(D_8028A858++, GPACK_RGBA5551(r, g, b, 1) << 16 | GPACK_RGBA5551(r, g, b, 1));
  gDPFillRectangle(D_8028A858++, x, y, x + w, y + t);
  gDPFillRectangle(D_8028A858++, x, y + h - t, x + w, y + h);
  gDPFillRectangle(D_8028A858++, x, y, x + t, y + h);
  gDPFillRectangle(D_8028A858++, x + w - t, y, x + w, y + h);
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
}

/* WHAT IT DOES: Fill a w by h rectangle at (x, y) in one colour (fill mode,
 * RGBA5551), in 640-wide coordinates halved on a low-res screen; at least
 * one pixel each way. */
/* @implements 0x80246E10 tgr BrFillRect */
void BrFillRect(int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b)
{
  if (D_8028A850 == 0) {
    x >>= 1;
    y >>= 1;
    w >>= 1;
    h >>= 1;
  }
  if (w < 2) {
    w = 1;
  }
  if (h < 2) {
    h = 1;
  }
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_FILL);
  gDPSetRenderMode(D_8028A858++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  gDPSetFillColor(D_8028A858++, GPACK_RGBA5551(r, g, b, 1) << 16 | GPACK_RGBA5551(r, g, b, 1));
  gDPFillRectangle(D_8028A858++, x, y, x + w, y + h);
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
}

/* WHAT IT DOES: Draw a bevelled panel: unless asked not to, a w by h fill
 * in r, g, b; then a bevel `bevel` pixels deep around it -- the left and
 * top edges light grey (0xE0), the right and bottom dark (0x30), the two
 * swapped for a sunken panel (sunken == 1).  In 640-wide coordinates,
 * halved on a low-res screen.  The loop index is declared first (its slot
 * sits above the two shades). */
/* @implements 0x80246F90 tgr BrBevelPanel */
void BrBevelPanel(int x, int y, int w, int h, int bevel, char noFill, char sunken, unsigned char r,
                  unsigned char g, unsigned char b)
{
  int i;
  unsigned char hi;
  unsigned char lo;

  hi = 0xe0;
  lo = 0x30;
  if (D_8028A850 == 0) {
    x >>= 1;
    y >>= 1;
    w >>= 1;
    h >>= 1;
    bevel >>= 1;
  }
  if (bevel < 2) {
    bevel = 1;
  }
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_FILL);
  gDPSetRenderMode(D_8028A858++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  if (noFill == 0) {
    gDPSetFillColor(D_8028A858++, GPACK_RGBA5551(r, g, b, 1) << 16 | GPACK_RGBA5551(r, g, b, 1));
    gDPFillRectangle(D_8028A858++, x, y, x + w, y + h);
    gDPPipeSync(D_8028A858++);
  }
  if (sunken == 1) {
    BrSwapBytes(&hi, &lo);
  }
  gDPSetFillColor(D_8028A858++, GPACK_RGBA5551(hi, hi, hi, 1) << 16 | GPACK_RGBA5551(hi, hi, hi, 1));
  for (i = 1; i <= bevel; i++) {
    gDPFillRectangle(D_8028A858++, x - i, y - i, x - i + 1, y + h + i);
  }
  for (i = 1; i <= bevel; i++) {
    gDPFillRectangle(D_8028A858++, x - i, y - i, x + w + i, y - i + 1);
  }
  gDPPipeSync(D_8028A858++);
  gDPSetFillColor(D_8028A858++, GPACK_RGBA5551(lo, lo, lo, 1) << 16 | GPACK_RGBA5551(lo, lo, lo, 1));
  for (i = 1; i <= bevel; i++) {
    gDPFillRectangle(D_8028A858++, x + w + i - 1, y - i, x + w + i, y + h + i);
  }
  for (i = 1; i <= bevel; i++) {
    gDPFillRectangle(D_8028A858++, x - i, y + h + i - 1, x + w + i, y + h + i);
  }
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
}

/* WHAT IT DOES: The paint shop's clear popup: four previews (the current
 * decal or the whole car, in the selected colour or the body colour), the
 * chosen one framed in cyan, its description below, and A (select) and B
 * (cancel) buttons.  Left/right move the choice; A fills the current decal
 * or all ten decals with the colour, B cancels.
 * The pad record's index is multiplied unsigned (sizeof), as the ROM keeps
 * 0x15C in a saved register for multu.
 * i is also the plots' x counter, which ranks it above the box-row
 * pointer; the offsets are computed dx1, dy1, dx2, dy2; the option
 * dispatch is a switch whose cases each restore the saved mode; the
 * restore after the all-decals clear indexes through the just-stored
 * current decal; the register locals sit above y in the frame. */
/* @implements 0x80247B0C tgr BrPaintClearMenu */
void BrPaintClearMenu()
{
  int i;
  unsigned int dx1;
  unsigned int dy1;
  unsigned int dx2;
  unsigned int dy2;
  int u0;
  int py;
  int k;
  int unused[8];
  int y;
  int bx;
  int r[4][4];
#if TGR_REGION_JPN
  char *msg[4][2] = {
    { "%j\xE7\x7F;\x81\x88(k", "  %jqt\xBB\x81(\xA8w\xE0" "B6R&" },
    { "%j\xCC\xBB\x9E\x81\x88(k", "  %jqt\xBB\x81(\xA8w\xE0" "B6R&" },
    { "%j\xE7\x7F;\x81\x88(k", "  %j\xEA" "ct\xBB\x81(\xA8w\xE0" "B6R&" },
    { "%j\xCC\xBB\x9E\x81\x88(k", "  %j\xEA" "ct\xBB\x81(\xA8w\xE0" "B6R&" },
  };
#else
  char *what[2] = { "CLEAR CURRENT DECAL", "CLEAR ALL DECALS" };
  char *to[2] = { "TO SELECTED COLOR", "TO BODY COLOR" };
#endif

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
    if (i == D_8028CFC0.kind) {
      BrFillFrame(r[i][0], r[i][1], r[i][2], r[i][3], 1, 0, 0xff, 0xff);
    } else {
      BrFillFrame(r[i][0], r[i][1], r[i][2], r[i][3], 1, 0x20, 0x20, 0x20);
    }
  }
  dx1 = (r[0][2] - ((unsigned int)D_8028DB0C[D_8028DB68]->w >> 1)) >> 1;
  dy1 = (r[0][3] - (D_8028DB0C[D_8028DB68]->h >> 1)) >> 1;
  dx2 = (unsigned int)(r[2][2] - D_8028CBA0.w) >> 1;
  dy2 = (r[2][3] - D_8028CBA0.h) >> 1;
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
  BrBevelPanel(0xba, r[0][1] + r[0][3] + 12, 0x10c, 0x3a, 1, 1, 1, 0x80, 0x80, 0x80);
#if TGR_REGION_JPN
  BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xf5, 0);
  BrTextSetFont(14);
  BrTextPrint(msg[D_8028CFC0.kind][0], 0x9e, 0x8e);
  BrTextPrint(msg[D_8028CFC0.kind][1], 0x9e, 0x9b);
#else
#if TGR_REGION_EUR
  BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xf5, 0);
  BrTextSetFont(11);
#else
  BrTextSetFont(11);
  BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xf5, 0);
#endif
  BrTextPrint(what[D_8028CFC0.kind >> 1], 0x9e, 0x8e);
  BrTextPrint(to[D_8028CFC0.kind & 1], 0x9e, 0x9b);
#endif
  BrTextSetFont(10);
  BrTextAlignLeft();
  BrTextPrint("%wwSELECT", ((unsigned int)D_8028D0B0.w + 0xd9) >> 1, (y + 18) >> 1);
  BrTextPrint("%wwCANCEL", (bx + (unsigned int)D_8028D0E0.w + 6) >> 1, (y + 18) >> 1);
  BrImageDrawAt(&D_8028D0B0, 0xd3, y);
  BrImageDrawAt(&D_8028D0E0, bx, y);
  BrPadStickToButtons(&PADS[D_8028DBBC]);
  if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)) & 4) {
    BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)), 4);
    if (D_8028CFC0.kind == 0) {
      D_8028CFC0.kind = 3;
    } else {
      D_8028CFC0.kind--;
    }
  } else if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)) & 1) {
    BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)), 1);
    if (D_8028CFC0.kind == 3) {
      D_8028CFC0.kind = 0;
    } else {
      D_8028CFC0.kind++;
    }
  }
  if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)) & 0x10) {
    BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)), 0x10);
    switch (D_8028CFC0.kind) {
    case 0:
    case 1:
      BrPaintDecalCommit();
      for (py = 0; py < D_8028DB8C; py++) {
        for (i = 0; i < D_8028DB88; i++) {
          if (D_8028CFC0.kind == 0) {
            BrPaintPlot(i, py, D_8028DB58);
          } else {
            BrPaintPlot(i, py, 0);
          }
        }
      }
      D_8028DB60 = D_8028DB64;
      break;
    case 2:
    case 3:
      D_8028DB70 = D_8028DB68;
      for (k = 0; k < 10; k++) {
        D_8028DB78 = ((BrDecalModel *)D_8028AB08)->parts[((BrDecalModel *)D_8028AB08)->decalPart[k]].tex;
        D_8028DB7C = ((BrDecalModel *)D_8028AB08)->masks[k];
        D_8028DB88 = ((BrDecalModel *)D_8028AB08)->parts[((BrDecalModel *)D_8028AB08)->decalPart[k]].w;
        D_8028DB8C = ((BrDecalModel *)D_8028AB08)->parts[((BrDecalModel *)D_8028AB08)->decalPart[k]].h;
        for (py = 0; py < D_8028DB8C; py++) {
          for (i = 0; i < D_8028DB88; i++) {
            if (D_8028CFC0.kind == 2) {
              BrPaintPlot(i, py, D_8028DB58);
            } else {
              BrPaintPlot(i, py, 0);
            }
          }
        }
      }
      D_8028DB68 = D_8028DB70;
      D_8028DB78 = ((BrDecalModel *)D_8028AB08)->parts[((BrDecalModel *)D_8028AB08)->decalPart[D_8028DB68]].tex;
      D_8028DB7C = ((BrDecalModel *)D_8028AB08)->masks[D_8028DB68];
      D_8028DB88 = ((BrDecalModel *)D_8028AB08)->parts[((BrDecalModel *)D_8028AB08)->decalPart[D_8028DB68]].w;
      D_8028DB8C = ((BrDecalModel *)D_8028AB08)->parts[((BrDecalModel *)D_8028AB08)->decalPart[D_8028DB68]].h;
      BrPaintDecalCommit();
      D_8028CFC0.kind = 0;
      D_8028DB60 = D_8028DB64;
      break;
    default:
      D_8028CFC0.kind = 0;
      D_8028DB60 = D_8028DB64;
      break;
    }
  } else if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)) & 0x20) {
    BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)), 0x20);
    D_8028CFC0.kind = 0;
    D_8028DB60 = D_8028DB64;
  }
}

/* WHAT IT DOES: The paint shop's mirror/flip popup: three option boxes
 * (MIRROR, greyed when the decal has no mirror side; FLIP H; FLIP V) with
 * the chosen one framed in blue, a description of the choice below (which
 * side a mirror goes to, by the decal), and A (select) and B (cancel)
 * buttons.  Left/right move the choice; A applies it (mirror or flip); B
 * cancels back to the previous screen.  Europe looks the mirror lines up
 * in a table by decal part instead of a switch. */
/* @implements 0x8024843C tgr BrPaintMirrorMenu */
void BrPaintMirrorMenu()
{
  int i;
  int unused[8];
  int y;
  int bx;
  int r[3][4];
#if TGR_REGION_JPN
  char *flip[2][2] = {
    { "%jqt\xBB\x81(\xA8w", " %j\xD8\xD9\xDC\xDD" "B6R&" },
    { "%jqt\xBB\x81(\xA8w", " %j\xDA\xDB\xDC\xDD" "B6R&" },
  };
  char *mirror[8][2] = {        /* by decal part: where its mirror goes */
    { "%jqt\xBB\x81(\xA8w", " %j\xD9\xCB\x80" "D\xDE\xDF" "B6R&" },
    { "%jqt\xBB\x81(\xA8w", " %j\xD8\xCB\x80" "D\xDE\xDF" "B6R&" },
    { "%jqt\xBB\x81(\xA8w\xD9\xA5\xC8\xCF\xC3", " %j\xA5\xBE\xCF\x8B(D\xDE\xDF" "B6R&" },
    { "%jqt\xBB\x81(\xA8w\xD8\xA5\xC8\xCF\xC3", " %j\xA5\xBE\xCF\x8B(D\xDE\xDF" "B6R&" },
    { 0, 0 },
    { 0, 0 },
    { "%jqt\xBB\x81(\xA8w\xD9\x98\x80", " %j\xA5\xBE\xCF\x8B(D\xDE\xDF" "B6R&" },
    { "%jqt\xBB\x81(\xA8w\xD8\x98\x80", " %j\xA5\xBE\xCF\x8B(D\xDE\xDF" "B6R&" },
  };
#else
  char *flip1[2] = { "FLIP CURRENT DECAL", "FLIP CURRENT DECAL" };
  char *flip2[2] = { "HORIZONTALLY", "VERTICALLY" };
#endif
#if TGR_REGION_EUR
  char *mirror[8][2] = {        /* by decal part: where its mirror goes */
    { "MIRROR CURRENT DECAL", "TO THE RIGHT DOOR" },
    { "MIRROR CURRENT DECAL", "TO THE LEFT DOOR" },
    { "MIRROR CURRENT DECAL TO", "THE RIGHT FRONT FENDER" },
    { "MIRROR CURRENT DECAL TO", "THE LEFT FRONT FENDER" },
    { 0, 0 },
    { 0, 0 },
    { "MIRROR CURRENT DECAL TO", "THE RIGHT REAR FENDER" },
    { "MIRROR CURRENT DECAL TO", "THE LEFT REAR FENDER" },
  };
#endif

  y = 0x155 - D_8028D0B0.w;
  bx = 0x163 - D_8028D0E0.w;
  if (D_8028DBC0 != 0) {
    D_8028DBB8 = D_8028CF90.kind;
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
#if TGR_REGION_JPN
  BrBevelPanel(0xac, 0xf8, 0x128, 0x3a, 1, 1, 1, 0x80, 0x80, 0x80);
  BrTextSetFont(14);
  BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xf5, 0);
  if (D_8028DBB8 == 0) {
    if (D_8028DBCC != 0) {
      BrTextPrint(mirror[D_8028DB68][0], 0x9f, 0x87);
      BrTextPrint(mirror[D_8028DB68][1], 0x9f, 0x94);
    } else {
      BrTextPrint("%jqt\xBB\x81(\xA8" "5", 0x9f, 0x87);
      BrTextPrint(" %j\xE9" "D\xDE\xDFkA6b\x7F&", 0x9f, 0x94);
    }
  } else {
    BrTextPrint(flip[D_8028DBB8 - 1][0], 0x9f, 0x87);
    BrTextPrint(flip[D_8028DBB8 - 1][1], 0x9f, 0x94);
  }
#else
  BrBevelPanel(0xac, 0xf8, 0x128, 0x3a, 1, 1, 1, 0x80, 0x80, 0x80);
  BrTextSetFont(11);
  BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xf5, 0);
  if (D_8028DBB8 == 0) {
    if (D_8028DBCC != 0) {
#if TGR_REGION_EUR
      BrTextPrint(mirror[D_8028DB68][0], 0x9f, 0x87);
      BrTextPrint(mirror[D_8028DB68][1], 0x9f, 0x94);
#else
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
#endif
    } else {
      BrTextPrint("MIRROR IS NOT APPLICABLE", 0x9f, 0x87);
      BrTextPrint("TO THE CURRENT DECAL", 0x9f, 0x94);
    }
  } else {
    BrTextPrint(flip1[D_8028DBB8 - 1], 0x9f, 0x87);
    BrTextPrint(flip2[D_8028DBB8 - 1], 0x9f, 0x94);
  }
#endif
  BrTextSetFont(10);
  BrTextAlignLeft();
  BrTextPrint("%wwSELECT", ((unsigned int)D_8028D0B0.w + 0xd9) >> 1, (y + 18) >> 1);
  BrTextPrint("%wwCANCEL", (bx + (unsigned int)D_8028D0E0.w + 6) >> 1, (y + 18) >> 1);
  BrImageDrawAt(&D_8028D0B0, 0xd3, y);
  BrImageDrawAt(&D_8028D0E0, bx, y);
  BrPadStickToButtons(&PADS[D_8028DBBC]);
  if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 1) {
    BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 1);
    if (D_8028DBB8 == 2) {
      D_8028DBB8 = 0;
    } else {
      D_8028DBB8++;
    }
  } else if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 4) {
    BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 4);
    if (D_8028DBB8 == 0) {
      D_8028DBB8 = 2;
    } else {
      D_8028DBB8--;
    }
  }
  if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 0x10) {
    BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 0x10);
    D_8028CF90.kind = D_8028DBB8;
    if (D_8028DBB8 == 0) {
      if (D_8028DBCC != 0) {
        BrPaintMirrorSide();
      }
    } else {
      BrPaintFlipApply();
    }
  } else if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 0x20) {
    BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 0x20);
    D_8028CF90.kind = 0;
    D_8028DBE0 = 0;
    D_8028DB60 = D_8028DB64;
  }
}

/* WHAT IT DOES: Copy the decal to the car's other side: when the mirror
 * is asked for (0x8028DBCC), switch the paint view to its opposite (0<->1,
 * 2<->3, 6<->7), load that side's decal, texture and mask, store the working
 * decal, then redraw it from the old side's texture mirrored left-right and
 * start the view turning.  The saved dirty flag is copied back either way. */
/* @implements 0x80248B80 tgr BrPaintMirrorSide */
void BrPaintMirrorSide()
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
    D_8028DCA4 = ((BrCarModel *)D_8028AB08)->parts[((BrCarModel *)D_8028AB08)->decalPart[D_8028DB6C]].a;
    D_8028DB68 = D_80369DAB;
    D_8028DB78 = ((BrCarModel *)D_8028AB08)->parts[((BrCarModel *)D_8028AB08)->decalPart[D_8028DB68]].a;
    D_8028DB7C = ((BrCarModel *)D_8028AB08)->x11c[D_8028DB68];
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

/* WHAT IT DOES: Apply the pending decal flip: store the working decal into
 * the decal buffer, then redraw every texel of the working decal from that
 * stored copy mirrored left-right (flip 1) or top-bottom (flip 2); clear
 * the pending flip and copy the saved dirty flag back. */
/* @implements 0x80248DB4 tgr BrPaintFlipApply */
void BrPaintFlipApply()
{
  int x;
  int y;

  BrPaintDecalCommit();
  if (D_8028CF90.kind == 1) {
    for (y = 0; y < D_8028DB8C; y++) {
      for (x = 0; x < D_8028DB88; x++) {
        BrPaintPlot(D_8028DB88 - x - 1, y, BrPaintPeek(D_8028DB80, x, y));
      }
    }
  } else if (D_8028CF90.kind == 2) {
    for (y = 0; y < D_8028DB8C; y++) {
      for (x = 0; x < D_8028DB88; x++) {
        BrPaintPlot(x, D_8028DB8C - y - 1, BrPaintPeek(D_8028DB80, x, y));
      }
    }
  }
  D_8028CF90.kind = 0;
  D_8028DB60 = D_8028DB64;
}

/* WHAT IT DOES: Set aside the 2 KB decal buffer from the interface memory
 * pool, then the paint shop's two car-texture buffers. */
/* @implements 0x80248F38 tgr BrDecalMemInit */
void BrDecalMemInit()
{
#if !TGR_REGION_JPN
  osSyncPrintf("\nAllocating %d bytes for decal buffer...\n\n",0x800);
#endif
  D_8028DB80 = BrIfaceMemAlloc(0x800);
  BrAllocPaintShopGfxMem(&D_8028D0B0);
  BrAllocPaintShopGfxMem(&D_8028D0E0);
}

/* WHAT IT DOES: One frame of moving the paint shop's ten decals and its
 * colour palette to or from the Controller Pak in `port` (op 9 loads, any
 * other saves; fromMenu 1 when called from the car-select menus, which skip
 * the confirmation).  A state machine: confirm; fade the sound; probe the
 * pak (swap a Rumble Pak, remember a new pak's id); find the decal file (on a
 * save, confirm an overwrite or allocate it); move one decal a frame, each
 * masked by its keep-mask on a load, with a progress bar; move the palette
 * and share it with the other painted parts; offer the Rumble Pak back; then
 * report.  Sets *done when the caller may leave; returns the pak status on
 * an error.  (Run by the box with an empty pak: save, load, overwrite; the
 * pak errors, a new pak and the Rumble Pak swap are not reached.)
 * Europe also dumps each loaded decal and the loaded palette on the debug
 * output; both dump loops keep their counters in blocks of their own, which
 * puts their homes below the arrays where the ROM frame has them. */
/* @implements 0x80248F88 tgr BrDecalPakTransfer */
int BrDecalPakTransfer(BrPaintModel *m, unsigned char port, char op, char fromMenu,
                       unsigned char *done)
{
  static unsigned char D_8028DCBC = 0;  /* started */
  static unsigned char D_8028DCC0 = 0;  /* the file still has to be allocated */
  static int D_8028DCC4 = 0;            /* bytes transferred so far */
  static unsigned char D_80369E50;      /* the Rumble Pak was taken out for this */
  static unsigned char D_80369E51;      /* a new pak turned up mid-transfer */
  static int D_80369E54;                /* the last Controller Pak status */
  static int D_80369E58;                /* the file */
  static int D_80369E5C;                /* the size of the decal being moved */
  static unsigned char D_80369E60;      /* the state */
  static unsigned char D_80369E61;      /* the decal being moved */
  static unsigned char D_80369E62;      /* a frame counter */
  BrPaintPart *part;
  OSPfs *pfs;
  unsigned char *tex;
  int top;
  unsigned char *mask;
  int stride;
  int half;
  int ty;
  int row;
  int y;
  int col;
  int freeBytes;
  int w;
  int h;
  unsigned short *pal;
  int k;
  int flip;
  int n;
  int x2;
  int tx;
  unsigned char pakBits;
  unsigned char name[16] = {
    0x2D, 0x28, 0x29, 0x0F, 0x20, 0x1E, 0x1A, 0x2B, 0x0F, 0x1D, 0x1E, 0x1C, 0x1A, 0x25, 0x2C, 0x00
  };
  unsigned char ext[4] = { 0, 0, 0, 0 };

  D_802A4BE8 = 0;
  if (D_8028DCBC == 0) {
    D_8028DCBC = 1;
    D_80369E50 = 0;
    D_80369E51 = 0;
    D_80369E54 = 0;
    D_80369E61 = 0;
    D_8028DCC4 = 0;
    D_80369E5C = 0;
    D_80369E62 = 0;
    if (fromMenu == 1) {
      D_80369E60 = 0;
    } else {
      D_80369E60 = 2;
    }
  }
  *done = 0;
  switch (D_80369E60) {
  case 0:
    BrSfxFadeTo(0.0f, 0.2f);
    BrMusicFadeTo(0.0f, 0.2f);
    D_80369E60 = 1;
    break;
  case 1:
    if (BrSfxFadeDone() != 0 && D_80369E62++ == 3) {
      D_80369E60 = 3;
      D_80369E62 = 0;
    }
    break;
  case 2:                       /* load/save? */
    y = 0x110 - D_8028D0B0.w;
#if TGR_REGION_JPN
    BrBevelPanel(0xAA, 200, 300, 0x50, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextAlignLeft();
    BrTextHighlightOff();
    BrTextSetFont(14);
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
    if (op == OP_LOAD) {
      BrTextPrint("%j\xBB\x81(\xA8w\xC8(\xCB" "B6R1%%", 94, 116);
    } else {
      BrTextPrint("%j\xBB\x81(\xA8w\xB2(\xAC" "B6R1%%", 94, 116);
    }
    BrTextSetFont(10);
    BrTextPrint("%wwOK", (D_8028D0B0.w + 0xC0U) >> 1, (y + 18) >> 1);
    BrTextPrint("%wwCANCEL", (D_8028D0E0.w + 0x168U) >> 1, (y + 18) >> 1);
    BrImageDrawAt(&D_8028D0B0, 0xBA, y);
    BrImageDrawAt(&D_8028D0E0, 0x162, y);
#else
    BrBevelPanel(0xDC, 200, 200, 0x50, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextAlignLeft();
    BrTextHighlightOff();
    BrTextSetFont(14);
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
    if (op == OP_LOAD) {
      BrTextPrint("LOAD DECALS?", 119, 116);
    } else {
      BrTextPrint("SAVE DECALS?", 119, 116);
    }
    BrTextSetFont(10);
    BrTextPrint("%wwOK", (D_8028D0B0.w + 0xF2U) >> 1, (y + 18) >> 1);
    BrTextPrint("%wwCANCEL", (D_8028D0E0.w + 0x136U) >> 1, (y + 18) >> 1);
    BrImageDrawAt(&D_8028D0B0, 0xEC, y);
    BrImageDrawAt(&D_8028D0E0, 0x130, y);
#endif
    if (PAD->pressed & 0x10) {
      BrPadConsume(PAD, 0x10);
      D_80369E60 = 0;
    } else {
      if (PAD->pressed & 0x20) {
        BrPadConsume(PAD, 0x20);
        *done = 1;
        D_8028DB60 = D_8028DB64;
        return 0;
      }
    }
    break;
  case 3:                       /* probe the pak */
    osPfsIsPlug(D_80272D48, &pakBits);
    if ((pakBits & (1 << port)) == 0) {
      D_80369E54 = 1;
      D_80369E60 = 12;
    } else {
#if !TGR_REGION_JPN
      osSyncPrintf("\nInitializing controller pak...\n");
#endif
      D_80369E54 = osPfsInitPak(D_80272D48, PFS, port);
      if (D_80369E54 == 0) {
        D_8031B1E8[D_80271FA8] = 0;
        if (D_80316420[port] == 0) {
          D_80316420[port] = 1;
          memcpy(D_803163E0[port], D_80369EC0[port].id, 32);
          D_80369E60 = 6;
        } else if (bcmp(D_803163E0[port], D_80369EC0[port].id, 32) != 0) {
          memcpy(D_803163E0[port], D_80369EC0[port].id, 32);
          D_80369E60 = 9;
        } else {
          D_80369E60 = 6;
        }
      } else if (D_80369E54 == 10) {
        if (osMotorInit(D_80272D48, &D_8031A3F8[port], port) == 0) {
          osMotorStop(&D_8031A3F8[port]);
          D_80369E54 = 9999;
          D_80369E50 = 1;
          D_80369E60 = 10;
        } else {
          D_8031B1E8[D_80271FA8] = 0;
          D_80369E54 = 0;
          D_80369E60 = 4;
        }
      } else {
        D_8031B1E8[D_80271FA8] = 0;
        D_80369E60 = 12;
      }
    }
    break;
  case 4:
    D_80369E54 = osPfsRepairId(PFS);
    if (D_80369E54 != 0) {
      D_80369E60 = 10;
    } else {
      D_80369E60 = 3;
    }
    break;
  case 6:                       /* find the file */
    D_8028DCC4 = 0;
    D_80369E5C = 0;
    D_80369E61 = 0;
    D_8028DCC0 = 0;
    pfs = PFS;
    D_80369E62 = 0;
    D_80369E54 = osPfsFindFile(pfs, COMPANY, GAME, name, ext, &D_80369E58);
    if (D_80369E54 == 0) {
      if (op == OP_LOAD) {
#if !TGR_REGION_EUR && !TGR_REGION_JPN
        top = 0xC5;
        if (fromMenu == 1) {
          top = 0xDB;
        }
        BrBevelPanel(0xD4, top, 0xD8, 0x56, 3, 0, 0, 0x80, 0x80, 0x80);
#elif TGR_REGION_EUR
        if (fromMenu == 1) {    /* the panel moved to state 7; the test stays */
        }
#endif
        BrTextAlignLeft();
        BrTextHighlightOff();
        BrTextSetFont(13);
        BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
#if !TGR_REGION_EUR && !TGR_REGION_JPN
        BrTextPrint("LOADING DECALS...", 114, (top + 30) >> 1);
        BrBevelPanel(0xE6, top + 0x34, 0xB4, 0x14, 1, 1, 1, 0x80, 0x80, 0x80);
#endif
        D_80369E60 = 7;
      } else {
        D_80369E60 = 5;
      }
    } else if (D_80369E54 == 5) {
      if (op == OP_LOAD) {
        D_80369E60 = 12;
      } else if (osPfsFreeBlocks(pfs, &freeBytes) == 0) {
        if (freeBytes < 0x3E00) {
          D_80369E54 = 7;
          D_80369E60 = 12;
        } else {
#if !TGR_REGION_EUR && !TGR_REGION_JPN
          BrBevelPanel(0xD4, 0xC5, 0xD8, 0x56, 3, 0, 0, 0x80, 0x80, 0x80);
#endif
          BrTextAlignLeft();
          BrTextHighlightOff();
          BrTextSetFont(13);
          BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
#if !TGR_REGION_EUR && !TGR_REGION_JPN
          BrTextPrint("SAVING DECALS...", 115, 113);
          BrBevelPanel(0xE6, 0xF9, 0xB4, 0x14, 1, 1, 1, 0x80, 0x80, 0x80);
#endif
          D_8028DCC0 = 1;
          D_80369E60 = 7;
          D_80369E62 = 0;
        }
      } else {
        D_80369E54 = 4;
        D_80369E60 = 12;
      }
    } else if (D_80369E54 == 3) {
      D_80369E54 = osPfsChecker(pfs);
      if (D_80369E54 == 0) {
        D_80369E60 = 6;
      } else {
        D_80369E60 = 12;
      }
    } else if (D_80369E54 == 2) {
      D_80369E60 = 3;
      D_80369E54 = 0;
    } else {
      D_80369E60 = 12;
    }
    break;
  case 5:                       /* overwrite? */
#if TGR_REGION_JPN
    BrBevelPanel(0xD6, 0xBB, 0xD4, 0x6A, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextAlignLeft();
    BrTextHighlightOff();
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
    BrTextSetFont(14);
    BrTextPrint("%j\xDA\xE4" "AB6R1%%", 121, 113);
    y = 0x118 - D_8028D0B0.w;
#elif TGR_REGION_EUR
    BrBevelPanel(0xD6, 0xBB, 0xD4, 0x6A, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextAlignLeft();
    BrTextHighlightOff();
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
    BrTextSetFont(13);
    BrTextPrint("OK TO OVERWRITE", 114, 108);
    BrTextPrint("SAVED DECALS?", 114, 122);
    y = 0x11D - D_8028D0B0.w;
#else
    y = 0x11D - D_8028D0B0.w;
    BrBevelPanel(0xD6, 0xBB, 0xD4, 0x6A, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextAlignLeft();
    BrTextHighlightOff();
    BrTextSetFont(13);
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
    BrTextPrint("OK TO OVERWRITE", 114, 108);
    BrTextPrint("SAVED DECALS?", 114, 122);
#endif
    BrTextSetFont(10);
    BrTextPrint("%wwOK", (D_8028D0B0.w + 0xEEU) >> 1, (y + 18) >> 1);
    BrTextPrint("%wwCANCEL", (D_8028D0E0.h + 0x13AU) >> 1, (y + 18) >> 1);
    BrImageDrawAt(&D_8028D0B0, 0xE8, y);
    BrImageDrawAt(&D_8028D0E0, 0x134, y);
    if (PAD->pressed & 0x10) {
      BrPadConsume(PAD, 0x10);
      D_80369E60 = 7;
    } else if (PAD->pressed & 0x20) {
      BrPadConsume(PAD, 0x20);
      D_80369E60 = 12;
    }
    break;
  case 7:                       /* one decal a frame */
#if TGR_REGION_JPN
    top = 0xC5;
    ty = 0x71;
    if (fromMenu == 1) {
      top = 0xDB;
      ty = 0x7C;
    }
    if (op == OP_LOAD) {
      BrBevelPanel(0xC4, top, 0xF8, 0x56, 3, 0, 0, 0x80, 0x80, 0x80);
    } else {
      BrBevelPanel(0xD4, top, 0xD8, 0x56, 3, 0, 0, 0x80, 0x80, 0x80);
    }
    BrTextHighlightOff();
    BrTextAlignCentre();
    BrTextSetFont(14);
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
    if (op == OP_LOAD) {
      BrTextPrint("%j\xBB\x81(\xA8\xC8(\xCB\xE6", D_8028AAB0 / 2, ty);
    } else {
      BrTextPrint("%j\xBB\x81(\xA8\xB2(\xAC\xE6", D_8028AAB0 / 2, ty);
    }
#else
    top = 0xC5;
    ty = 0x71;
    if (fromMenu == 1) {
      ty = 0x7C; if (ty); top = 0xDB;
    }
    BrBevelPanel(0xD4, top, 0xD8, 0x56, 3, 0, 0, 0x80, 0x80, 0x80);
#if TGR_REGION_EUR
    BrTextHighlightOff();
    BrTextAlignLeft();
#else
    BrTextAlignLeft();
    BrTextHighlightOff();
#endif
    BrTextSetFont(13);
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
    tx = 114;
    if (op == OP_LOAD) {
      BrTextPrint("LOADING DECALS...", tx, ty);
    } else {
      tx++;
      BrTextPrint("SAVING DECALS...", tx, ty);
    }
#endif
    BrBevelPanel(0xE6, top + 0x34, 0xB4, 0x14, 1, 1, 1, 0x80, 0x80, 0x80);
    if (D_80369E62++ > 1) {
      if (D_8028DCC0 != 0) {
        pfs = PFS;
        D_8028DCC0 = 0;
        D_80369E54 = osPfsAllocateFile(pfs, COMPANY, GAME, name, ext, 0x3E00, &D_80369E58);
        switch (D_80369E54) {
        case 3:
          D_80369E54 = osPfsChecker(pfs);
          if (D_80369E54 == 0) {
            D_80369E60 = 3;
            return 0;
          }
          D_80369E60 = 2;
          return D_80369E54;
        case 5:
        case 9:
          D_80369E54 = 3;
          break;
        case 2:
          D_80369E60 = 3;
          break;
        case 0:
          break;
        }
        if (D_80369E54 != 0) {
          if (D_80369E54 == 2) {
            D_80369E54 = 0;
            D_80369E60 = 3;
            *done = 0;
            return 0;
          }
          D_80369E60 = 12;
          break;
        }
      }
      if (D_80369E61 < 10) {
        n = D_80369E61;
        w = m->parts[m->decal[D_80369E61]].w;
        h = m->parts[m->decal[D_80369E61]].h;
        pfs = PFS;
        if (n == 0 || n == 1) {
          h++;
        }
        D_80369E5C = (int)(w * h) >> 1;
        if (op == OP_LOAD) {
          D_80369E54 = osPfsReadWriteFile(pfs, D_80369E58, 0, D_8028DCC4, D_80369E5C, D_8028DB80);
        } else {
          D_80369E54 = osPfsReadWriteFile(pfs, D_80369E58, 1, D_8028DCC4, D_80369E5C, m->parts[m->decal[D_80369E61]].tex);
        }
        switch (D_80369E54) {
        case 0:
          if (op == OP_LOAD) {
#if !TGR_REGION_EUR && !TGR_REGION_JPN
            osSyncPrintf("Loading decal %d...\n", D_80369E61);
#endif
            tex = m->parts[m->decal[D_80369E61]].tex;
            mask = m->mask[D_80369E61];
            if (mask == 0) {
              memcpy(tex, D_8028DB80, D_80369E5C);
            } else {
              /* keep the pixels the mask marks, take the rest from the pak */
              for (row = 0; row < h; row++) {
                stride = (int)(w + 7) >> 3;
                half = (int)w >> 1;
                flip = (row & 1) << 3;
                for (col = 0; col < w; col++) {
                  x2 = ((int)col >> 3) + row * stride;
                  if ((mask[x2] & (1 << ((col ^ 7) & 7))) == 0) {
                    k = ((int)(flip ^ col) >> 1) + row * half;
                    tex[k] = D_8028DB80[k];
                  }
                }
              }
            }
#if TGR_REGION_EUR
            {
              /* dump the decal's bytes on the debug output, one a line */
              int i;
              int size;

              size = (m->parts[m->decal[D_80369E61]].w * m->parts[m->decal[D_80369E61]].h) >> 1;
              if (D_80369E61 == 0) {
                osSyncPrintf("# Format SB BitSize 8 Rows 1 Columns 1\n");
              }
              for (i = 0; i < size; i++) {
                osSyncPrintf("%02x\n", tex[i]);
              }
            }
#endif
          } else {
            osSyncPrintf("Saving decal %d...\n", D_80369E61);
          }
          D_8028DCC4 += D_80369E5C;
          break;
        case 5:
          D_80369E54 = 99999;
          break;
        case 3:
          D_80369E54 = osPfsChecker(pfs);
          if (D_80369E54 == 1 || D_80369E54 == 4 || D_80369E54 == 3) {
            D_80369E61 = 0;
            D_8028DCC4 = 0;
            D_80369E5C = 0;
            D_80369E60 = 12;
          } else {
            D_80369E61--;
            D_8028DCC4 -= D_80369E5C;
            D_80369E54 = 0;
          }
          break;
        }
        if (D_80369E54 != 0) {
          if (D_80369E54 == 2) {
            D_80369E54 = 0;
            D_80369E60 = 9;
            D_80369E51 = 1;
            *done = 0;
            return 0;
          }
          D_80369E60 = 12;
        } else {
          BrFillRect(0xE7, top + 0x35, D_8028DCC4 * 0xB2 / 0x3D00, 0x12, 0, 0, 0xC0);
          D_80369E61++;
        }
      } else {
        D_80369E61 = 0;
        BrFillRect(0xE7, top + 0x35, 0xB2, 0x12, 0, 0, 0xC0);
        D_80369E60 = 8;
      }
    }
    break;
  case 8:                       /* the palette */
    if (op == OP_LOAD) {
#if !TGR_REGION_EUR && !TGR_REGION_JPN
      osSyncPrintf("Loading color palette...\n");
#endif
      pfs = PFS;
      D_80369E54 = osPfsReadWriteFile(pfs, D_80369E58, 0, D_8028DCC4, 0x20,
                                      (unsigned char *)m->parts[m->decal[2]].tlut);
      if (D_80369E54 == 0) {
        pal = m->parts[m->decal[2]].tlut;
#if TGR_REGION_EUR
        {
          /* dump the palette on the debug output, a byte a line */
          int i;

          for (i = 0; i < 16; i++) {
            osSyncPrintf("%02x\n%02x\n", pal[i] >> 8, pal[i] & 0xFF);
          }
        }
#endif
        m->parts[m->paint[0]].tlut[0] = pal[0];
        m->parts[m->paint[0]].tlut[1] = pal[1];
        m->parts[m->paint[1]].tlut[0] = pal[0];
        m->parts[m->paint[1]].tlut[1] = pal[1];
        if (fromMenu == 0) {
          BrPaintPaletteLoad();
          BrPaintDecalCommit();
        }
      }
    } else {
#if !TGR_REGION_JPN
      osSyncPrintf("Saving color palette...\n");
#endif
      pfs = PFS;
      D_80369E54 = osPfsReadWriteFile(pfs, D_80369E58, 1, D_8028DCC4, 0x20,
                                      (unsigned char *)m->parts[m->decal[2]].tlut);
    }
    switch (D_80369E54) {
    case 0:
#if !TGR_REGION_JPN
      if (op == OP_LOAD) {
        osSyncPrintf("Color palette loaded successfully!\n\n");
      } else {
        osSyncPrintf("Color palette saved successfully!\n\n");
      }
#endif
      D_80369E60 = 11;
      if (fromMenu == 0) {
        D_8028DBB4 = 0;
      }
      break;
    case 3:
      D_80369E54 = osPfsChecker(pfs);
      if (D_80369E54 == 1 || D_80369E54 == 4 || D_80369E54 == 3) {
        D_80369E60 = 12;
      } else {
        D_8028DCC4 -= D_80369E5C;
        D_80369E54 = 0;
        D_80369E60 = 8;
      }
      break;
    }
    if (D_80369E54 != 0) {
#if !TGR_REGION_JPN
      osSyncPrintf("err_code = %d\n\n", D_80369E54);
#endif
      if (D_80369E54 == 2) {
        D_80369E60 = 9;
        D_80369E51 = 1;
        *done = 0;
        D_80369E54 = 0;
        return 0;
      }
      D_80369E60 = 12;
      *done = 0;
    }
    break;
  case 9:                       /* a different pak */
    top = 0xAB;
    if (fromMenu == 1) {
      top = 0xD3;
    }
    BrTextHighlightOff();
    BrTextAlignLeft();
#if TGR_REGION_JPN
    BrTextSetFont(14);
    BrBevelPanel(0x99, top, 0x14E, 0x89, 3, 0, 0, 0x80, 0x80, 0x80);
#else
    BrTextSetFont(12);
    BrBevelPanel(0xAF, top, 0x122, 0x89, 3, 0, 0, 0x80, 0x80, 0x80);
#endif
    if (fromMenu == 1) {
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
    } else {
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
    }
#if TGR_REGION_JPN
    BrTextPrint("%j038B@\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1" "9", 84, (top + 32) >> 1);
    BrTextPrint("%j2Bq6hc@6R&", 84, ((top + 32) >> 1) + 13);
    BrTextPrint("%jqt\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1w'", 84, ((top + 32) >> 1) + 32);
    BrTextPrint("%j\xDE@6R&", 84, ((top + 32) >> 1) + 45);
#else
    BrTextPrint("A NEW CONTROLLER PAK", 95, (top + 32) >> 1);
    BrTextPrint("WAS INSERTED.", 95, ((top + 32) >> 1) + 13);
    BrTextPrint("THIS CONTROLLER PAK", 95, ((top + 32) >> 1) + 32);
    BrTextPrint("WILL BE USED.", 95, ((top + 32) >> 1) + 45);
#endif
    if (PAD->pressed & 0x8030) {
      BrPadConsume(PAD, 0x8030);
      if (D_80369E51 != 0) {
        D_80369E51 = 0;
        D_80369E60 = 3;
      } else {
        D_80369E60 = 6;
      }
    }
    break;
  case 10:                      /* a Rumble Pak is in the way, or an error */
    BrTextHighlightOff();
    BrTextAlignLeft();
#if TGR_REGION_JPN
    BrTextSetFont(14);
#else
    BrTextSetFont(12);
#endif
    if (fromMenu == 1) {
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
    } else {
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
    }
    if (D_80369E54 == 9999) {
#if TGR_REGION_JPN
      top = 0xAD;
      if (fromMenu == 1) {
        top = 0xD5;
      }
      y = top - D_8028D0B0.w + 0x7E;
      BrBevelPanel(200, top, 0xF0, 0x86, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("%j\xD2\xD3\x8D\x8F\xA1s", 108, (top + 32) >> 1);
      BrTextPrint("%j\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1w", 108, ((top + 32) >> 1) + 13);
      BrTextPrint("%j\xD4\xD5" "Bc\xDB" "2@&", 108, ((top + 32) >> 1) + 26);
#else
      top = 0xA0;
      if (fromMenu == 1) {
        top = 200;
      }
      y = top - D_8028D0B0.w + 0x98;
      BrBevelPanel(0x7A, top, 0x18C, 0xA0, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("PLEASE REMOVE THE RUMBLE PAK", 69, (top + 32) >> 1);
      BrTextPrint("AND INSERT THE CONTROLLER PAK", 69, ((top + 32) >> 1) + 13);
      BrTextPrint("INTO CONTROLLER.  PRESS THE A", 69, ((top + 32) >> 1) + 26);
      BrTextPrint("BUTTON WHEN READY.", 69, ((top + 32) >> 1) + 39);
#endif
      BrTextSetFont(10);
      BrTextPrint("%wwOK", (D_8028D0B0.w + 0xDEU) >> 1, (y + 18) >> 1);
      BrTextPrint("%wwCANCEL", (D_8028D0E0.w + 0x14AU) >> 1, (y + 18) >> 1);
      BrImageDrawAt(&D_8028D0B0, 0xD8, y);
      BrImageDrawAt(&D_8028D0E0, 0x144, y);
    }
    if (D_80369E54 != 9999) {
      if (PAD->pressed & 0x8030) {
        BrPadConsume(PAD, 0x8030);
        D_80369E60 = 12;
      }
    } else if (PAD->pressed & 0x10) {
      BrPadConsume(PAD, 0x10);
      D_80369E54 = 0;
      D_80369E60 = 3;
    } else if (PAD->pressed & 0x20) {
      BrPadConsume(PAD, 0x20);
      D_80369E54 = 0;
      D_80369E60 = 12;
      if (fromMenu == 1) {
        D_80369E54 = 999;
      }
    }
    break;
  case 11:                      /* put the Rumble Pak back */
    D_80369E61 = 0;
    D_8028DCC4 = 0;
    D_80369E5C = 0;
    D_80369E62 = 0;
    if (fromMenu == 0) {
      D_80369E60 = 12;
    } else if (D_80369E50 != 0) {
#if TGR_REGION_JPN
      BrBevelPanel(0xAA, 0xE6, 300, 100, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextHighlightOff();
      BrTextAlignLeft();
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
      BrTextSetFont(14);
      BrTextPrint("%j\xD2\xD3\x8D\x8F\xA1w\xDEPsA5'", 93, 131);
      BrTextPrint("%j\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1s", 93, 144);
      BrTextPrint("%j\xD2\xD3\x8D\x8F\xA1w\xD4\xD5" "Bc\xDB" "2@&", 93, 157);
#else
      BrBevelPanel(0x73, 0xCC, 0x19A, 0x98, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextHighlightOff();
      BrTextAlignLeft();
#if TGR_REGION_EUR
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
      BrTextSetFont(12);
#else
      BrTextSetFont(12);
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
#endif
      BrTextPrint("IF THE RUMBLE PAK IS TO BE USED,", 65, 118);
      BrTextPrint("REMOVE THE CONTROLLER PAK AND", 65, 131);
      BrTextPrint("INSERT THE RUMBLE PAK INTO THE", 65, 144);
      BrTextPrint("CONTROLLER.  PRESS THE A BUTTON", 65, 157);
      BrTextPrint("TO CONTINUE.", 65, 170);
#endif
      if (PAD->pressed & 0x8010) {
        BrPadConsume(PAD, 0x8010);
        D_80369E50 = 0;
        D_80369E54 = 0;
        if (osMotorInit(D_80272D48, &D_8031A3F8[port], port) == 0) {
          D_8031B1E8[port] = 1;
        }
        D_80369E60 = 12;
      }
    } else {
      D_80369E60 = 12;
    }
    break;
  case 12:                      /* done */
    D_8028DCBC = 0;
    BrSfxFadeTo(1.0f, 0.2f);
    BrMusicFadeTo(1.0f, 0.2f);
    D_802A4BE8 = 1;
    if (D_80369E54 == 0) {
      *done = 1;
      if (fromMenu == 0) {
        D_8028DB60 = D_8028DB64;
      }
      return 0;
    }
    if (fromMenu == 0 && (PAD->pressed & 0x8030) != 0) {
      BrPadConsume(PAD, 0x8030);
      D_8028DB60 = D_8028DB64;
    }
    *done = 0;
    return D_80369E54;
  }
  return 0;
}

BrGlyph *D_80369E68[16];                /* the typed text's keys */
char D_80369EA8[16];                    /* the typed text */


/* WHAT IT DOES: The paint shop's text keyboard: draw it with the text box
 * under it, and while the cursor is over the box, move the key cursor to
 * the key it touches (flashing it for 3 frames when A or Z picks it); a
 * picked key appends its glyph to the text (while the kerned width fits
 * the decal, up to 16), key 47 deletes the last one and key 49 closes the
 * keyboard (starting the text stamp when there is text).  The text is
 * printed centred in the box, in the chosen paint colour.
 * Source facts: the text length is read and written through the global
 * D_8028DBA8 itself, with no local copy.  Deleting is D_8028DBA8--, so the
 * byte store keeps the unmasked difference and the reloads use the masked
 * one (the else arm's reload of the stored 0 is the andi of $zero); adding
 * ends with D_80369EA8[++D_8028DBA8] = 0, whose incremented value is a
 * temp of its own beside the old length. */
/* @implements 0x8024AC70 tgr BrPaintKeyboard */
void BrPaintKeyboard()
{
  int spare;                    /* declared, never used: the frame holds it */
  int x;
  int y;
  int i;


  int kern;
  BrPadRec *pad;

  D_8028DBD8 = 0;
  D_8028DBC8 = 0;
  BrImageDrawTinted(&D_8028D260, 0xe0, 0xe0, 0xe0);
  if (D_8028D290.kind != 0) {
    D_8028D290.kind = 0;
    if (D_8028DBB0 > 0) {
      D_8028DBB0--;
    }
    if (D_8028DBB0 == 0) {
      BrImageDrawTinted((BrImage *)&D_8028D290, 0x20, 200, 0xff);
    } else {
      BrImageDrawTinted((BrImage *)&D_8028D290, 0xff, 0xeb, 0);
    }
  }
  BR_IMAGE_DRAW(&D_8028D230);
  x = D_8028D230.x;
  y = D_8028D230.y + D_8028D230.h + 32;
  BrImageDrawRect(&D_8028D260, x, y, 200, 48, 0xe0, 0xe0, 0xe0);
  BrFillFrame(x, y, 200, 48, 2, 0xff, 0xff, 0xff);
  if (BrPaintCursorInBox((int *)&D_8028D260) != 0) {
    for (i = 0; i < 50; i++) {
      if (BrPaintCursorInBrush(&D_8028D540[i].x0) != 0) {
        D_8028DBA4 = i;
        D_8028D290.kind = 1;
        D_8028D290.x = D_8028D540[i].x0;
        D_8028D290.y = D_8028D540[i].x4;
      }
      if (BrPaintCursorInBox((int *)&D_8028D290) != 0 && ((pad = &PADS[D_8028DBBC])->pressed & 0x8010)) {
        BrPadConsume((unsigned int *)pad, 0x8010);
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
          if (D_80369E68[D_8028DBA8 - 1] != 0) {
            D_8028DBA8--;
            if (D_8028DBA8 == 0) {
              D_8028DBAC -= D_80369E68[D_8028DBA8]->w;
            } else {
              kern = D_80369E68[D_8028DBA8 - 1]->kernR < D_80369E68[D_8028DBA8]->kernL ? D_80369E68[D_8028DBA8 - 1]->kernR : D_80369E68[D_8028DBA8]->kernL;
              D_8028DBAC = D_8028DBAC - D_80369E68[D_8028DBA8]->w + kern - 2;
            }
          } else {
            D_8028DBA8 = 0;
            D_8028DBAC = 0;
          }
          D_80369EA8[D_8028DBA8] = 0;
          D_80369E68[D_8028DBA8] = 0;
        } else {
          if (D_8028D540[D_8028DBA4].w + D_8028DBAC + 2 < D_8028DB88) {
            D_80369EA8[D_8028DBA8] = D_8028D540[D_8028DBA4].c;
            D_80369E68[D_8028DBA8] = &D_8028D540[D_8028DBA4];
            if (D_8028DBA8 == 0) {
              D_8028DBAC = D_8028D540[D_8028DBA4].w + D_8028DBAC;
            } else {
              kern = D_80369E68[D_8028DBA8 - 1]->kernR < D_80369E68[D_8028DBA8]->kernL ? D_80369E68[D_8028DBA8 - 1]->kernR : D_80369E68[D_8028DBA8]->kernL;
              D_8028DBAC = D_8028D540[D_8028DBA4].w + D_8028DBAC - kern + 2;
            }
            D_80369EA8[++D_8028DBA8] = 0;
            if (D_8028DBA8 > 16) {
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
void BrPaintColourMix()
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
  BrBevelPanel(0xbb, 0xc2, 0x3c, 0x3c, 1, 0, 1, ((BrMixSwatch *)D_80369B98)[D_8028DB58].c[0], ((BrMixSwatch *)D_80369B98)[D_8028DB58].c[1],
               ((BrMixSwatch *)D_80369B98)[D_8028DB58].c[2]);
  ch = D_8028DCE0;
  for (i = 0, y = 0xc2; (int)i < 3; i++, y += 0x16) {
    if (i == ch) {
      BrBevelPanel(0x11d, y, 0x80, 0x10, 1, 0, 1, 0xb4, 0xb4, 0xb4);
    } else {
      BrBevelPanel(0x11d, y, 0x80, 0x10, 1, 1, 1, 0x80, 0x80, 0x80);
    }
    if ((v = ((BrMixSwatch *)D_80369B98)[D_8028DB58].c[i]) > 0) {
      if (D_8028DB58 == 1) {
        BrFillRect(0x11d, y, ((BrMixSwatch *)D_80369B98)[D_8028DB58].c[i], 0x10, bar[i][0], bar[i][1], bar[i][2]);
        if (i == ch) {
          BrBevelPanel(0x11d, y, ((BrMixSwatch *)D_80369B98)[D_8028DB58].c[i], 0x10, 1, 0, 0, bar[i][0], bar[i][1], bar[i][2]);
        }
      } else {
        BrFillRect(0x11d, y, ((BrMixSwatch *)D_80369B98)[D_8028DB58].c[i] >> 1, 0x10, bar[i][0], bar[i][1], bar[i][2]);
        if (i == ch) {
          BrBevelPanel(0x11d, y, ((BrMixSwatch *)D_80369B98)[D_8028DB58].c[i] >> 1, 0x10, 1, 0, 0, bar[i][0], bar[i][1],
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
    sprintf(buf, "%d", ((BrMixSwatch *)D_80369B98)[D_8028DB58].c[i]);
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
    v = ((BrMixSwatch *)D_80369B98)[D_8028DB58].c[D_8028DCE0] + step;
    if (v < 0) {
      ((BrMixSwatch *)D_80369B98)[D_8028DB58].c[D_8028DCE0] = 0;
    } else if (v >= 0x80) {
      ((BrMixSwatch *)D_80369B98)[D_8028DB58].c[D_8028DCE0] = 0x7f;
    } else {
      ((BrMixSwatch *)D_80369B98)[D_8028DB58].c[D_8028DCE0] = v;
    }
  } else {
    v = ((BrMixSwatch *)D_80369B98)[D_8028DB58].c[D_8028DCE0] + step;
    if (v < 0) {
      ((BrMixSwatch *)D_80369B98)[D_8028DB58].c[D_8028DCE0] = 0;
    } else if (v >= 0x100) {
      ((BrMixSwatch *)D_80369B98)[D_8028DB58].c[D_8028DCE0] = 0xff;
    } else {
      ((BrMixSwatch *)D_80369B98)[D_8028DB58].c[D_8028DCE0] = v;
    }
  }
  if (step != 0) {
    D_8028DB90[D_8028DB58] = ((((BrMixSwatch *)D_80369B98)[D_8028DB58].c[0] & 0xf8) << 8) | ((((BrMixSwatch *)D_80369B98)[D_8028DB58].c[1] & 0xf8) << 3) | ((((BrMixSwatch *)D_80369B98)[D_8028DB58].c[2] & 0xf8) >> 2) | (D_8028DB90[D_8028DB58] & 1);
    if (D_8028DB58 == 0) {
      D_8028DB90[1] = (((((BrMixSwatch *)D_80369B98)[0].c[0] >> 1) & 0xf8) << 8) | (((((BrMixSwatch *)D_80369B98)[0].c[1] >> 1) & 0xf8) << 3) | (((((BrMixSwatch *)D_80369B98)[0].c[2] >> 1) & 0xf8) >> 2) | (D_8028DB90[1] & 1);
      for (i = 0; i < 3; i++) {
        if (((BrMixSwatch *)D_80369B98)[0].c[i] == 0xff) {
          ((BrMixSwatch *)D_80369B98)[1].c[i] = 0x7f;
        } else {
          ((BrMixSwatch *)D_80369B98)[1].c[i] = ((BrMixSwatch *)D_80369B98)[0].c[i] >> 1;
        }
      }
    } else if (D_8028DB58 == 1) {
      D_8028DB90[0] = (((((BrMixSwatch *)D_80369B98)[1].c[0] << 1) & 0xf8) << 8) | (((((BrMixSwatch *)D_80369B98)[1].c[1] << 1) & 0xf8) << 3) | (((((BrMixSwatch *)D_80369B98)[1].c[2] << 1) & 0xf8) >> 2) | (D_8028DB90[0] & 1);
      for (i = 0; i < 3; i++) {
        if (((BrMixSwatch *)D_80369B98)[1].c[i] == 0x7f) {
          ((BrMixSwatch *)D_80369B98)[0].c[i] = 0xff;
        } else {
          ((BrMixSwatch *)D_80369B98)[0].c[i] = ((BrMixSwatch *)D_80369B98)[1].c[i] << 1;
        }
      }
    }
    if (D_8028DB58 == 0 || D_8028DB58 == 1) {
      ((BrMixModel *)D_8028AB08)->parts[((BrMixModel *)D_8028AB08)->bodyPart].tlut[0] = D_8028DB90[0];
      ((BrMixModel *)D_8028AB08)->parts[((BrMixModel *)D_8028AB08)->bodyPart].tlut[1] = D_8028DB90[1];
      ((BrMixModel *)D_8028AB08)->parts[((BrMixModel *)D_8028AB08)->shadePart].tlut[0] = D_8028DB90[0];
      ((BrMixModel *)D_8028AB08)->parts[((BrMixModel *)D_8028AB08)->shadePart].tlut[0] = D_8028DB90[0];
    }
  }
  if (D_8036A8E0[D_8028DBBC].pressed & 0x10) {
    BrPadConsume(&D_8036A8E0[D_8028DBBC].pressed, 0x10);
    D_8028DCE0 = 0;
    D_8028DBD4 = 0;
    D_8028DBB4++;
  } else if (D_8036A8E0[D_8028DBBC].pressed & 0x20) {
    BrPadConsume(&D_8036A8E0[D_8028DBBC].pressed, 0x20);
    D_8028DB90[D_8028DB58] = ((D_80369DA8[0] & 0xf8) << 8) | ((D_80369DA8[1] & 0xf8) << 3) | ((D_80369DA8[2] & 0xf8) >> 2) | (D_8028DB90[D_8028DB58] & 1);
    for (i = 0; i < 3; i++) {
      ((BrMixSwatch *)D_80369B98)[D_8028DB58].c[i] = D_80369DA8[i];
    }
    if (D_8028DB58 == 0) {
      D_8028DB90[1] = (((D_80369DA8[0] >> 1) & 0xf8) << 8) | (((D_80369DA8[1] >> 1) & 0xf8) << 3) | (((D_80369DA8[2] >> 1) & 0xf8) >> 2) | (D_8028DB90[1] & 1);
      for (i = 0; i < 3; i++) {
        if (((BrMixSwatch *)D_80369B98)[0].c[i] == 0xff) {
          ((BrMixSwatch *)D_80369B98)[1].c[i] = 0x7f;
        } else {
          ((BrMixSwatch *)D_80369B98)[1].c[i] = D_80369DA8[i] >> 1;
        }
      }
    } else if (D_8028DB58 == 1) {
      D_8028DB90[0] = (((D_80369DA8[0] << 1) & 0xf8) << 8) | (((D_80369DA8[1] << 1) & 0xf8) << 3) | (((D_80369DA8[2] << 1) & 0xf8) >> 2) | (D_8028DB90[0] & 1);
      for (i = 0; i < 3; i++) {
        if (((BrMixSwatch *)D_80369B98)[1].c[i] == 0x7f) {
          ((BrMixSwatch *)D_80369B98)[0].c[i] = 0xff;
        } else {
          ((BrMixSwatch *)D_80369B98)[0].c[i] = D_80369DA8[i] << 1;
        }
      }
    }
    if (D_8028DB58 == 0 || D_8028DB58 == 1) {
      ((BrMixModel *)D_8028AB08)->parts[((BrMixModel *)D_8028AB08)->bodyPart].tlut[0] = D_8028DB90[0];
      ((BrMixModel *)D_8028AB08)->parts[((BrMixModel *)D_8028AB08)->bodyPart].tlut[1] = D_8028DB90[1];
      ((BrMixModel *)D_8028AB08)->parts[((BrMixModel *)D_8028AB08)->shadePart].tlut[0] = D_8028DB90[0];
      ((BrMixModel *)D_8028AB08)->parts[((BrMixModel *)D_8028AB08)->shadePart].tlut[0] = D_8028DB90[0];
    }
    D_8028DCE0 = 0;
    D_8028DBD4 = 0;
  }
}


unsigned char D_8028DCE0 = 0;           /* the channel being mixed */

/* WHAT IT DOES: Move the paint shop cursor from the stick once it is pushed
 * past the dead zone, unless the cursor is locked.  The x arms re-point pad
 * at the controller; the y arms read the controller directly, so pad's one
 * web stays the top-level pointer (a0) and the cursor's address takes v1. */
/* @implements 0x8024BE78 tgr BrPaintStickMove */
void BrPaintStickMove()
{
  BrPadRec *pad;
  float a;

  if (D_8028DBE8 == 0) {
    pad = &PADS[D_8028DBBC];
    a = pad->axis[0] < 0.0 ? -pad->axis[0] : pad->axis[0];
    if (a >= 0.05f) {
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
    if (a >= 0.05f) {
      if (BrPaintCursorInRect((int *)&D_8028DB94) != 0 && D_8028DBC4 == 0) {
        D_8028D110.y -= (int)(PADS[D_8028DBBC].axis[1] * 6.0f);
      } else {
        D_8028D110.y -= (int)(PADS[D_8028DBBC].axis[1] * 12.0f);
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

/* WHAT IT DOES: The paint shop's click handling, A or Z on the cursor.
 * A decal slot button picks the decal to paint (setting up its texture,
 * mask, size and place on screen, and turning the view to it); a palette
 * swatch picks the colour, and a second click within 350 ms opens the
 * mixer on it; a tool button picks the tool (a second quick click on the
 * brush, line, rectangle or oval tool opens its options popup), and the
 * others undo, clear, mirror or pick a brush size.  In the paint area the
 * tool acts: the brush paints a dot or a square, the fill floods, the line,
 * rectangle and oval take a first click as the anchor and a second to
 * draw, the text tool opens the keyboard and then stamps the text; between
 * the clicks a dashed outline follows the cursor.
 * The anchors are function statics.  Coordinates go through the locals x
 * and y and are adjusted in place, the paint area is placed by halving the
 * free width and height first, the brush index is held in i and the brush
 * square's half size in r, and the circle radius is a statement of its
 * own: each of these decides a register or a stack slot.  Three groups of
 * statements share one line because as1 breaks scheduling ties on the line
 * number. */
/* @implements 0x8024C184 tgr BrPaintClick */
void BrPaintClick()
{
  static int D_80369B80;               /* the drag anchor */
  static int D_80369B84;
  static unsigned int D_80369B88;      /* the last click, in milliseconds */
  static int D_80369B8C;               /* the anchor has been set */
  static int D_80369B90;
  int j;
  int i;
  int w;
  int h;
  int r;
  unsigned int t;
  int u0;                       /* unused: the frame has it */
  int x;
  int y;
  int u1[3];                    /* unused: the frame has them */
  char clicked;

  if (D_80369B8C != 0) { } else { D_80369B8C = 1; D_80369B80 = D_8028D110.x; }
  if (D_80369B90 == 0) {
    D_80369B90 = 1;
    D_80369B84 = D_8028D110.y;
  }
  if (D_8028DBE8 != 0) {
    return;
  }
  if (D_8028DBC4 == 0 && D_8028DBD0 == 0 && (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 0x8010)) {
    for (i = 0; i < 10; i++) {
      if (BrPaintCursorInRect(&D_80369DB0[i])) {
        BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 0x8010);
        D_8028DB6C = D_8028DB68;
        D_8028DB08 = D_8028DB0C[i];
        D_8028DB68 = i;
        if (D_8028DB68 != D_8028DB6C) {
          D_8028DBD0 = 1;
          D_8028DBB0 = 0;
          D_8028DB78 = ((BrDecalModel *)D_8028AB08)->parts[((BrDecalModel *)D_8028AB08)->decalPart[i]].tex;
          D_8028DB7C = ((BrDecalModel *)D_8028AB08)->masks[D_8028DB68];
          D_8028DB88 = ((BrDecalModel *)D_8028AB08)->parts[((BrDecalModel *)D_8028AB08)->decalPart[i]].w;
          D_8028DB8C = ((BrDecalModel *)D_8028AB08)->parts[((BrDecalModel *)D_8028AB08)->decalPart[i]].h;
#if !TGR_REGION_JPN
          osSyncPrintf("decal_width = %d\n", D_8028DB88);
          osSyncPrintf("decal_height = %d\n", D_8028DB8C);
#endif
          w = (D_8028D470.w - D_8028DB88 * 4) >> 1;
          h = (D_8028D470.h - D_8028DB8C * 4) >> 1;
          D_8028DB94.x = D_8028D470.x + w;
          D_8028DB94.y = D_8028D470.y + h;
          D_8028DB94.w = D_8028DB88 * 4;
          D_8028DB94.h = D_8028DB8C * 4;
        }
        if (D_8028DB68 == 4 || D_8028DB68 == 5 || D_8028DB68 == 8 || D_8028DB68 == 9) {
          D_8028DBCC = 0;
        } else {
          D_8028DBCC = 1;
        }
        return;
      }
    }
  }
  if ((BrPaintCursorInRect(&D_8028D480) || BrPaintCursorInRect(&D_8028D490)) && D_8028DBC8 != 0 &&
      (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 0x8010)) {
    for (j = 0; j < 16; j++) {
      if (BrPaintCursorInRect2(&((BrClickSwatch *)D_80369B98)[j])) {
        BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 0x8010);
        D_8028DB54 = D_8028DB58;
        D_8028DB58 = j;
        if (D_8028DB54 != j) {
          D_80369B88 = BR_MSEC();
          return;
        }
        t = BR_MSEC();
        if (t - D_80369B88 < 350) {
          for (j = 0; j < 3; j++) {
            D_80369DA8[j] = ((BrClickSwatch *)D_80369B98)[D_8028DB58].c[j];
          }
          D_8028DBD4 = 1;
          return;
        }
        D_80369B88 = t;
        return;
      }
    }
  }
  if (D_8028DBC4 == 0) {
    for (i = 0; i < 13; i++) {
      if (BrPaintCursorInRect(&D_80369CD8[i]) && (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 0x8010)) {
        BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 0x8010);
        D_8028DBC0 = 0;
        if (i != 0) {
          D_8028DB64 = D_8028DB60;
          D_8028DB60 = i;
          if (D_8028DB60 != D_8028DB64) {
            if (D_8028DB60 == 5 || D_8028DB60 == 6 || D_8028DB60 == 1 || D_8028DB60 == 4) {
              D_80369B88 = BR_MSEC();
            }
          } else if (D_8028DB60 == 5 || D_8028DB60 == 6 || D_8028DB60 == 1 || D_8028DB60 == 4) {
            t = BR_MSEC();
            if (t - D_80369B88 < 350) {
              D_8028DBC0 = 1;
              D_8028DBE0 = 1;
              return;
            }
            D_80369B88 = t;
          }
        }
        switch (i) {
        case 0:
          for (j = 0; j < 5; j++) {
            if (BrPaintCursorInRect(&D_8028D4F0[j])) {
              D_8028DAC0 = j;
              return;
            }
          }
          return;
        case 7:
          D_8028DBC0 = 1;
          return;
        case 11:
          BrPaintDecalApply();
          D_8028DB60 = D_8028DB64;
          return;
        case 8:
          D_8028CFC0.kind = 0;
          return;
        case 9:
        case 10:
          D_8028DBC0 = 0;
          return;
        case 12:
          return;
        }
        return;
      }
    }
  }
  if (D_8028DB60 == 4) {
    clicked = 0;
    if (BrPaintCursorInRect(&D_8028DB94) && D_8028DBC4 == 0 && (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 0x8010)) {
      BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 0x8010);
      D_80369B80 = D_8028D110.x;
      D_80369B84 = D_8028D110.y;
      clicked = 1;
      D_8028DBC4 = 1;
    }
    if (D_8028DBC4 != 0 && D_8028DBC0 == 0) {
      if (clicked) {
        D_8028D110.x = D_8028D540[24].x0 + (D_8028D290.drawW >> 1);
        D_8028D110.y = D_8028D540[24].x4 + (D_8028D290.drawH >> 1);
        D_8028DBB0 = 0;
      }
      BrPaintKeyboard();
      return;
    }
    if (D_8028DBC0 != 0 && !BrPaintCursorInRect(&D_8028D480) && !BrPaintCursorInRect(&D_8028D490) &&
        !(*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 0x8010)) {
      if (D_8028DBA8 > 0) {
        if (D_8028DBE4 != 0) {
          D_8028D110.x = D_80369B80;
          D_8028D110.y = D_80369B84;
          D_8028DBE4 = 0;
        }
        BrPaintTextDraw(D_8028D110.x, D_8028D110.y + 1);
        return;
      }
      if (D_8028DBA8 == 0) {
        D_8028DBC0 = 0;
        D_8028DBC4 = 0;
      }
      return;
    }
    if (D_8028DBC0 != 0 && (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 0x8010)) {
      BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 0x8010);
      BrPaintDecalCommit();
      BrPaintTextStamp(D_8028D110.x, D_8028D110.y + 1);
      D_8028DBA8 = 0;
      D_8028DBAC = 0;
      D_8028DBC0 = 0;
      D_8028DBC4 = 0;
    }
  } else if (D_8028DB60 != 4 && BrPaintCursorInRect(&D_8028DB94) && (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 0x8010)) {
    switch (D_8028DB60) {
    case 1:
      if (D_8028DBC0 == 0) {
        BrPaintDecalCommit();
        D_8028DBC0 = 1;
      }
      i = D_8028DAC0;
      if (i == 0) {
        x = D_8028D110.x;
        y = D_8028D110.y;
        x -= D_8028DB94.x;
        x >>= 2; y = D_8028DB94.y + D_8028DB94.h - y; y >>= 2;
        BrPaintPlot(x, y, D_8028DB58);
      } else if (D_8028CE70.kind == 0) {
        BrPaintDisc(D_8028D110.x, D_8028D110.y, D_8028D4A0[i].w >> 1, 1);
      } else {
        r = D_8028D4A0[i].w * 2;
        BrPaintFillRect(D_8028D110.x - r, D_8028D110.y - r, D_8028D110.x + r, D_8028D110.y + r);
      }
      return;
    case 3:
      if (D_8028DBC0 == 0) {
        D_80369B80 = D_8028D110.x;
        D_80369B84 = D_8028D110.y;
        D_8028DBC0 = 1;
      } else {
        BrPaintDecalCommit();
        BrPaintLine(D_80369B80, D_80369B84, D_8028D110.x, D_8028D110.y);
        D_8028DBC0 = 0;
      }
      BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 0x8010);
      return;
    case 2:
      BrPaintDecalCommit();
      BrPaintFloodFill(D_8028D110.x, D_8028D110.y);
      BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 0x8010);
      return;
    case 5:
      if (D_8028DBC0 == 0) {
        D_80369B80 = D_8028D110.x;
        D_80369B84 = D_8028D110.y;
        D_8028DBC0 = 1;
      } else {
        BrPaintDecalCommit();
        if (D_8028CF30.kind == 0) {
          BrPaintFillRect(D_80369B80, D_80369B84, D_8028D110.x, D_8028D110.y);
        } else if (D_8028CF30.kind == 1) {
          BrPaintFrameRect(D_80369B80, D_80369B84, D_8028D110.x, D_8028D110.y);
        } else if (D_8028CF30.kind == 2) {
          BrPaintFillRoundRect(D_80369B80, D_80369B84, D_8028D110.x, D_8028D110.y);
        } else if (D_8028CF30.kind == 3) {
          BrPaintFrameRoundRect(D_80369B80, D_80369B84, D_8028D110.x, D_8028D110.y);
        }
        D_8028DBC0 = 0;
      }
      BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 0x8010);
      return;
    case 6:
      if (0 == D_8028DBC0) {
        D_80369B80 = D_8028D110.x;
        D_80369B84 = D_8028D110.y;
        D_8028DBC0 = 1;
      } else {
        BrPaintDecalCommit();
        if (D_8028CF60.kind == 2 || D_8028CF60.kind == 3) {
          x = D_8028D110.x - D_80369B80;
          y = D_8028D110.y - D_80369B84;
          r = sqrtf(x * x + y * y);
          r >>= 2;
        }
        if (D_8028CF60.kind == 0) {
          BrPaintFillOval(D_80369B80, D_80369B84, D_8028D110.x, D_8028D110.y);
        } else if (D_8028CF60.kind == 1) {
          BrPaintFrameOval(D_80369B80, D_80369B84, D_8028D110.x, D_8028D110.y);
        } else if (D_8028CF60.kind == 2) {
          BrPaintDisc(D_80369B80, D_80369B84, r, 1);
        } else if (D_8028CF60.kind == 3) {
          BrPaintCircle(D_80369B80, D_80369B84, r);
        }
        D_8028DBC0 = 0;
      }
      BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 0x8010);
      return;
    }
  } else if (D_8028DB60 != 4 && D_8028DBC0 != 0 && BrPaintCursorInRect(&D_8028DB94) &&
             !(*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 0x8010)) {
    switch (D_8028DB60) {
    case 1:
      D_8028DBC0 = 0;
      return;
    case 3:
      BrPaintDashLine(D_80369B80, D_80369B84, D_8028D110.x, D_8028D110.y);
      return;
    case 5:
      if (D_8028CF30.kind == 0 || D_8028CF30.kind == 1) {
        BrPaintDashRect(D_80369B80, D_80369B84, D_8028D110.x, D_8028D110.y);
        return;
      }
      BrPaintDashRoundRect(D_80369B80, D_80369B84, D_8028D110.x, D_8028D110.y);
      return;
    case 6:
      if (D_8028CF60.kind == 0 || D_8028CF60.kind == 1) {
        BrPaintDashOval(D_80369B80, D_80369B84, D_8028D110.x, D_8028D110.y);
        return;
      }
      x = D_8028D110.x - D_80369B80;
      y = D_8028D110.y - D_80369B84;
      r = sqrtf(x * x + y * y);
      BrPaintDashCircle(D_80369B80, D_80369B84, r);
      return;
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
  if (r[0] <= D_8028D110.x && D_8028D110.x <= r[0] + D_8028D290.drawW && r[1] <= D_8028D110.y && D_8028D110.y <= r[1] + D_8028D290.drawH) {
    return 1;
  }
  return 0;
}

/* WHAT IT DOES: Copy the 16 decal palette colours (RGBA5551) into the
 * palette swatches as 8-bit r, g, b, each 5-bit channel widened by
 * repeating its top bits. */
/* @implements 0x8024D53C tgr BrPaintPaletteLoad */
void BrPaintPaletteLoad()
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
void BrPaintPaletteDraw()
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

#if !TGR_REGION_JPN
BrPaintNames2 D_8028DCE4 = { { "ROUND BRUSH", "SQUARE BRUSH" } };
#endif

/* WHAT IT DOES: The paint shop's brush-shape chooser: a box with the round
 * brush picture and a square, the chosen one outlined, and its name, with
 * A (select) and B (cancel) buttons; left or right swaps the choice, A
 * keeps it and B restores the previous one, both closing the box.  The
 * pad record is re-addressed at every test (no pad or pressed local: the
 * ROM's frame has no slots for them).
 * Japan leaves out the style's name. */
/* @implements 0x8024D89C tgr BrPaintBrushSelect */
void BrPaintBrushSelect()
{
  char spare[28];               /* declared, never used: the frame holds it */
  int y;
  int bx;
#if !TGR_REGION_JPN
  BrPaintNames2 names;
#endif

#if !TGR_REGION_JPN
  names = D_8028DCE4;
#endif
  y = 322 - D_8028D0B0.w;
  bx = 349 - D_8028D0E0.w;
  BrBevelPanel(200, 0x96, 0xf0, 0xb4, 3, 0, 0, 0x80, 0x80, 0x80);
  if (D_8028DBC0 != 0) {
    D_8028DBB8 = D_8028CE70.kind;
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
#if !TGR_REGION_JPN
  BrBevelPanel(0xe0, 0xff, 0xc0, 0x1e, 1, 1, 1, 0x80, 0x80, 0x80);
#endif
  BrTextSetFont(11);
  BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xf5, 0);
#if !TGR_REGION_JPN
  BrTextPrint(names.n[D_8028DBB8], 159, 138);
#endif
  BrImageDrawAt(&D_8028D0B0, 216, y);
  BrImageDrawAt(&D_8028D0E0, bx, y);
  BrTextSetFont(10);
  BrTextAlignLeft();
  BrTextPrint("%wwSELECT", (unsigned int)(D_8028D0B0.w + 222) >> 1, (y + 18) >> 1);
  BrTextPrint("%wwCANCEL", (unsigned int)(bx + D_8028D0E0.w + 6) >> 1, (y + 18) >> 1);
  BrPadStickToButtons(&PADS[D_8028DBBC]);
  if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 5) {
    BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 5);
    D_8028DBB8 ^= 1;
  }
  if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 0x10) {
    BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 0x10);
    D_8028CE70.kind = D_8028DBB8;
    D_8028DBE0 = 0;
  } else if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 0x20) {
    BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 0x20);
    D_8028DBE0 = 0;
  }
}

/* WHAT IT DOES: Flood-fill the region of one colour under a screen point
 * with the chosen colour (Heckbert's scanline seed fill, a 400-segment
 * stack): the fill is painted into a scratch copy of the decal with the
 * mask off, then the whole copy is plotted back into the decal so the mask
 * applies.  Nothing when the region is already that colour.
 * Source facts: the screen point arrives in x and y themselves (no
 * locals for it), which sets the frame.  The pop reads dy through
 * stack[--n] before the other fields, the segment loop is a top-tested
 * while, and the plot loops step x inside the call (x--, x++). */
/* @implements 0x8024DCA0 tgr BrPaintFloodFill */
void BrPaintFloodFill(int x, int y)
{
  unsigned char ov;
  int dy;
  int n;
  int l;
  int x1;
  int x2;
  BrFillSeg stack[400];
  unsigned char *mask;
  unsigned char *decal;

  x = (x - D_8028DB94.x) >> 2; y = (D_8028DB94.y + D_8028DB94.h - y) >> 2;
  ov = BrPaintGet(x, y);
  if (ov != D_8028DB58) {
    stack[0].y = y;
    stack[0].xl = x;
    stack[0].xr = x;
    stack[0].dy = 1;
    stack[1].y = y + 1;
    stack[1].xl = x;
    stack[1].xr = x;
    stack[1].dy = -1; n = 2;
    mask = D_8028DB7C;
    D_8028DB7C = 0;
    memcpy(D_8028DB84, D_8028DB78, 0x800);
    decal = D_8028DB78;
    D_8028DB78 = D_8028DB84;
    while (n > 0 && n < 400) {
      dy = stack[--n].dy;
      y = stack[n].y + dy;
      x = x1 = stack[n].xl;
      x2 = stack[n].xr;
      while (x >= 0 && y >= 0 && y < D_8028DB8C && BrPaintGet(x, y) == ov) {
        BrPaintPlot(x--, y, D_8028DB58);
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
        while (x < D_8028DB88 && y >= 0 && y < D_8028DB8C && BrPaintGet(x, y) == ov) {
          BrPaintPlot(x++, y, D_8028DB58);
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
    }
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

/* WHAT IT DOES: The paint shop's text-style popup: a normal and a
 * drop-shadow sample letter, the chosen style framed in blue and named
 * below (Japan leaves the name out); left/right toggle it.  A on drop shadow opens the shadow colour
 * picker (sixteen palette swatches in two rows, the current one framed;
 * left/right walk a row, up/down switch rows, A keeps the colour, B
 * restores it); A on normal closes, B cancels.
 * The A button's x is one char variable: 0xd6 in the style view, stepped
 * by 0x10 for the picker, so the picker keeps it in a register past the
 * draws instead of folding it into the SELECT label's x. */
/* @implements 0x8024E128 tgr BrPaintTextStyleMenu */
void BrPaintTextStyleMenu()
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
#if !TGR_REGION_JPN
  char *names[2] = { "NORMAL TEXT", "DROP-SHADOW TEXT" };
#endif

  BrBevelPanel(0xb4, 0x82, 0x118, 0xd0, 3, 0, 0, 0x80, 0x80, 0x80);
  BrTextSetFont(16);
  BrTextAlignCentre();
  BrTextHighlightOff();
  BrTextPrint("%rySELECT OPTION", 0x9f, 0x53);
  if (D_8028DBC0 != 0) {
    D_8028DBB8 = D_8028CF00.kind;
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
#if TGR_REGION_JPN
    BrTextSetFont(11);
    BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xf5, 0);
#else
    BrBevelPanel(0xc6, r[0][1] + r[0][3] + 0x11, 0xf4, 0x1e, 1, 1, 1, 0x80, 0x80, 0x80);
    BrTextSetFont(11);
    BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xf5, 0);
    BrTextPrint(names[D_8028DBB8], 0x9e, (r[0][1] + r[1][3] + 0x26) >> 1);
#endif
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
    if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 5) {
      BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 5);
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
    if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 4) {
      BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 4);
      if (D_8028DB5C == 0) {
        D_8028DB5C = 7;
      } else if (D_8028DB5C == 8) {
        D_8028DB5C = 15;
      } else {
        D_8028DB5C--;
      }
    } else if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 1) {
      BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 1);
      if (D_8028DB5C == 7) {
        D_8028DB5C = 0;
      } else if (D_8028DB5C == 15) {
        D_8028DB5C = 8;
      } else {
        D_8028DB5C++;
      }
    } else if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 10) {
      BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 10);
      if (D_8028DB5C < 8) {
        D_8028DB5C += 8;
      } else {
        D_8028DB5C -= 8;
      }
    }
    if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 0x10) {
      BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 0x10);
      D_8028CF00.kind = D_8028DBB8;
      D_8028DBE4 = 0;
      D_8028DBE0 = 0;
    } else if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 0x20) {
      BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 0x20);
      D_8028DB5C = D_8028DB54;
      D_8028DBE4 = 0;
    }
  }
  if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 0x10) {
    BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 0x10);
    if (D_8028DBB8 != 0) {
      D_8028DBE4 = 1;
    } else {
      D_8028DBE0 = 0;
    }
  } else if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 0x20) {
    BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 0x20);
    D_8028DBE0 = 0;
    D_8028DBE4 = 0;
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
    if (D_8028CF00.kind != 0) {
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

/* WHAT IT DOES: Draw a line on the decal between two screen points (a
 * quarter scale, y flipped) by Bresenham's method, stepping along the
 * longer axis from the lower end: each point a texel in the chosen colour,
 * or a disc the brush's size.
 * Source facts: each step is one conditional expression (y1 < y0 ? -1 : 1),
 * which gives both steps the callee registers and leaves the loop ends to be
 * spilled; the two corners are converted one point per line. */
/* @implements 0x8024F000 tgr BrPaintLine */
void BrPaintLine(int x0, int y0, int x1, int y1)
{
  int dx;
  int dy;
  int r;
  int x;
  int y;
  int stepx;
  int stepy;
  int e;
  int t;

  x0 = (x0 - D_8028DB94.x) >> 2; y0 = (D_8028DB94.y + D_8028DB94.h - y0) >> 2;
  x1 = (x1 - D_8028DB94.x) >> 2; y1 = (D_8028DB94.y + D_8028DB94.h - y1) >> 2;
  r = D_8028D4A0[D_8028DAC0].w >> 1;
  dx = x1 - x0 < 0 ? -(x1 - x0) : x1 - x0;
  dy = y1 - y0 < 0 ? -(y1 - y0) : y1 - y0;
  if ((dy < dx && x1 < x0) || (dx < dy && y1 < y0)) {
    t = x0;
    x0 = x1;
    x1 = t;
    t = y0;
    y0 = y1;
    y1 = t;
  }
  stepy = y1 < y0 ? -1 : 1;
  stepx = x1 < x0 ? -1 : 1;
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
 * Source facts: the mask byte's index is a named local m, computed in the
 * masked arm, so its address is the mask base plus m. */
/* @implements 0x8024F25C tgr BrPaintPlot */
void BrPaintPlot(int x, int y, unsigned char c)
{
  int o;
  int m;

  if (x >= 0 && x < D_8028DB88 && y >= 0 && y < D_8028DB8C) {
    o = ((x ^ ((y & 1) << 3)) >> 1) + y * (D_8028DB88 >> 1);
    if (D_8028DB7C == 0) {
      if (x & 1) {
        D_8028DB78[o] = (D_8028DB78[o] & 0xf0) | c;
      } else {
        D_8028DB78[o] = (D_8028DB78[o] & 0xf) | (c << 4);
      }
    } else {
      m = (x >> 3) + y * ((D_8028DB88 + 7) >> 3);
      if (!(D_8028DB7C[m] & (1 << ((x ^ 7) & 7)))) {
        if (x & 1) {
          D_8028DB78[o] = (D_8028DB78[o] & 0xf0) | c;
        } else {
          D_8028DB78[o] = (D_8028DB78[o] & 0xf) | (c << 4);
        }
      }
    }
  }
}

#if !TGR_REGION_JPN
BrPaintNames D_8028DCF4 = { { "FILLED RECTANGLE", "OUTLINED RECTANGLE",
                               "FILLED ROUND RECTANGLE", "OUTLINED ROUND RECTANGLE" } };
#endif

/* WHAT IT DOES: The paint shop's decal-style chooser: a box with the four
 * style pictures (the chosen one outlined) and the chosen style's name,
 * A (select) and B (cancel) buttons under it; left and right move the
 * choice round, A keeps it and B restores the previous one, both closing
 * the box.  The four names are a local copy of the table at 0x8028DCF4.
 * The pictures' x is written out per call (i * 76 + 175), and the pad
 * record is re-addressed at every test with an unsigned (sizeof) multiply:
 * the ROM keeps 0x15C in a saved register for multu.
 * Japan leaves out the style's name. */
/* @implements 0x8024F39C tgr BrPaintStyleSelect */
void BrPaintStyleSelect()
{
  char spare[24];               /* declared, never used: the frame holds it */
  int x;                        /* likewise */
  int i;
  int y;
  int bx;
#if !TGR_REGION_JPN
  BrPaintNames names;
#endif

#if !TGR_REGION_JPN
  names = D_8028DCF4;
#endif
  y = 323 - D_8028D0B0.w;
  bx = 353 - D_8028D0E0.w;
  BrBevelPanel(0x9f, 0x95, 0x142, 0xb6, 3, 0, 0, 0x80, 0x80, 0x80);
  BrTextSetFont(16);
  BrTextAlignCentre();
  BrTextHighlightOff();
  BrTextPrint("%rySELECT STYLE", 159, 92);
  if (D_8028DBC0 != 0) {
    D_8028DBB8 = D_8028CF30.kind;
    D_8028DBC0 = 0;
  }
  for (i = 0; i < 4; i++) {
    if (i == D_8028DBB8) {
      BrImageDrawRect(D_8028DB34[i], i * 76 + 175, 203, D_8028DB34[i]->drawW, D_8028DB34[i]->drawH, 0x20, 200, 0xff);
    } else {
      BrImageDrawAt(D_8028DB34[i], i * 76 + 175, 203);
    }
  }
#if !TGR_REGION_JPN
  BrBevelPanel(0xb1, 0x103, 0x11e, 0x1e, 1, 1, 1, 0x80, 0x80, 0x80);
#endif
  BrTextSetFont(11);
  BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xf5, 0);
#if !TGR_REGION_JPN
  BrTextPrint(names.n[D_8028DBB8], 159, 140);
#endif
  BrTextSetFont(10);
  BrTextAlignLeft();
  BrTextPrint("%wwSELECT", (unsigned int)(D_8028D0B0.w + 219) >> 1, (y + 18) >> 1);
  BrTextPrint("%wwCANCEL", (unsigned int)(bx + D_8028D0E0.w + 6) >> 1, (y + 18) >> 1);
  BrImageDrawAt(&D_8028D0B0, 213, y);
  BrImageDrawAt(&D_8028D0E0, bx, y);
  BrPadStickToButtons(&PADS[D_8028DBBC]);
  if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)) & 4) {
    BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)), 4);
    if (D_8028DBB8 == 0) {
      D_8028DBB8 = 3;
    } else {
      D_8028DBB8--;
    }
  } else if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)) & 1) {
    BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)), 1);
    if (D_8028DBB8 == 3) {
      D_8028DBB8 = 0;
    } else {
      D_8028DBB8++;
    }
  }
  if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)) & 0x10) {
    BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)), 0x10);
    D_8028CF30.kind = D_8028DBB8;
    D_8028DBE0 = 0;
  } else if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)) & 0x20) {
    BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * sizeof(BrPadRec)), 0x20);
    D_8028DBE0 = 0;
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
 * As BrPaintDisc: each corner span's start and end are locals set before
 * its loop, the half-widths are read as (a >> 1) and (b >> 1), a starts at
 * 0 before err = -b, and the walk steps err += a++ and err -= b--. */
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
  int e;
  int s;
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
  a = 0;
  err = -b;
  for (; a <= b;) {
    if (!(a & 1)) {
      s = sx0 - (a >> 1);
      e = (a >> 1) + x1;
      for (x = s; x < e; x++) {
        BrPaintPlot(x, sy0 - (b >> 1), D_8028DB58);
      }
      s = sx0 - (b >> 1);
      e = (b >> 1) + x1;
      for (x = s; x < e; x++) {
        BrPaintPlot(x, sy0 - (a >> 1), D_8028DB58);
      }
      s = sx0 - (a >> 1);
      e = (a >> 1) + x1;
      for (x = s; x < e; x++) {
        BrPaintPlot(x, sy1 + (b >> 1), D_8028DB58);
      }
      s = sx0 - (b >> 1);
      e = (b >> 1) + x1;
      for (x = s; x < e; x++) {
        BrPaintPlot(x, sy1 + (a >> 1), D_8028DB58);
      }
    }
    err += a++;
    if (err >= 0) {
      err -= b--;
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
 * The far corner is the sx1 parameter itself (no local copy), the half
 * brush is read straight from the table, the radius is the shorter side's
 * quarter plus a quarter brush in one expression, and the corner walk reads
 * (a >> 1) and (b >> 1) at each use with a starting at 0 before err = -b,
 * stepping err += a++ and err -= b--. */
/* @implements 0x8024FEB8 tgr BrPaintFrameRoundRect */
void BrPaintFrameRoundRect(int sx0, int sy0, int sx1, int sy1)
{
  int t;
  int hw;
  int unused;                   /* declared, never used: the frame holds it */
  int x;
  int y;
  int r;
  int a;
  int b;
  int k;
  int err;

  sx0 = (sx0 - D_8028DB94.x) >> 2; sy0 = (D_8028DB94.y + D_8028DB94.h - sy0) >> 2;
  sx1 = (sx1 - D_8028DB94.x) >> 2; sy1 = (D_8028DB94.y + D_8028DB94.h - sy1) >> 2;
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
  hw = D_8028D4A0[D_8028DAC0].w >> 1;
  r = ((sx1 - sx0 < sy1 - sy0 ? sx1 - sx0 : sy1 - sy0) >> 2) + (hw >> 1);
  if (hw == 0) {
    for (y = sy0 + r; y < sy1 - r; y++) {
      BrPaintPlot(sx0, y, D_8028DB58);
    }
    for (y = sy0 + r; y < sy1 - r; y++) {
      BrPaintPlot(sx1, y, D_8028DB58);
    }
    for (x = sx0 + r; x < sx1 - r; x++) {
      BrPaintPlot(x, sy1, D_8028DB58);
    }
    for (x = sx0 + r; x < sx1 - r; x++) {
      BrPaintPlot(x, sy0, D_8028DB58);
    }
  } else {
    for (y = sy0 + r; y <= sy1 - r; y++) {
      for (x = sx0 - hw; x < sx0 + hw; x++) {
        BrPaintPlot(x, y, D_8028DB58);
      }
    }
    for (y = sy0 + r; y <= sy1 - r; y++) {
      for (x = sx1 - hw; x < sx1 + hw; x++) {
        BrPaintPlot(x, y, D_8028DB58);
      }
    }
    for (y = sy1 - hw; y < sy1 + hw; y++) {
      for (x = sx0 + r; x <= sx1 - r; x++) {
        BrPaintPlot(x, y, D_8028DB58);
      }
    }
    for (y = sy0 - hw; y < sy0 + hw; y++) {
      for (x = sx0 + r; x <= sx1 - r; x++) {
        BrPaintPlot(x, y, D_8028DB58);
      }
    }
  }
  sx0 += r;
  sx1 -= r;
  sy0 += r;
  sy1 -= r;
  if (D_8028DAC0 > 2) {
    sx0--;
    sy0--;
  }
  r += hw >> 1;
  b = r * 2;
  a = 0;
  err = -b;
  for (; a <= b;) {
    if (!(a & 1)) {
      if (hw == 0) {
        BrPaintPlot(sx0 - (a >> 1), sy0 - (b >> 1), D_8028DB58);
      } else {
        for (k = 0; k < hw * 2; k++) {
          BrPaintPlot(sx0 - (a >> 1), sy0 - (b >> 1) + k, D_8028DB58);
        }
      }
      if (hw == 0) {
        BrPaintPlot(sx0 - (a >> 1), (b >> 1) + sy1, D_8028DB58);
      } else {
        for (k = 0; k < hw * 2; k++) {
          BrPaintPlot(sx0 - (a >> 1), (b >> 1) + sy1 - k, D_8028DB58);
        }
      }
      if (hw == 0) {
        BrPaintPlot((a >> 1) + sx1, (b >> 1) + sy1, D_8028DB58);
      } else {
        for (k = 0; k < hw * 2; k++) {
          BrPaintPlot((a >> 1) + sx1, (b >> 1) + sy1 - k, D_8028DB58);
        }
      }
      if (hw == 0) {
        BrPaintPlot((a >> 1) + sx1, sy0 - (b >> 1), D_8028DB58);
      } else {
        for (k = 0; k < hw * 2; k++) {
          BrPaintPlot((a >> 1) + sx1, sy0 - (b >> 1) + k, D_8028DB58);
        }
      }
      if (hw == 0) {
        BrPaintPlot((b >> 1) + sx1, (a >> 1) + sy1, D_8028DB58);
      } else {
        for (k = 0; k < hw * 2; k++) {
          BrPaintPlot((b >> 1) + sx1 - k, (a >> 1) + sy1, D_8028DB58);
        }
      }
      if (hw == 0) {
        BrPaintPlot((b >> 1) + sx1, sy0 - (a >> 1), D_8028DB58);
      } else {
        for (k = 0; k < hw * 2; k++) {
          BrPaintPlot((b >> 1) + sx1 - k, sy0 - (a >> 1), D_8028DB58);
        }
      }
      if (hw == 0) {
        BrPaintPlot(sx0 - (b >> 1), sy0 - (a >> 1), D_8028DB58);
      } else {
        for (k = 0; k < hw * 2; k++) {
          BrPaintPlot(sx0 - (b >> 1) + k, sy0 - (a >> 1), D_8028DB58);
        }
      }
      if (hw == 0) {
        BrPaintPlot(sx0 - (b >> 1), (a >> 1) + sy1, D_8028DB58);
      } else {
        for (k = 0; k < hw * 2; k++) {
          BrPaintPlot(sx0 - (b >> 1) + k, (a >> 1) + sy1, D_8028DB58);
        }
      }
    }
    err += a++;
    if (err >= 0) {
      err -= b--;
    }
  }
}

#if !TGR_REGION_JPN
BrPaintNames D_8028DD04 = { { "FILLED OVAL", "OUTLINED OVAL", "FILLED CIRCLE",
                               "OUTLINED CIRCLE" } };
#endif

/* WHAT IT DOES: The paint shop's oval-style chooser, as the decal-style one:
 * a box with the four pictures (placed once when the box opens: the first
 * two 72 apart, the last two each after its partner's right edge, 52 apart)
 * and the chosen style's name, A (select) and B (cancel) under it; left and
 * right move the choice round, A keeps it and B restores the previous one,
 * both closing the box.  The pad record is re-addressed at every test
 * (no pad or pressed local), as the ROM rebuilds its address each time.
 * Japan leaves out the style's name. */
/* @implements 0x80250698 tgr BrPaintOvalStyleSelect */
void BrPaintOvalStyleSelect()
{
  char spare[24];               /* declared, never used: the frame holds it */
  int x;
  int i;
  int y;
  int bx;
#if !TGR_REGION_JPN
  BrPaintNames names;
#endif

#if !TGR_REGION_JPN
  names = D_8028DD04;
#endif
  y = 323 - D_8028D0B0.w;
  bx = 352 - D_8028D0E0.w;
  BrBevelPanel(0xb8, 0x95, 0x110, 0xb6, 3, 0, 0, 0x80, 0x80, 0x80);
  BrTextSetFont(16);
  BrTextAlignCentre();
  BrTextHighlightOff();
  BrTextPrint("%rySELECT STYLE", 160, 91);
  if (D_8028DBC0 != 0) {
    D_8028DBB8 = D_8028CF60.kind;
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
      BR_IMAGE_DRAW(D_8028DB44[i]);
    }
  }
#if !TGR_REGION_JPN
  BrBevelPanel(0xce, 0x101, 0xe4, 0x1e, 1, 1, 1, 0x80, 0x80, 0x80);
#endif
  BrTextSetFont(11);
  BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xf5, 0);
#if !TGR_REGION_JPN
  BrTextPrint(names.n[D_8028DBB8], 159, 139);
#endif
  BrTextSetFont(10);
  BrTextAlignLeft();
  BrTextPrint("%wwSELECT", (unsigned int)(D_8028D0B0.w + 220) >> 1, (y + 18) >> 1);
  BrTextPrint("%wwCANCEL", (unsigned int)(bx + D_8028D0E0.w + 6) >> 1, (y + 18) >> 1);
  BrImageDrawAt(&D_8028D0B0, 214, y);
  BrImageDrawAt(&D_8028D0E0, bx, y);
  BrPadStickToButtons(&PADS[D_8028DBBC]);
  if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 4) {
    BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 4);
    if (D_8028DBB8 == 0) {
      D_8028DBB8 = 3;
    } else {
      D_8028DBB8--;
    }
  } else if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 1) {
    BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 1);
    if (D_8028DBB8 == 3) {
      D_8028DBB8 = 0;
    } else {
      D_8028DBB8++;
    }
  }
  if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 0x10) {
    BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 0x10);
    D_8028CF60.kind = D_8028DBB8;
    D_8028DBE0 = 0;
  } else if (*(unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c) & 0x20) {
    BrPadConsume((unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c), 0x20);
    D_8028DBE0 = 0;
  }
}

/* WHAT IT DOES: Fill an oval on the decal inside two screen corners (either
 * order) in the chosen colour: a midpoint ellipse walk at twice the
 * resolution, radii half the sides (at least 1), and on every other step a
 * span above and below the centre -- first the region where the walk steps
 * across, then the one where it steps up.
 * Source facts: the walk is written with products, as in
 * BrPaintDashOval: the loop tests b2b * x against a2b * y and the error
 * steps add b2 * x before and after x++, and uopt strength-reduces all
 * three into frame temps (the running sums at 0x4c, 0x48, 0x44).  b2 and a2
 * are copies of the squares that uopt replaces by the products themselves.
 * The centres are recomputed at the top of every step and the span ends
 * are formed from them.  t = b2b * x at the top of the first walk is dead
 * and emits nothing, but it makes uopt meet b2b * x before a2b * y, which
 * gives the sums their ROM stack slots and puts the b2b load of the second
 * walk's start first (hoisted above its guard). */
/* @implements 0x80250B58 tgr BrPaintFillOval */
void BrPaintFillOval(int sx0, int sy0, int sx1, int sy1)
{
  int t;
  int ry;
  int rx;
  int x;
  int y;
  int err;
  int cx;
  int cy;
  int xe;
  int i;
  int b2;
  int a2;
  int b2b;
  int a2b;

  sx0 = (sx0 - D_8028DB94.x) >> 2; sy0 = (D_8028DB94.y + D_8028DB94.h - sy0) >> 2;
  sx1 = (sx1 - D_8028DB94.x) >> 2; sy1 = (D_8028DB94.y + D_8028DB94.h - sy1) >> 2;
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
  ry = (sy1 - sy0) >> 1 >= 2 ? (sy1 - sy0) >> 1 : 1;
  rx = (sx1 - sx0) >> 1 >= 2 ? (sx1 - sx0) >> 1 : 1;
  b2 = b2b = ry * ry;
  a2 = a2b = rx * rx;
  y = ry * 2;
  x = 0;
  err = -a2 * y;
  while (b2b * x <= a2b * y) {
    t = b2b * x;
    cx = (sx0 + sx1) >> 1;
    cy = (sy0 + sy1) >> 1;
    if (!(x & 1)) {
      xe = (x >> 1) + cx;
      for (i = cx - (x >> 1); i < xe; i++) {
        BrPaintPlot(i, (y >> 1) + cy, D_8028DB58);
      }
      for (i = cx - (x >> 1); i < xe; i++) {
        BrPaintPlot(i, cy - (y >> 1), D_8028DB58);
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
  y = 0;
  x = rx * 2;
  err = -b2 * x;
  if (a2 * y <= x * b2) {
    do {
      cx = (sx0 + sx1) >> 1;
      cy = (sy0 + sy1) >> 1;
      if (!(y & 1)) {
        xe = (x >> 1) + cx;
        for (i = cx - (x >> 1); i < xe; i++) {
          BrPaintPlot(i, (y >> 1) + cy, D_8028DB58);
        }
        for (i = cx - (x >> 1); i < xe; i++) {
          BrPaintPlot(i, cy - (y >> 1), D_8028DB58);
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
    } while (a2b * y <= b2b * x);
  }
}

/* WHAT IT DOES: Draw an oval outline on the decal inside two screen corners
 * (either order) in the chosen colour: the midpoint ellipse walk of
 * BrPaintFillOval with the radii grown by half the brush, and on every
 * other step each quadrant's point drawn as a run of texels the brush's
 * size inward (up/down in the first region, across in the second).
 * Source facts: the corners are swapped when sx1 - sx0 (sy1 - sy0) is
 * negative, with no width variable; the radii are the pair r[] and the
 * squares the pair sq, whose address is taken (p), so both stay in memory:
 * the walk reads them back after every plot and keeps the source operand
 * order.  b2b and a2b are copies of the squares for the running sums ey and
 * ex.  Each region is guarded by the product test, then runs as a for loop
 * whose inits sit on its header line and which breaks at the bottom.  The
 * left, top and bottom edges are computed in that order at each step and
 * the right edge is written inline, which orders the four runs' setup as in
 * the ROM. */
/* @implements 0x80250FCC tgr BrPaintFrameOval */
void BrPaintFrameOval(int sx0, int sy0, int sx1, int sy1)
{
  int *p;
  int r[2];
  int l;
  int t;
  int b;
  int w;
  int x;
  int y;
  int err;
  int k;
  int d;
  int b2b;
  int a2b;
  struct {
    int a2;
    int b2;
  } sq;
  int ey;
  int ex;

  sx0 = (sx0 - D_8028DB94.x) >> 2; sy0 = (D_8028DB94.y + D_8028DB94.h - sy0) >> 2;
  sx1 = (sx1 - D_8028DB94.x) >> 2; sy1 = (D_8028DB94.y + D_8028DB94.h - sy1) >> 2;
  if (sx1 - sx0 < 0) {
    t = sx0;
    sx0 = sx1;
    sx1 = t;
  }
  if (sy1 - sy0 < 0) {
    t = sy0;
    sy0 = sy1;
    sy1 = t;
  }
  w = D_8028D4A0[D_8028DAC0].w;
  r[1] = (sy1 - sy0 + w) >> 1 >= 2 ? (sy1 - sy0 + w) >> 1 : 1;
  r[0] = (sx1 - sx0 + w) >> 1 >= 2 ? (sx1 - sx0 + w) >> 1 : 1;
  p = &sq.a2;
  sq.b2 = r[1] * r[1];
  b2b = sq.b2;
  sq.a2 = r[0] * r[0];
  a2b = sq.a2;
  y = r[1] * 2;
  x = 0;
  err = -sq.a2 * y;
  if (b2b * x <= a2b * y) {
    for (ex = a2b * y, ey = 0, d = 0;;) {
      if (!(x & 1)) {
        l = ((sx0 + sx1) >> 1) - (x >> 1);
        t = ((sy0 + sy1) >> 1) - (y >> 1);
        b = (y >> 1) + ((sy0 + sy1) >> 1);
        for (k = 0; k < w; k++) {
          BrPaintPlot((x >> 1) + ((sx0 + sx1) >> 1), b - k, D_8028DB58);
        }
        for (k = 0; k < w; k++) {
          BrPaintPlot((x >> 1) + ((sx0 + sx1) >> 1), t + k, D_8028DB58);
        }
        for (k = 0; k < w; k++) {
          BrPaintPlot(l, t + k, D_8028DB58);
        }
        for (k = 0; k < w; k++) {
          BrPaintPlot(l, b - k, D_8028DB58);
        }
      }
      err += d;
      ey += b2b;
      x++;
      d += r[1] * r[1];
      err += d;
      if (err > 0) {
        err -= sq.a2 * y;
        y--;
        ex -= a2b;
        err -= sq.a2 * y;
      }
      if (ex < ey) {
        break;
      }
    }
  }
  y = 0;
  x = r[0] * 2;
  err = -sq.b2 * x;
  if (sq.a2 * y <= x * sq.b2) {
    for (ey = b2b * x, ex = 0, d = 0;;) {
      if (!(y & 1)) {
        l = ((sx0 + sx1) >> 1) - (x >> 1);
        t = ((sy0 + sy1) >> 1) - (y >> 1);
        b = (y >> 1) + ((sy0 + sy1) >> 1);
        for (k = 0; k < w; k++) {
          BrPaintPlot((x >> 1) + ((sx0 + sx1) >> 1) - k, b, D_8028DB58);
        }
        for (k = 0; k < w; k++) {
          BrPaintPlot((x >> 1) + ((sx0 + sx1) >> 1) - k, t, D_8028DB58);
        }
        for (k = 0; k < w; k++) {
          BrPaintPlot(l + k, t, D_8028DB58);
        }
        for (k = 0; k < w; k++) {
          BrPaintPlot(l + k, b, D_8028DB58);
        }
      }
      err += d;
      ex += a2b;
      y++;
      d += r[0] * r[0];
      err += d;
      if (err > 0) {
        err -= sq.b2 * x;
        x--;
        ey -= b2b;
        err -= sq.b2 * x;
      }
      if (ey < ex) {
        break;
      }
    }
  }
}

/* WHAT IT DOES: Paint a filled disc of radius r in the chosen colour
 * centred on (x, y) -- in texels, or in screen pixels over the paint area
 * when asked (a quarter scale, y flipped) -- as horizontal spans from a
 * midpoint circle walk.
 * Each span's start and end are their own locals (s, e), set before the
 * loop, so IDO keeps the start in a register and spills the end as the ROM
 * does; the half-widths are read as (a >> 1) and (b >> 1) at each use; the
 * walk steps err += a++ and err -= b--; the screen conversion is written on
 * one line (x then y), which orders the two stores y first as in the ROM. */
/* @implements 0x8025159C tgr BrPaintDisc */
void BrPaintDisc(int x, int y, int r, unsigned char screen)
{
  int i;
  int e;
  int unused;                   /* declared, never used: the frame holds it */
  int s;
  int a;
  int b;
  int err;

  if (screen) {
    x = (x - D_8028DB94.x) >> 2; y = (D_8028DB94.y + D_8028DB94.h - y) >> 2;
  }
  b = r * 2;
  a = 0;
  err = -b;
  for (; a <= b;) {
    if (!(a & 1)) {
      s = x - (a >> 1);
      e = x + (a >> 1);
      for (i = s; i <= e; i++) {
        BrPaintPlot(i, y + (b >> 1), D_8028DB58);
      }
      s = x - (b >> 1);
      e = x + (b >> 1);
      for (i = s; i <= e; i++) {
        BrPaintPlot(i, y + (a >> 1), D_8028DB58);
      }
      s = x - (a >> 1);
      e = x + (a >> 1);
      for (i = s; i <= e; i++) {
        BrPaintPlot(i, y - (b >> 1), D_8028DB58);
      }
      s = x - (b >> 1);
      e = x + (b >> 1);
      for (i = s; i <= e; i++) {
        BrPaintPlot(i, y - (a >> 1), D_8028DB58);
      }
    }
    err += a++;
    if (err >= 0) {
      err -= b--;
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
 * y1 - 1 for the two stubs.  on is a byte toggled by an increment and a
 * mask.  t is first set to the vertical dash bound and never read with it:
 * that assignment numbers the y1 - 4 bound before x1 - 4, and the two tie
 * for the last saved registers, so it gives them the ROM's s6/s7. */
/* @implements 0x80251CD4 tgr BrPaintDashRect */
void BrPaintDashRect(int x0, int y0, int x1, int y1)
{
  int x;
  unsigned char on;
  int y;
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
  t = y1 - 4;
  for (x = x0; x < x1 - 4; x += 4) {
    on++;
    on &= 1;
    c = on ? D_8028DAB8 : D_8028DABC;
    BrFillRect(x, y0, 4, 1, c, c, c);
  }
  for (y = y0; y < y1 - 4; y += 4) {
    on++;
    on &= 1;
    c = on ? D_8028DAB8 : D_8028DABC;
    BrFillRect(x1, y, 1, 4, c, c, c);
  }
  t = y1 - 1;
  BrFillRect(x1, t, 1, 2, c, c, c);
  for (x = x0; x < x1 - 4; x += 4) {
    on++;
    on &= 1;
    c = on ? D_8028DAB8 : D_8028DABC;
    BrFillRect(x, y1, 4, 1, c, c, c);
  }
  for (y = y0; y < y1 - 4; y += 4) {
    on++;
    on &= 1;
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
 * Source facts: the corners stay in the parameters, and after the edges
 * they are moved in place to the corner centres (x0 += r, x1 -= r,
 * y0 += r, y1 -= r) for the walk.  The edge loops name their bounds as
 * y0 + r .. y1 - r (x0 + r .. x1 - r), and the dash toggle is on++;
 * on &= 1;. */
/* @implements 0x80251F68 tgr BrPaintDashRoundRect */
void BrPaintDashRoundRect(int x0, int y0, int x1, int y1)
{
  int t;
  int r;
  int x;
  int y;
  int a;
  int b;
  int err;
  int ha;
  int hb;                       /* declared, never used: the frame holds it */
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
  for (y = y0 + r; y <= y1 - r; y += 4) {
    on++;
    on &= 1;
    c = on ? D_8028DAB8 : D_8028DABC;
    BrFillRect(x0, y, 1, 4, c, c, c);
  }
  for (y = y0 + r; y <= y1 - r; y += 4) {
    on++;
    on &= 1;
    c = on ? D_8028DAB8 : D_8028DABC;
    BrFillRect(x1, y, 1, 4, c, c, c);
  }
  for (x = x0 + r; x <= x1 - r; x += 4) {
    on++;
    on &= 1;
    c = on ? D_8028DAB8 : D_8028DABC;
    BrFillRect(x, y0, 4, 1, c, c, c);
  }
  for (x = x0 + r; x <= x1 - r; x += 4) {
    on++;
    on &= 1;
    c = on ? D_8028DAB8 : D_8028DABC;
    BrFillRect(x, y1, 4, 1, c, c, c);
  }
  x0 += r;
  x1 -= r;
  y0 += r;
  y1 -= r;
  b = r * 2;
  a = 0;
  err = -b;
  while (a <= b) {
    if (!(a & 1)) {
      on++;
      on %= 8;
      c = on < 4 ? D_8028DAB8 : D_8028DABC;
      ha = a >> 1;
      if ((x0 - ha) & 1) {
        BrFillPoint(x0 - ha, y0 - (b >> 1), c);
      }
      if ((x0 - ha) & 1) {
        BrFillPoint(x0 - ha, (b >> 1) + y1, c);
      }
      if ((ha + x1) & 1) {
        BrFillPoint(ha + x1, (b >> 1) + y1, c);
      }
      if ((ha + x1) & 1) {
        BrFillPoint(ha + x1, y0 - (b >> 1), c);
      }
      if ((ha + y1) & 1) {
        BrFillPoint((b >> 1) + x1, ha + y1, c);
      }
      if ((y0 - ha) & 1) {
        BrFillPoint((b >> 1) + x1, y0 - ha, c);
      }
      if ((y0 - ha) & 1) {
        BrFillPoint(x0 - (b >> 1), y0 - ha, c);
      }
      if ((ha + y1) & 1) {
        BrFillPoint(x0 - (b >> 1), ha + y1, c);
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
 * every 8 frames).
 * Source facts: the corners are swapped when x1 - x0 (y1 - y0) is negative,
 * with no width variable, and the centres are named once (cx, cy) and
 * propagated: that numbers the centre expressions ahead of the walk's x >> 1,
 * so the second walk's centre (10, tied with x >> 1) is coloured first and
 * stays in a3, and the dash counter n, written n++; n %= 8;, gets $fp.  The
 * radii stay in memory.  The squares are chains (b2 = b2b = ry * ry), so the
 * walks strength-reduce b2 * x and a2 * y with the step recomputed as ry * ry;
 * the squares come before y = ry * 2; x = 0; as in BrPaintFrameOval.  The
 * second walk is an explicit guard on a2 * y <= x * b2 around a do-while. */
/* @implements 0x802523CC tgr BrPaintDashOval */
void BrPaintDashOval(int x0, int y0, int x1, int y1)
{
  unsigned char c;
  int ry;
  int rx;
  int t;
  int cx;
  int cy;
  int x;
  int y;
  int err;
  int b2;
  int a2;
  int b2b;
  int a2b;
  int n;

  n = 0;
  if (x1 - x0 < 0) {
    t = x0;
    x0 = x1;
    x1 = t;
  }
  if (y1 - y0 < 0) {
    t = y0;
    y0 = y1;
    y1 = t;
  }
  cx = (x0 + x1) >> 1;
  cy = (y0 + y1) >> 1;
  ry = (y1 - y0) >> 1 >= 2 ? (y1 - y0) >> 1 : 1;
  rx = (x1 - x0) >> 1 >= 2 ? (x1 - x0) >> 1 : 1;
  b2 = b2b = ry * ry;
  a2 = a2b = rx * rx;
  y = ry * 2;
  x = 0;
  err = -a2 * y;
  if ((++D_8028DBB0 & 7) == 0) {
    BrSwapBytes((char *)&D_8028DAB8, (char *)&D_8028DABC);
  }
  while (x * b2b <= a2b * y) {
    if (!(x & 1)) {
      n++;
      n %= 8;
      c = n < 4 ? D_8028DAB8 : D_8028DABC;
      if (((x >> 1) + cx) & 1) {
        BrFillPoint((x >> 1) + cx, (y >> 1) + cy, c);
      }
      if (((x >> 1) + cx) & 1) {
        BrFillPoint((x >> 1) + cx, cy - (y >> 1), c);
      }
      if ((cx - (x >> 1)) & 1) {
        BrFillPoint(cx - (x >> 1), cy - (y >> 1), c);
      }
      if ((cx - (x >> 1)) & 1) {
        BrFillPoint(cx - (x >> 1), (y >> 1) + cy, c);
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
  y = 0;
  x = rx * 2;
  err = -b2 * x;
  if (a2 * y <= x * b2) {
    do {
      if (!(y & 1)) {
        n++;
        n %= 8;
        c = n < 4 ? D_8028DAB8 : D_8028DABC;
        if (((y >> 1) + cy) & 1) {
          BrFillPoint((x >> 1) + cx, (y >> 1) + cy, c);
        }
        if ((cy - (y >> 1)) & 1) {
          BrFillPoint((x >> 1) + cx, cy - (y >> 1), c);
        }
        if ((cy - (y >> 1)) & 1) {
          BrFillPoint(cx - (x >> 1), cy - (y >> 1), c);
        }
        if (((y >> 1) + cy) & 1) {
          BrFillPoint(cx - (x >> 1), (y >> 1) + cy, c);
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
    } while (a2b * y <= x * b2b);
  }
}

/* WHAT IT DOES: Draw a dashed circle outline of radius r around a screen
 * point, clipped to the paint area: a midpoint walk at double resolution,
 * on every other step the eight octant points on odd pixels only, in dashes
 * of the two dash colours (swapped every 8 frames), four points each.
 * Source facts: each point names its coordinates first (px, py) and then
 * tests the parity, the clip window and plots; the dash counter is n++;
 * n %= 8;, and x = 0 comes before d = -y.  Naming the coordinates numbers each point's x and y
 * expressions ahead of its parity, which gives the parity temps the ROM's
 * spill slots (the parities of points 1-2, 3-4 and 5/8 share 0x4c, the
 * parity of points 6-7 uses 0x44). */
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
  int px;
  int d;
  int py;

  ax = D_8028DB94.x;
  ay = D_8028DB94.y;
  ax2 = D_8028DB94.x + D_8028DB94.w;
  ay2 = D_8028DB94.y + D_8028DB94.h;
  n = 0;
  if ((++D_8028DBB0 & 7) == 0) {
    BrSwapBytes((char *)&D_8028DAB8, (char *)&D_8028DABC);
  }
  y = r * 2;
  x = 0;
  d = -y;
  while (x <= y) {
    if ((x & 1) == 0) {
      n++;
      n %= 8;
      c = n < 4 ? D_8028DAB8 : D_8028DABC;
      px = (x >> 1) + cx;
      py = (y >> 1) + cy;
      if ((px & 1) && px >= ax && px < ax2 && py >= ay && py < ay2) {
        BrFillPoint(px, py, c);
      }
      px = (x >> 1) + cx;
      py = cy - (y >> 1);
      if ((px & 1) && px >= ax && px < ax2 && py >= ay && py < ay2) {
        BrFillPoint(px, py, c);
      }
      px = cx - (x >> 1);
      py = cy - (y >> 1);
      if ((px & 1) && px >= ax && px < ax2 && py >= ay && py < ay2) {
        BrFillPoint(px, py, c);
      }
      px = cx - (x >> 1);
      py = (y >> 1) + cy;
      if ((px & 1) && px >= ax && px < ax2 && py >= ay && py < ay2) {
        BrFillPoint(px, py, c);
      }
      px = (y >> 1) + cx;
      py = (x >> 1) + cy;
      if ((py & 1) && px >= ax && px < ax2 && py >= ay && py < ay2) {
        BrFillPoint(px, py, c);
      }
      px = (y >> 1) + cx;
      py = cy - (x >> 1);
      if ((py & 1) && px >= ax && px < ax2 && py >= ay && py < ay2) {
        BrFillPoint(px, py, c);
      }
      px = cx - (y >> 1);
      py = cy - (x >> 1);
      if ((py & 1) && px >= ax && px < ax2 && py >= ay && py < ay2) {
        BrFillPoint(px, py, c);
      }
      px = cx - (y >> 1);
      py = (x >> 1) + cy;
      if ((py & 1) && px >= ax && px < ax2 && py >= ay && py < ay2) {
        BrFillPoint(px, py, c);
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
      if (D_8028CF00.kind != 0) {
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
void BrPaintExitPrompt()
{
  char unused[28];             /* declared, never used: the frame holds it */
  unsigned int *pad;
  int y;
  int bx;

  if (D_8028DBE8 == 0) {
    if (D_8028DBB4 <= 0) {
      D_8028DBE8 = 1;
      BrFadeTo(0, 0.2f);
    } else {
      y = 285 - D_8028D0B0.w;
      bx = 361 - D_8028D0E0.w;
      BrBevelPanel(0xbf, 0xbb, 0x102, 0x6a, 3, 0, 0, 0x80, 0x80, 0x80);
#if TGR_REGION_JPN
      BrTextHighlightOff();
      BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xca, 0);
      BrTextAlignCentre();
      BrTextSetFont(14);
      BrTextPrint("%j\xB2(\xAC" "bZD", D_8028AAB0 / 2, 109);
      BrTextPrint("EXIT%jB6R1%%", D_8028AAB0 / 2, 123);
      BrTextAlignLeft();
#else
#if TGR_REGION_EUR
      BrTextHighlightOff();
      BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xca, 0);
      BrTextAlignLeft();
      BrTextSetFont(12);
#else
      BrTextSetFont(12);
      BrTextAlignLeft();
      BrTextHighlightOff();
      BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xca, 0);
#endif
      BrTextPrint("CHANGES NOT SAVED.", 103, 109);
      BrTextPrint("EXIT ANYWAY?", 103, 123);
#endif
      BrTextSetFont(10);
      BrTextPrint("%wwYES", (D_8028D0B0.drawW + 249) >> 1, (y + 18) >> 1);
      BrTextPrint("%wwNO", (bx + D_8028D0E0.drawW + 8) >> 1, (y + 18) >> 1);
      BrImageDrawAt(&D_8028D0B0, 243, y);
      BrImageDrawAt(&D_8028D0E0, bx, y);
      pad = (unsigned int *)((char *)D_8036A8E0 + D_8028DBBC * 0x15c);
      if (*pad & 0x10) {
        BrPadConsume(pad, 0x10);
        D_8028DBE8 = 1;
        BrFadeTo(0, 0.2f);
      } else if (*pad & 0x20) {
        BrPadConsume(pad, 0x20);
        D_8028DB60 = D_8028DB64;
      }
    }
  }
}

/* WHAT IT DOES: Take the chosen preset's rectangle, and make every view
 * slot (previous, current, saved) the current view. */
/* @implements 0x802533F4 tgr BrPaintPresetApply */
void BrPaintPresetApply()
{
  D_8028DAC4 = D_8028D4A0[D_8028DAC0];
  D_8028DB6C = D_8028DB68;
  D_8028DB70 = D_8028DB68;
  D_8028DB74 = D_8028DB68;
}

void *D_8028DD14[3] = { 0, (void *)BrPaintPresetApply, 0 };
