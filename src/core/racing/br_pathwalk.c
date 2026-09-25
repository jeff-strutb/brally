/* br_pathwalk.c -- racing: walking a distance along the track path.
 *
 * BrPathWalk (glide 0x1005EB90) walks the node/point path chain a given
 * distance, leaves the landing node, segment index and interpolated point in
 * globals, and counts how many stored segment lines the walk crossed (and how
 * often that count wrapped back to the first).
 *
 * Filed out of the address batch slice3_40.c, with that file's preamble.
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
#ifdef BR_MATCHING_BUILD
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
typedef struct PwSeg { BrVec2 a, b; int f10; } PwSeg;
extern int     DAT_10b1ca20, DAT_10b1cea4, DAT_106eee38, DAT_10af07f0;
extern PwSeg   DAT_106eed70[];
extern BrVec3  DAT_10b1ce98;
extern PwNode *DAT_10b1cbec;
void BrPathWalk(PwNode *pNode, float dist)
{
    int i, k;

    DAT_10b1ca20 = 0;
    DAT_10b1cea4 = 0;
    for (;;) {
        while (pNode != 0 && (pNode->flags & 1) != 0)
            pNode = pNode->pSib;
        if (pNode == 0)
            return;
        for (i = 0; i < pNode->count; i++) {
            float seg = pNode->pts[i].arc - pNode->pts[i + 1].arc;
            if (!(dist > seg)) {
                BrVec3Lerp(&DAT_10b1ce98, &pNode->pts[i + 1].centre,
                           &pNode->pts[i].centre, dist / seg);
                if (DAT_106eee38 != 0) {
                    k = (DAT_10b1ca20 + 1) % DAT_106eee38;
                    if (BrSeg2Intersect(&DAT_106eed70[k].b, &DAT_106eed70[k].a,
                                        (const BrVec2 *)&pNode->pts[i].centre,
                                        (const BrVec2 *)&DAT_10b1ce98) != 0) {
                        DAT_10b1ca20++;
                        if (k == 0)
                            DAT_10b1cea4++;
                    }
                }
                DAT_10b1cbec = pNode;
                DAT_10af07f0 = i;
                return;
            }
            dist -= seg;
            if (DAT_106eee38 != 0) {
                k = (DAT_10b1ca20 + 1) % DAT_106eee38;
                if (BrSeg2Intersect(&DAT_106eed70[k].b, &DAT_106eed70[k].a,
                                    (const BrVec2 *)&pNode->pts[i].centre,
                                    (const BrVec2 *)&pNode->pts[i + 1].centre) != 0) {
                    DAT_10b1ca20++;
                    if (k == 0)
                        DAT_10b1cea4++;
                }
            }
        }
        pNode = pNode->pNext;
    }
}
#else
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
#endif
