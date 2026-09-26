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
void osContGetReadData(short *param_1);
extern int D_8031A3E0;
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
  osContGetReadData((&D_8031A3E0));
  D_802A4BE8 = 1;
  D_8028AB6C = 0;
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
/* @t4-pass 0x80255934 1 2026-09-26 compiles 14 best 8 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80255934 2 2026-09-26 compiles 12 best 8 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80255934 3 2026-09-26 compiles 12 best 8 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80255934 tgr BrPadEdges */
void BrPadEdges(unsigned int *param_1)
{
  unsigned int uVar1;
  
  uVar1 = *param_1;
  *param_1 = uVar1 ^ uVar1 & param_1[1];
  param_1[1] = param_1[1] & uVar1;
}

/* WHAT IT DOES: Reset a pad record and point it at its own controller data. */
/* @t4-pass 0x80255B54 1 2026-09-26 compiles 16 best 12 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80255B54 2 2026-09-26 compiles 17 best 12 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80255B54 3 2026-09-26 compiles 15 best 12 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80255B54 tgr BrPadInit */
void BrPadInit(int param_1)
{
  int iVar1;
  
  iVar1 = (param_1 + 0x7fc95720) / 0x15c;
  *(int *)(param_1 + 0x30) = 0;
  *(int *)(param_1 + 0x2c) = 0;
  *(int *)(param_1 + 0x44) = 0;
  *(int *)(param_1 + 0x154) = iVar1;
  *(int *)(param_1 + 0x158) = iVar1 * 6 + -0x7fce5c20;
}
