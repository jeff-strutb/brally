/* main.c -- the main game thread
 */
#include "tgr/common.h"

/* -- declarations -- */
void BrSchedInit(void);
void BrStub8021C740(void);
void BrStub8021C6B0(void);
void BrEntAllReset(void);
void BrBootCheck(void);
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
void BrMusicStop(void);
void BrMusicStart(void *data, char *heap);
extern char *D_8026FF24[];              /* each track's music in ROM */
extern int D_8028B940;                  /* the track */
extern int D_8026FF58;                  /* animate the scene: 0 no, -1 frozen, else run */
extern void *D_8026FF54;                /* the scene's animations */
extern float D_8028AAD8;                /* seconds this frame */
void BrAnimUpdate(void *set);
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
  BrBootCheck();
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

/* WHAT IT DOES: Switch to the current track's music: stop the player,
 * unpack the track's module into the music buffer and start it. */
/* @implements 0x80200320 tgr BrMusicLoadTrack */
void BrMusicLoadTrack(void)
{
  BrMusicStop();
  BrRomUnpack((void *)0x80025C00, D_8026FF24[D_8028B940], 0);
  BrMusicStart((void *)0x80025C00, D_802AC400);
}

/* WHAT IT DOES: Step the scene's animations, if it has any; when they are
 * frozen, step them by zero time (so they are posed but do not move). */
/* @implements 0x8020037C tgr BrSceneAnimate */
void BrSceneAnimate(void)
{
  float dt;

  if (D_8026FF58 != 0) {
    if (D_8026FF58 == -1) {
      dt = D_8028AAD8;
      D_8028AAD8 = 0.0f;
    }
    BrAnimUpdate(D_8026FF54);
    if (D_8026FF58 == -1) {
      D_8028AAD8 = dt;
    }
  }
}
