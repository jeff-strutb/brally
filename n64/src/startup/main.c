/* main.c -- the main game thread
 */
#include "tgr/common.h"

/* -- declarations -- */
void BrSchedInit(void);
void BrStub8021C740(void);
void BrStub8021C6B0(void);
void BrEntAllReset(void);
void func_8021C188(void);
void BrFadeSet(float level);
void BrModeSet(void (*fn)(void));
void func_8020686C(void);
unsigned int BrRomReadSize(int rom);
unsigned int BrRomUnpack(void *dst, char *rom, void *stream);
void BrIfaceMemReset(void);
void BrPaintShopMemInit(void);
int BrCpakCheck(int a, int b);
void BrVolumesApply(void);
void BrMusicInit(void *data, void *heap);
void osSyncPrintf(char *fmt, ...);
void BrMainLoop(void);
extern int D_80270784;
extern char D_80307F00[];
extern char D_000AD400[];
extern char D_000EBC00[];
extern char D_802AC400[];
/* -- end declarations -- */

/* WHAT IT DOES: The main game thread: bring up the scheduler, reset the
 * cars, set the first game mode (the title), unpack the front end's data,
 * wait until the controller-pak checks pass, apply the volumes, load and
 * start the music data, report the big record sizes on the debug output
 * and run the main loop for good. */
/* @implements 0x802001B4 tgr BrMainThread */
void BrMainThread(void)
{
  BrSchedInit();
  BrStub8021C740();
  BrStub8021C6B0();
  BrEntAllReset();
  func_8021C188();
  BrFadeSet(0.0f);
  BrModeSet(func_8020686C);
  D_80270784 = BrRomReadSize((int)D_000AD400);
  BrRomUnpack(D_80307F00, D_000AD400, 0);
  D_80270784 = 8;
  BrIfaceMemReset();
  BrPaintShopMemInit();
  while (!BrCpakCheck(0, 1)) {
  }
  BrIfaceMemReset();
  BrPaintShopMemInit();
  while (!BrCpakCheck(2, 1)) {
  }
  BrVolumesApply();
  BrRomUnpack((void *)0x80025C00, D_000EBC00, 0);
  BrMusicInit((void *)0x80025C00, D_802AC400);
  osSyncPrintf("sizeof(UltraCarHeader)=%d\n", 0xdf88);
  osSyncPrintf("sizeof(Vehicle)=%d\n", 0x2090);
  osSyncPrintf("sizeof(Enemy)=%d\n", 0x78);
  BrMainLoop();
}
