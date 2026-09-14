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
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>

#ifdef BR_MATCHING_BUILD

extern int   DAT_105ccb88;      /* 0x105CCB88 replay-mode flag       */
extern int   DAT_100a9360;      /* 0x100A9360 game mode              */
extern float DAT_1007778c;      /* body-roll decay, first pass       */
extern float DAT_10077794;      /* body-roll decay, second pass      */
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
/* 4x4 matrix multiply, dst = a * b (VC5 argument order TBD -- not yet
 * cross-checked against a matched sibling). */
void FUN_10029d70(BrMat4_ *pDst, const BrMat4_ *pA, const BrMat4_ *pB);
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
/* T2, not yet byte-exact: 786/776 B, REGNORM 21+15 -- close for a function
 * whose two callees, FUN_1002a590 (axis-angle rotation build) and
 * FUN_10029d70 (matrix multiply), are themselves still unmatched leaves
 * (declared here, not implemented).  Residue includes a `mov [R+I]` /
 * `fld [R+I]` swap around the call sites, most likely the BrMat4_* output
 * argument's exact passing convention -- not resolvable with confidence
 * before those two callees are matched themselves. Transcribed field-for-
 * field from the Ghidra draft. */
/* @implements 0x1005ACE0 glide BrCarWheelSteerStep_1005ACE0 */
void __fastcall BrCarWheelSteerStep_1005ACE0(int pCar)
{
    BrMat4_ rot, rot2;
    int     combined = pCar + 0x220;

    if (DAT_105ccb88 == 0) {
        if (DAT_100a9360 == 2 && CI(pCar, 0x140) == 1 &&
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
    } else {
        FUN_10063ca0(pCar);
        FUN_1005ac60(pCar);
    }

    FUN_1002a590(&rot, CF(pCar, 0x338), 1.0f, 0.0f, 0.0f);
    FUN_10029d70(&rot, (const BrMat4_ *)combined, (const BrMat4_ *)pCar);

    CF(pCar, 0x464) = CF(pCar, 0x464) - DAT_1007778c;
    CF(pCar, 0x670) = CF(pCar, 0x670) - DAT_1007778c;
    CF(pCar, 0x87c) = CF(pCar, 0x87c) - DAT_1007778c;
    CF(pCar, 0xa88) = CF(pCar, 0xa88) - DAT_1007778c;

    FUN_1002a590(&rot, CF(pCar, 0x544), 0.0f, 1.0f, 0.0f);
    FUN_10029d70(&rot, (const BrMat4_ *)combined, (const BrMat4_ *)(pCar + 0xc0));
    BrMat4TransformPoint((BrVec3_ *)(pCar + 0xf0), &rot, (const BrVec3_ *)(pCar + 0x45c));

    FUN_1002a590(&rot, CF(pCar, 0x95c), 0.0f, 1.0f, 0.0f);
    FUN_10029d70(&rot, (const BrMat4_ *)combined, (const BrMat4_ *)(pCar + 0x100));
    BrMat4TransformPoint((BrVec3_ *)(pCar + 0x130), &rot, (const BrVec3_ *)(pCar + 0x874));

    FUN_1002a590(&rot2, CF(pCar, 0x750), 0.0f, 1.0f, 0.0f);
    FUN_1002a590(&rot, CF(pCar, 0x73c) * DAT_10077790, 0.0f, 0.0f, 1.0f);
    FUN_10029d70(&rot2, &rot2, &rot);
    FUN_10029d70(&rot2, (const BrMat4_ *)combined, (const BrMat4_ *)(pCar + 0x80));
    BrMat4TransformPoint((BrVec3_ *)(pCar + 0xb0), &rot2, (const BrVec3_ *)(pCar + 0x668));

    FUN_1002a590(&rot2, CF(pCar, 0xb68), 0.0f, 1.0f, 0.0f);
    FUN_1002a590(&rot, CF(pCar, 0xb54) * DAT_10077790, 0.0f, 0.0f, 1.0f);
    FUN_10029d70(&rot2, &rot2, &rot);
    FUN_10029d70(&rot2, (const BrMat4_ *)combined, (const BrMat4_ *)(pCar + 0x40));
    BrMat4TransformPoint((BrVec3_ *)(pCar + 0x70), &rot2, (const BrVec3_ *)(pCar + 0xa80));

    {
        float f670 = CF(pCar, 0x670) - DAT_10077794;
        float f87c = CF(pCar, 0x87c) - DAT_10077794;
        float fa88 = CF(pCar, 0xa88) - DAT_10077794;

        CF(pCar, 0x464) = CF(pCar, 0x464) - DAT_10077794;
        CF(pCar, 0x670) = f670;
        CF(pCar, 0x87c) = f87c;
        CF(pCar, 0xa88) = fa88;
    }
}

#endif /* BR_MATCHING_BUILD */
