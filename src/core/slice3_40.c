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
