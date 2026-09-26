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
void func_8021A920(void);
int func_802642E0(int param_1,int *param_2,int param_3);
void func_80264824(short *param_1);
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
  func_8021A920();
  func_802642E0(&D_80272D48,0,1);
  func_80264824((&D_8031A3E0));
  D_802A4BE8 = 1;
  D_8028AB6C = 0;
}
