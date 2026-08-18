/* slice2_14.h -- decompiled from BRD3D.dll, packet range 0x10010B00-0x100169B0.
 *
 * Contents, by original address:
 *
 *   0x10010B00  BrLerpNodeAlloc        pop a free node, lerp 8 floats into it
 *   0x10010BF0  BrScrPtKeepNearest     conditional transform-and-store
 *   0x10010D10  BrScrPtProject         project +0x00..+0x08 into +0x0C/+0x10
 *   0x10010D90  BrSortCellCompare      qsort comparator (s16 key at +2)
 *   0x10013A10  BrRasterSelect         latch a 4-word image header into globals
 *   0x10015BD0  BrHudDrawText          draw the debug text list
 *   0x10016910  BrLruInit              init the 5-entry slot cache
 *   0x10016990  BrLruShutdown          clear the init flag
 *   0x100169B0  BrLruAcquire           pick and stamp the least-recently-used slot
 *
 * Skipped functions and the reason for each are listed at the bottom of
 * slice2_14.c.
 *
 * Float constants used below were read out of .rdata rather than guessed:
 *   0x1008F260 = -1.0f   0x1008F264 =  0.03125f   0x1008F268 = -0.03125f
 *   0x1008F26C =  2.0f   0x1008F270 =  0.0 (qword)
 */
#ifndef SLICE2_14_H
#define SLICE2_14_H

#include <stdint.h>

#include "br_vec.h"
#include "br_mat.h"

/* ------------------------------------------------------------------ */
/* 0x10010B00 -- free-list pop + 8-component lerp                      */
/* ------------------------------------------------------------------ */
/* The node is a singly-linked free-list cell with an INDIRECT payload
 * pointer:
 *
 *   +0x00  next free node
 *   +0x04  float *  -- always re-pointed at +0x08 on allocation
 *   +0x08  8 floats
 *
 * Every one of the eight unrolled steps re-reads +0x04 through the pointer
 * rather than caching it, which is what pins the indirection: the two INPUT
 * records are also read as `*(float**)(p + 4)`, so the inputs need not be
 * whole nodes -- only their +0x04 pointer is touched. */
typedef struct BrLerpNode {
    struct BrLerpNode *pNext;    /* +0x00 */
    float             *pData;    /* +0x04 */
    float              data[8];  /* +0x08 */
} BrLerpNode;

/* 0x102E5ECC -- head of the free list. Exposed so callers (and tests) can
 * seed it; the original has no allocator behind it. */
extern BrLerpNode *g_pBrLerpFree;

/* 0x10010B00  pop the head of g_pBrLerpFree and fill its 8 floats with
 *
 *     out[i] = (pTo->pData[i] - pFrom->pData[i]) * t + pFrom->pData[i]
 *
 * GOTCHA: the interpolation endpoints are in the OPPOSITE order to
 * BrVec3Lerp() in br_vec.h. There t=0 yields the THIRD argument; here t=0
 * yields the FIRST. The original computes (arg2 - arg1)*t + arg1.
 *
 * Returns the node, whose pData points at its own data[]. */
BrLerpNode *BrLerpNodeAlloc(const BrLerpNode *pFrom, const BrLerpNode *pTo,
                            float t);

/* ------------------------------------------------------------------ */
/* 0x10010BF0 / 0x10010D10 -- screen-point records                     */
/* ------------------------------------------------------------------ */
/* Stride 0x20, of which only 0x14 is used. Field names are positional: the
 * first three are a position (they are what the 4x4 transforms), and the last
 * two are a 2D key that both routines treat as a plane coordinate. */
typedef struct BrScrPt {
    float f00, f04, f08;   /* +0x00 position */
    float f0C, f10;        /* +0x0C 2D key (see BrScrPtProject) */
    float pad[3];          /* +0x14 unused; brings the stride to 0x20 */
} BrScrPt;

/* Third argument of 0x10010BF0: only +0x38 is read, as a float. */
typedef struct BrDepthRef {
    unsigned char pad[0x38];
    float         f38;
} BrDepthRef;

/* 0x1008F260, used as the depth bias in BrScrPtKeepNearest. */
#define BR_DEPTH_BIAS (-1.0f)

/* 0x10010BF0  conditionally overwrite aOut[idx].
 *
 * Two guards, both evaluated before anything is written:
 *
 *   1. squared 2D distance from (cx,cy) to aOut[idx].f0C/f10 must be
 *      STRICTLY GREATER than the same distance to pIn->f0C/f10 -- i.e. the
 *      incoming point must be strictly nearer than the one already stored.
 *      Ties keep the incumbent.
 *   2. the transformed third component z must satisfy
 *          z <= pRef->f38 - BR_DEPTH_BIAS
 *      (the original compares with fcomp/C0, so an unordered comparison
 *      also rejects; the port spells both guards so NaN rejects too).
 *
 * On success it writes the row-vector transform of pIn's position
 *
 *     out.x = x*m[0][0] + y*m[1][0] + z*m[2][0] + m[3][0]     (and .y, .z)
 *
 * -- the same v*M convention as BrMat4MulVec3 in br_mat.h, but including the
 * translation row -- copies pIn->f0C/f10 verbatim, and sets aFlags[idx] = 1.
 *
 * GOTCHA: the third component z is computed FIRST (it is the guard), and only
 * then x and y; on the rejecting path nothing at all is stored, so a caller
 * cannot distinguish "not written" from "written with the old value" except
 * through aFlags. */
void BrScrPtKeepNearest(const BrMat4 *pM, BrScrPt *aOut, int *aFlags, int idx,
                        const BrScrPt *pIn, float cx, float cy,
                        const BrDepthRef *pRef);

/* 0x106C0860 -- the 4x4 that BrScrPtProject uses. Confirmed to be a matrix
 * independently: 0x100147B0 passes this same address to BrMat4Scale
 * (0x100310F0). */
extern BrMat4 g_BrScrProjMat;

/* 0x100147B0 (glide 0x10011D20) -- draw the model-lights prop marker at the
 * head of the AI path. See the banner in slice2_14.c. g_BrModelLights holds
 * the pointer to the misc\\modelLights.blob prop list (0x10680944). */
struct BrPropList;
extern struct BrPropList *g_BrModelLights;
void BrModelLightsDraw(void);

/* 0x10010D10  IN PLACE: project the position at +0x00..+0x08 through
 * g_BrScrProjMat and store only the first TWO output components:
 *
 *   p->f0C = x*m[0][0] + y*m[1][0] + z*m[2][0] + m[3][0]
 *   p->f10 = x*m[0][1] + y*m[1][1] + z*m[2][1] + m[3][1]
 *
 * Same row-vector convention as BrScrPtKeepNearest. The position is left
 * untouched. */
void BrScrPtProject(BrScrPt *pPt);

/* ------------------------------------------------------------------ */
/* 0x10010D90 -- qsort comparator                                      */
/* ------------------------------------------------------------------ */
/* Element size is 4, fixed by the qsort call at 0x10010E9E
 * (`push cmp / push 4 / push count / push array`). The producer loop at
 * 0x10010DFF..0x10010E93 writes two bytes at +0x00/+0x01 and a word at +0x02,
 * so the key is the SIGNED 16-bit field at +0x02. */
typedef struct BrSortCell {
    unsigned char c00;   /* +0x00 */
    unsigned char c01;   /* +0x01 */
    short         key;   /* +0x02, signed */
} BrSortCell;

/* 0x10010D90  ascending by `key`. Returns exactly 1 / 0 / -1 (the original
 * builds -1 with setge/dec, so the negative result is always -1). */
int BrSortCellCompare(const void *pA, const void *pB);

/* ------------------------------------------------------------------ */
/* 0x10013A10 -- image header latch                                    */
/* ------------------------------------------------------------------ */
/* The argument is a header of four u16 followed by the payload at +0x08.
 * +0x00 is NOT read. */
typedef struct BrRasterHdr {
    uint16_t f00;   /* +0x00 -- ignored by 0x10013A10 */
    uint16_t f02;   /* +0x02 */
    uint16_t f04;   /* +0x04 */
    uint16_t f06;   /* +0x06 */
    /* payload begins at +0x08 */
} BrRasterHdr;

/* Four adjacent globals at 0x1039B710..0x1039B71C. Grouped because they are
 * contiguous and written together; the mapping is deliberately NOT tidy --
 * see BrRasterSelect. */
typedef struct BrRasterState {
    uint16_t    f10;    /* 0x1039B710 */
    uint16_t    f14;    /* 0x1039B714 */
    uint16_t    f18;    /* 0x1039B718 */
    const void *p1C;    /* 0x1039B71C */
} BrRasterState;

extern BrRasterState g_BrRaster;

/* 0x10013A10  latch a header into g_BrRaster.
 *
 * GOTCHA: the fields are CROSSED, and this is not a transcription slip --
 *   0x1039B710 <- hdr->f04     0x1039B714 <- hdr->f02
 *   0x1039B718 <- hdr->f06     0x1039B71C <- (const char *)hdr + 8
 * The store order in the original is 71C, 714, 710, 718. */
void BrRasterSelect(const BrRasterHdr *pHdr);

/* ------------------------------------------------------------------ */
/* 0x10015BD0 -- debug text list                                       */
/* ------------------------------------------------------------------ */
/* 16-byte records at 0x100A66F0, terminated by a NULL text pointer. */
typedef struct BrTextItem {
    int         y;       /* +0x00 */
    int         size;    /* +0x04 -- passed to the text-size setter */
    int         f08;     /* +0x08 -- not read */
    const char *pText;   /* +0x0C -- NULL terminates the list */
} BrTextItem;

/* The globals 0x10015BD0 reads, gathered so the routine is testable. */
typedef struct BrHudCtx {
    const BrTextItem *aItems;   /* 0x100A66F0 */
    int               yLimit;   /* 0x106C299C */
    int               width;    /* 0x106C0684 */
} BrHudCtx;

/* Vertical clip window, from `cmp eax,-0x50 / jle` and `add ecx,0x28 / jge`.
 * Note the asymmetry: the lower bound is exclusive-of-equal at -0x50 and the
 * upper bound is yLimit + 0x28. */
#define BR_HUD_Y_MIN  (-0x50)
#define BR_HUD_Y_SLOP (0x28)

/* 0x10015BD0  set the six colour channels to 255, reset two text-mode flags,
 * call 0x1003289F with the four ints at aClear[0..3], then walk the item list
 * drawing each in-range entry horizontally centred at width/2.
 *
 * GOTCHA: the list is terminated by the NEXT record's pText, read before the
 * cursor advances (`mov eax,[esi+0x1C] / add esi,0x10 / test eax,eax`). The
 * FIRST record's pText is checked by the early-out at the top of the
 * function, so a table whose first pText is NULL draws nothing. */
void BrHudDrawText(const BrHudCtx *pCtx, const int32_t aClear[4]);

/* ------------------------------------------------------------------ */
/* 0x10016910 / 0x10016990 / 0x100169B0 -- 5-slot LRU cache            */
/* ------------------------------------------------------------------ */
/* Five slot records of 0x2E0F0 bytes each, based at 0x1039B760; the first
 * dword of each record is a monotonically increasing recency stamp. The slot
 * count is pinned twice over: the init loop runs
 * 0x1039B760 -> 0x10481C10 step 0x2E0F0 (exactly 5), and the scan loop runs
 * 0x1039B728 -> 0x1039B73C step 4 (also exactly 5). */
#define BR_LRU_SLOTS   5
#define BR_LRU_STRIDE  0x2E0F0

typedef struct BrLru {
    int      inited;                  /* 0x104AFD1C */
    int      aLocked[BR_LRU_SLOTS];   /* 0x1039B728 .. 0x1039B738 */
    uint32_t aStamp[BR_LRU_SLOTS];    /* first dword of each slot record */
    int      cur;                     /* 0x104AFD00 */
    int      prev;                    /* 0x104AFD04 */
    int      prev2;                   /* 0x104AFD18 */
    int      f14;                     /* 0x104AFD14 */
    int      f08;                     /* 0x104AFD08 */
    int      f0C;                     /* 0x104AFD0C */
    int      f5C;                     /* 0x1039B75C -- the dword before slot 0 */
    int      f3C;                     /* 0x1039B73C -- the dword after aLocked */
    int      fA66E8;                  /* 0x100A66E8 */
} BrLru;

/* 0x10016910  one-shot init: no-op if already initialised.
 * cur/prev/prev2/f14/f5C are set to -1 (NOT 0 -- -1 is the "none" marker),
 * every lock flag and every stamp is zeroed, fA66E8 is set to 1. */
void BrLruInit(BrLru *pLru);

/* 0x10016990  clear the init flag if it is set. Nothing else is touched, so
 * the stamps and the cur/prev chain survive a shutdown/init cycle only in the
 * sense that init overwrites them again. */
void BrLruShutdown(BrLru *pLru);

/* 0x100169B0  choose the slot with the SMALLEST stamp among those that are
 * unlocked and are not the current slot, stamp it with (stamp[cur] + 1), and
 * rotate the history: prev2 <- prev, prev <- cur, cur <- chosen.
 * Returns the chosen index.
 *
 * GOTCHAS:
 *  - the stamp comparison is UNSIGNED (`jb`), so a wrapped stamp sorts low
 *    and the slot is re-picked immediately;
 *  - ties go to the LATER index (the test is `keep if best >= candidate`);
 *  - when cur is negative the base stamp is taken as 0, so the new stamp is 1;
 *  - if NO slot qualifies the original leaves the index at -1 and then writes
 *    the stamp at base + (-1)*0x2E0F0, i.e. out of bounds. See the DEVIATION
 *    in the .c -- the port returns -1 and writes nothing. */
int BrLruAcquire(BrLru *pLru);

#endif /* SLICE2_14_H */
