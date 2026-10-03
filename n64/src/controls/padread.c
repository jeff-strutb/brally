/* padread.c -- reading the controllers
 */
#include "tgr/common.h"

/* -- declarations -- */
int osContStartReadData(int param_1);
extern int D_80272D48;
extern int D_8028AB6C;
extern short D_802A4BE8;
void BrPadStickRepeatPos(unsigned int *pressed, int *timer, float *axis, unsigned int bit);
void BrPadStickRepeatNeg(unsigned int *pressed, int *timer, float *axis, unsigned int bit);
extern float D_802AB414;
extern float D_802AB418;
extern float D_802AB41C;
extern float D_802AB420;
extern int D_8028AADC;
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

/* WHAT IT DOES: One stick direction's menu auto-repeat, for the positive
 * side of an axis: pushing past the threshold presses the bit and starts a
 * 500 ms delay; held, it presses again each time the timer runs out (then
 * every 200 ms); letting go below the release threshold resets it. */
/* @implements 0x80255954 tgr BrPadStickRepeatPos */
void BrPadStickRepeatPos(unsigned int *pressed, int *timer, float *axis, unsigned int bit)
{
  int t;

  t = *timer;
  if (t != 0) {
    if (*axis < D_802AB414) {
      *timer = 0;
      return;
    }
    if (t > 0) {
      *timer = t - D_8028AADC;
      if (*timer <= 0) {
        *pressed |= bit;
        *timer = -200;
      }
    } else {
      *timer = t + D_8028AADC;
      if (*timer >= 0) {
        *pressed |= bit;
        *timer = -200;
      }
    }
  } else if (*axis > D_802AB418) {
    *pressed |= bit;
    *timer = 500;
  }
}


/* WHAT IT DOES: The same auto-repeat for the negative side of an axis. */
/* @implements 0x80255A18 tgr BrPadStickRepeatNeg */
void BrPadStickRepeatNeg(unsigned int *pressed, int *timer, float *axis, unsigned int bit)
{
  int t;

  t = *timer;
  if (t != 0) {
    if (*axis > D_802AB41C) {
      *timer = 0;
      return;
    }
    if (t > 0) {
      *timer = t - D_8028AADC;
      if (*timer <= 0) {
        *pressed |= bit;
        *timer = -200;
      }
    } else {
      *timer = t + D_8028AADC;
      if (*timer >= 0) {
        *pressed |= bit;
        *timer = -200;
      }
    }
  } else if (*axis < D_802AB420) {
    *pressed |= bit;
    *timer = 500;
  }
}


/* WHAT IT DOES: Turn the analogue stick into menu presses: pushing past the
 * threshold on either axis sets the matching up, down, left or right bit,
 * with a delay before it starts repeating. */
/* @implements 0x80255ADC tgr BrPadStickToButtons */
void BrPadStickToButtons(int param_1)
{
  BrPadStickRepeatPos(param_1,param_1 + 8,param_1 + 0x1c,8);
  BrPadStickRepeatNeg(param_1,param_1 + 0xc,param_1 + 0x1c,2);
  BrPadStickRepeatNeg(param_1,param_1 + 0x10,param_1 + 0x18,4);
  BrPadStickRepeatPos(param_1,param_1 + 0x14,param_1 + 0x18,1);
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
