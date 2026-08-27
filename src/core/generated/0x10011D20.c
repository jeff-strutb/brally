/* Auto-generated from Ghidra decompilation — 0x10011D20 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
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
#ifndef NAN
unsigned long _ghidra_nan_bits = 0x7FC00000;
#define NAN (*(float*)&_ghidra_nan_bits)
#endif

typedef int (*funcptr)();

/* Forward declarations for unknown functions/globals */
#include "br_vec.h"
#include "br_mat.h"

extern int DAT_100a9ec0;
extern int DAT_105bcaec;
extern int DAT_105ccb78;
extern BrMat4 DAT_106e78f0;
extern int DAT_106ea360;
extern float _DAT_10077284;
extern float _DAT_105bc764;
extern int BrG_6C7CB8;
void BrMat4Mul(BrMat4 *pOut, const BrMat4 *pA, const BrMat4 *pB);
void BrScenePropsDraw(int, BrMat4 *);
void BrVec3Direction(BrVec3 *pOut, const BrVec3 *pFrom, const BrVec3 *pTo);

#ifndef BR_DLCMD_DEFINED
#define BR_DLCMD_DEFINED
typedef struct BrDlCmd { int op; int arg; } BrDlCmd;
#endif
extern BrDlCmd *DAT_106e7710;




/* @implements 0x10011D20 glide FUN_10011d20 */
void FUN_10011d20(void)

{
  BrMat4 m;
  
  if ((DAT_105ccb78 != 0) && (BrG_6C7CB8 != 0)) {
    m.m[0][3] = 0.0f;
    m.m[1][3] = 0.0f;
    m.m[2][3] = 0.0f;
    m.m[3][3] = 1.0f;
    m.m[2][0] = 0.0f;
    m.m[2][1] = 0.0f;
    m.m[2][2] = 1.0f;
    BrVec3Direction((BrVec3 *)m.m[1], (BrVec3 *)(BrG_6C7CB8 + 0x4c), (BrVec3 *)(BrG_6C7CB8 + 0x58));
    BrVec3Cross((BrVec3 *)m.m[0], (BrVec3 *)m.m[1], (BrVec3 *)m.m[2]);
    BrVec3Cross((BrVec3 *)m.m[1], (BrVec3 *)m.m[2], (BrVec3 *)m.m[0]);
    m.m[3][0] = *(float *)(BrG_6C7CB8 + 0x4c);
    m.m[3][1] = *(float *)(BrG_6C7CB8 + 0x50);
    m.m[3][2] = (*(float *)(BrG_6C7CB8 + 0x54) + _DAT_105bc764) - _DAT_10077284;
    BrVec3MulAdd((BrVec3 *)m.m[3], (BrVec3 *)m.m[3], (BrVec3 *)m.m[0], -0.3f);
    BrVec3MulAdd((BrVec3 *)m.m[3], (BrVec3 *)m.m[3], (BrVec3 *)m.m[1], -0.6f);
    { BrDlCmd *pEmit_ = DAT_106e7710++; pEmit_->op = 0x1060040; pEmit_->arg = &DAT_100a9ec0; }
    { BrDlCmd *pEmit_ = DAT_106e7710++; pEmit_->op = 0x1030040; pEmit_->arg = DAT_106ea360; }
    BrMat4Scale(&DAT_106e78f0, 0.0009765625f, 0.0009765625f, 0.0009765625f);
    BrMat4Mul(&DAT_106e78f0, &m, &m);
    BrScenePropsDraw(DAT_105bcaec, &m);
  }
  return;
}


#endif /* BR_MATCHING_BUILD */
