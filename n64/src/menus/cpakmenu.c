/* cpakmenu.c -- the Controller Pak menu
 */
#include "tgr/common.h"
#include "tgr/gbi.h"
#include "tgr/pad.h"

/* -- declarations -- */
typedef struct BrImage {        /* as drawing/image.c (0x30 bytes) */
  unsigned char *data;
  int x4;
  int x8;
  unsigned char siz;
  char pad0d[3];
  int w;                        /* 0x10 */
  unsigned int h;               /* 0x14 */
  unsigned int stripH;          /* 0x18 */
  int x;                        /* 0x1C */
  int y;                        /* 0x20 */
  char pad24[0x30 - 0x24];
} BrImage;
typedef struct OSPfs {          /* libultra's Controller Pak handle (0x68 bytes) */
  int status;
  void *queue;
  int channel;
  unsigned char id[32];         /* 0x0C */
  char pad2c[0x68 - 0x2C];
} OSPfs;
typedef struct OSPfsState {     /* libultra's note directory entry (0x20 bytes) */
  unsigned int size;
  unsigned int game;            /* 0x04 */
  unsigned short company;       /* 0x08 */
  unsigned char ext[4];         /* 0x0A */
  unsigned char name[16];       /* 0x0E */
  char pad1e[2];
} OSPfsState;
extern short D_802A4BE8;
extern Gfx *D_8028A858;                 /* the display list */
extern int D_802724F0;                  /* the last Controller Pak status */
extern char D_80272D48[];               /* the serial message queue */
extern int D_8028AAB0;                  /* the screen width */
extern int D_8028AAB4;                  /* and height */
extern BrImage D_8028CB40;              /* the backdrop */
extern BrImage D_8028CB70;              /* the title panel */
extern BrImage D_8028D0B0;              /* the A button */
extern BrImage D_8028D0E0;              /* the B button */
extern BrImage D_8028DD20;              /* the START button */
extern unsigned char D_8028DD94;        /* the manager is running */
extern unsigned short D_8028DD98;       /* one bit per empty note */
extern OSPfs D_80369EC0[4];
extern OSPfs D_8031A3F8[4];
extern unsigned char D_803163E0[];      /* the pak's id when it was last read */
static unsigned char D_8036A060;        /* the note list has been read */
static unsigned char D_8036A061;        /* the note under the cursor */
static unsigned char D_8036A062;        /* the first note in use */
static unsigned char D_8036A063;        /* the last note in use */
static int D_8036A064;                  /* notes in use */
static int D_8036A068;                  /* free bytes */
static OSPfsState D_8036A070[16];       /* the notes */
static unsigned char D_8036A270;        /* the manager's state */
void BrFrontReturnToTitle(void);
void BrZBufferClear(void);
void BrScreenClear(int r, int g, int b);
void BrFrameBeginLayout1(void);
void BrViewportSet(int x, int y, int w, int h, int scissor);
void BrFrameEnd(void);
void BrMenuCameraSet(float w, float h);
void BrClockTick(void);
void BrModeSet(void (*fn)(void));
void BrFadeTo(float dir, float speed);
int BrFadeOutDone(void);
int BrFadeInDone(void);
void BrFadeBarsDraw(void);
void BrFadeStep(void);
void BrTextHighlightOff(void);
void BrTextAlignCentre(void);
void BrTextAlignLeft(void);
void BrTextAlignRight(void);
void BrTextSetColours(int r, int g, int b, int r2, int g2, int b2);
void BrTextSetFont(int font);
void BrTextPrint(char *s, int x, int y);
void BrIfaceMemReset(void);
void BrAllocPaintShopGfxMem(BrImage *img);
void BrImageDrawAt(BrImage *img, int x, int y);
void BrImageDrawTinted(BrImage *img, unsigned char r, unsigned char g, unsigned char b);
void BrImageDrawRect(BrImage *img, int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b);
void func_80246F90(int x, int y, int w, int h, int a, int b, int c, int r, int g, int bl);
char BrPaintCharset(unsigned char i);
void BrPadConsume(BrPadRec *pad, unsigned int bits);
void BrPadStickToButtons(BrPadRec *pad);
void osViBlack(unsigned char active);
int sprintf(char *buf, char *fmt, ...);
int osPfsIsPlug(void *mq, unsigned char *pattern);
int osPfsInitPak(void *mq, OSPfs *pfs, int channel);
int osMotorInit(void *mq, OSPfs *pfs, int channel);
int osPfsRepairId(OSPfs *pfs);
int osPfsChecker(OSPfs *pfs);
int osPfsFreeBlocks(OSPfs *pfs, int *bytes);
void bcopy(void *src, void *dst, int n);
int osPfsNumFiles(OSPfs *pfs, int *maxFiles, int *used);
int osPfsFileState(OSPfs *pfs, int file, OSPfsState *state);
int osPfsDeleteFile(OSPfs *pfs, unsigned short company, unsigned int game, unsigned char *name,
                    unsigned char *ext);
extern int D_8028A850;                  /* hi-res screen */
extern int D_8028A85C;                  /* the frame buffer being drawn */
extern unsigned int D_8031AA28[];       /* the frame buffers */
extern int D_8028AAB0;                  /* the screen width */
extern int D_8028DDD0;                  /* the debug text's x */
extern int D_8028DDD4;                  /* and y */
extern int D_8028DDD8;                  /* use the small font */
extern short D_8028DDDC;       /* the text colour */
extern short D_8028DFC0[][7][5];   /* the 5 by 7 font */
extern short D_8028F2A0[][7][3];   /* the 3 by 7 font, its own colours */

/* -- end declarations -- */

/* WHAT IT DOES: The Controller Pak manager, one frame of it: on the first
 * frame check a pak is in (else back to the title), load the menu's images
 * and fade in; then draw the sixteen note slots with each used note's name,
 * extension and size in pages, and run the state machine -- 0 initialise a
 * newly inserted pak, 1 read the notes and free space, 2 move the cursor
 * over the used notes (up/down), A to ask to delete, START to leave, 3 the
 * "DELETE GAME NOTE?" box, 4 the pak error box, 5 delete the note and move
 * the cursor to a neighbouring one, 6 the new-pak box, 7 fade out -- and
 * finally the pages used/free line and the button prompts.  Once faded out
 * blank two frames and go back to the title.
 * RESIDUE: the ROM re-forms each note-state global's address with lui at
 * every access and keeps y, x and the note pointer in a larger frame (0xC8);
 * ours hoists those addresses into saved registers. */
/* @t4-pass 0x802534DC 1 2026-10-03 compiles 31 best 1055 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x802534DC 2 2026-10-03 compiles 30 best 1055 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x802534DC tgr BrPakManager */
void BrPakManager(void)
{
  int y;
  int i;
  int k;
  int w;
  int x;
  int by;
  int ty;
  int bx;
  char c;
  OSPfsState *st;
  int maxFiles;
  unsigned char pattern;
  char num[11];
  unsigned char name[28];

  D_802A4BE8 = 0;
  if (D_8028DD94 == 0) {
    D_8028DD94 = 1;
    D_8036A060 = 0;
    osPfsIsPlug(D_80272D48, &pattern);
    if ((pattern & 1) == 0) {
      BrModeSet(BrFrontReturnToTitle);
      D_8028DD94 = 0;
      D_802A4BE8 = 1;
      return;
    }
    BrIfaceMemReset();
    BrAllocPaintShopGfxMem(&D_8028CB40);
    BrAllocPaintShopGfxMem(&D_8028CB70);
    BrAllocPaintShopGfxMem(&D_8028D0B0);
    BrAllocPaintShopGfxMem(&D_8028D0E0);
    BrAllocPaintShopGfxMem(&D_8028DD20);
    D_802724F0 = 0;
    D_8036A270 = 1;
    BrFadeTo(1.0f, 0.2f);
  }
  BrClockTick();
  BrFadeStep();
  BrFrameBeginLayout1();
  BrMenuCameraSet(D_8028AAB0, D_8028AAB4);
  BrViewportSet(0, 0, D_8028AAB0, D_8028AAB4, 1);
  BrZBufferClear();
  BrScreenClear(0, 0, 0);
  BrImageDrawTinted(&D_8028CB40, 0x50, 8, 0x80);
  BrImageDrawRect(&D_8028CB70, D_8028CB70.x + 0x18, D_8028CB70.y + 2, 0x208, 0x41, 0x80, 0x80, 0x80);
  BrTextAlignCentre();
  BrTextHighlightOff();
  BrTextSetFont(22);
  BrTextPrint("%ryCONTROLLER PAK MENU", 0xa0, 0x20);
  BrTextAlignLeft();
  BrTextSetFont(12);
  BrTextPrint("%yw#", 0x48, 0x34);
  BrTextPrint("%ywGAME NOTE", 0x74, 0x34);
  BrTextAlignRight();
  BrTextPrint("%ywPAGES", 0xfa, 0x34);
  BrTextSetFont(9);
  y = 0x3f;
  for (i = 0; i < 16; i++) {
    sprintf(num, "%d", i + 1);
    if (D_8036A064 != 0 && i == D_8036A061) {
      BrTextSetColours(0x80, 0xff, 0x80, 0, 0xff, 0);
    } else {
      BrTextSetColours(0xff, 0xff, 0xff, 0x80, 0x80, 0x80);
    }
    BrTextPrint(num, 0x52, y);
    y += 9;
  }
  if (D_8036A060 != 0 && D_8036A270 != 4) {
    for (i = 0; i < 16; i++) {
      if ((D_8028DD98 & (1 << i)) == 0) {
        y = i * 9 + 0x3f;
        st = &D_8036A070[i];
        for (k = 0; k < 16; k++) {
          name[k] = BrPaintCharset(D_8036A070[i].name[k]);
          if (name[k] == 0) {
            break;
          }
        }
        c = BrPaintCharset(st->ext[0]);
        if (c != 0) {
          name[k] = ' ';
          name[k + 1] = '.';
          name[k + 2] = c;
          name[k + 3] = 0;
        } else {
          name[k] = 0;
        }
        BrTextAlignLeft();
        if (D_8036A064 != 0 && i == D_8036A061) {
          BrTextSetColours(0x80, 0xff, 0x80, 0, 0xff, 0);
        } else {
          BrTextSetColours(0xff, 0xff, 0xff, 0x80, 0x80, 0x80);
        }
        BrTextPrint((char *)name, 0x62, y);
        sprintf(num, "%d", st->size >> 8);
        BrTextAlignRight();
        BrTextPrint(num, 0xf4, y);
      }
    }
  }
  switch (D_8036A270) {
  case 0:
    D_802724F0 = osPfsInitPak(D_80272D48, &D_80369EC0[0], 0);
    if (D_802724F0 != 0 && D_802724F0 == 10) {
      if (osMotorInit(D_80272D48, &D_8031A3F8[0], 0) == 0) {
        D_802724F0 = 0;
        BrModeSet(BrFrontReturnToTitle);
        D_802A4BE8 = 1;
        return;
      }
      D_802724F0 = osPfsRepairId(&D_80369EC0[0]);
      if (D_802724F0 == 0) {
        osPfsInitPak(D_80272D48, &D_80369EC0[0], 0);
      }
    }
    if (D_802724F0 == 0) {
      bcopy(D_80369EC0[0].id, D_803163E0, 0x20);
      D_8036A270 = 1;
    } else {
      D_8036A270 = 4;
    }
    break;
  case 1:
    D_802724F0 = osPfsNumFiles(&D_80369EC0[0], &maxFiles, &D_8036A064);
    if (D_802724F0 != 0) {
      if (D_802724F0 == 3) {
        if (osPfsChecker(&D_80369EC0[0]) != 0) {
          D_8036A270 = 4;
          break;
        }
      } else {
        D_8036A270 = 4;
        break;
      }
    }
    for (i = 0; i < 16; i++) {
      if (osPfsFileState(&D_80369EC0[0], i, &D_8036A070[i]) != 0) {
        D_8028DD98 |= 1 << i;
      }
    }
    D_8036A062 = 0;
    for (i = 0; i < 16; i++) {
      if ((D_8028DD98 & (1 << i)) == 0) {
        D_8036A062 = i;
        break;
      }
    }
    D_8036A063 = 0;
    for (i = 0; i < 16; i++) {
      if ((D_8028DD98 & (1 << i)) == 0) {
        D_8036A063 = i;
      }
    }
    D_802724F0 = osPfsFreeBlocks(&D_80369EC0[0], &D_8036A068);
    if (D_802724F0 != 0) {
      if (D_802724F0 == 3) {
        if (osPfsChecker(&D_80369EC0[0]) != 0) {
          D_8036A270 = 4;
          break;
        }
      } else {
        D_8036A270 = 4;
        break;
      }
    }
    D_8036A060 = 1;
    D_8036A270 = 2;
    D_8036A061 = D_8036A062;
    break;
  case 2:
    BrPadStickToButtons(&D_8036A8E0[0]);
    if (D_8036A8E0[0].pressed & 8) {
      BrPadConsume(&D_8036A8E0[0], 8);
      if (D_8036A064 != 0 && D_8036A062 != D_8036A061) {
        D_8036A061--;
        while ((D_8028DD98 & (1 << D_8036A061)) && D_8036A062 != D_8036A061) {
          D_8036A061--;
        }
      }
    } else if (D_8036A8E0[0].pressed & 2) {
      BrPadConsume(&D_8036A8E0[0], 2);
      if (D_8036A064 != 0 && D_8036A063 != D_8036A061) {
        D_8036A061++;
        while ((D_8028DD98 & (1 << D_8036A061)) && D_8036A063 != D_8036A061) {
          D_8036A061++;
        }
      }
    } else if (D_8036A8E0[0].pressed & 0x10) {
      BrPadConsume(&D_8036A8E0[0], 0x10);
      if (D_8036A064 != 0 && BrFadeInDone()) {
        D_8036A270 = 3;
      }
    } else if ((D_8036A8E0[0].pressed & 0x4000) && BrFadeInDone()) {
      BrPadConsume(&D_8036A8E0[0], 0x4000);
      D_8036A270 = 7;
    }
    break;
  case 3:
    if (D_8036A061 + 1 < 10) {
      w = 0x128;
    } else {
      w = 0x134;
    }
    x = (0x280 - w) >> 1;
    by = 0x110 - D_8028D0B0.h;
    func_80246F90(x, 200, w, 0x50, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextAlignCentre();
    BrTextHighlightOff();
    BrTextSetFont(12);
    sprintf((char *)name, "%%ywDELETE GAME NOTE #%d?", D_8036A061 + 1);
    BrTextPrint((char *)name, 0x9f, 0x73);
    BrTextAlignLeft();
    BrTextSetFont(10);
    ty = (by + 0x12) >> 1;
    BrTextPrint("%wwOK", (x + 0x38 + D_8028D0B0.w + 7U) >> 1, ty);
    bx = x + w - 0x9c;
    BrTextPrint("%wwCANCEL", (bx + D_8028D0E0.w + 7U) >> 1, ty);
    BrImageDrawAt(&D_8028D0B0, x + 0x38, by);
    BrImageDrawAt(&D_8028D0E0, bx, by);
    if (D_8036A8E0[0].pressed & 0x10) {
      BrPadConsume(&D_8036A8E0[0], 0x10);
      D_8036A270 = 5;
    } else if (D_8036A8E0[0].pressed & 0x20) {
      BrPadConsume(&D_8036A8E0[0], 0x20);
      D_8036A270 = 2;
    }
    break;
  case 4:
    func_80246F90(0xb1, 0xcb, 0x11e, 0x4a, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextAlignCentre();
    BrTextHighlightOff();
    BrTextSetFont(12);
    BrTextPrint("%ywCONTROLLER PAK ERROR", 0x9f, 0x75);
    BrTextPrint("%ywHAS BEEN DETECTED", 0x9f, 0x83);
    if (D_8036A8E0[0].pressed & 0x4030) {
      BrPadConsume(&D_8036A8E0[0], 0x4030);
      D_8036A270 = 7;
    }
    D_8036A068 = 0;
    maxFiles = 16;
    D_8036A064 = 0;
    break;
  case 5:
    st = &D_8036A070[D_8036A061];
    D_802724F0 = osPfsDeleteFile(&D_80369EC0[0], st->company, st->game, st->name, st->ext);
    if (D_802724F0 == 0) {
      D_8028DD98 |= 1 << D_8036A061;
      D_8036A070[D_8036A061].name[0] = 0;
      D_8036A070[D_8036A061].ext[0] = 0;
      D_8036A068 += D_8036A070[D_8036A061].size;
      D_8036A070[D_8036A061].size = 0;
      if (D_8036A064 > 0) {
        D_8036A064--;
      }
      if (D_8036A063 == D_8036A061) {
        D_8036A063--;
        while ((D_8028DD98 & (1 << D_8036A063)) && D_8036A063 != 0) {
          D_8036A063--;
        }
        D_8036A061--;
        while ((D_8028DD98 & (1 << D_8036A061)) && D_8036A061 != 0) {
          D_8036A061--;
        }
      } else {
        if (D_8036A062 == D_8036A061) {
          D_8036A062++;
          while ((D_8028DD98 & (1 << D_8036A062)) && D_8036A063 != D_8036A062) {
            D_8036A062++;
          }
        }
        D_8036A061++;
        while ((D_8028DD98 & (1 << D_8036A061)) && D_8036A063 != D_8036A061) {
          D_8036A061++;
        }
      }
      D_8036A270 = 2;
    } else if (D_802724F0 == 2) {
      D_8036A270 = 6;
    } else {
      D_8036A270 = 4;
    }
    break;
  case 6:
    BrTextHighlightOff();
    BrTextAlignLeft();
    BrTextSetFont(12);
    func_80246F90(0xaf, 0xb5, 0x122, 0x89, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextPrint("%ywA NEW CONTROLLER PAK", 0x5f, 0x6a);
    BrTextPrint("%ywWAS INSERTED.", 0x5f, 0x77);
    BrTextPrint("%ywTHIS CONTROLLER PAK", 0x5f, 0x8a);
    BrTextPrint("%ywWILL BE USED.", 0x5f, 0x97);
    if (D_8036A8E0[0].pressed & 0x8030) {
      BrPadConsume(&D_8036A8E0[0], 0x8030);
      D_8036A060 = 0;
      D_802724F0 = 0;
      D_8036A270 = 0;
    }
    break;
  case 7:
    BrFadeTo(0.0f, 0.2f);
    D_8036A270 = 8;
    break;
  }
  BrTextAlignLeft();
  BrTextSetFont(10);
  BrTextPrint("%ywPAGES USED:", 0x47, 0xd3);
  sprintf(num, "%%yw%d", 0x7b - (D_8036A068 >> 8));
  BrTextPrint(num, 0x85, 0xd3);
  BrTextPrint("%ywPAGES FREE:", 0xac, 0xd3);
  sprintf(num, "%%yw%d", D_8036A068 >> 8);
  BrTextAlignRight();
  BrTextPrint(num, 0xfa, 0xd3);
  BrImageDrawAt(&D_8028D0B0, 0xd6, 0x1c8 - D_8028D0B0.h);
  BrImageDrawAt(&D_8028DD20, 0x158, 0x1c8 - D_8028D0B0.h);
  BrTextAlignLeft();
  BrTextPrint("%wwSELECT", 0x7b, 0xe1);
  BrTextPrint("%wwEXIT", 0xbc, 0xe1);
  gDPPipeSync(D_8028A858++);
  BrFadeBarsDraw();
  BrFrameEnd();
  if (BrFadeOutDone()) {
    D_8028DD94 = 0;
    D_802724F0 = 0;
    BrFrameBeginLayout1();
    BrScreenClear(0, 0, 0);
    BrFrameEnd();
    osViBlack(1);
    BrFrameBeginLayout1();
    BrScreenClear(0, 0, 0);
    BrFrameEnd();
    BrModeSet(BrFrontReturnToTitle);
  }
  D_802A4BE8 = 1;
}

/* WHAT IT DOES: Does nothing. An empty function the retail build kept in
 * the Controller Pak menu code. */
/* @implements 0x80254870 tgr BrStub80254870 */
void BrStub80254870(void)
{
}

/* WHAT IT DOES: Print a string straight into the frame buffer shown last
 * (the debug text): each glyph, 5 by 7 (or 3 by 7 in the small font, whose
 * entries are their own colours), is drawn one pixel right and down of a
 * black 3 by 3 block around each of its pixels, in the text colour; a
 * newline moves down 8 rows (9 on a hi-res screen) and sets the next
 * line's x to 16 (64 hi-res); lower case prints as upper case, and
 * anything else unprintable as a space.  The frame buffer, fonts and colour
 * are shorts (the ROM re-reads the first store of each block with lh).
 * RESIDUE (308): the ROM keeps the column in a stack home and orders the
 * block stores row by row; ours holds more in saved registers.
 *
 * NEVER RUN IN THE RETAIL GAME: nothing in the ROM refers to 0x80254878 -- no
 * jal to it, no lui/addiu pair forming its address (n64rom xref: none), and
 * no data word holding it (the whole ROM searched for the value). */
/* @implements 0x80254878 tgr BrDebugPrint */
void BrDebugPrint(unsigned char *s)
{
  short col;
  int w;
  unsigned char c;
  short *p;
  int x0;
  unsigned int n;
  short *g;
  int i;

  col = D_8028DDDC;
  w = D_8028AAB0 << D_8028A850;
  c = *s;
  p = (short *)D_8031AA28[D_8028A85C ^ 1] + D_8028DDD4 * w + D_8028DDD0;
  x0 = D_8028DDD0;
  for (;;) {
    if (c == 0) {
      D_8028DDD0 = x0;
      return;
    }
    if (c < 0x20 || c > 0x7e) {
      n = 0;
      if (c != '\n') {
        goto draw;
      }
      x0 = 16 << (D_8028A850 << 1);
      D_8028DDD4 += D_8028A850 + 8;
      p += (D_8028A850 + 8) * w;
    } else {
      if (c >= 'a' && c <= 'z') {
        n = (unsigned char)(c - 0x40);
      } else {
        if (c > 'z' && c < 0x7f) {
          c -= 0x1a;
        }
        n = (unsigned char)(c - 0x20);
      }
    draw:
#define DOT(k) \
      if (g[k]) { \
        p[k + 2] = p[k + 1] = p[k] = p[w + k + 2] = p[w + k] = p[w * 2 + k] = p[w * 2 + k + 1] = p[w * 2 + k + 2] = 1; \
      }
      if (D_8028DDD8 == 0) {
        g = D_8028DFC0[n][0];
        for (i = 0; i != 8; i++, p += w, g += 5) {
          if (i < 7) {
            DOT(0)
            DOT(1)
            DOT(2)
            DOT(3)
            DOT(4)
          }
          if (i != 0) {
            if (g[-5]) {
              p[1] = col;
            }
            if (g[-4]) {
              p[2] = col;
            }
            if (g[-3]) {
              p[3] = col;
            }
            if (g[-2]) {
              p[4] = col;
            }
            if (g[-1]) {
              p[5] = col;
            }
          }
        }
        p -= w * 8 - D_8028A850 - 6;
      } else {
        g = D_8028F2A0[n][0];
        for (i = 0; i != 8; i++, p += w, g += 3) {
          if (i < 7) {
            DOT(0)
            DOT(1)
            DOT(2)
          }
          if (i != 0) {
            if (g[-3]) {
              p[1] = g[-3];
            }
            if (g[-2]) {
              p[2] = g[-2];
            }
            if (g[-1]) {
              p[3] = g[-1];
            }
          }
        }
        p -= w * 8 - D_8028A850 - 4;
      }
#undef DOT
    }
    c = *++s;
  }
}

/* WHAT IT DOES: Does nothing. A second empty function in the Controller Pak
 * menu code. */
/* @implements 0x80254F2C tgr BrStub80254F2C */
void BrStub80254F2C(void)
{
}
