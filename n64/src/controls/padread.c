/* padread.c -- reading the controllers
 */
#include "tgr/common.h"

/* -- declarations -- */
int osContStartReadData(int param_1);
extern int D_80272D48;
extern int D_8028AB6C;
extern short D_802A4BE8;
void func_80255954(unsigned int *param_1,int *param_2,float *param_3,unsigned int param_4);
void func_80255A18(unsigned int *param_1,int *param_2,float *param_3,unsigned int param_4);
void BrPadStartRead(void);
int osRecvMesg(int param_1,int *param_2,int param_3);
typedef struct OSContPad {
  unsigned short button;
  signed char stick_x;
  signed char stick_y;
  unsigned char errnum;
} OSContPad;
void osContGetReadData(OSContPad *data);
extern OSContPad D_8031A3E0[4];
void func_80255120(unsigned int *param_1);
void BrPadEdges(unsigned int *param_1);
extern unsigned int D_8036A8E0[4][0x57];
/* -- end declarations -- */

/* WHAT IT DOES: Start the next controller read, unless one is already in
 * flight: the result arrives on the controller message queue. */
/* @implements 0x8021A920 tgr BrPadStartRead */
void BrPadStartRead(void)
{
  if (D_8028AB6C == 0) {
    D_8028AB6C = 1;
    D_802A4BE8 = 0;
    osContStartReadData(&D_80272D48);
  }
}

/* WHAT IT DOES: Turn the analogue stick into menu presses: pushing past the
 * threshold on either axis sets the matching up, down, left or right bit,
 * with a delay before it starts repeating. */
/* @implements 0x80255ADC tgr BrPadStickToButtons */
void BrPadStickToButtons(int param_1)
{
  func_80255954(param_1,param_1 + 8,param_1 + 0x1c,8);
  func_80255A18(param_1,param_1 + 0xc,param_1 + 0x1c,2);
  func_80255A18(param_1,param_1 + 0x10,param_1 + 0x18,4);
  func_80255954(param_1,param_1 + 0x14,param_1 + 0x18,1);
}

/* WHAT IT DOES: Finish a controller read: start one if none is in flight,
 * wait for it to complete, copy the results into the pad state, and mark
 * fresh input as ready. */
/* @implements 0x8021A964 tgr BrPadRead */
void BrPadRead(void)
{
  BrPadStartRead();
  osRecvMesg(&D_80272D48,0,1);
  osContGetReadData(D_8031A3E0);
  D_802A4BE8 = 1;
  D_8028AB6C = 0;
}

/* WHAT IT DOES: Poll all four controllers: wait for the pending read, then
 * update each pad record (0x15C bytes apiece) and derive this frame's fresh
 * presses from its raw buttons. */
/* @implements 0x8021A9B4 tgr BrPadPollAll */
void BrPadPollAll(void)
{
  int i;

  BrPadRead();
  for (i = 0; i < 4; i++) {
    func_80255120(D_8036A8E0[i]);
    BrPadEdges(D_8036A8E0[i]);
  }
}

/* WHAT IT DOES: Mark buttons as handled: moves the given bits from the
 * pad's pressed word to its held word, so one press is acted on once. */
/* @implements 0x80255910 tgr BrPadConsume */
void BrPadConsume(unsigned int *param_1,unsigned int param_2)
{
  param_1[1] |= *param_1 & param_2;
  *param_1 = *param_1 & ~param_2;
}

/* WHAT IT DOES: Turn the raw button word into presses: a button counts as
 * pressed only on the frame it goes down. */
/* @implements 0x80255934 tgr BrPadEdges */
void BrPadEdges(unsigned int *pad)
{
  unsigned int cur = pad[0];

  pad[0] ^= cur & pad[1];
  pad[1] &= cur;
}


/* WHAT IT DOES: Reset a pad record and point it at its own controller data. */
/* @implements 0x80255B54 tgr BrPadInit */
void BrPadInit(unsigned int *pad)
{
  int i;

  i = (unsigned int (*)[0x57])pad - D_8036A8E0;
  pad[12] = 0;
  pad[11] = 0;
  pad[17] = 0;
  pad[0x55] = i;
  ((OSContPad **)pad)[0x56] = &D_8031A3E0[i];
}
