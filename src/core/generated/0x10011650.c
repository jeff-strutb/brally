/* Auto-generated from Ghidra decompilation — 0x10011650 */
#ifdef BR_MATCHING_BUILD

#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h>

#ifndef true
#define true 1
#define false 0
#endif

typedef struct BrDlCmd { int op; int arg; } BrDlCmd;
typedef struct BrMat4 { float m[4][4]; } BrMat4;

extern BrDlCmd *DAT_106e7710;
extern int DAT_1184c484;
extern int DAT_106ed520;
extern BrMat4 DAT_106e78f0;
extern BrMat4 DAT_106e7930;
extern BrMat4 DAT_106e72a8;
extern BrMat4 DAT_10396eb8;
extern int DAT_106ed6ac;
extern int DAT_100b3014;
extern unsigned DAT_10396ef8;
extern unsigned DAT_10396f00;
extern int DAT_106e7718;
extern int DAT_106e79b0;

int FUN_1001cf90(BrDlCmd *, int, int, int, int, int, int, int, int, int, int,
                 int, int, int, int, int, int);
void FUN_10034b70(BrMat4 *, int);
void FUN_10034af0(BrMat4 *, BrMat4 *, BrMat4 *);
void FUN_10011300(void *, unsigned, int, int, int);

#define BR_EMIT(c, a)                                                          \
  {                                                                            \
    BrDlCmd *p_ = DAT_106e7710++;                                              \
    p_->op = (c);                                                              \
    p_->arg = (a);                                                             \
  }

/* WHAT IT DOES: set up the fixed rendering state for one scene: emits the
 * display-list preamble, installs the standard viewport, and loads a hard-
 * coded axis-swapping matrix that converts the game's coordinate convention
 * into the renderer's. */
/* @implements 0x10011650 glide FUN_10011650 */
void FUN_10011650(void *param_1)
{
  BR_EMIT(0xb900031d, 0x504240)
  FUN_1001cf90(DAT_106e7710++, 0, 0, 0, 0x3eb, 0x3e9, 0, 0x3eb, 0, 0, 0, 0,
               0x3eb, 0x3e9, 0, 0x3eb, 0);
  BR_EMIT(DAT_1184c484 & 0xffffff | 0xdc000000, 1)
  FUN_10034b70(&DAT_106e78f0, DAT_106ed520);
  DAT_106e7930.m[0][0] = 0.0f;
  DAT_106e7930.m[0][1] = 0.0f;
  DAT_106e7930.m[0][2] = -1.0f;
  DAT_106e7930.m[0][3] = 0.0f;
  DAT_106e7930.m[1][0] = -1.0f;
  DAT_106e7930.m[1][1] = 0.0f;
  DAT_106e7930.m[1][2] = 0.0f;
  DAT_106e7930.m[1][3] = 0.0f;
  DAT_106e7930.m[2][0] = 0.0f;
  DAT_106e7930.m[2][1] = -1.0f;
  DAT_106e7930.m[2][2] = 0.0f;
  DAT_106e7930.m[2][3] = 0.0f;
  DAT_106e7930.m[3][0] = 0.0f;
  DAT_106e7930.m[3][1] = 0.0f;
  DAT_106e7930.m[3][2] = 0.0f;
  DAT_106e7930.m[3][3] = 1.0f;
  FUN_10034af0(&DAT_106e78f0, &DAT_106e78f0, &DAT_106e7930);
  memcpy(&DAT_106e7930, &DAT_106e78f0, 0x40);
  FUN_10034af0(&DAT_10396eb8, &DAT_106e78f0, &DAT_106e72a8);
  BR_EMIT(0xb9000201, 4)
  BR_EMIT(0xba000602, 0xc0)
  if (DAT_106ed6ac == 0) {
    switch (DAT_100b3014) {
    case 4:
    case 10:
      FUN_10011300(param_1, DAT_10396ef8 & 0xffff, 0x70, 0x58, 0x38);
      FUN_10011300(param_1, DAT_10396f00 & 0xffff, 0x70, 0x68, 0x58);
      break;
    case 1:
    case 7:
      FUN_10011300(param_1, DAT_10396ef8 & 0xffff, 0x60, 0x54, 0x38);
      FUN_10011300(param_1, DAT_10396f00 & 0xffff, 0x60, 0x5c, 0x50);
      break;
    default:
      FUN_10011300(param_1, DAT_10396ef8 & 0xffff, 0xa0, 0x88, 0x60);
      FUN_10011300(param_1, DAT_10396f00 & 0xffff, 0x70, 0x68, 0x58);
      break;
    }
  }
  BR_EMIT(0xe7000000, 0)
  BR_EMIT(0xba001301, 0x80000)
  BR_EMIT(0xb9000201, 0)
  BR_EMIT(0xba000602, DAT_106e7718)
  BR_EMIT(0xba000402, DAT_106e79b0)
  BR_EMIT(0xb9000002, 1)
}

#endif /* BR_MATCHING_BUILD */
