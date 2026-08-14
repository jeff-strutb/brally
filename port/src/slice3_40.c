/* slice3_40.c -- Boss Rally (BRD3D.dll) slice 3, a later pass.
 *
 * See slice3_40.h for the packet contents, the offset maps and every
 * GOTCHA.  This file carries only the code and the line-level notes.
 *
 * x87 NOTE (applies throughout).  The original is MSVC x87 code and the CRT
 * leaves the precision control at 53 bits, so an intermediate the original
 * never spills carries double precision, not float.  Wherever that is
 * observable the intermediate is a `double` here and the rounding to float
 * happens exactly where the original has an `fstp dword`.  Where the
 * original stores every intermediate, plain float is used.
 */

#include <string.h>

#include "slice3_40.h"

/* ------------------------------------------------------------------ */
/* Byte-offset accessors into the car record.                          */
/* BrCar's first member is a byte array, so &pCar->a0000 == pCar and    */
/* the struct is at least 4-aligned (it contains floats), which every   */
/* offset used below is a multiple of.                                  */
/* ------------------------------------------------------------------ */
#define CAR_BYTES(c)     ((uint8_t *)(void *)(c))
#define CAR_AT(c, off)   ((void *)(CAR_BYTES(c) + (off)))
#define CAR_U8(c, off)   (*(uint8_t  *)CAR_AT(c, off))
#define CAR_U16(c, off)  (*(uint16_t *)CAR_AT(c, off))
#define CAR_I32(c, off)  (*(int32_t  *)CAR_AT(c, off))
#define CAR_U32(c, off)  (*(uint32_t *)CAR_AT(c, off))
#define CAR_F32(c, off)  (*(float    *)CAR_AT(c, off))
#define CAR_PTR(c, off)  (*(void *   *)CAR_AT(c, off))

/* ------------------------------------------------------------------ */
/* Globals owned by this slice.                                        */
/* ------------------------------------------------------------------ */

/* 0x100ADF68 -- ten dwords, linear 0..255.  The code reads only the low
 * byte of the selected dword. */
const int32_t BrOptLevelATable[BR_OPT_LEVEL_STEPS] = {
    0, 0x1C, 0x38, 0x55, 0x71, 0x8D, 0xAA, 0xC6, 0xE2, 0xFF
};

/* 0x100ADF90 -- ten dwords, a perceptual curve. */
const int32_t BrOptLevelBTable[BR_OPT_LEVEL_STEPS] = {
    0, 0x55, 0x78, 0x93, 0xAA, 0xBE, 0xD0, 0xE1, 0xF0, 0xFF
};

int32_t   BrG_6909B4;      /* 0x106909B4 */
BrNode   *BrG_6C7CB8;      /* 0x106C7CB8 */
uint8_t   BrG_0BBAD8;      /* 0x100BBAD8 */

BrPathSeg BrPathSegs[BR_PATH_SEG_MAX];  /* 0x106C7CE0 */
int32_t   BrPathSegCount;               /* 0x106C7DA8 */

int32_t   BrPathCrossCount;             /* 0x10AF96C0 */
int32_t   BrPathWrapCount;              /* 0x10AF9B44 */
BrVec3    BrPathWalkPoint;              /* 0x10AF9B38 */
BrNode   *BrPathWalkNode;               /* 0x10AF988C */
int32_t   BrPathWalkIndex;              /* 0x10ACD490 */

/* Float constants, read out of BRD3D.dll .rdata rather than assumed. */
#define BR_K_08F7A8    0.0f            /* 0x1008F7A8 */
#define BR_K_08F7B0 (-1000.0f)         /* 0x1008F7B0 -- SUBTRACTED, so +1000 */
#define BR_K_08F9AC    0.137f          /* 0x1008F9AC */
#define BR_K_08F9B0 (-0.034f)          /* 0x1008F9B0 -- SUBTRACTED, so +0.034 */
#define BR_K_08F9B4    0.15f           /* 0x1008F9B4 */

/* ==================================================================== */
/* 1. Network car-state apply / predict                                 */
/* ==================================================================== */

/* 0x100609E0 */
void BrCarNetSendState(BrCar *pCar)
{
    BrCarState state;   /* the original's 0xA0-byte stack buffer */

    BrSub100607B0(&state, pCar);
    BrNetCarStateSend(&state);
    /* GOTCHA: BrNetCarStateSend's int result is discarded here. */
}

/* 0x10060A10 */
void BrCarApplyState(BrCar *pCar, const BrCarState *pState)
{
    /* --- the BrRbState at pCar+0x1DC ------------------------------- */
    /* quaternion, scalar first (slice3_44 pins the ordering) */
    CAR_F32(pCar, 0x1F4) = pState->f00;
    CAR_F32(pCar, 0x1F8) = pState->f04;
    CAR_F32(pCar, 0x1FC) = pState->f08;
    CAR_F32(pCar, 0x200) = pState->f0C;
    /* position */
    CAR_F32(pCar, 0x1DC) = pState->f10;
    CAR_F32(pCar, 0x1E0) = pState->f14;
    CAR_F32(pCar, 0x1E4) = pState->f18;

    /* The original makes this call HERE, between the f18 and f1C stores,
     * not before or after the block.  Order preserved in case it reads
     * what has been written so far. */
    BrSub100695D0(CAR_AT(pCar, 0x220), pState);

    /* velocity */
    CAR_F32(pCar, 0x1E8) = pState->f1C;
    CAR_F32(pCar, 0x1EC) = pState->f20;
    CAR_F32(pCar, 0x1F0) = pState->f24;
    /* angular velocity */
    CAR_F32(pCar, 0x204) = pState->f28;
    CAR_F32(pCar, 0x208) = pState->f2C;
    CAR_F32(pCar, 0x20C) = pState->f30;

    /* --- seven scattered dwords, copied verbatim ------------------- */
    CAR_F32(pCar, 0x338) = pState->f34;
    CAR_F32(pCar, 0x73C) = pState->f38;
    CAR_F32(pCar, 0xB54) = pState->f38;   /* GOTCHA: f38 stored twice */
    CAR_F32(pCar, 0x544) = pState->f3C;
    CAR_F32(pCar, 0x95C) = pState->f40;
    CAR_F32(pCar, 0x750) = pState->f44;
    CAR_F32(pCar, 0xB68) = pState->f48;

    /* --- four truncated to int32 ----------------------------------- */
    CAR_I32(pCar, 0x524) = BrFtolTrunc(pState->f4C);
    CAR_I32(pCar, 0x93C) = BrFtolTrunc(pState->f50);
    CAR_I32(pCar, 0x730) = BrFtolTrunc(pState->f54);
    CAR_I32(pCar, 0xB48) = BrFtolTrunc(pState->f58);

    /* --- five truncated and narrowed to a byte (the original keeps
     *     only AL, so the wrap is a truncation of the low dword) ----- */
    CAR_U8(pCar, 0x510) = (uint8_t)BrFtolTrunc(pState->f5C);
    CAR_U8(pCar, 0x928) = (uint8_t)BrFtolTrunc(pState->f60);
    CAR_U8(pCar, 0x71C) = (uint8_t)BrFtolTrunc(pState->f64);
    CAR_U8(pCar, 0xB34) = (uint8_t)BrFtolTrunc(pState->f68);
    CAR_U8(pCar, 0x36D) = (uint8_t)BrFtolTrunc(pState->f6C);

    /* --- two "== 0.0f" booleans ------------------------------------ */
    /* Both are `fcomp 0.0f` + `test ah,0x40`, i.e. the C3 bit alone.  An
     * unordered compare sets C3 as well, so a NaN takes the EQUAL branch.
     * Written as "not (< 0 or > 0)" so that holds in C too. */
    {
        uint32_t *pFlags = (uint32_t *)CAR_PTR(pCar, 0x29C0);
        uint32_t  v = *pFlags;
        if (pState->f70 < BR_K_08F7A8 || pState->f70 > BR_K_08F7A8) {
            v |= 0x00040000u;
        } else {
            v &= 0xFFFBFFFFu;     /* equal, or NaN -> CLEAR bit 0x40000 */
        }
        *pFlags = v;
    }
    CAR_F32(pCar, 0xE68) =
        (pState->f74 < BR_K_08F7A8 || pState->f74 > BR_K_08F7A8)
            ? -1.0f : 1.0f;

    /* --- the pCar+0xFF4 guard -------------------------------------- */
    {
        float f = CAR_F32(pCar, 0xFF4);
        int   take;
        if (!(f > BR_K_08F7A8)) {
            /* `test ah,0x41` after fcomp: equal-or-less, NaN included */
            take = 1;
        } else {
            /* the extra 1000 is held in an x87 register, never stored.
             * `test ah,0x41` again folds unordered in with less-or-equal,
             * so a NaN here does NOT take the overwrite. */
            double biased = (double)f - (double)BR_K_08F7B0;
            take = (biased > (double)pState->f78);
        }
        if (take) {
            CAR_F32(pCar, 0xFF4) = pState->f78;
        }
    }

    CAR_F32(pCar, 0xE24) = pState->f7C;

    /* --- eight more truncated bytes -------------------------------- */
    CAR_U8(pCar, 0x362) = (uint8_t)BrFtolTrunc(pState->f80);
    CAR_U8(pCar, 0x363) = (uint8_t)BrFtolTrunc(pState->f84);
    CAR_U8(pCar, 0x36C) = (uint8_t)BrFtolTrunc(pState->f88);
    CAR_U8(pCar, 0x366) = (uint8_t)BrFtolTrunc(pState->f8C);
    CAR_U8(pCar, 0x367) = (uint8_t)BrFtolTrunc(pState->f90);
    CAR_U8(pCar, 0x368) = (uint8_t)BrFtolTrunc(pState->f94);
    CAR_U8(pCar, 0x369) = (uint8_t)BrFtolTrunc(pState->f98);
    CAR_U8(pCar, 0x36A) = (uint8_t)BrFtolTrunc(pState->f9C);

    /* --- close the rigid-body state and snapshot it twice ---------- */
    BrRbQuatDerivative((BrRbState *)CAR_AT(pCar, 0x1DC));
    memcpy(CAR_AT(pCar, 0x278), CAR_AT(pCar, 0x1DC), 0x44);
    memcpy(CAR_AT(pCar, 0x2BC), CAR_AT(pCar, 0x1DC), 0x44);
}

/* 0x10060CC0 */
int32_t BrCarPredictRemote(BrCar *pCar, int32_t slot)
{
    BrCarState state;   /* the original's 0xA0-byte stack buffer */

    if (slot == BrSub10005D30()) {
        return 1;
    }
    if (BrG_6909B4 != 0) {
        return 1;
    }
    if (!BrNetSlotPredictOrig(&state, slot)) {
        return 0;
    }
    BrCarApplyState(pCar, &state);
    BrCarBuildMatrices(pCar);
    return 1;
}

/* ==================================================================== */
/* 2. Two 10-step option sliders                                        */
/* ==================================================================== */

/* DEVIATION (all four): the original indexes its table with whatever the
 * index global holds and would read out of bounds if it were ever outside
 * 0..9.  Both this packet and slice2_25's cyclers keep it in range, so the
 * clamp below is unreachable in practice; it is here so a corrupted global
 * cannot turn into undefined behaviour in C. */
static int32_t BrOptClampIndex(int32_t i)
{
    if (i < 0) {
        return 0;
    }
    if (i > BR_OPT_LEVEL_STEPS - 1) {
        return BR_OPT_LEVEL_STEPS - 1;
    }
    return i;
}

/* 0x10060D50 */
void BrOptLevelAStepUp(void)
{
    int32_t i = g_brB4E70C;
    if (i < 9) {
        g_brB4E70C = ++i;
    }
    /* the lookup runs even when the index did not move */
    BrG_0BBAD8 = (uint8_t)BrOptLevelATable[BrOptClampIndex(i)];
}

/* 0x10060D70 */
void BrOptLevelAStepDown(void)
{
    int32_t i = g_brB4E70C;
    if (i > 0) {
        g_brB4E70C = --i;
    }
    BrG_0BBAD8 = (uint8_t)BrOptLevelATable[BrOptClampIndex(i)];
}

/* 0x10060DC0 */
void BrOptLevelBStepUp(void)
{
    int32_t i = g_brB4E708;
    if (i < 9) {
        g_brB4E708 = ++i;
    }
    BrSndMasterVolume = (uint8_t)BrOptLevelBTable[BrOptClampIndex(i)];
}

/* 0x10060DE0 */
void BrOptLevelBStepDown(void)
{
    int32_t i = g_brB4E708;
    if (i > 0) {
        g_brB4E708 = --i;
    }
    BrSndMasterVolume = (uint8_t)BrOptLevelBTable[BrOptClampIndex(i)];
}

/* ==================================================================== */
/* 3. Controller-state translation                                      */
/* ==================================================================== */

/* 0x10060E00 -- name and `void *` parameter taken from slice2_18.h's
 * existing extern so the two declarations agree. */
void BrGfx60E00(void *p0)
{
    uint8_t *pOut = (uint8_t *)p0;
    int32_t  axis0 = 0;
    int32_t  axis1 = 0;
    uint32_t f;

    f = BrSub100773F0(&axis0, &axis1);

    /* The two axis bytes are written BEFORE the flag word is cleared in
     * the original; they are disjoint, but the order is kept. */
    pOut[2] = (uint8_t)axis0;
    pOut[3] = (uint8_t)axis1;

    pOut[0] = 0;
    pOut[1] = 0;
    if (f & 0x0010u) {                 /* assignment, not OR */
        pOut[0] = 0x00;
        pOut[1] = 0x84;
    }
    if (f & 0x0004u) { pOut[1] = (uint8_t)(pOut[1] | 0x88u); }  /* two bits */
    if (f & 0x0001u) { pOut[1] = (uint8_t)(pOut[1] | 0x02u); }
    if (f & 0x0002u) { pOut[1] = (uint8_t)(pOut[1] | 0x01u); }
    if (f & 0x0008u) { pOut[1] = (uint8_t)(pOut[1] | 0x40u); }
    if (f & 0x0100u) { pOut[0] = (uint8_t)(pOut[0] | 0x08u); }
    if (f & 0x0200u) { pOut[0] = (uint8_t)(pOut[0] | 0x02u); }
    if (f & 0x0400u) { pOut[0] = (uint8_t)(pOut[0] | 0x04u); }
    if (f & 0x8000u) { pOut[1] = (uint8_t)(pOut[1] | 0x10u); }
    if (f & 0x0020u) { pOut[0] = (uint8_t)(pOut[0] | 0x10u); }
    if (f & 0x0040u) { pOut[0] = (uint8_t)(pOut[0] | 0x20u); }
    /* bits 0x0800..0x4000 are never examined */
}

/* ==================================================================== */
/* 4. The node 0x8000 mark / clear pass                                 */
/* ==================================================================== */

/* 0x10061660 */
void BrNodeMarkPass(BrNode *pNode)
{
    while (pNode != NULL) {
        uint16_t flags = pNode->flags;

        if (!(flags & BR_NODE_FLAG_MARK) && !(flags & BR_NODE_FLAG_SKIP)) {
            uint8_t f11 = pNode->f11;

            /* set the mark BEFORE recursing -- this is the cycle guard */
            pNode->flags = (uint16_t)(pNode->flags | BR_NODE_FLAG_MARK);

            if (f11 == 2 && (BrG_0B380C == 3 || BrG_0B380C == 9)) {
                pNode->f11 = 0;
            }
            BrNodeMarkPass(pNode->f00);
        }
        pNode = pNode->f04;
    }
}

/* 0x100616C0 */
void BrNodeClearMarkPass(BrNode *pNode)
{
    while (pNode != NULL) {
        if (pNode->flags & BR_NODE_FLAG_MARK) {
            BrNode *pChild = pNode->f00;
            pNode->flags = (uint16_t)(pNode->flags & 0x7FFFu);
            BrNodeClearMarkPass(pChild);
        }
        pNode = pNode->f04;
    }
}

/* 0x10061700 */
void BrNodeRunMarkPass(void)
{
    /* the root is re-read from the global between the two calls */
    BrNodeMarkPass(BrG_6C7CB8);
    BrNodeClearMarkPass(BrG_6C7CB8);
}

/* ==================================================================== */
/* 5. Car per-frame / per-race init                                     */
/* ==================================================================== */

/* 0x10061BE0 */
void BrCarBuildMatrices(BrCar *pCar)
{
    int i;

    BrSub1006F4A0(CAR_AT(pCar, 0x164));

    /* 0x168, 0x16C, 0x170, 0x174 in the original -- see BR_CAR_SUBPTR */
    for (i = 0; i < 4; ++i) {
        uint8_t *pSub = (uint8_t *)BR_CAR_SUBPTR(pCar, i);
        BrRbBuildMatrix((BrMat4 *)(void *)(pSub + BR_CARSUB_MAT),
                        (const BrRbState *)(void *)(pSub + BR_CARSUB_RB));
    }
}

/* 0x100633E0 */
void BrZeroRegions(BrZeroRegion *pList)
{
    if (pList == NULL || pList->p == NULL) {
        return;
    }
    for (;;) {
        uint8_t *pBeg = (uint8_t *)pList->p;
        uint8_t *pEnd = pBeg + pList->size;

        /* the original's guard is an UNSIGNED `jae`, i.e. skip when the
         * end pointer did not advance; size 0 is the only reachable way */
        if (pBeg < pEnd) {
            memset(pBeg, 0, (size_t)(pEnd - pBeg));
        }
        ++pList;
        if (pList->p == NULL) {
            break;
        }
    }
}

/* 0x10065630 */
void BrCarInitTables(BrCar *pCar)
{
    /* fild + fmul, both at x87 precision; nothing is stored yet */
    double v = (double)CAR_I32(pCar, 0x140) * (double)BR_K_08F9AC;
    double a1, a2, a3;
    int i, j, k;

    a1 = v  - (double)BR_K_08F9B0;
    a2 = a1 - (double)BR_K_08F9B0;
    a3 = a2 - (double)BR_K_08F9B0;

    /* DEAD STORES -- see the header.  The loop below zeroes all four. */
    CAR_F32(pCar, 0x10AC) = (float)v;
    CAR_F32(pCar, 0x10B0) = (float)a1;
    CAR_F32(pCar, 0x10B4) = (float)a2;
    CAR_F32(pCar, 0x10B8) = (float)a3;

    for (i = 0; i < 4; ++i) {
        CAR_I32(pCar, 0x10BC + 4 * i) = 2;
        CAR_I32(pCar, 0x10AC + 4 * i) = 0;   /* kills the store above */
        CAR_I32(pCar, 0x10DC + 4 * i) = 0;
        CAR_I32(pCar, 0x10CC + 4 * i) = 0;
        CAR_F32(pCar, 0x106C + 4 * i) =
            (float)((double)i * (double)BR_K_08F9B4);
    }

    for (j = 0; j < 0x90; ++j) {
        uint8_t *pW = CAR_BYTES(pCar) + 0x2320 + 6 * j;
        uint8_t *pR = CAR_BYTES(pCar) + 0x1120 + 0x20 * j;

        *(uint16_t *)(void *)(pW + 4) = 0;
        *(uint16_t *)(void *)(pW + 2) = 0;
        *(uint16_t *)(void *)(pW + 0) = 0;

        *(int32_t *)(void *)(pR + 0x00) = 0;
        *(int32_t *)(void *)(pR + 0x04) = 0;
        *(int32_t *)(void *)(pR + 0x08) = 0;
        *(int32_t *)(void *)(pR + 0x14) = 0;
        /* the original's `test dl,1` two-way branch here writes the same
         * zero on both arms -- a compiler artifact, not a condition */
        *(int32_t *)(void *)(pR + 0x18) = 0;
        *(int32_t *)(void *)(pR + 0x1C) = 0;
        /* +0x0C and +0x10 are deliberately left alone */
    }

    for (k = 0; k < 0x12; ++k) {
        CAR_U32(pCar, 0x2680 + 4 * k) = 0x00020002u;
    }
}

/* 0x10065710 */
void BrCarClear29C8(BrCar *pCar)
{
    CAR_I32(pCar, 0x29C8) = 0;
    CAR_I32(pCar, 0x29CC) = 0;
    CAR_I32(pCar, 0x29D0) = 0;
    CAR_I32(pCar, 0x29D4) = 0;
    CAR_U16(pCar, 0x29D8) = 0;   /* a WORD, not a dword */
}

/* ==================================================================== */
/* 6. Path walking                                                      */
/* ==================================================================== */

/* The 2D operands handed to BrSeg2Intersect are the leading x/y of BrVec3
 * point fields; the original passes the addresses straight through and the
 * callee reads only offsets 0 and 4, which slice2_21.h records. */
#define BR_XY(pv) ((const BrVec2 *)(const void *)(pv))

/* Shared tail of both walks: test the path segment against the next entry
 * of BrPathSegs and fold the result into the two counters. */
static void BrPathCountCrossing(const BrVec3 *pA, const BrVec3 *pB)
{
    int32_t modulus = BrPathSegCount;
    int32_t idx;

    if (modulus == 0) {
        return;          /* the original guards the idiv, not the table */
    }
    idx = (BrPathCrossCount + 1) % modulus;

    /* GOTCHA: the record's SECOND point is the FIRST argument. */
    if (BrSeg2Intersect(&BrPathSegs[idx].b, &BrPathSegs[idx].a,
                        BR_XY(pA), BR_XY(pB)) != 0) {
        ++BrPathCrossCount;
        if (idx == 0) {
            ++BrPathWrapCount;
        }
    }
}

/* 0x10065B20 */
void BrPathWalk(BrNode *pNode, float t)
{
    BrPathCrossCount = 0;
    BrPathWrapCount  = 0;

    while (pNode != NULL) {
        int32_t i;
        int32_t count;

        /* skipped nodes hand over to their f04 */
        while (pNode != NULL && (pNode->flags & BR_NODE_FLAG_SKIP)) {
            pNode = pNode->f04;
        }
        if (pNode == NULL) {
            return;
        }

        count = (int32_t)pNode->count;
        for (i = 0; i < count; ++i) {
            /* pts[i+1] is read while i is still < count: one past the end
             * on the last iteration.  The original does this. */
            const BrPathPoint *p0 = &pNode->pts[i];
            const BrPathPoint *p1 = &pNode->pts[i + 1];
            /* held in an x87 register, never rounded to float */
            double seg = (double)p0->f18 - (double)p1->f18;

            /* `test ah,0x41` folds unordered in with less-or-equal, so a
             * NaN t ends the walk here rather than running off the end */
            if (!((double)t > seg)) {
                /* partial segment: this is where the walk ends */
                float u = (float)((double)t / seg);

                /* operand order is the MIRROR of BrPathWalkFrom's */
                BrVec3Lerp(&BrPathWalkPoint, &p1->pos, &p0->pos, u);
                BrPathCountCrossing(&p0->pos, &BrPathWalkPoint);

                BrPathWalkNode  = pNode;
                BrPathWalkIndex = i;
                return;
            }

            t = (float)((double)t - seg);
            BrPathCountCrossing(&p0->pos, &p1->pos);
        }
        pNode = pNode->f00;
    }
}

/* 0x10065C80 */
void BrPathWalkFrom(BrNode *pNode, int32_t index, float s, float t)
{
    while (pNode != NULL) {
        int32_t count;

        while (pNode != NULL && (pNode->flags & BR_NODE_FLAG_SKIP)) {
            pNode = pNode->f04;
        }
        if (pNode == NULL) {
            return;
        }

        count = (int32_t)pNode->count;
        for (; index < count; ++index) {
            const BrPathPoint *p0 = &pNode->pts[index];
            const BrPathPoint *p1 = &pNode->pts[index + 1];
            double seg = (double)p0->f18 - (double)p1->f18;
            /* the original spills this product to a 4-byte slot, so it IS
             * rounded to float before the comparison and the divide */
            float  scaled = (float)(seg * (double)s);

            if (!(t > scaled)) {      /* unordered ends the walk here too */
                float u = t / scaled;

                /* operand order is the MIRROR of BrPathWalk's */
                BrVec3Lerp(&BrPathWalkPoint, &p0->pos, &p1->pos, s);
                /* aliasing is intentional: BrVec3Lerp reads each component
                 * before it writes it */
                BrVec3Lerp(&BrPathWalkPoint, &p1->pos, &BrPathWalkPoint, u);

                BrPathWalkNode  = pNode;
                BrPathWalkIndex = index;
                return;
            }

            t = t - scaled;
            s = 1.0f;      /* only the FIRST segment is scaled */
        }
        pNode = pNode->f00;
        index = 0;
    }
}

/* ==================================================================== */
/* 7. Image tint scale                                                  */
/* ==================================================================== */

/* 0x10061460 */
void BrImgTintSetScale(int32_t r, int32_t g, int32_t b)
{
    BrImgTintState.scaleR = r;   /* 0x10AA3440 */
    BrImgTintState.scaleG = g;   /* 0x10AA3448 */
    BrImgTintState.scaleB = b;   /* 0x10AA345C */
}
