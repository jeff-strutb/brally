/* slice3_42.c -- decompiled from BRD3D.dll, packet 0x100695D0-0x1006CCD0.
 *
 * See slice3_42.h for what each routine does and why.  Everything in this
 * file was traced instruction by instruction.  Five of the packet's 29
 * functions are absent on purpose and reported rather than guessed:
 * 0x1006A8A0 (Win32 registry), and the four large x87 physics routines
 * 0x1006B5F0, 0x1006C1F0, 0x1006C9D0 and 0x1006CCD0.
 *
 * FLOAT PRECISION.  This paragraph used to read "the original is x87 with a
 * 64-bit mantissa ... where [the spills] only affect the last ulp they are
 * not [reproduced].  Flagged once here rather than at every line."  The
 * mantissa is 53 bits, not 64: the CRT's x87 control word is 0x027F (see
 * CONVENTIONS.md), so an unspilled intermediate is EXACTLY a C `double` and
 * computing it in `float` rounds where the original does not.  There is no
 * last-ulp band inside which the spills stop mattering, which is what the old
 * wording licensed.
 *
 * The rule here: unspilled intermediates are `double`; every point where the
 * original stores to a 4-byte slot rounds through a `float` temporary,
 * because the store is the rounding.  Spill points are recorded per function
 * with the instruction that makes them.
 */

#include <string.h>

#include "slice3_42.h"

/* =====================================================================
 * .rdata constants, read out of reference/brally/orig/BRD3D.dll rather than assumed.
 * ===================================================================== */

#define BR_K_0008FA54   0.0f    /* 0x1008FA54 */
#define BR_K_0008FA58   2.0f    /* 0x1008FA58 */
#define BR_K_0008FA5C   1.0f    /* 0x1008FA5C */
#define BR_K_0008FAA8  30.0f    /* 0x1008FAA8 -- the simulation rate */

/* =====================================================================
 * 1. 0x100695D0
 * ===================================================================== */

/* 0x100695D0 / glide 0x10062640 BrMat4FromCarState now lives in
 * src/brally/core/geometry/br_quatmat.c. */

/* 2. The control-binding object (g_BrCtrlDefaults, g_BrCtrlCfg,
 * BrCtrlCfgInit/Copy/Assign) is filed in src/brally/core/controls/br_ctrlcfgobj.c. */

/* =====================================================================
 * 4. 0x1006AE20
 * ===================================================================== */

BrFxRecord g_BrFx1750338[BR_FX_RECORDS];
uint32_t   g_BrFx1754E50[BR_FX_PAIRS][2];
int32_t    g_BrX1754E38;
int32_t    g_BrX17554A0, g_BrX17554A4;
int32_t    g_BrX17554C8, g_BrX17554CC;
int32_t    g_BrX17554D0, g_BrX17554D4;
int32_t    g_BrX17554D8, g_BrX17554DC;
int32_t    g_BrX17554E0, g_BrX17554E4;

void BrFxClearAll(void)
{
    int i;

    g_BrX17554C8 = 0;
    g_BrX17554A0 = 0;
    g_BrX17554CC = 0;
    g_BrX17554A4 = 0;
    g_BrX17554D0 = 0;
    g_BrX17554D8 = 0;
    g_BrX17554D4 = 0;
    g_BrX17554DC = 0;

    /* GOTCHA: seven dwords per record, but the cursor advances by eight.
     * f1C is deliberately (or at least reproducibly) left alone. */
    for (i = 0; i < BR_FX_RECORDS; ++i) {
        g_BrFx1750338[i].f00 = 0;
        g_BrFx1750338[i].f04 = 0;
        g_BrFx1750338[i].f08 = 0;
        g_BrFx1750338[i].f0C = 0;
        g_BrFx1750338[i].f10 = 0;
        g_BrFx1750338[i].f14 = 0;
        g_BrFx1750338[i].f18 = 0;
    }

    for (i = 0; i < BR_FX_PAIRS; ++i) {
        g_BrFx1754E50[i][0] = 0;
        g_BrFx1754E50[i][1] = 0;
    }

    g_BrX1754E38 = 0;
    g_BrX17554E4 = 0;
    g_BrX17554E0 = 0;
}

/* =====================================================================
 * 5. Rigid-body force accumulation
 * ===================================================================== */

static BrVec3 BrS42Cross(const BrVec3 *pA, const BrVec3 *pB)
{
    BrVec3 r;
    r.x = pA->y * pB->z - pA->z * pB->y;
    r.y = pA->z * pB->x - pA->x * pB->z;
    r.z = pA->x * pB->y - pA->y * pB->x;
    return r;
}

/* 0x1006AEB0.  In the matching build the definition lives in
 * br_rbaccum.c (glide 0x10063E60), where the stale-slot GOTCHA is kept
 * rather than zeroed. */
void BrRbAccumOwnForces(BrRbBodyFull *pB)
{
    const BrRbForce *pN;
    /* DEVIATION: the original's `v` is an uninitialised stack slot that is
     * only written on the kind==0 and kind==1 paths, so a kind >= 2 node uses
     * whatever the previous node left there -- and uninitialised stack on the
     * first node.  Reading uninitialised storage is undefined in C, so this
     * starts at zero.  The carry-over between nodes IS reproduced. */
    BrVec3 v;
    v.x = 0.0f; v.y = 0.0f; v.z = 0.0f;

    for (pN = pB->pForces; pN != NULL; pN = pN->pNext) {
        if (pN->kind == 0) {
            v = pN->f;
        } else if (pN->kind == 1) {
            BrMat4MulVec3Transposed(&v, &pB->m, &pN->f);
        }

        pB->accel.x += v.x;
        pB->accel.y += v.y;
        pB->accel.z += v.z;

        if (pB->mode != 2) {
            BrVec3 r, t;
            BrMat4MulVec3Transposed(&r, &pB->m, &pN->r);
            t = BrS42Cross(&r, &v);
            pB->angAccel.x += t.x;
            pB->angAccel.y += t.y;
            pB->angAccel.z += t.z;
        }
    }
}

/* 0x1006AFF0 -- port form; the matching arm is in src/brally/core/driving/br_rbaccum.c. */
void BrRbAccumChildForces(BrRbBodyFull *pParent, BrRbBodyFull *pChild)
{
    const BrRbForce *pN;
    /* Same stale-slot construction as above; same DEVIATION. */
    BrVec3 v;
    v.x = 0.0f; v.y = 0.0f; v.z = 0.0f;

    for (pN = pChild->pForces; pN != NULL; pN = pN->pNext) {
        BrVec3 flat, b;

        if (pN->kind == 0)
            BrMat4MulVec3(&v, &pParent->m, &pN->f);
        if (pN->kind == 1)
            v = pN->f;

        /* Computed BEFORE the force is folded in and BEFORE the f1B4 test,
         * from the pre-accumulation v.  Z is dropped. */
        flat.x = v.x;
        flat.y = v.y;
        flat.z = 0.0f;
        BrMat4MulVec3Transposed(&b, &pParent->m, &flat);

        pChild->accel.x += v.x;
        pChild->accel.y += v.y;
        pChild->accel.z += v.z;

        if (pChild->f1B4 != 0.0f) {
            BrVec3 lever, a, t;

            /* 0xEC..0xF4 == m[3][0..2], the child's translation row. */
            lever.x = pChild->m.m[BR_S42_LEVER_ROW][0];
            lever.y = pChild->m.m[BR_S42_LEVER_ROW][1];
            lever.z = pChild->m.m[BR_S42_LEVER_ROW][2];
            BrMat4MulVec3Transposed(&a, &pParent->m, &lever);

            t = BrS42Cross(&a, &b);
            pParent->angAccel.x += t.x;
            pParent->angAccel.y += t.y;
            pParent->angAccel.z += t.z;
        }
    }
}


/* 0x1006B260 BrRbAccumAll now lives in src/brally/core/driving/br_rbaccum.c. */

/* 0x1006B510 BrRbVelAtPoint, 0x1006B430 BrRbVelAtBodyPoint and 0x1006B340
 * BrRbVelAtBodyPointXY are filed in src/brally/core/driving/br_rbvel.c. */

/* -- Ghidra-matched functions --------------------------- */
