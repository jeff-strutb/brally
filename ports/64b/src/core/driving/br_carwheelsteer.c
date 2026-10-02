/* br_carwheelsteer.c -- driving: the per-frame wheel visual-transform step.
 *
 * RESPONSIBILITY: driving/ -- turn per-frame inputs into car motion.
 *
 * One function, 0x1005ACE0: picks which of three "wheel state" passes runs
 * (a net-mode-gated pair that also plays a sound/vfx cue through
 * 0x1005AC60, or the plain fallback 0x10063A60), then builds the four wheel
 * display matrices from the car's own body matrix (+0x220, the combined
 * matrix br_carphys.c's BrCarPhysBuildCombined writes) composed with a
 * steer/roll/camber rotation per wheel, and positions each wheel's visual
 * anchor with BrMat4TransformPoint (0x1006DA20).  The front two wheels also
 * fold in a steer angle scaled by a fixed constant before the roll/camber
 * pair.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_race.h"   /* br_globals: its objects */
#include <stdint.h>
#include "slice3_41.h"


/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x105CCB88 replay-mode flag       */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x100A9360 game mode              */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* body-roll decay, first pass       */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* the wheel steer-angle scale        */

typedef struct BrMat4_ { float m[16]; } BrMat4_;
typedef struct BrVec3_ { float x, y, z; } BrVec3_;

/* FUN_10063ca0: prototype in br_funcs.h */
/* FUN_1005ac60: prototype in br_funcs.h */
/* FUN_10063b80: prototype in br_funcs.h */
/* FUN_10063a60: prototype in br_funcs.h */

/* Axis-angle rotation matrix builder: (pOut, angle, axisX, axisY, axisZ).
 * Always called with a one-hot axis in this function (X, Y or Z). */
/* FUN_1002a590: prototype in br_funcs.h */
/* 0x10029D70 BrMat4Mul (geometry/br_mat.c, byte-exact): out = a * b, the
 * OUTPUT LAST. */
/* FUN_10029d70: prototype in br_funcs.h */
/* 64-bit core: declared once, by its definition's header */ /* 0x1006DA20 */


/* WHAT IT DOES: one wheel-visuals pass.  Picks the state update (a net-mode
 * pair that also fires a cue through 0x1005AC60, or the plain fallback),
 * then for each of the four wheels builds a roll/steer matrix, composes it
 * with the car's combined matrix, and places the wheel's visual anchor
 * through the result -- the front pair additionally folding in the steer
 * angle (scaled) before the roll.  Two body-roll fields decay by a fixed
 * amount either side of the wheel work. */
/* Retranscribed from the bytes once both callees were matched: the anchor
 * transforms read the CAR's combined matrix (+0x220), not the rotation
 * temp; BrMat4Mul takes its output last; three matrix locals (0xC0 frame);
 * the replay flag is tested first; float fields read through a float
 * pointer (a char-offset cast moves them through the x87 instead of
 * integer registers).  The closing decay subtracts a LITERAL 0.254f (the
 * original's 0x10077794): an extern could alias the car fields, so VC5 kept
 * each subtract behind the previous store; a literal lets all four loads
 * and subtracts run ahead of the stores, as the original does. */
/* @implements 0x1005ACE0 glide BrCarWheelSteerStep_1005ACE0 */
void __fastcall BrCarWheelSteerStep_1005ACE0(BrDriverCar *pCar)
{
    BrMat4_ rot, rot2, rot3;
    float  *pf = &pCar->fwd.x;

    if ((*(int *)&DAT_105ccb68[8]) != 0) {
        BrReplayApplyCar(pCar);
        BrCarBuildMatrices(pCar);
    } else if ((*(int *)&g_brRaceRules.mode) == 2 && ((pCar->f140)) == 1 &&
               pCar->pCtl->pHdr != 0) {
        BrReplayApplyCar(pCar);
        BrCarBuildMatrices(pCar);
    } else if ((*(int *)&g_brRaceRules.mode) == 4 && ((pCar->f140)) == 0 &&
               pCar->pCtl->pHdr != 0) {
        BrReplayApply(pCar, 1);
        BrCarBuildMatrices(pCar);
    } else {
        BrReplayRecord(pCar);
    }

    BrMat4RotateAxis(&rot, pf[0x338 / 4], 1.0f, 0.0f, 0.0f);
    BrMat4Mul(&rot, (const BrMat4_ *)&pCar->aBody[0].rb.m.m[0], (BrMat4_ *)&pCar->fwd.x);
    pf[0x464 / 4] = pf[0x464 / 4] - DAT_1007778c;
    pf[0x670 / 4] = pf[0x670 / 4] - DAT_1007778c;
    pf[0x87c / 4] = pf[0x87c / 4] - DAT_1007778c;
    pf[0xa88 / 4] = pf[0xa88 / 4] - DAT_1007778c;

    BrMat4RotateAxis(&rot, pf[0x544 / 4], 0.0f, 1.0f, 0.0f);
    BrMat4Mul(&rot, (const BrMat4_ *)&pCar->aBody[0].rb.m.m[0], (BrMat4_ *)&pCar->aWheel[2].m[0]);
    BrMat4TransformPoint((BrVec3_ *)&pCar->aWheel[2].m[3], (const BrMat4_ *)&pCar->aBody[0].rb.m.m[0], (const BrVec3_ *)&pCar->aBody[1].rb.m.m[3]);

    BrMat4RotateAxis(&rot, pf[0x95c / 4], 0.0f, 1.0f, 0.0f);
    BrMat4Mul(&rot, (const BrMat4_ *)&pCar->aBody[0].rb.m.m[0], (BrMat4_ *)&pCar->aWheel[3].m[0]);
    BrMat4TransformPoint((BrVec3_ *)&pCar->aWheel[3].m[3], (const BrMat4_ *)&pCar->aBody[0].rb.m.m[0], (const BrVec3_ *)&pCar->aBody[3].rb.m.m[3]);

    BrMat4RotateAxis(&rot3, pf[0x750 / 4], 0.0f, 1.0f, 0.0f);
    BrMat4RotateAxis(&rot2, pf[0x73c / 4] * DAT_10077790, 0.0f, 0.0f, 1.0f);
    BrMat4Mul(&rot3, &rot2, &rot);
    BrMat4Mul(&rot, (const BrMat4_ *)&pCar->aBody[0].rb.m.m[0], (BrMat4_ *)&pCar->aWheel[1].m[0]);
    BrMat4TransformPoint((BrVec3_ *)&pCar->aWheel[1].m[3], (const BrMat4_ *)&pCar->aBody[0].rb.m.m[0], (const BrVec3_ *)&pCar->aBody[2].rb.m.m[3]);

    BrMat4RotateAxis(&rot3, pf[0xb68 / 4], 0.0f, 1.0f, 0.0f);
    BrMat4RotateAxis(&rot2, pf[0xb54 / 4] * DAT_10077790, 0.0f, 0.0f, 1.0f);
    BrMat4Mul(&rot3, &rot2, &rot);
    BrMat4Mul(&rot, (const BrMat4_ *)&pCar->aBody[0].rb.m.m[0], (BrMat4_ *)&pCar->aWheel[0].m[0]);
    BrMat4TransformPoint((BrVec3_ *)&pCar->aWheel[0].m[3], (const BrMat4_ *)&pCar->aBody[0].rb.m.m[0], (const BrVec3_ *)&pCar->aBody[4].rb.m.m[3]);

    pf[0x464 / 4] = pf[0x464 / 4] - 0.254f;
    pf[0x670 / 4] = pf[0x670 / 4] - 0.254f;
    pf[0x87c / 4] = pf[0x87c / 4] - 0.254f;
    pf[0xa88 / 4] = pf[0xa88 / 4] - 0.254f;
}

