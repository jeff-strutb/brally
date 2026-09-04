/* br_chasecam.c -- drawing: where the chase camera sits.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.  The
 * camera is here for the same reason br_cammatrix.c is: what it produces is
 * the transform a frame draws through.
 *
 * Filed out of slice4_53.c, an address batch and not a module.  0x100018F0
 * pushes the camera back along the car's own forward axis and then up, with
 * a different height in one game mode; 0x10001BB0 is the unit-direction
 * helper it leans on, and leaves its output UNCHANGED when the two points
 * coincide rather than zeroing it.
 *
 * slice4_53.c's preamble is carried over verbatim.  An include set that
 * looks redundant has already been shown elsewhere in this module to move
 * VC5's register allocation (see br_rdpmode.c).
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#ifdef BR_MATCHING_BUILD
#define BrCarSub9020 BrCarSub9020_port2
#include "slice4_53.h"
#undef BrCarSub9020
#else
#include "slice4_53.h"
#endif
#include "slice1_03.h"      /* BrComCallLocked68 (0x1000C4D0) */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice2_17.h"      /* BrS17BankFlip, BrRenderCountersReset       */
#include "slice2_18.h"      /* BrGfx2C210, BrGfx31227 declarations        */
#include "slice2_19.h"      /* BrSub10002240, BrSub100088B0, BrSub10037740 */
#include "slice2_20.h"      /* BrPoolEmit, BrRcaLoadCar                   */
#ifdef BR_MATCHING_BUILD
#define BrCarSub9020 BrCarSub9020_port
#include "slice2_21.h"      /* BrSinF, BrSqrtF, BrCarSub9020              */
#undef BrCarSub9020
#else
#include "slice2_21.h"      /* BrSinF, BrSqrtF, BrCarSub9020              */
#endif
#include "slice2_22.h"      /* BrDPlayLink, BrDPlaySendTag4               */
#include "slice2_24.h"      /* BrStringById, BrMenuSub10044B90, ...       */

/* slice2_16.h cannot be included here: it defines a TYPE called BrRcaFixup
 * and slice2_20.h defines a FUNCTION of that name, so the two headers cannot
 * share a translation unit.  This is the one declaration needed from it, and
 * it is copied verbatim. */
/* XSLICE 0x1007CC00 */
extern void BrGbiStackOverflow(int code);

#ifdef BR_MATCHING_BUILD

extern float _DAT_10077000;

/* WHAT IT DOES: work out the unit direction from one object to another,
 * leaving the result in the caller's vector. GOTCHA: when the two are in
 * exactly the same place the length is zero and the output is left UNCHANGED
 * rather than zeroed -- the caller keeps whatever direction it had. */
/* @implements 0x10001BB0 glide FUN_10001bb0 */
/* auto-filed from ghidra --refine; transforms: as-is */

void __fastcall FUN_10001bb0(int *param_1,int _edx_unused,int *param_2)
{
  float len;
  BrVec3 local;
  BrVec3 *pFwd;
  
  pFwd = (BrVec3 *)param_2;
  BrVec3Sub(&local, (BrVec3 *)(param_1 + 0xa38), (BrVec3 *)(param_2 + 0xc));
  len = BrVec3Length(&local);
  if (len != _DAT_10077000) {
    BrVec3Div(pFwd, &local, len);
  }
  else {
    len = BrVec3Length(pFwd);
    if (len == _DAT_10077000) {
      pFwd->x = *(float *)param_1;
      pFwd->y = *(float *)(param_1 + 1);
      pFwd->z = *(float *)(param_1 + 2);
    }
  }
  if (param_1[0x3df] != 0) {
    BrVec3Cross((BrVec3 *)(param_2 + 4), (BrVec3 *)(param_1 + 8), pFwd);
  }
  else {
    local.x = 0.0f;
    local.y = 0.0f;
    local.z = 1.0f;
    BrVec3Cross((BrVec3 *)(param_2 + 4), &local, pFwd);
  }
  BrVec3Cross((BrVec3 *)(param_2 + 8), pFwd, (BrVec3 *)(param_2 + 4));
}

extern int DAT_100aa044;
extern int DAT_105ccb88;
extern float _DAT_10077000;
extern float _DAT_10077014;
extern float _DAT_10077018;
extern float _DAT_1007701c;
extern float _DAT_10077020;
extern int g_br0AA010;
void BrVec3AddTo(void *, void *);
float BrVec3Length(void *);
void BrVec3Lerp(void *, void *, void *, float);
void BrVec3MulAdd(void *, void *, void *, float);
void BrVec3MulAddTo(void *, void *, float);
void BrVec3Scale(void *, void *, float);
void BrVec3ScaleBy(void *, float);
void BrVec3Sub(void *, void *, void *);

/* WHAT IT DOES: place the chase camera relative to the car -- pushes it back
 * along the car's own forward axis and then up, using a different height in
 * one of the game modes. Does nothing when the car has no camera attached. */
/* @implements 0x100018F0 glide FUN_100018f0 */
/* auto-filed from ghidra --refine; transforms: as-is */

void __fastcall FUN_100018f0(int param_1, int _edx_unused, int param_2, float param_3)
{
  float tmp[3];
  int dst;
  float len;
  float s;

  if (*(int *)(param_1 + 0xf7c) != 0) {
    dst = param_2 + 0x30;
    BrVec3MulAdd((void *)dst, (void *)(param_1 + 0x30), (void *)(param_1 + 0x20), 2.4f);
    s = -11.0f;
    if (DAT_100aa044 != 1) {
      s = -19.8f;
    }
    BrVec3MulAdd((void *)dst, (void *)dst, (void *)param_1, s);
    return;
  }
  dst = param_2 + 0x30;
  *(float *)(param_2 + 0x38) = *(float *)(param_2 + 0x38) - _DAT_10077014;
  BrVec3Sub(tmp, (void *)dst, (void *)(param_1 + 0x30));
  len = BrVec3Length(tmp);
  if (len != _DAT_10077000) {
    if (DAT_100aa044 == 1) {
      BrVec3ScaleBy(tmp, _DAT_10077018 / len);
    }
    else {
      BrVec3ScaleBy(tmp, _DAT_1007701c / len);
    }
  }
  if (DAT_105ccb88 != 0) {
    BrVec3Scale((void *)dst, (void *)param_1, 11.0f);
  }
  else if (g_br0AA010 == 5) {
    BrVec3Scale((void *)dst, (void *)(param_1 + 0x10), -11.0f);
    BrVec3MulAddTo((void *)dst, (void *)param_1, -13.0f);
  }
  else {
    BrVec3Scale((void *)dst, (void *)param_1, -11.0f);
  }
  len = BrVec3Length((void *)dst);
  if (len != _DAT_10077000) {
    BrVec3ScaleBy((void *)dst, _DAT_10077018 / len);
  }
  BrVec3Lerp((void *)dst, (void *)dst, tmp, param_3);
  BrVec3AddTo((void *)dst, (void *)(param_1 + 0x30));
  *(float *)(param_2 + 0x38) = *(float *)(param_2 + 0x38) - _DAT_10077020;
}

#endif /* BR_MATCHING_BUILD */
