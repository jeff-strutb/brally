/* br_pathwalk.c -- racing: walking a distance along the track path.
 *
 * BrPathWalk (glide 0x1005EB90) walks the node/point path chain a given
 * distance, leaves the landing node, segment index and interpolated point in
 * globals, and counts how many stored segment lines the walk crossed (and how
 * often that count wrapped back to the first).
 *
 * Filed out of the address batch slice3_40.c, with that file's preamble.
 */

#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_race.h"   /* br_globals: its objects */
#include "br_vec.h"   /* br_globals: its objects */
#include "slice3_41.h"   /* br_globals: its objects */
#include <string.h>

/* Header prototype is cdecl; the original is thiscall.  Rename the
 * prototype so the thiscall definition is not a C2373 redefinition. */
#define BrCarInitTables BrCarInitTables_cdecl_hdr
#define BrCarClear29C8  BrCarClear29C8_cdecl_hdr
#define BrZeroRegions   BrZeroRegions_cdecl_hdr
#define BrPathWalk      BrPathWalk_port_hdr   /* defined on the raw node */
#include "slice3_40.h"
#undef BrCarInitTables
#undef BrCarClear29C8
#undef BrZeroRegions
#undef BrPathWalk
/* BrZeroRegions: prototype in br_funcs.h */

#include "br_match.h"    /* BR_THISCALL1 */

/* ------------------------------------------------------------------ */
/* Byte-offset accessors into the car record.                          */
/* BrCar's first member is a byte array, so &pCar->a0000 == pCar and    */
/* the struct is at least 4-aligned (it contains floats), which every   */
/* offset used below is a multiple of.                                  */
/* ------------------------------------------------------------------ */
#define CAR_BYTES(c)     ((uint8_t *)(void *)(c))

/* Float constants, read out of BRD3D.dll .rdata rather than assumed. */
#define BR_K_08F7A8    0.0f            /* 0x1008F7A8 */
#define BR_K_08F7B0 (-1000.0f)         /* 0x1008F7B0 -- SUBTRACTED, so +1000 */
#define BR_K_08F9AC    0.137f          /* 0x1008F9AC */
#define BR_K_08F9B0 (-0.034f)          /* 0x1008F9B0 -- SUBTRACTED, so +0.034 */
#define BR_K_08F9B4    0.15f           /* 0x1008F9B4 */

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
    idx = ((*(int32_t *)((char *)&g_aBrRaceCar + 0x84B8)) /* BR_LP64_BYTE_VIEW */ + 1) % modulus;

    /* GOTCHA: the record's SECOND point is the FIRST argument. */
    if (BrSeg2Intersect(&BrPathSegs[idx].b, &BrPathSegs[idx].a,
                        BR_XY(pA), BR_XY(pB)) != 0) {
        ++(*(int32_t *)((char *)&g_aBrRaceCar + 0x84B8)) /* BR_LP64_BYTE_VIEW */;
        if (idx == 0) {
            ++(*(int32_t *)((char *)&g_aBrRaceCar + 0x893C)) /* BR_LP64_BYTE_VIEW */;
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
/* Glide arm, hand-transcribed from 0x1005EB90.  Same node layout and loop
 * shape as BrRacePathAdvance (0x1005ECF0): the node loop's null test is the
 * skip walk's own guard.  The segment-crossing count is written out at both
 * sites (the original has no helper), the partial segment's lerp runs first. */
typedef struct PwPoint { BrVec3 left, centre, right; float arc; } PwPoint;
typedef struct PwNode {
    struct PwNode *pNext;
    struct PwNode *pSib;
    char           pad08[0x0C];
    unsigned short count;
    unsigned short flags;
    char           pad18[0x28];
    PwPoint        pts[1];
} PwNode;
/* PwSeg: br_coretypes.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
void BrPathWalk(PwNode *pNode, float dist)
{
    int i, k;

    (*(int *)&DAT_10b1ca20) = 0;
    (*(int *)&DAT_10b1cea4) = 0;
    for (;;) {
        while (pNode != 0 && ((*(unsigned short *)&((BrAiPathNode *)(pNode))->flags) & 1) != 0)
            pNode = (*(struct PwNode * *)&((BrAiPathNode *)(pNode))->pSib);
        if (pNode == 0)
            return;
        for (i = 0; i < (*(unsigned short *)&((BrAiPathNode *)(pNode))->count); i++) {
            float seg = (*(PwPoint *)&((BrAiPathNode *)(pNode))->aPt[i]).arc - (*(PwPoint *)&((BrAiPathNode *)(pNode))->aPt[i + 1]).arc;
            if (!(dist > seg)) {
                BrVec3Lerp(&g_brRacePathPos, &(*(PwPoint *)&((BrAiPathNode *)(pNode))->aPt[i + 1]).centre,
                           &(*(PwPoint *)&((BrAiPathNode *)(pNode))->aPt[i]).centre, dist / seg);
                if ((*(int *)&g_brRaceNGate) != 0) {
                    k = ((*(int *)&DAT_10b1ca20) + 1) % (*(int *)&g_brRaceNGate);
                    if (BrSeg2Intersect(&(*(PwSeg (*)[])&g_aBrRaceGate)[k].b, &(*(PwSeg (*)[])&g_aBrRaceGate)[k].a,
                                        (const BrVec2 *)&(*(PwPoint *)&((BrAiPathNode *)(pNode))->aPt[i]).centre,
                                        (const BrVec2 *)&g_brRacePathPos) != 0) {
                        (*(int *)&DAT_10b1ca20)++;
                        if (k == 0)
                            (*(int *)&DAT_10b1cea4)++;
                    }
                }
                (*(PwNode * *)&g_brRacePathNode) = pNode;
                (*(int *)&(*(int *)&g_brRacePathIndex)) = i;
                return;
            }
            dist -= seg;
            if ((*(int *)&g_brRaceNGate) != 0) {
                k = ((*(int *)&DAT_10b1ca20) + 1) % (*(int *)&g_brRaceNGate);
                if (BrSeg2Intersect(&(*(PwSeg (*)[])&g_aBrRaceGate)[k].b, &(*(PwSeg (*)[])&g_aBrRaceGate)[k].a,
                                    (const BrVec2 *)&(*(PwPoint *)&((BrAiPathNode *)(pNode))->aPt[i]).centre,
                                    (const BrVec2 *)&(*(PwPoint *)&((BrAiPathNode *)(pNode))->aPt[i + 1]).centre) != 0) {
                    (*(int *)&DAT_10b1ca20)++;
                    if (k == 0)
                        (*(int *)&DAT_10b1cea4)++;
                }
            }
        }
        pNode = (*(struct PwNode * *)&((BrAiPathNode *)(pNode))->pNext);
    }
}

/* RcPoint: br_coretypes.h */

/* RcNode: br_coretypes.h */


/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x10B1CE98 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x10B1CBEC */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AF07F0 */

/* WHAT IT DOES: move a car's marker along the racing line to the node it is
 * nearest now, walking forward from where it was last frame. This is what
 * keeps track of a car's progress round the lap, and it feeds both the
 * position table and the AI. */
/* @implements 0x1005ECF0 glide BrRacePathAdvance */
void BrRacePathAdvance(RcNode *pNode, int index, float ratio, float dist)
{
    for (;;) {
        int count;

        /* 0x1005ECFD/0x1005ED05: hop SKIP nodes along the sibling link.
         * The null test at the top of the node loop IS this walk's guard --
         * VC5 threads its exit straight to the return -- so there is no
         * separate `if (pNode == 0) return` before it; with one, VC5 rotates
         * the node loop (+15 B, a second epilogue). */
        while (pNode != 0 && (pNode->flags & 1) != 0)
            pNode = pNode->pSib;
        if (pNode == 0)                 /* 0x1005ED11 */
            return;

        count = pNode->count;           /* 0x1005ED1B, u16 -> int */
        while (index < count) {         /* 0x1005ED1F signed, 0x1005ED57 */
            float avail = ((*(RcPoint (*)[1])&pNode->aPt[0].left.x)[index].arc - (*(RcPoint (*)[1])&pNode->aPt[0].left.x)[index + 1].arc)
                        * ratio;        /* 0x1005ED2D..0x1005ED3C */

            /* 0x1005ED40 `fcomp` + `test ah,0x41` + `jne`: C0 is set for
             * LESS and C3 for EQUAL, both for UNORDERED, so the walk stops
             * on less, on equal and on a NaN distance. */
            if (!(dist > avail)) {
                /* BrVec3Lerp is (a - b) * t + b, so t == 1 gives pts[i]. */
                BrVec3Lerp(&g_brRacePathPos,
                           &(*(RcPoint (*)[1])&pNode->aPt[0].left.x)[index].centre,
                           &(*(RcPoint (*)[1])&pNode->aPt[0].left.x)[index + 1].centre, ratio);
                BrVec3Lerp(&g_brRacePathPos,
                           &(*(RcPoint (*)[1])&pNode->aPt[0].left.x)[index + 1].centre,
                           &g_brRacePathPos, dist / avail);
                g_brRacePathNode  = pNode;      /* 0x10B1CBEC */
                (*(int *)&g_brRacePathIndex) = index;      /* 0x10AF07F0 */
                return;
            }

            dist -= avail;              /* 0x1005ED4F */
            index++;                    /* 0x1005ED53 */
            ratio = 1.0f;               /* 0x1005ED59 */
        }

        pNode = pNode->pNext;           /* 0x1005ED67 */
        index = 0;
    }
}
