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
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#define BrCarSub9020 BrCarSub9020_port2
#include "br_collrespsolve.h"   /* br_globals: its objects */
#include "br_race.h"   /* br_globals: its objects */
#include "slice1_08.h"   /* br_globals: its objects */
#include "slice4_53.h"
#include "slice3_41.h"
#undef BrCarSub9020
#include "slice1_03.h"      /* BrComCallLocked68 (0x1000C4D0) */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice2_17.h"      /* BrS17BankFlip, BrRenderCountersReset       */
#include "slice2_18.h"      /* BrGfx2C210, BrGfx31227 declarations        */
#include "slice2_19.h"      /* BrSub10002240, BrSub100088B0, BrSub10037740 */
#include "slice2_20.h"      /* BrPoolEmit, BrRcaLoadCar                   */
#define BrCarSub9020 BrCarSub9020_port
#include "slice2_21.h"      /* BrSinF, BrSqrtF, BrCarSub9020              */
#undef BrCarSub9020
#include "slice2_22.h"      /* BrDPlayLink, BrDPlaySendTag4               */
#include "slice2_24.h"      /* BrStringById, BrMenuSub10044B90, ...       */

/* slice2_16.h cannot be included here: it defines a TYPE called BrRcaFixup
 * and slice2_20.h defines a FUNCTION of that name, so the two headers cannot
 * share a translation unit.  This is the one declaration needed from it, and
 * it is copied verbatim. */
/* XSLICE 0x1007CC00 */
extern void BrGbiStackOverflow(int code);


/* 64-bit core: declared once, in br_globals.h or its struct's header */

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

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared by the platform headers */
/* 64-bit core: declared by the platform headers */
/* 64-bit core: declared by the platform headers */
/* 64-bit core: declared by the platform headers */
/* 64-bit core: declared by the platform headers */
/* 64-bit core: declared by the platform headers */
/* 64-bit core: declared by the platform headers */
/* 64-bit core: declared by the platform headers */

/* WHAT IT DOES: place the chase camera relative to the car -- pushes it back
 * along the car's own forward axis and then up, using a different height in
 * one of the game modes. Does nothing when the car has no camera attached. */
/* @implements 0x100018F0 glide FUN_100018f0 */
/* auto-filed from ghidra --refine; transforms: as-is */

void __fastcall FUN_100018f0(BrDriverCar *param_1, int _edx_unused, int param_2, float param_3)
{
  float tmp[3];
  int dst;
  float len;
  float s;

  if (param_1->fF7C != 0) {
    dst = param_2 + 0x30;
    BrVec3MulAdd((void *)dst, &param_1->pos.x, &param_1->up.x, 2.4f);
    s = -11.0f;
    if (DAT_100aa044 != 1) {
      s = -19.8f;
    }
    BrVec3MulAdd((void *)dst, (void *)dst, &param_1->fwd.x, s);
    return;
  }
  dst = param_2 + 0x30;
  *(float *)(param_2 + 0x38) = *(float *)(param_2 + 0x38) - _DAT_10077014;
  BrVec3Sub(tmp, (void *)dst, &param_1->pos.x);
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
    BrVec3Scale((void *)dst, &param_1->fwd.x, 11.0f);
  }
  else if (g_br0AA010 == 5) {
    BrVec3Scale((void *)dst, &param_1->right.x, -11.0f);
    BrVec3MulAddTo((void *)dst, &param_1->fwd.x, -13.0f);
  }
  else {
    BrVec3Scale((void *)dst, &param_1->fwd.x, -11.0f);
  }
  len = BrVec3Length((void *)dst);
  if (len != _DAT_10077000) {
    BrVec3ScaleBy((void *)dst, _DAT_10077018 / len);
  }
  BrVec3Lerp((void *)dst, (void *)dst, tmp, param_3);
  BrVec3AddTo((void *)dst, &param_1->pos.x);
  *(float *)(param_2 + 0x38) = *(float *)(param_2 + 0x38) - _DAT_10077020;
}

/* ------------------------------------------------------------------ */
/* 0x10001A80 -- ease the chase camera's zoom-out toward its target    */
/* ------------------------------------------------------------------ */

/* BrSub10034660: prototype in br_funcs.h */
/* BrSub100347F0: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* -0.1f: the upward step, subtracted   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* -0.66f                               */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /*  3.5f                                */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /*  2.0f                                */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /*  7.0f                                */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /*  4/7                                 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /*  4.0f                                */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /*  0.1f: the downward step             */

/* WHAT IT DOES: eases the chase camera's pull-back toward where it wants
 * to be. In the special mode it just blends the stored point toward the
 * car. Otherwise it seeds the target point from the car (dropped by a
 * constant), measures how far away the tracked point is, turns that
 * distance into a wanted zoom (close: 2, far: 0, a ramp between), and
 * walks the current zoom a tenth per frame toward it, clamping at the
 * target. Camera mode 5 skips all of it. */
/* @implements 0x10001A80 glide BrCamChaseZoomStep */
void __fastcall BrCamChaseZoomStep(BrDriverCar *pCam)
{
    float d;
    float t;
    float v;

    if (pCam->fF7C != 0) {
        BrSub10034660(&pCam->f28E0, &pCam->pos.x, &pCam->up.x, 1.1f);
        return;
    }

    pCam->f28E8 = pCam->pos.z - _DAT_10077024;
    *(int *)&pCam->f28E0   = *(int *)&pCam->pos.x;
    *(int *)&pCam->f28E4   = *(int *)&pCam->pos.y;

    d = BrSub100347F0(&pCam->aBody[0].rb.st.angVel.x);
    if (DAT_100a9360 == 5) {
        return;
    }

    t = _DAT_10077000;
    if (d < _DAT_10077028) {
        t = _DAT_1007702c;
    } else if (d < _DAT_10077030) {
        t = _DAT_10077038 - d * _DAT_10077034;
    }

    if (t > pCam->f28DC) {
        v = pCam->f28DC - _DAT_10077004;
        pCam->f28DC = v;
        if (v > t) {
            pCam->f28DC = t;
        }
    } else if (t < pCam->f28DC) {
        v = pCam->f28DC - _DAT_1007703c;
        pCam->f28DC = v;
        if (v < t) {
            pCam->f28DC = t;
        }
    }

    BrVec3MulAddTo((BrVec3 *)&pCam->f28E0, &pCam->fwd, pCam->f28DC);
}

typedef struct { float x, y, z; } BrCamV3;
typedef struct {
    BrCamV3  n;                 /* +0x00 plane normal     */
    int      f0c;
    BrCamV3 *pV0, *pV1, *pV2;   /* +0x10 the triangle     */
    int      f1c;
} BrCamPlane;                   /* 0x20 bytes             */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* collision grid planes, 4800 B per cell */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* planes per cell                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* the camera was pushed out this frame  */
/* FUN_100686d0: prototype in br_funcs.h */
/* FUN_10034560: prototype in br_funcs.h */
/* FUN_100347f0: prototype in br_funcs.h */
/* FUN_10034310: prototype in br_funcs.h */
/* FUN_10034660: prototype in br_funcs.h */
/* FUN_10034fc0: prototype in br_funcs.h */
/* FUN_10034760: prototype in br_funcs.h */
/* FUN_10034390: prototype in br_funcs.h */
/* FUN_100345c0: prototype in br_funcs.h */

/* Byte-exact 2026-09-24, hand-transcribed from the asm (the D3D twin
 * 0x100011F0 is the byte-identical body).  Source facts, all measured:
 *  - the cell lookups read x through the anchor / camera-position pointer and
 *    y straight off the record (`car + 0x28E4`, `cam + 0x34`);
 *    `nCells = (a != b) + 1`; the two sweep passes are written out;
 *  - the final re-projection vector is its OWN block-scoped local: VC5 gives
 *    block-scoped locals slots after the function-scope ones and lets them
 *    share, which is what puts the cell list at +0x28 with the vector over it
 *    and dir/hitOut at +0x40/+0x4C.  Reusing `dir` there, or any declaration
 *    order / rename / cell-list type, left the three 12-byte slots permuted;
 *  - the float constants are the ORIGINAL's pooled ones (_DAT_10077000 =
 *    0.0f, _DAT_10077004 = -0.1f), never literals: the ray's slack is
 *    `len - (-0.1f)`, 0.1 LONG.  A `0.1f` literal compiled to the same bytes
 *    (the sweep masks relocation targets) but made the ray 0.1 short, and
 *    the live oracle caught it (21_quickrace_finish frame 4486).
 * Every relocation target checked against the original. */
/* WHAT IT DOES: keeps the chase camera out of walls.  It casts a ray from
 * the car's camera anchor to the camera against every collision triangle in
 * the grid cells of both ends (one cell if they share it), allowing the ray
 * 0.1 units of slack.  If that hits, a second ray from the camera's previous
 * position to the camera finds the wall it came through; the camera is put
 * 0.3 units in front of that wall along its normal and, if that brought it
 * nearer the anchor than it was, pushed back out to its old distance along
 * the new line.  A flag records that the camera was moved. */
/* @implements 0x10001510 glide FUN_10001510 */
void __fastcall FUN_10001510(BrDriverCar *car, int _edx_unused, char *cam, BrCamV3 *prev)
{
    BrCamV3    *pAnchor = (BrCamV3 *)&car->f28E0;
    BrCamV3    *pPos;
    BrCamPlane *pPlane, *pEnd, *pBest;
    BrCamV3     hitOut;
    int         cells[2];
    BrCamV3     dir;
    BrCamV3     hit;
    BrCamV3     toV0;
    int         nCells, c;
    float       len, tBest, denom, t, dist;

    DAT_100bcdcc = 0;
    cells[0] = FUN_100686d0(car->f28E0, car->f28E4);
    pPos = (BrCamV3 *)(cam + 0x30);
    cells[1] = FUN_100686d0(pPos->x, *(float *)(cam + 0x34));
    nCells = (cells[0] != cells[1]) + 1;

    FUN_10034560(&dir, pPos, pAnchor);
    pBest = 0;
    len = FUN_100347f0(&dir);
    if (len != _DAT_10077000)
        tBest = (len - _DAT_10077004) / len;
    else
        tBest = 1.0f;

    for (c = 0; c < nCells; c++) {
        pPlane = DAT_11773698[cells[c]];
        pEnd = pPlane + DAT_11778800[cells[c]];
        for (; pPlane != pEnd; pPlane++) {

            denom = FUN_10034310(&dir, pPlane);
            if (denom < _DAT_10077000) {
                FUN_10034560(&toV0, pPlane->pV0, pAnchor);
                t = FUN_10034310(&toV0, pPlane) / denom;
                if (t > _DAT_10077000 && t < tBest) {
                    FUN_10034660(&hit, pAnchor, &dir, t);
                    if (FUN_10034fc0(&hit, pPlane->pV0, pPlane->pV1, pPlane->pV2, pPlane)) {
                        tBest = t;
                        pBest = pPlane;
                        hitOut = hit;
                    }
                }
            }
        }
    }
    if (pBest == 0)
        return;

    FUN_10034560(&dir, pPos, prev);
    pBest = 0;
    tBest = 1.0f;
    for (c = 0; c < nCells; c++) {
        pPlane = DAT_11773698[cells[c]];
        pEnd = pPlane + DAT_11778800[cells[c]];
        for (; pPlane != pEnd; pPlane++) {

            denom = FUN_10034310(&dir, pPlane);
            if (denom < _DAT_10077000) {
                FUN_10034560(&toV0, pPlane->pV0, prev);
                t = FUN_10034310(&toV0, pPlane) / denom;
                if (t > _DAT_10077000 && t < tBest) {
                    FUN_10034660(&hit, prev, &dir, t);
                    if (FUN_10034fc0(&hit, pPlane->pV0, pPlane->pV1, pPlane->pV2, pPlane)) {
                        tBest = t;
                        pBest = pPlane;
                        hitOut = hit;
                    }
                }
            }
        }
    }
    if (pBest == 0)
        return;

    dist = FUN_10034760(pPos, pAnchor);
    FUN_10034660(pPos, &hitOut, pBest, 0.3f);
    {
        BrCamV3 v;
        FUN_10034560(&v, pPos, pAnchor);
        len = FUN_100347f0(&v);
        if (dist < len && len != _DAT_10077000) {
            FUN_10034390(&v, dist / len);
            FUN_100345c0(pPos, pAnchor, &v);
        }
    }
    DAT_100bcdcc = 1;
}

