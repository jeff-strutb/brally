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
void BrPadEdges(unsigned int *param_1);
extern unsigned int D_8036A8E0[4][0x57];
typedef struct BrPadMap {       /* the pad record (0x15C bytes), as tgr/pad.h */
  unsigned int pressed;         /* 0x00  the game's buttons */
  unsigned int held;            /* 0x04 */
  int repeat[4];                /* 0x08 */
  float axis[3];                /* 0x18  stick x and y, and the steering */
  signed char steer;            /* 0x24  the steering, -128..127 */
  unsigned char layout;         /* 0x25  the control layout, 0..4 */
  char pad26[2];
  int absent;                   /* 0x28  non-zero while no controller answers */
  unsigned char *rec[2];        /* 0x2C  the laps being recorded */
  int recLen[2];                /* 0x34  bytes recorded */
  int recMax[2];                /* 0x3C  room for */
  unsigned char *ghost;         /* 0x44  the recording being played back */
  int ghostPos;                 /* 0x48 */
  int ghostLen;                 /* 0x4C */
  char pad50[0x158 - 0x50];
  OSContPad *cont;              /* 0x158  its read */
} BrPadMap;
extern int D_8026FF10;                  /* the race is paused */
int BrModeIs(void (*mode)(void));
void BrRaceTick(void);
void BrCheatInput(BrPadMap *p);
/* -- end declarations -- */

/* WHAT IT DOES: Turn a controller's read into the game's pad record.  A
 * controller that does not answer reads as nothing (and as absent when the
 * error is "no controller").  The buttons map to the game's own bits
 * (d-pad 1/2/4/8, A 0x10, B 0x20, C 0x100-0x800, Z 0x1000, R 0x2000,
 * L 0x4000, start 0x8000); in a race the control layout (0-4) then adds the
 * driving bits (accelerate 0x10000, brake 0x20000, the gears, the look-back
 * and the horn) and sets the steering from the stick, or from the d-pad in
 * layout 2.  A lap being recorded stores the steering and driving bits two
 * bytes a frame (not while paused); a recording being played back feeds
 * them in instead.  The three axes are the stick and the steering over 70,
 * held to -1..1.  Last, the cheat sequences watch the buttons. */
/* @implements 0x80255120 tgr BrPadMapRead */
void BrPadMapRead(BrPadMap *p)
{
  unsigned int b;
  int k;

  if ((b = p->cont->errnum) != 0) {
    p->absent = b == 8;
    p->cont->stick_x = 0;
    p->cont->stick_y = 0;
    p->cont->button = 0;
  } else {
    p->absent = 0;
  }
  b = p->cont->button;
  p->pressed = 0;
  if (b & 0x800) {
    p->pressed |= 8;
  }
  if (b & 0x400) {
    p->pressed |= 2;
  }
  if (b & 0x200) {
    p->pressed |= 4;
  }
  if (b & 0x100) {
    p->pressed |= 1;
  }
  if (b & 0x8000) {
    p->pressed |= 0x10;
  }
  if (b & 0x4000) {
    p->pressed |= 0x20;
  }
  if (b & 0x20) {
    p->pressed |= 0x1000;
  }
  if (b & 0x10) {
    p->pressed |= 0x2000;
  }
  if (b & 0x2000) {
    p->pressed |= 0x8000;
  }
  if (b & 0x1000) {
    p->pressed |= 0x4000;
  }
  if (b & 8) {
    p->pressed |= 0x100;
  }
  if (b & 1) {
    p->pressed |= 0x200;
  }
  if (b & 4) {
    p->pressed |= 0x400;
  }
  if (b & 2) {
    p->pressed |= 0x800;
  }
  if (BrModeIs(BrRaceTick)) {
    switch (p->layout) {
    case 4:
      if (p->pressed & 0x1000) {
        p->pressed |= 0x200000;
      }
      if (p->pressed & 0x2000) {
        p->pressed |= 0x100000;
      }
      goto stick;
    case 0:
      if (p->pressed & 0x8000) {
        p->pressed |= 0x200000;
      }
      if (p->pressed & 0x2000) {
        p->pressed |= 0x100000;
      }
    stick:
      p->steer = p->cont->stick_x;
      if (p->pressed & 0x10) {
        if (p->cont->stick_y < -0x40) {
          p->pressed |= 0x20000;
        }
        p->pressed |= 0x10000;
      }
      if (p->pressed & 0x20) {
        if (p->pressed & 0x10000) {
          p->pressed |= 0x80000;
        } else {
          p->pressed |= 0x40000;
        }
      }
      if (p->pressed & 0x100) {
        p->pressed |= 0x1000000;
      }
      if (p->pressed & 0x400) {
        p->pressed |= 0x4000000;
      }
      if (p->pressed & 0x800) {
        p->pressed |= 0x8000000;
      }
      break;
    case 1:
      if (p->pressed & 0x8000) {
        p->pressed |= 0x200000;
      }
      if (p->pressed & 0x2000) {
        p->pressed |= 0x100000;
      }
      p->steer = p->cont->stick_x;
      if (p->pressed & 0x10) {
        if (p->cont->stick_y < -0x40) {
          p->pressed |= 0x20000;
        }
        p->pressed |= 0x10000;
      }
      if (p->pressed & 0x20) {
        p->pressed |= 0x40000;
      }
      if (p->pressed & 0x400) {
        p->pressed |= 0x80000;
      }
      if (p->pressed & 0x100) {
        p->pressed |= 0x1000000;
      }
      if (p->pressed & 0x200) {
        p->pressed |= 0x4000000;
      }
      if (p->pressed & 0x800) {
        p->pressed |= 0x8000000;
      }
      break;
    case 2:
      if (p->pressed & 4) {
        if (!(p->pressed & 1)) {
          p->steer = -0x50;
        } else {
          p->steer = 0;
        }
      } else if (p->pressed & 1) {
        p->steer = 0x50;
      } else {
        p->steer = 0;
      }
      if (p->pressed & 0x10) {
        if (p->pressed & 2) {
          p->pressed |= 0x20000;
        }
        p->pressed |= 0x10000;
      }
      if (p->pressed & 0x20) {
        if (p->pressed & 0x10000) {
          p->pressed |= 0x80000;
        } else {
          p->pressed |= 0x40000;
        }
      }
      if (p->pressed & 0x1000) {
        p->pressed |= 0x200000;
      }
      if (p->pressed & 0x2000) {
        p->pressed |= 0x100000;
      }
      if (p->pressed & 0x100) {
        p->pressed |= 0x1000000;
      }
      if (p->pressed & 0x400) {
        p->pressed |= 0x4000000;
      }
      if (p->pressed & 0x800) {
        p->pressed |= 0x8000000;
      }
      break;
    case 3:
      if (p->cont->stick_y > 0x20) {
        p->pressed |= 0x10000;
      } else if (p->cont->stick_y < -0x10) {
        p->pressed |= 0x30000;
      }
      p->steer = p->cont->stick_x;
      if (p->pressed & 0x8000) {
        if (p->pressed & 0x10000) {
          p->pressed |= 0x80000;
        } else {
          p->pressed |= 0x40000;
        }
      }
      if (p->pressed & 0x12) {
        p->pressed |= 0x200000;
      }
      if (p->pressed & 0x28) {
        p->pressed |= 0x100000;
      }
      if (p->pressed & 0x100) {
        p->pressed |= 0x1000000;
      }
      if (p->pressed & 0x400) {
        p->pressed |= 0x4000000;
      }
      if (p->pressed & 0x800) {
        p->pressed |= 0x8000000;
      }
      break;
    }
  }
  if (p->rec[0] != 0 || p->rec[1] != 0) {
    for (k = 0; k < 2; k++) {
      if (p->rec[k] != 0 && p->recLen[k] < p->recMax[k]) {
        p->rec[k][p->recLen[k]] = p->steer;
        p->rec[k][p->recLen[k] + 1] = (p->pressed & 0x3f0000) >> 16;
        if (D_8026FF10 == 0) {
          p->recLen[k] += 2;
        }
      }
    }
  } else if (p->ghost != 0) {
    if (p->ghostPos < p->ghostLen) {
      p->steer = p->ghost[p->ghostPos];
      p->pressed = (p->ghost[p->ghostPos + 1] << 16) | (p->pressed & 0xffc0ffff);
      if (D_8026FF10 == 0) {
        p->ghostPos += 2;
      }
    } else {
      p->steer = 0;
      p->pressed &= 0xffc0ffff;
    }
  }
  p->axis[0] = (float)p->cont->stick_x / 70.0f;
  p->axis[1] = (float)p->cont->stick_y / 70.0f;
  p->axis[2] = (float)p->steer / 70.0f;
  if (p->axis[0] > 1.0f) {
    p->axis[0] = 1.0f;
  } else if (p->axis[0] < -1.0f) {
    p->axis[0] = -1.0f;
  }
  if (p->axis[1] > 1.0f) {
    p->axis[1] = 1.0f;
  } else if (p->axis[1] < -1.0f) {
    p->axis[1] = -1.0f;
  }
  if (p->axis[2] > 1.0f) {
    p->axis[2] = 1.0f;
  } else if (p->axis[2] < -1.0f) {
    p->axis[2] = -1.0f;
  }
  BrCheatInput(p);
}

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
