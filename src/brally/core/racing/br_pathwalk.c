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
void BrZeroRegions(void);

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

typedef struct RcPoint {            /* 0x28 */
    BrVec3 left;                    /* +0x00 */
    BrVec3 centre;                  /* +0x0C */
    BrVec3 right;                   /* +0x18 */
    float  arc;                     /* +0x24 */
} RcPoint;

typedef struct RcNode {
    struct RcNode *pNext;           /* +0x00 */
    struct RcNode *pSib;            /* +0x04 */
    char           pad08[0x0C];     /* +0x08 */
    unsigned short count;           /* +0x14 */
    unsigned short flags;           /* +0x16 */
    char           pad18[0x28];     /* +0x18 */
    RcPoint        pts[1];          /* +0x40 */
} RcNode;

typedef char chk_pts[sizeof(RcPoint) == 0x28 && sizeof(RcNode) == 0x68
                     ? 1 : -1];

extern BrVec3  g_brRacePathPos;     /* 0x10B1CE98 */
extern RcNode *g_brRacePathNode;    /* 0x10B1CBEC */
extern int     g_brRacePathIndex;   /* 0x10AF07F0 */

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
            float avail = (pNode->pts[index].arc - pNode->pts[index + 1].arc)
                        * ratio;        /* 0x1005ED2D..0x1005ED3C */

            /* 0x1005ED40 `fcomp` + `test ah,0x41` + `jne`: C0 is set for
             * LESS and C3 for EQUAL, both for UNORDERED, so the walk stops
             * on less, on equal and on a NaN distance. */
            if (!(dist > avail)) {
                /* BrVec3Lerp is (a - b) * t + b, so t == 1 gives pts[i]. */
                BrVec3Lerp(&g_brRacePathPos,
                           &pNode->pts[index].centre,
                           &pNode->pts[index + 1].centre, ratio);
                BrVec3Lerp(&g_brRacePathPos,
                           &pNode->pts[index + 1].centre,
                           &g_brRacePathPos, dist / avail);
                g_brRacePathNode  = pNode;      /* 0x10B1CBEC */
                g_brRacePathIndex = index;      /* 0x10AF07F0 */
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
