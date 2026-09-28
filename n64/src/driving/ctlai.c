/* ctlai.c -- the computer drivers
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_8022762C(int ctl);
typedef struct BrAiNode {       /* 0x10 bytes */
  short x0[3];
  char pad6[6];
  unsigned char xc;
  unsigned char xd;
  unsigned char xe;
  unsigned char xf;
} BrAiNode;
typedef struct BrAiCar {        /* the AI's view of a car record */
  char pad000[0x140];
  int slot;                     /* 0x140 */
  char pad144[0x1020 - 0x144];
  float x1020[4];               /* 0x1020 */
  char pad1030[0x1060 - 0x1030];
  float lane[4];                /* 0x1060 */
  int x1070[4];                 /* 0x1070 */
  float x1080[4];               /* 0x1080 */
  int x1090[4];                 /* 0x1090 */
  char pad10a0[0x10d0 - 0x10a0];
  BrAiNode node[0x90];          /* 0x10D0 */
  short x19d0[0x90][3];         /* 0x19D0 */
  short x1d30[36];              /* 0x1D30 */
} BrAiCar;
/* -- end declarations -- */

/* WHAT IT DOES: Drive one computer-controlled car for this frame: the
 * out-of-line entry to the AI driver's main body. */
/* @implements 0x802288B4 tgr BrCtlAi */
void BrCtlAi(int ctl)
{
    func_8022762C(ctl);
}

/* WHAT IT DOES: Does nothing with its argument. An empty function the
 * retail build kept among the AI driver code. */
/* @implements 0x80228E44 tgr BrStub80228E44 */
void BrStub80228E44(int arg0)
{
}

/* WHAT IT DOES: Set up a computer driver's racing lanes: four lane offsets
 * stepping 0.034 from slot * 0.137 (then cleared again with their
 * partners), the lane kinds to 2, the targets to i * 0.15, the 144 path
 * nodes emptied and 36 flags set to 2.
 * RESIDUE (83): the ROM loads 0.034 before 0.137 (so its literal pool
 * order differs too) and finishes the lane chain before the other small
 * loops; ours interleaves them.  Each small loop needs its own counter or
 * IDO leaves it rolled. */
/* @t4-pass 0x802288D4 1 2026-09-26 compiles 17 best 144 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x802288D4 2 2026-09-26 compiles 17 best 144 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x802288D4 3 2026-09-26 compiles 16 best 144 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x802288D4 tgr BrAiLaneSetup */
void BrAiLaneSetup(BrAiCar *a)
{
  int i;
  int j;
  int k;
  int m;
  int q;
  float x;
  BrAiNode *n;
  short (*p)[3];

  x = a->slot * 0.137f;
  for (j = 0; j < 4; j++) {
    a->lane[j] = x;
    x += 0.034f;
  }
  for (k = 0; k < 4; k++) {
    a->x1090[k] = 0;
    a->x1070[k] = 2;
  }
  for (m = 0; m < 4; m++) {
    a->x1020[m] = 0.15f * m;
  }
  for (q = 0; q < 4; q++) {
    a->lane[q] = 0.0f;
    a->x1080[q] = 0.0f;
  }
  for (i = 0, p = a->x19d0, n = a->node; i < 0x90; i++, p++, n++) {
    (*p)[0] = (*p)[1] = (*p)[2] = 0;
    n->x0[0] = 0;
    n->x0[1] = 0;
    n->x0[2] = 0;
    n->xc = 0;
    if (i & 1) {
      n->xd = 0;
    } else {
      n->xd = 0;
    }
    n->xe = 0;
    n->xf = 0xff;
  }
  for (i = 0; i < 36; i++) {
    a->x1d30[i] = 2;
  }
}

/* WHAT IT DOES: Clear a computer driver's steering and pedal outputs. */
/* @implements 0x80228A3C tgr BrAiInputClear */
void BrAiInputClear(short *car)
{
  int i;

  for (i = 0; i < 8; i++) {
    car[0x103e + i] = 0;
  }
  car[0x1046] = 0;
}

