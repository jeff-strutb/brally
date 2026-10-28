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
#if TGR_REGION_JPN
void BrScreenBlank(void);
extern int D_8028A884;
#endif
/* -- end declarations -- */

/* WHAT IT DOES: The warning screen shown at boot when there is no
 * controller, no Controller Pak, or a pak full of another game's data, run
 * once per frame: on entry blank two frames and load its images, then draw
 * the backdrop, the panel and the message; START (or about 16 seconds)
 * fades out and goes on to the main menu.  Japan's messages are
 * Japanese, its button reads OK, and it blanks three frames on entry. */
/* @implements 0x80208570 tgr BrPakWarnScreen */
void BrPakWarnScreen(void)
{
  static unsigned char entered = 0;     /* 0x80270844 */

  if (entered == 0) {
    entered = 1;
#if TGR_REGION_JPN
    D_8028A884 = 1;
    BrScreenBlank();
    BrFrameBeginLayout1();
    func_8021AA08();
    BrIfaceMemReset();
    BrScreenBlank();
    BrFrameBeginLayout1();
    func_8021AA08();
    BrScreenBlank();
    BrFrameBeginLayout1();
    func_8021AA08();
#else
    BrIfaceMemReset();
    osViBlack(1);
    BrFrameBeginLayout1();
    func_8021AA08();
    osViBlack(1);
    BrFrameBeginLayout1();
    func_8021AA08();
#endif
    BrAllocPaintShopGfxMem(&D_8028CB40);
    BrAllocPaintShopGfxMem(&D_8028CB70);
    BrAllocPaintShopGfxMem(&D_8028D0B0);
    BrFadeTo(1.0f, 0.2f);
    D_8028DBB0 = 0;
#if TGR_REGION_JPN
    D_8028A884 = 0;
#endif
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
#if TGR_REGION_JPN
  BrTextSetFont(0x14);
  switch (D_80270840) {
  case 0:
    BrTextPrint(" %yw%j\xC1\xCF\xC3\xC8(\x88" "90H6b\x7F&", 0xa0, 0x7c);
    break;
  case 1:
    BrTextPrint("%yw%j \xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1" "94@tk'", 0xa0, 0x5f);
    BrTextPrint("%yw%j\xC9(\xA2\xC3\x81(\xBB(\x83s\xBD\x90\xCF\xC3\x92\xBF\x8F\xAD", 0xa0, 0x73);
    BrTextPrint("%yw %jt\xBB\x81(\xA8\xBB(\x83" "9\xB2(\xACkA6b\x7F&", 0xa0, 0x87);
    BrTextPrint("%yw%j\xB9(\xA6w5Jf6R1%%", 0xa0, 0x9b);
    break;
  case 2:
    BrTextPrint("%yw%j\xE9t\xB9(\xA6\xBB(\x83" "9", 0xa0, 0x55);
    BrTextPrint(" %yw%j\xB2(\xAC" "2hc@6R&", 0xa0, 0x69);
    BrTextPrint("%yw%j\xE1" "A\xBD(\x9A" "9\xE2" "84@tk", 0xa0, 0x7d);
    BrTextPrint("%yw%jqt\xB9(\xA6\xBB(\x83w", 0xa0, 0x91);
    BrTextPrint(" %yw%j\xB2(\xACkA6b\x7F&", 0xa0, 0xa5);
    break;
  }
  BrTextAlignLeft();
  BrTextSetFont(0xb);
  BrImageDrawAt(&D_8028D0B0, 0x116, 0x1b0);
  BrTextPrint("%wwOK", 0x9a, D_8028AAB4 * 19 / 20 - 3);
#else
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
#endif
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
