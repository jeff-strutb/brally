/* br_state.h -- small state predicates, decompiled from BRD3D.dll.
 *
 * 0x1003E080 tests nine globals in sequence. Eight of them return 1 when
 * non-zero -- but the fifth, g_AA285C, jumps to the *return 0* path instead.
 * That inversion is in the original (the branch at 0x1003E0AB targets
 * 0x1003E0D1, while every sibling targets 0x1003E0D4) and reads as an
 * override flag: when it is set the answer is forced negative regardless of
 * the checks that follow.
 *
 * The globals are not yet named; they are exposed positionally so the
 * inversion is not lost while their meaning is still unknown.
 */
#ifndef BR_STATE_H
#define BR_STATE_H

#include "br_match.h"   /* BR_THISCALL1 -- thiscall via __fastcall on VC5 */

typedef struct BrActiveFlags {
    int a0;        /* 0x10AA33D0 */
    int a1;        /* 0x10AA33D4 */
    int a2;        /* 0x10AA33D8 */
    int a3;        /* 0x10AA33DC */
    int override;  /* 0x10AA285C -- set forces the result to 0 */
    int a5;        /* 0x10AA2AF0 */
    int a6;        /* 0x10AA2CF0 */
    int a7;        /* 0x10AA2DAC */
    int a8;        /* 0x10AA2DB4 */
} BrActiveFlags;

/* 0x1003E080  1 if anything is active, 0 otherwise (see the override note). */
int BrIsAnyActive(const BrActiveFlags *pFlags);

/* 0x10073F40  thiscall: returns field at +0x0C, plus one if +0x08 is set. */
typedef struct BrCounted { int pad0, pad4, flag, count; } BrCounted;
int BR_THISCALL1 BrCountedTotal(const BrCounted *pObj);

#endif /* BR_STATE_H */
