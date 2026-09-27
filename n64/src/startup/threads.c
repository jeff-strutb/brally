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
