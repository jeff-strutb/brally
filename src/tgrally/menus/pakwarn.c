/* pakwarn.c -- the boot-time warning when the Controller Pak is missing or
 * holds another game's data
 */
#include "tgr/common.h"
#include "tgr/gbi.h"
#include "tgr/pad.h"

/* -- declarations -- */
void BrIfaceMemReset(void);
void osViBlack(int black);
void BrFrameBeginLayout1(void);
void func_8021AA08(void);
void BrAllocPaintShopGfxMem(void *img);
void BrFadeTo(float level, float seconds);
void BrClockTick(void);
void BrFadeStep(void);
void BrMenuCameraSet(float w, float h);
void BrViewportSet(int x, int y, int w, int h, int scissor);
void BrZBufferClear(void);
void BrScreenClear(int r, int g, int b);
void BrImageDrawTinted(void *img, unsigned char r, unsigned char g, unsigned char b);
void BrImageDrawAt(void *img, int x, int y);
void BrTextHighlightOff(void);
void BrTextAlignCentre(void);
void BrTextAlignLeft(void);
void BrTextSetFont(int font);
void BrTextPrint(char *s, int x, int y);
void func_80223A70(void);
void BrPadConsume(unsigned int *pressed, unsigned int buttons);
int BrFadeOutDone(void);
void BrScreenFlush3Layout1(void);
void BrModeSet(void (*fn)(void));
void BrMainMenu(void);
extern Gfx *D_8028A858;
extern int D_8028AAB0;
extern int D_8028AAB4;
typedef struct BrImage {        /* as drawing/image.c */
  unsigned char *data;
  int x4;
  int x8;
  unsigned char siz;
  char pad0d[3];
  int w;
  unsigned int h;
  unsigned int stripH;
  int x;                        /* 0x1C  where it is drawn */
  int y;                        /* 0x20 */
  int drawW;
  int drawH;
} BrImage;
extern BrImage D_8028CB40;              /* the backdrop */
extern BrImage D_8028CB70;              /* the warning panel */
extern BrImage D_8028D0B0;              /* the PROCEED button */
extern unsigned char D_80270840;        /* 0 no controller, 1 no pak, 2 foreign pak data */
extern unsigned short D_8028DBB0;       /* frames on this screen */
extern int D_802724F0;
/* -- end declarations -- */

/* WHAT IT DOES: The warning screen shown at boot when there is no
 * controller, no Controller Pak, or a pak full of another game's data, run
 * once per frame: on entry blank two frames and load its images, then draw
 * the backdrop, the panel and the message; START (or about 16 seconds)
 * fades out and goes on to the main menu. */
/* @implements 0x80208570 tgr BrPakWarnScreen */
void BrPakWarnScreen(void)
{
  static unsigned char entered = 0;     /* 0x80270844 */

  if (entered == 0) {
    entered = 1;
    BrIfaceMemReset();
    osViBlack(1);
    BrFrameBeginLayout1();
    func_8021AA08();
    osViBlack(1);
    BrFrameBeginLayout1();
    func_8021AA08();
    BrAllocPaintShopGfxMem(&D_8028CB40);
    BrAllocPaintShopGfxMem(&D_8028CB70);
    BrAllocPaintShopGfxMem(&D_8028D0B0);
    BrFadeTo(1.0f, 0.2f);
    D_8028DBB0 = 0;
  }
  BrClockTick();
  BrFadeStep();
  BrFrameBeginLayout1();
  BrMenuCameraSet(D_8028AAB0, D_8028AAB4);
  BrViewportSet(0, 0, D_8028AAB0, D_8028AAB4, 1);
  BrZBufferClear();
  BrScreenClear(0, 0, 0);
  BrImageDrawTinted(&D_8028CB40, 0xc0, 0, 0x40);
  BrImageDrawAt(&D_8028CB70, D_8028CB70.x, D_8028CB70.y + 8);
  BrTextHighlightOff();
  BrTextAlignCentre();
  BrTextSetFont(0x1e);
  BrTextPrint("%ryWARNING", 0xa0, 0x29);
  BrTextSetFont(0x12);
  switch (D_80270840) {
  case 0:
    BrTextPrint("%ywCONTROLLER NOT FOUND", 0xa0, 0x7c);
    break;
  case 1:
    BrTextPrint("%ywCONTROLLER PAK NOT FOUND", 0xa0, 0x69);
    BrTextPrint("%ywGAME DATA CANNOT BE SAVED", 0xa0, 0x7d);
    BrTextPrint("%ywIF YOU CHOOSE TO PROCEED", 0xa0, 0x91);
    break;
  case 2:
    BrTextPrint("%ywCONTROLLER PAK CONTAINS GAME", 0xa0, 0x55);
    BrTextPrint("%ywDATA SAVED FROM ANOTHER GAME.", 0xa0, 0x69);
    BrTextPrint("%ywTHERE MAY NOT BE ENOUGH PAGES", 0xa0, 0x7d);
    BrTextPrint("%ywAND/OR FILES FREE WHEN YOU NEED", 0xa0, 0x91);
    BrTextPrint("%ywTO SAVE GAME DATA FROM THIS GAME.", 0xa0, 0xa5);
    break;
  }
  BrImageDrawAt(&D_8028D0B0, 0xfe, 0x1b0);
  BrTextAlignLeft();
  BrTextSetFont(0xb);
  BrTextPrint("%wwPROCEED", 0x8e, D_8028AAB4 * 19 / 20 - 3);
  gDPPipeSync(D_8028A858++);
  func_80223A70();
  func_8021AA08();
  if (D_8036A8E0[0].pressed & 0x10) {
    BrPadConsume(&D_8036A8E0[0].pressed, 0x10);
    BrFadeTo(0.0f, 0.2f);
  } else if (D_8028DBB0++ == 910) {
    BrFadeTo(0.0f, 0.2f);
  }
  if (BrFadeOutDone()) {
    entered = 0;
    BrScreenFlush3Layout1();
    D_802724F0 = 0;
    D_8028DBB0 = 0;
    BrModeSet(BrMainMenu);
  }
}
