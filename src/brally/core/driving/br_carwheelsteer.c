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
#include <stdint.h>


extern int   DAT_105ccb88;      /* 0x105CCB88 replay-mode flag       */
extern int   DAT_100a9360;      /* 0x100A9360 game mode              */
extern float DAT_1007778c;      /* body-roll decay, first pass       */
extern float DAT_10077790;      /* the wheel steer-angle scale        */

typedef struct BrMat4_ { float m[16]; } BrMat4_;
typedef struct BrVec3_ { float x, y, z; } BrVec3_;

void FUN_10063ca0(int pCar);
void FUN_1005ac60(int pCar);
void FUN_10063b80(int pCar, int a);
void FUN_10063a60(int pCar);

/* Axis-angle rotation matrix builder: (pOut, angle, axisX, axisY, axisZ).
 * Always called with a one-hot axis in this function (X, Y or Z). */
void FUN_1002a590(BrMat4_ *pOut, float angle, float ax, float ay, float az);
/* 0x10029D70 BrMat4Mul (geometry/br_mat.c, byte-exact): out = a * b, the
 * OUTPUT LAST. */
void FUN_10029d70(const BrMat4_ *pA, const BrMat4_ *pB, BrMat4_ *pOut);
void BrMat4TransformPoint(BrVec3_ *pOut, const BrMat4_ *pM, const BrVec3_ *pV); /* 0x1006DA20 */

#define CF(p, off)  (*(float *)((char *)(p) + (off)))
#define CI(p, off)  (*(int   *)((char *)(p) + (off)))

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
void __fastcall BrCarWheelSteerStep_1005ACE0(int pCar)
{
    BrMat4_ rot, rot2, rot3;
    float  *pf = (float *)pCar;

    if (DAT_105ccb88 != 0) {
        FUN_10063ca0(pCar);
        FUN_1005ac60(pCar);
    } else if (DAT_100a9360 == 2 && CI(pCar, 0x140) == 1 &&
               CI(CI(pCar, 0x29c0), 0x44) != 0) {
        FUN_10063ca0(pCar);
        FUN_1005ac60(pCar);
    } else if (DAT_100a9360 == 4 && CI(pCar, 0x140) == 0 &&
               CI(CI(pCar, 0x29c0), 0x44) != 0) {
        FUN_10063b80(pCar, 1);
        FUN_1005ac60(pCar);
    } else {
        FUN_10063a60(pCar);
    }

    FUN_1002a590(&rot, pf[0x338 / 4], 1.0f, 0.0f, 0.0f);
    FUN_10029d70(&rot, (const BrMat4_ *)(pCar + 0x220), (BrMat4_ *)pCar);
    pf[0x464 / 4] = pf[0x464 / 4] - DAT_1007778c;
    pf[0x670 / 4] = pf[0x670 / 4] - DAT_1007778c;
    pf[0x87c / 4] = pf[0x87c / 4] - DAT_1007778c;
    pf[0xa88 / 4] = pf[0xa88 / 4] - DAT_1007778c;

    FUN_1002a590(&rot, pf[0x544 / 4], 0.0f, 1.0f, 0.0f);
    FUN_10029d70(&rot, (const BrMat4_ *)(pCar + 0x220), (BrMat4_ *)(pCar + 0xc0));
    BrMat4TransformPoint((BrVec3_ *)(pCar + 0xf0), (const BrMat4_ *)(pCar + 0x220), (const BrVec3_ *)(pCar + 0x45c));

    FUN_1002a590(&rot, pf[0x95c / 4], 0.0f, 1.0f, 0.0f);
    FUN_10029d70(&rot, (const BrMat4_ *)(pCar + 0x220), (BrMat4_ *)(pCar + 0x100));
    BrMat4TransformPoint((BrVec3_ *)(pCar + 0x130), (const BrMat4_ *)(pCar + 0x220), (const BrVec3_ *)(pCar + 0x874));

    FUN_1002a590(&rot3, pf[0x750 / 4], 0.0f, 1.0f, 0.0f);
    FUN_1002a590(&rot2, pf[0x73c / 4] * DAT_10077790, 0.0f, 0.0f, 1.0f);
    FUN_10029d70(&rot3, &rot2, &rot);
    FUN_10029d70(&rot, (const BrMat4_ *)(pCar + 0x220), (BrMat4_ *)(pCar + 0x80));
    BrMat4TransformPoint((BrVec3_ *)(pCar + 0xb0), (const BrMat4_ *)(pCar + 0x220), (const BrVec3_ *)(pCar + 0x668));

    FUN_1002a590(&rot3, pf[0xb68 / 4], 0.0f, 1.0f, 0.0f);
    FUN_1002a590(&rot2, pf[0xb54 / 4] * DAT_10077790, 0.0f, 0.0f, 1.0f);
    FUN_10029d70(&rot3, &rot2, &rot);
    FUN_10029d70(&rot, (const BrMat4_ *)(pCar + 0x220), (BrMat4_ *)(pCar + 0x40));
    BrMat4TransformPoint((BrVec3_ *)(pCar + 0x70), (const BrMat4_ *)(pCar + 0x220), (const BrVec3_ *)(pCar + 0xa80));

    pf[0x464 / 4] = pf[0x464 / 4] - 0.254f;
    pf[0x670 / 4] = pf[0x670 / 4] - 0.254f;
    pf[0x87c / 4] = pf[0x87c / 4] - 0.254f;
    pf[0xa88 / 4] = pf[0xa88 / 4] - 0.254f;
}

