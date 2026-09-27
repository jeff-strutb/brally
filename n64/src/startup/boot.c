/* boot.c -- power-on entry and fatal errors
 */
#include "tgr/common.h"

/* -- declarations -- */
void BrIdleThread(void *arg);
int osCreateThread();
void osStartThread(int param_1);
void osInitialize(void);
extern int D_80272680;
void BrHaltLoop();
extern int D_8028A88C;
int osRecvMesg(int *mq, int *msg, int flag);
extern int D_8031A390;
void osSetEventMesg(int event, int *mq, int msg);
void osSyncPrintf(char *fmt, ...);
int __osGetCurrFaultedThread(void);
extern int D_8031B1B0[6];
int osPiReadIo(unsigned int devAddr, unsigned int *data);
void osCreateViManager(int pri);
void osViSetMode(void *mode);
void osViBlack(int active);
void osCreatePiManager(int pri, int *mq, int *msgs, int count);
void osCreateMesgQueue(int *mq, int *msgs, int count);
void osSetThreadPri(void *t, int pri);
void func_802001B4(void *arg);
typedef struct BrArgs { int argc; char **argv; } BrArgs;
extern char *D_80316450[];           /* argv */
extern BrArgs *D_8026FF00;
extern char **D_8026FF04;
extern int D_8028AAB0;
extern int D_8028AAB4;
extern char D_802A5D70[];            /* MPAL video mode */
extern char D_802A54B0[];            /* NTSC video mode */
extern int D_80319ED0[];
extern int D_80319EE8[];
extern int D_8031B1C8[];
extern char D_8031AC00[];
extern char D_80272830[];
extern unsigned long long D_80316CD0[0x400];
extern int osTvType;
extern unsigned long long D_8031ADB0[0x80];   /* the fault thread's stack */
extern unsigned long long D_80318CD0[];
extern unsigned long long D_80318ED0[0x100];
extern unsigned long long D_803196D0[0x100];
/* -- end declarations -- */

/* WHAT IT DOES: Wait for the next vertical retrace: block until the video
 * interrupt posts its message. */
/* @implements 0x8021E1C0 tgr BrWaitRetrace */
void BrWaitRetrace(void)
{
  osRecvMesg((&D_8031A390),0,1);
}

/* WHAT IT DOES: The end of the line after a fatal error: takes the error
 * code and returns straight away in the retail build (its body was compiled
 * out). */
/* @implements 0x8021E1EC tgr BrHaltLoop */
void BrHaltLoop(int arg0)
{
}

/* WHAT IT DOES: Stop the game with an error message: store the message for
 * the error screen and hand over to the halt loop. Used for out-of-memory
 * and overflow checks. */
/* @implements 0x8021E1F4 tgr BrFatal */
void BrFatal(int param_1)
{
  D_8028A88C = param_1;
  BrHaltLoop(0);
}

/* WHAT IT DOES: The fault thread: wait for the CPU-fault event, report it
 * on the debug output, and hand the faulted thread to the halt loop.
 * Never returns.
 * RESIDUE (4 nops): every infinite-loop epilogue in the ROM sits at 16 mod
 * 32; ours is aligned to 32 from this object's start (0x8021E1C0), so the
 * dead epilogue lands 16 bytes early. */
/* @implements 0x8021E21C tgr BrFaultThread */
void BrFaultThread(void *arg)
{
  static int faulted;         /* 0x8031B1CC: the faulted thread */
  static int x8031B1D0;
  int msg;

  osSetEventMesg(12, D_8031B1B0, 16);
  x8031B1D0 = 0;
  for (;;) {
    do {
      osRecvMesg(D_8031B1B0, &msg, 1);
      osSyncPrintf("\n=> faultproc - got a fault message...\n");
      faulted = __osGetCurrFaultedThread();
    } while (faulted == 0);
    BrHaltLoop(faulted);
  }
}

/* WHAT IT DOES: The idle thread: read the argument string from the
 * cartridge (0xFFB000) and split it into argc/argv, start the video and PI
 * managers (MPAL or NTSC, 320x240), start the fault thread, paint the
 * thread stacks with a marker pattern, start the main game thread and drop
 * to the lowest priority for good.
 * RESIDUE (34): IDO unrolls the second stack fill and not the third (the
 * ROM the other way round); and the final for (;;) has the same dead-
 * epilogue padding limit as BrFaultThread. */
/* @implements 0x8021E2C8 tgr BrIdleThread */
void BrIdleThread(void *arg)
{
  static BrArgs args = { 1, D_80316450 };   /* 0x8028B2EC */
  char *p;
  unsigned int i;
  unsigned int buf[16];
  unsigned long long *s;

  for (i = 0; i < 16; i++) {
    osPiReadIo(0xFFB000 + i * 4, &buf[i]);
  }
  p = (char *)buf;
  while (*p != 0) {
    while (*p != 0 && *p == ' ') {
      *p++ = 0;
    }
    if (*p != 0) {
      D_80316450[args.argc] = p;
      args.argc++;
    }
    while (*p != 0 && *p != ' ') {
      p++;
    }
  }
  D_8026FF04 = args.argv;
  D_8026FF00 = &args;
  osCreateViManager(0xfe);
  D_8028AAB0 = 320;
  D_8028AAB4 = 240;
  if (osTvType == 2) {
    osViSetMode(D_802A5D70);
  } else {
    osViSetMode(D_802A54B0);
  }
  osViBlack(1);
  osCreatePiManager(150, D_80319ED0, D_80319EE8, 32);
  osCreateMesgQueue(D_8031B1B0, D_8031B1C8, 1);
  osCreateThread(D_8031AC00, 5, BrFaultThread, args.argv, D_8031ADB0 + 0x80, 50);
  osStartThread(D_8031AC00);
  s = D_80316CD0;
  for (i = 0; i < 0x400; i++) {
    *s++ = 0x5015A1DBFED15C00ULL;
  }
  s = D_80318ED0;
  for (i = 0; i < 0x100; i++) {
    *s++ = 0x5015A1DBFED15C00ULL;
  }
  for (s = D_803196D0; s != D_803196D0 + 0x100; s++) {
    *s = 0x5015A1DBFED15C00ULL;
  }
  osCreateThread(D_80272830, 3, func_802001B4, arg, D_80318CD0, 10);
  osStartThread(D_80272830);
  osSetThreadPri(0, 0);
  for (;;) {
  }
}

/* WHAT IT DOES: The game's entry point after the boot stub clears memory:
 * initialise the N64 operating system, then create and start the idle
 * thread that brings everything else up. */
/* @implements 0x8021E5C4 tgr BrBoot */
void BrBoot(void)
{
  osInitialize();
  osCreateThread(&D_80272680,1,BrIdleThread,0,D_80316CD0,10);
  osStartThread(&D_80272680);
}
