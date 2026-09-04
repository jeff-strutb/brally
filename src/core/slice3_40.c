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

#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl; the original is thiscall.  Rename the
 * prototype so the thiscall definition is not a C2373 redefinition. */
#define BrCarInitTables BrCarInitTables_cdecl_hdr
#define BrCarClear29C8  BrCarClear29C8_cdecl_hdr
#define BrZeroRegions   BrZeroRegions_cdecl_hdr
#endif
#include "slice3_40.h"
#ifdef BR_MATCHING_BUILD
#undef BrCarInitTables
#undef BrCarClear29C8
#undef BrZeroRegions
void BrZeroRegions(void);
#endif

#include "br_match.h"    /* BR_THISCALL1 */

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
/* WHAT IT DOES: packs up this car's current state and sends it to the other
 * players. Whether the send succeeded is thrown away. */
/* @implements 0x100609E0 d3d BrCarNetSendState */
/* @n64 0x80261058 located */
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
/* WHAT IT DOES: brings one other player's car up to date from the network.
 * It does nothing for the local player's own car, and nothing at all when a
 * particular flag is set; otherwise it asks the networking code to predict
 * where that car should be by now, applies the answer, and rebuilds the
 * car's transform matrices so it can be drawn. */
/* @implements 0x10060CC0 d3d BrCarPredictRemote */
int32_t BrCarPredictRemote(BrCar *pCar, int32_t slot)
{
    BrCarState state;   /* the original's 0xA0-byte stack buffer */

    if (slot == BrSub10005D30()) {
        return 1;
    }
    if (BrG_6909B4 != 0) {
        return 1;
    }
    /* The LAST test is written in POSITIVE form -- `if (ok) { work; return 1; }
     * then `return 0;` -- and that is not cosmetic. Written as the guard
     * `if (!ok) return 0;` VC5 tail-merges the two `return 1`s above into one
     * shared exit and the function comes out 28 bytes short. In this form all
     * four exits are emitted in full, as the original has them. See
     * docs/VC5-IDIOMS.md, "the last test's polarity decides whether VC5
     * tail-merges the earlier returns". */
    if (BrNetSlotPredictOrig(&state, slot)) {
        BrCarApplyState(pCar, &state);
        BrCarBuildMatrices(pCar);
        return 1;
    }
    return 0;
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
/* WHAT IT DOES: walks the whole scene tree and, for each node it has not
 * already visited, stamps a visit mark on it and then descends into it. The
 * visible effect is that one particular per-node byte is cleared, but only
 * in two specific game modes. The mark is set before descending, which is
 * what stops a loop in the tree from running away forever -- it is a scratch
 * bit, not a visibility flag. */
/* @implements 0x10061660 d3d BrNodeMarkPass */
/* @n64 0x80255BA0 located */
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
/* WHAT IT DOES: the exact reverse walk: it takes the visit mark off every
 * node the pass above stamped, leaving the tree ready to be walked again. */
/* @implements 0x100616C0 d3d BrNodeClearMarkPass */
/* @n64 0x80255C50 located */
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
/* WHAT IT DOES: runs the mark pass and then the unmark pass over the whole
 * scene tree, so that the only lasting effect is whatever the first pass
 * changed on the way through. */
/* @implements 0x10061700 d3d BrNodeRunMarkPass */
/* @n64 0x80255CA0 located */
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
/* WHAT IT DOES: rebuilds the four transform matrices that place a car and
 * its parts in the world, from the physics state of each. Called after
 * anything moves the car -- the physics step, or a network update. */
/* @implements 0x10061BE0 d3d BrCarBuildMatrices */
/* @implements 0x1005AC60 glide BrCarBuildMatrices */
void BrCarBuildMatrices(BrCar *pCar)
{
    uint8_t *pSub;

    BrSub1006F4A0(CAR_AT(pCar, 0x164));

    /* Orig unrolls the four sub-object pointers at +0x168..+0x174. */
    pSub = (uint8_t *)BR_CAR_SUBPTR(pCar, 0);
    BrRbBuildMatrix((BrMat4 *)(void *)(pSub + BR_CARSUB_MAT),
                    (const BrRbState *)(void *)(pSub + BR_CARSUB_RB));
    pSub = (uint8_t *)BR_CAR_SUBPTR(pCar, 1);
    BrRbBuildMatrix((BrMat4 *)(void *)(pSub + BR_CARSUB_MAT),
                    (const BrRbState *)(void *)(pSub + BR_CARSUB_RB));
    pSub = (uint8_t *)BR_CAR_SUBPTR(pCar, 2);
    BrRbBuildMatrix((BrMat4 *)(void *)(pSub + BR_CARSUB_MAT),
                    (const BrRbState *)(void *)(pSub + BR_CARSUB_RB));
    pSub = (uint8_t *)BR_CAR_SUBPTR(pCar, 3);
    BrRbBuildMatrix((BrMat4 *)(void *)(pSub + BR_CARSUB_MAT),
                    (const BrRbState *)(void *)(pSub + BR_CARSUB_RB));
}

/* 0x100633E0 */
/* WHAT IT DOES: zeroes each block of memory in a list of address-and-size
 * pairs, stopping at the first entry with no address. A block of size zero
 * is stepped over rather than cleared. */
/* @implements 0x100633E0 d3d BrZeroRegions */
#ifdef BR_MATCHING_BUILD
extern BrZeroRegion DAT_100b2f08[];    /* list head, 0x100B2F08 */
void BrZeroRegions(void)
{
    BrZeroRegion *pList = DAT_100b2f08;

    if (pList->p == NULL)
        return;
    for (;;) {
        uint8_t *pBeg = (uint8_t *)pList->p;
        uint8_t *pEnd = pBeg + pList->size;

        if (pBeg < pEnd)
            memset(pBeg, 0, (size_t)(pEnd - pBeg));
        ++pList;
        if (pList->p == NULL)
            break;
    }
}
#else
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
#endif

/* The two per-car controller entry points. Both are the same nine bytes --
 * `mov ecx,[esp+4]` then a `jmp` -- which is what VC5 emits for a cdecl
 * one-liner that tail-calls a thiscall function: the argument moves into ecx
 * and the frame is never built. Their bodies (0x1005D770 and 0x1005C8B0) are
 * not ported. */
void BR_THISCALL1 BrCtlHumanBody(BrCar *pCar);   /* 0x1005C8B0 */
void BR_THISCALL1 BrCtlAiBody(BrCar *pCar);      /* 0x1005D770 */

/* WHAT IT DOES: drive one car from the player's controls for this frame. This
 * is the routine installed in a car's controller slot when a human is at the
 * wheel; its AI counterpart is directly below. */
/* @implements 0x1005D050 glide BrCtlHuman */
void BrCtlHuman(BrCar *pCar)
{
    BrCtlHumanBody(pCar);
}

/* WHAT IT DOES: drive one car from the computer opponent's logic for this
 * frame -- the routine every slot without a human in it gets. */
/* @implements 0x1005E690 glide BrCtlAi */
void BrCtlAi(BrCar *pCar)
{
    BrCtlAiBody(pCar);
}

/* 0x10065630 */
/* WHAT IT DOES: resets a car's working tables at the start of a race: a set
 * of thresholds spaced evenly apart, several arrays of counters, and a block
 * of paired values seeded with a fixed pattern. It opens by computing four
 * floats and then immediately zeroes the very slots it just wrote -- a dead
 * store that is in the original and is kept. */
/* @implements 0x10065630 d3d BrCarInitTables */
/* Original is __thiscall (`fild [ecx+0x140]` / `ret`).  BR_THISCALL1 is the
 * single-arg fastcall spelling; the header stays cdecl. */
void BR_THISCALL1 BrCarInitTables(BrCar *pCar)
{
    /* float, not double: MSVC then emits `fmul/fsub dword` against .rdata.
     * On x87 the unspilled product still has 53-bit precision until fstp. */
    float v = CAR_I32(pCar, 0x140) * BR_K_08F9AC;
    float a1, a2, a3;
    int32_t *p;
    uint8_t *pW;
    uint8_t *pR;
    int i, j, k;
    int two;

    p = (int32_t *)CAR_AT(pCar, 0x10AC);
    a1 = v - BR_K_08F9B0;
    /* DEAD STORES -- see the header.  The loop below zeroes all four. */
    *(float *)(void *)p = v;

    /* Hoist both later walking pointers so they occupy registers across
     * the first loop, as the original's early lea edi/eax do. */
    pW = CAR_BYTES(pCar) + 0x2320;
    pR = CAR_BYTES(pCar) + 0x1120;

    a2 = a1 - BR_K_08F9B0;
    CAR_F32(pCar, 0x10B0) = a1;
    two = 2;
    a3 = a2 - BR_K_08F9B0;
    CAR_F32(pCar, 0x10B4) = a2;
    CAR_F32(pCar, 0x10B8) = a3;

    i = 0;
    do {
        /* named so fild/fmul stay on the x87 stack; ++i then ++p matches
         * the original `inc ebx` / `add edx,4` order. */
        float f = i * BR_K_08F9B4;
        p[4] = two;
        p[0] = 0;
        p[12] = 0;
        p[8] = 0;
        ++i;
        ++p;
        /* orig: add edx,4 then fstp [edx-0x44] (= -17 floats) */
        ((float *)(void *)p)[-17] = f;
    } while (i < 4);

    for (j = 0; j < 0x90; ++j) {
        *(uint16_t *)(void *)(pW + 4) = 0;
        *(uint16_t *)(void *)(pW + 2) = 0;
        *(uint16_t *)(void *)(pW + 0) = 0;

        *(int32_t *)(void *)(pR + 0x00) = 0;
        *(int32_t *)(void *)(pR + 0x04) = 0;
        *(int32_t *)(void *)(pR + 0x08) = 0;
        *(int32_t *)(void *)(pR + 0x14) = 0;
        /* orig `test dl,1` two-way branch.  Both arms write zero; the odd
         * arm reloads the dword just stored at +0x00 so MSVC cannot fold
         * the stores (mov ebx, esi / mov [eax+0x18], ebx vs esi). */
        if (j & 1) {
            *(int32_t *)(void *)(pR + 0x18) = *(int32_t *)(void *)(pR + 0x00);
        } else {
            *(int32_t *)(void *)(pR + 0x18) = 0;
        }
        *(int32_t *)(void *)(pR + 0x1C) = 0;
        /* +0x0C and +0x10 are deliberately left alone */
        pW += 6;
        pR += 0x20;
    }

    for (k = 0; k < 0x12; ++k) {
        CAR_U32(pCar, 0x2680 + 4 * k) = 0x00020002u;
    }
}

/* 0x10065710 */
/* WHAT IT DOES: clears a small block of a car's bookkeeping -- four numbers
 * and one half-sized one -- back to zero. */
/* @implements 0x10065710 d3d BrCarClear29C8 */
#ifdef BR_MATCHING_BUILD
/* Orig is thiscall, no stack args: `xor eax,eax` then stores through ecx.
 * BR_THISCALL1 is __fastcall with one arg -- ecx = this, identical bytes. */
void BR_THISCALL1 BrCarClear29C8(BrCar *pCar)
#else
void BrCarClear29C8(BrCar *pCar)
#endif
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
/* WHAT IT DOES: walks along a path a given distance and works out where that
 * lands: which node, which segment, and the exact point between two path
 * points. Along the way it counts how many of the stored segment lines the
 * path crossed, and how many times that crossing wrapped back round to the
 * first one. A distance of nonsense stops the walk where it is rather than
 * running off the end. */
/* @implements 0x10065B20 d3d BrPathWalk */
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
/* WHAT IT DOES: sets the three colour multipliers that tint an image as it
 * is drawn. Its caller passes three consecutive bytes out of an opponent
 * car's record, so these are 0-to-255 colour components; what the tint is
 * used for there is not established here. */
/* @implements 0x10061460 d3d BrImgTintSetScale */
void BrImgTintSetScale(int32_t r, int32_t g, int32_t b)
{
    BrImgTintState.scaleR = r;   /* 0x10AA3440 */
    BrImgTintState.scaleG = g;   /* 0x10AA3448 */
    BrImgTintState.scaleB = b;   /* 0x10AA345C */
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
extern unsigned char DAT_100ad770;
extern unsigned char DAT_100ad798;
extern unsigned char DAT_100bb2e0;
extern unsigned char DAT_100bb2e8;
int FUN_1006e590();

/* WHAT IT DOES: thunk — forwards to the shared no-op at 0x1006E590. */
/* @implements 0x1005C440 glide BrThunk5C440 */

int BrThunk5C440(void)

{
  FUN_1006e590();
  return;
}

/* WHAT IT DOES: step selection index A up (clamped at 9) and latch its byte from the
 * 4-stride table at 0x100AD770 into 0x100BB2E0. */
/* @implements 0x10059DC0 glide BrUiSelAInc */

void BrUiSelAInc(void)

{
  if (g_brB4E70C < 9) {
    g_brB4E70C = g_brB4E70C + 1;
  }
  DAT_100bb2e0 = (&DAT_100ad770)[g_brB4E70C * 4];
  return;
}

/* WHAT IT DOES: step selection index A down (clamped at 0) and latch its byte. */
/* @implements 0x10059DE0 glide BrUiSelADec */

void BrUiSelADec(void)

{
  if (0 < g_brB4E70C) {
    g_brB4E70C = g_brB4E70C + -1;
  }
  DAT_100bb2e0 = (&DAT_100ad770)[g_brB4E70C * 4];
  return;
}

/* Declared here with a BYTE parameter, which is not how slice1_01.c defines
 * it. The call below pushes eax with the index's upper three bytes still in
 * it -- VC5 only leaves a stack argument dirty when the callee's parameter is
 * a byte type. The definition at 0x10002D30 reads the whole slot and hands it
 * straight on, so both spellings agree on the only byte that is ever read. */
extern int BrCdVolumeSet(unsigned char v);

/* WHAT IT DOES: push both volume selections where the audio code reads them --
 * selection A becomes the music volume (and is applied to the CD/mixer right
 * away), selection B becomes the sound-effect master volume, which the SFX
 * code picks up from the global on its own. This is the "commit" for the two
 * sliders BrUiSelAInc/Dec and BrUiSelBInc/Dec move. */
/* @implements 0x10059E00 glide BrUiVolumeApply */

void BrUiVolumeApply(void)

{
  DAT_100bb2e0 = (&DAT_100ad770)[g_brB4E70C * 4];
  BrCdVolumeSet(DAT_100bb2e0);
  DAT_100bb2e8 = (&DAT_100ad798)[g_brB4E708 * 4];
  return;
}

/* WHAT IT DOES: step selection index B up (clamped at 9) and latch its byte from the
 * 4-stride table at 0x100AD798 into 0x100BB2E8. */
/* @implements 0x10059E30 glide BrUiSelBInc */

void BrUiSelBInc(void)

{
  if (g_brB4E708 < 9) {
    g_brB4E708 = g_brB4E708 + 1;
  }
  DAT_100bb2e8 = (&DAT_100ad798)[g_brB4E708 * 4];
  return;
}

/* WHAT IT DOES: step selection index B down (clamped at 0) and latch its byte. */
/* @implements 0x10059E50 glide BrUiSelBDec */

void BrUiSelBDec(void)

{
  if (0 < g_brB4E708) {
    g_brB4E708 = g_brB4E708 + -1;
  }
  DAT_100bb2e8 = (&DAT_100ad798)[g_brB4E708 * 4];
  return;
}


extern int DAT_100ad7d8;
extern char DAT_100ad7e8;
extern int DAT_100b22d8;

/* WHAT IT DOES: look up one field of one car livery entry, and leave that
 * entry's identifier in a global on the way past -- so the caller gets a
 * value AND the table remembers which row it came from. */
/* @implements 0x10059FE0 glide FUN_10059fe0 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_10059fe0(int param_1,int param_2,int param_3)

{
  DAT_100b22d8 = *(int *)((char *)&DAT_100ad7e8 + (param_2 + param_1 * 0x1e) * 0x28);
  return ((int *)((char *)&DAT_100ad7d8 + param_1 * 1200 + param_2 * 40))[param_3];
}

extern int DAT_10ac67b0;
__declspec(dllimport) void __cdecl free(void *);
void FUN_1005a420(void);
void FUN_1005a6b0(void);

/* WHAT IT DOES: release everything the car-livery system holds: the bitmaps
 * first, then the three shared buffers. The single call site for shutting
 * that subsystem down. */
/* @implements 0x1005A6A0 glide FUN_1005a6a0 */
/* Tail-call wrapper: `call A; jmp B` -- VC5 /O2 turns the trailing call
 * into a jmp. The map had this merged with the 45-byte loop at
 * 0x1005A6B0 as one 61-byte function; split 2026-09-01. */

void FUN_1005a6a0(void)
{
  FUN_1005a420();
  FUN_1005a6b0();
}

/* WHAT IT DOES: free the three shared livery buffers and null the slots, so
 * a second call is harmless. */
/* @implements 0x1005A6B0 glide FUN_1005a6b0 */
/* Frees and zeroes the three pointer slots at DAT_10ac67b0..bc; the
 * dllimport free is hoisted into edi across the loop. */

void FUN_1005a6b0(void)
{
  int *puVar1;

  puVar1 = &DAT_10ac67b0;
  do {
    if (*puVar1 != 0) {
      free((void *)*puVar1);
      *puVar1 = 0;
    }
    puVar1 = puVar1 + 1;
  } while ((int)puVar1 < 0x10ac67bc);
  return;
}

#endif /* BR_MATCHING_BUILD */
