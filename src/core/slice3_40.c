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
#define BrPathWalk      BrPathWalk_port_hdr   /* defined on the raw node */
#endif
#include "slice3_40.h"
#ifdef BR_MATCHING_BUILD
#undef BrCarInitTables
#undef BrCarClear29C8
#undef BrZeroRegions
#undef BrPathWalk
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

/* 0x100616C0 */
/* 0x10061700 */

/* ==================================================================== */
/* 5. Car per-frame / per-race init                                     */
/* ==================================================================== */

/* 0x100633E0 BrZeroRegions is filed in src/core/gamedata/br_zeroregions.c. */

/* The two per-car controller entry points, 0x1005D050 BrCtlHuman and
 * 0x1005E690 BrCtlAi, are filed in src/core/driving/br_ctlstep.c. */

/* ==================================================================== */
/* 6. Path walking (0x10065B20 BrPathWalk is filed in                   */
/*    src/core/racing/br_pathwalk.c)                                   */
/* ==================================================================== */

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

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
extern unsigned char DAT_100ad770;
extern unsigned char DAT_100ad798;
extern unsigned char DAT_100bb2e0;
extern unsigned char DAT_100bb2e8;
int FUN_1006e590();


#endif /* BR_MATCHING_BUILD */
