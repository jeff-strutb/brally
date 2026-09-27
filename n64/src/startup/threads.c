/* threads.c -- the event threads: RSP done, RDP done and vertical retrace,
 * each forwarding its event to the queue the game waits on.
 *
 * TU POSITION: IDO pads an infinite loop's dead epilogue to 32 bytes from
 * the start of the object's .text, so the nops after each loop depend on
 * where the function sits in its original file.  This object starts at
 * 0x8021B72C (its .data has 0x8028ADE8, the display-list patchers' flag,
 * directly before the retrace counter), so 0x8021BBDC stays T2 until
 * 0x8021B72C and 0x8021B97C precede it here.
 */
#include "tgr/common.h"

/* -- declarations -- */
void osCreateMesgQueue(int *mq, int *msgs, int count);
void osSetEventMesg(int event, int *mq, int msg);
void osViSetEvent(int *mq, int msg, int retraceCount);
int osRecvMesg(int *mq, int *msg, int flag);
int osSendMesg(int *mq, int msg, int flag);
void BrPerfMark(int bar, int r, int g, int b, int a);
extern int D_8031A320[6];
extern int D_8031A338[6];
extern int D_8031A354[1];
extern int D_8031A358[6];
extern int D_8031A370[6];
extern int D_8031A38C[1];
extern int D_8031A390[6];
extern int D_8031A3B0[6];
extern int D_8031A3C8[1];
extern int D_8031A3CC;
typedef struct BrContStatus {   /* an OSContStatus */
  unsigned short type;
  unsigned char status;
  unsigned char errno;
} BrContStatus;
typedef struct { char raw[0x68]; } BrPfs;   /* an OSPfs */
void osCreateThread(void *t, int id, void (*entry)(void *), void *arg, void *sp, int pri);
void osStartThread(void *t);
int osContInit(int *mq, unsigned char *bitpattern, BrContStatus *status);
void func_802607AC(void);
int func_80265CD0(int *mq, BrPfs *pfs, int channel);
int func_80262370(int *mq, BrPfs *pfs, int channel);
void func_80261F20(BrPfs *pfs);
extern void *D_8028A848;
extern char D_80272D68[];
extern void *D_8031AA28[2];
extern unsigned short D_801B5000[];
extern unsigned short D_801DA800[];
extern int D_80319F88[6];
extern int D_80319FA0[32];
extern int D_8031A350[1];
extern int D_8031A388[1];
extern int D_8031A3A8[1];
extern char D_802729E0[];
extern char D_80272B90[];
extern void *D_8026FF04;
extern char D_803196D0[];
extern char D_80319ED0[];
extern float D_8028A8B0;
extern int D_80272D48[6];
extern int D_80272D60[1];
extern BrContStatus D_8031A3D0[4];
extern BrPfs D_8031A3F8[4];
extern unsigned char D_8031B1E8[4];
void BrPadPollAll(void);
void BrModeSet(void (*fn)(void));
void func_80208570(void);
void func_802534DC(void);
int func_80254620(void);
int osPfsIsPlug(int *mq, unsigned char *pattern);
int osPfsInitPak(int *mq, BrPfs *pfs, int channel);
void osSyncPrintf(char *fmt, ...);
void *memcpy(void *dst, void *src, unsigned int n);
void BrPadConsume(void *pad, int button);
extern short D_802A4BE8;
extern void (*D_8031B318)(void);
extern char D_80270840;
extern int D_802724F0;
extern char D_803163E0[];
extern char D_80316400[];
extern unsigned char D_80316420;
extern unsigned char D_80316421;
extern unsigned int D_8036A8E0;          /* pad 1's pressed buttons (the head of its record) */
extern int D_8036A908;
extern BrPfs D_80369EC0[2];
/* -- end declarations -- */

/* WHAT IT DOES: The RSP event thread: every time the RSP finishes a task,
 * mark the performance meter's first bar and pass the event on to the
 * scheduler's RSP queue. Never returns. */
/* @implements 0x8021BBDC tgr BrSpEventThread */
void BrSpEventThread(void *arg)
{
  osCreateMesgQueue(D_8031A338, D_8031A354, 1);
  osSetEventMesg(4, D_8031A338, D_8031A3CC);
  for (;;) {
    osRecvMesg(D_8031A338, 0, 1);
    BrPerfMark(1, 0, 0, 0, 0xff);
    osSendMesg(D_8031A320, D_8031A3CC, 1);
  }
}

/* WHAT IT DOES: The RDP event thread: every time the RDP finishes a frame's
 * drawing, mark the performance meter's second bar and pass the event on to
 * the scheduler's RDP queue. Never returns. */
/* @implements 0x8021BCA8 tgr BrDpEventThread */
void BrDpEventThread(void *arg)
{
  osCreateMesgQueue(D_8031A370, D_8031A38C, 1);
  osSetEventMesg(9, D_8031A370, D_8031A3CC);
  for (;;) {
    osRecvMesg(D_8031A370, 0, 1);
    BrPerfMark(2, 0, 0, 0, 0xff);
    osSendMesg(D_8031A358, D_8031A3CC, 1);
  }
}

/* WHAT IT DOES: The retrace thread: on every vertical retrace, pass the
 * event on to the retrace queue the game waits on and step the 16-frame
 * retrace counter. Never returns. */
/* @implements 0x8021BD68 tgr BrRetraceThread */
void BrRetraceThread(void *arg)
{
  static int retraceCount = 0;    /* 0x8028ADEC */

  osCreateMesgQueue(D_8031A3B0, D_8031A3C8, 1);
  osViSetEvent(D_8031A3B0, D_8031A3CC, 1);
  for (;;) {
    osRecvMesg(D_8031A3B0, 0, 1);
    osSendMesg(D_8031A390, D_8031A3CC, 1);
    retraceCount = (retraceCount + 1) & 0xf;
  }
}

/* WHAT IT DOES: Bring up the scheduler side of the game: the two frame
 * buffers, the RSP, RDP and retrace event queues, the RSP and RDP event
 * threads, the controllers (through the SI queue), and for each controller
 * with a pak that answers as a rumble pak, mark it and stop its motor.
 * RESIDUE (14): IDO gives the last callee-saved register to the flag value
 * 1; the ROM gives it to the loop bound 4 and loads the 1 at the store. */
/* @implements 0x8021BE88 tgr BrSchedInit */
void BrSchedInit(void)
{
  int i;
  int q[6];
  int msg;
  unsigned char bits;
  int r;

  D_8028A848 = D_80272D68;
  D_8031AA28[0] = D_801B5000;
  D_8031AA28[1] = D_801DA800;
  osCreateMesgQueue(D_80319F88, D_80319FA0, 32);
  osCreateMesgQueue(D_8031A320, D_8031A350, 1);
  osSetEventMesg(4, D_8031A320, D_8031A3CC);
  osCreateMesgQueue(D_8031A358, D_8031A388, 1);
  osSetEventMesg(9, D_8031A358, D_8031A3CC);
  osCreateMesgQueue(D_8031A390, D_8031A3A8, 1);
  osViSetEvent(D_8031A390, D_8031A3CC, 1);
  osCreateThread(D_802729E0, 10, BrSpEventThread, D_8026FF04, D_803196D0, 60);
  osStartThread(D_802729E0);
  osCreateThread(D_80272B90, 11, BrDpEventThread, D_8026FF04, D_80319ED0, 60);
  osStartThread(D_80272B90);
  D_8028A8B0 = 46875.0f;
  osCreateMesgQueue(q, &msg, 1);
  osSetEventMesg(5, q, 1);
  osContInit(q, &bits, D_8031A3D0);
  osCreateMesgQueue(D_80272D48, D_80272D60, 1);
  osSetEventMesg(5, D_80272D48, 0);
  func_802607AC();
  for (i = 0; i < 4; i++) {
    D_8031B1E8[i] = 0;
    if ((bits >> i & 1) && !(D_8031A3D0[i].errno & 8) && (D_8031A3D0[i].type & 4) &&
        (D_8031A3D0[i].status & 1)) {
      r = func_80265CD0(D_80272D48, &D_8031A3F8[i], i);
      if (r != 0 && (r == 10 || r == 11) && func_80262370(D_80272D48, &D_8031A3F8[i], i) == 0) {
        D_8031B1E8[i] = 1;
        func_80261F20(&D_8031A3F8[i]);
      }
    }
  }
}

/* WHAT IT DOES: The boot-time controller check: with no first controller,
 * or no controller pak in port 1, or a pak that will not initialise (one
 * reporting "no pak file system" is set up as a rumble pak), or no valid
 * save, run the message screen until it is dismissed (its reason in
 * 0x80270840); a pak in port 2 is initialised too; holding B at boot runs
 * the debug screen first.
 * RESIDUE (47): ours computes pad 1's address once for the B test and the
 * consume call; the ROM loads the word through lui/lw and builds the
 * address again in the branch. */
/* @implements 0x8021C188 tgr BrBootCheck */
void BrBootCheck(void)
{
  static unsigned char pakInit = 0;   /* 0x8028ADF0 */
  unsigned char plugged;

  BrPadPollAll();
  D_802A4BE8 = 0;
  if (D_8036A908 != 0) {
    BrModeSet(func_80208570);
    D_80270840 = 0;
    while (D_8031B318 == func_80208570) {
      func_80208570();
    }
  }
  osPfsIsPlug(D_80272D48, &plugged);
  if (!(plugged & 1)) {
    BrModeSet(func_80208570);
    D_80270840 = 1;
    while (D_8031B318 == func_80208570) {
      func_80208570();
    }
  } else {
    if (pakInit == 0) {
      pakInit = 1;
      osSyncPrintf("\nInitializing controller pak...\n");
      D_802724F0 = osPfsInitPak(D_80272D48, &D_80369EC0[0], 0);
    }
    if (D_802724F0 != 0) {
      if (D_802724F0 == 10) {
        func_80262370(D_80272D48, &D_8031A3F8[0], 0);
      }
      BrModeSet(func_80208570);
      D_80270840 = 1;
      while (D_8031B318 == func_80208570) {
        func_80208570();
      }
    } else {
      memcpy(D_803163E0, D_80369EC0[0].raw + 0xc, 0x20);
      D_80316420 = 1;
    }
  }
  if (plugged & 2) {
    D_802724F0 = osPfsInitPak(D_80272D48, &D_80369EC0[1], 1);
    if (D_802724F0 != 0) {
      if (D_802724F0 == 10) {
        func_80262370(D_80272D48, &D_8031A3F8[1], 1);
      }
    } else {
      memcpy(D_80316400, D_80369EC0[1].raw + 0xc, 0x20);
      D_80316421 = 1;
    }
  }
  if (D_8036A8E0 & 0x4000) {
    BrPadConsume(&D_8036A8E0, 0x4000);
    BrModeSet(func_802534DC);
    while (D_8031B318 == func_802534DC) {
      func_802534DC();
    }
  }
  if (func_80254620() == 0) {
    BrModeSet(func_80208570);
    D_80270840 = 2;
    while (D_8031B318 == func_80208570) {
      func_80208570();
    }
  }
  D_802A4BE8 = 1;
}
