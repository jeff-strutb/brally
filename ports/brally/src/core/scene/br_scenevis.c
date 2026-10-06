/* br_scenevis.c -- scene: the per-frame visibility pre-pass.
 *
 * RESPONSIBILITY: scene/ -- what is in view this frame.
 *
 * One function, 0x1000E320.  It runs once per frame before the draw list is
 * built: it lists the coarse grid cells the camera can see (sorted by
 * distance from the camera cell), turns them into the set of track spans to
 * draw, ranks the cars by how far ahead of the camera they are, ORs together
 * the environment flags of the visible spans, points the frame's light at
 * the camera, records the light in a four-deep history, and clamps every
 * driver's projected screen box to the current view rectangle.
 */
#include "br_trkhdr.h"   /* g_brTrkHdr, the loaded track header */
#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_mat.h"   /* br_globals: its objects */
#include "br_vec.h"   /* br_globals: its objects */
#include "slice1_05.h"   /* br_globals: its objects */
#include <stdint.h>
#include "slice3_41.h"


/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <stdlib.h>

/* ---- grid cells ------------------------------------------------------- */

/* One coarse grid cell: column, row and the squared cell distance from the
 * camera's cell.  0x1035F7E8, 192 entries plus the 0xFF/0xFF terminator. */
/* BrVisCell: br_coretypes.h */

#define BR_VIS_CELL_MAX 192

/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x1035F7E8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* 0x102E16B8 */

/* 64-bit core: declared once, in br_globals.h or its struct's header */                             /* 0x10AC2C5C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                             /* 0x10AC2C54 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                         /* 0x10AC2C60 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                         /* 0x10AC2D60 */

/* 64-bit core: declared once, in br_globals.h or its struct's header */                       /* 0x10077214 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                      /* 0x10077218 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                            /* 0x106E72A0 */

/* ---- spans ------------------------------------------------------------ */

/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* 0x10386CA8 one byte per span */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                 /* 0x106EED3C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* 0x1035E710 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x1035FB8C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x1035F7D4 first span past 8 cells */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* 0x102E16A8 first span past thr  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* 0x102E170C first span past thr2 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* 0x105BC7C0 span forced visible */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* 0x105CCB88 */

/* BrVec3: br_vec.h */

/* 84-byte track span records (br_drawcar.h). */
typedef struct BrSpanRec {
    unsigned char  pad0[0x30];
    BrVec3         pos;           /* +0x30 */
    unsigned char  pad1[0x0E];
    unsigned short envFlags;      /* +0x4A */
    unsigned char  pad2[0x08];
} BrSpanRec;
/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* 0x106EED38 */

/* BrGrid16Pair: prototype in br_funcs.h */
/* BrU16QueuePop: prototype in br_funcs.h */
/* BrQsortCmpS2: prototype in br_funcs.h */
/* BrSpanTestPoint: prototype in br_funcs.h */
/* FUN_100597f0: prototype in br_funcs.h */

/* The two u16 halves of a BrGrid16Pair result, read back by BrU16QueuePop. */
typedef struct BrU16Queue {
    unsigned short head;
    unsigned short count;
} BrU16Queue;

/* ---- camera / cars ---------------------------------------------------- */

/* BrCamera: br_coretypes.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                         /* 0x106ED520 */

typedef struct BrPlayerCar {
    BrVec3        vel;            /* +0x00 */
    unsigned char pad[0x14];
    BrVec3        pos;            /* +0x20 */
} BrPlayerCar;
/* 64-bit core: declared once, in br_globals.h or its struct's header */                   /* 0x106E9D88 */

/* 64-bit core: declared once, in br_globals.h or its struct's header */                   /* 0x10273648 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                         /* 0x100B2F04 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                      /* 0x100B2F00 */

/* 64-bit core: declared once, in br_globals.h or its struct's header */                       /* 0x102E0C90 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                       /* 0x1007721C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                       /* 0x102E16C0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                       /* 0x102E16C8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                       /* 0x1035F710 */

/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* 0x10396EB4 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x106E8A18 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x106ED528 */

/* BrVec3Dot: prototype in br_funcs.h */
/* BrVec3Sub: prototype in br_funcs.h */
/* BrVec3ScaleBy: prototype in br_funcs.h */
/* br_dl_normalise: prototype in br_funcs.h */

/* ---- light ------------------------------------------------------------ */

/* 64-bit core: declared once, in br_globals.h or its struct's header */                             /* 0x10077220 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                       /* 0x1007722C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                       /* 0x10077230 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                      /* 0x106E78F0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                         /* 0x1035FBA0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                   /* 0x106ED6AC */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                       /* 0x1035FB90 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                   /* 0x106E7700 */

/* BrLightHist: br_coretypes.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x100A5CA8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* 0x100A9FF0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x1035FB70 */

/* BrPool32Alloc: prototype in br_funcs.h */
/* BrLightDirsFromLookAt: prototype in br_funcs.h */

/* ---- drivers ---------------------------------------------------------- */

typedef struct BrVisPt {
    int   x;
    int   y;
    float h;
} BrVisPt;

typedef struct BrViewRect {
    int x, y, w, h;               /* +0x00 .. +0x0C */
    unsigned char pad[0x48];
} BrViewRect;                     /* 0x58 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                                /* 0x106EC798 */

/* FUN_1000c9e0: prototype in br_funcs.h */

/* WHAT IT DOES: the once-per-frame visibility pass.  Lists every coarse grid
 * cell inside the camera's row/column window (192 at most, flagging
 * overflow), sorts them by squared distance from the camera's own cell, and
 * walks them nearest-first collecting the spans of each cell into the
 * visible-span list, noting where the list crosses three distance
 * thresholds.  Ranks the cars by how far ahead of the camera they sit
 * (keeping the top four) and marks their spans with a per-car bit; ORs the
 * environment flags of the flagged spans; aims the frame's light along the
 * camera direction (or, when a car drives the light, from the player car's
 * velocity), normalises it, and pushes its byte-packed direction into a
 * four-deep history; finally clamps every driver's projected box to the
 * current view rectangle. */
/* @t4-pass 0x1000e320 1 2026-09-20 probes 40 bytes 1992 insns 543 regions 2 rows 20 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x1000e320 2 2026-09-20 probes 20 bytes 1992 insns 543 regions 2 rows 20 census yes  (tools/brally/crank.py) */
/* @t3 0x1000E320 2026-09-20 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 1992/1992 insns 543/543 rows 10+10 regions 2 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Residue is the two value-preserving VC5 orderings described above: region 1
 * schedules the two independent driver-loop loads (hoist vs just-in-time, same
 * values, no aliasing); region 2 reads a commutative 16-bit integer add in the
 * other operand order (same value, same flags, same two reads).  The A5
 * behavioural oracle RUNS the whole pass and returns EQUIVALENT over 48 seeds
 * with per-region negative controls, superseding the byte-distance gates.
 *  Do not reopen before the end-grind. */
/* @implements 0x1000E320 glide BrSceneVisPrepare */
void BrSceneVisPrepare(BrViewRect *pView, unsigned char *pRace, unsigned char *pCars)
{
    int            n, row, col, cx, cy, i, k, c, thr, thr2, r;
    int            cnt, j;
    short          d;
    unsigned int   pair;
    BrU16Queue     q;
    unsigned short s;
    float          dot, fx, fy;
    BrVisPt        pt;
    BrVisCell     *pCell;
    BrDriverCar *pCar;
    BrDriverCar *pDrv;
    short         *pMin, *pMax;
    short         *pd;

    BrVec3        *pPos;


    n = 0;
    g_BrVisCellOverflow = 0;
    for (row = g_BrVisRowLo; row <= g_BrVisRowHi; row++) {
        for (col = g_BrVisColLo[row]; col <= g_BrVisColHi[row]; col++) {
            if (n == BR_VIS_CELL_MAX) {
                g_BrVisCellOverflow = 1;
                goto done;
            }
            g_BrVisCells[n].col = (unsigned char)col;
            g_BrVisCells[n].row = (unsigned char)row;
            n++;
        }
    }
done:
    g_BrVisCells[n].col = 0xFF;
    g_BrVisCells[n].row = 0xFF;

    cx = (int)(g_BrCamera->pos.x * g_BrVisCellRecip);
    if (cx == 64)
        cx = 63;
    cy = (int)(g_BrCamera->pos.y * g_BrVisCellRecip);
    if (cy == 64)
        cy = 63;

    for (i = 0; i < n; i++) {
        int dx = g_BrVisCells[i].col - cx;
        int dy = g_BrVisCells[i].row - cy;
        g_BrVisCells[i].dist = (short)(dx * dx + dy * dy);
    }
    qsort(g_BrVisCells, n, 4, BrQsortCmpS2);

    FUN_100597f0(g_BrSpanPending, g_brTrkHdr.cInstances, -1);
    g_BrVisSpanCount = 0;
    g_BrVisFirstNear = -1;
    g_BrVisFirstFar  = -1;
    g_BrVisFirstMid  = -1;
    r    = (int)(g_BrCamDist * g_BrVisRangeRecip);
    thr2 = (-3 - r) * (-3 - r);
    thr = ((DAT_105ccb68[8]) != 0 && g_BrCamera == (BrCamera *)&((BrDriverCar *)(*(BrPlayerCar * *)&g_pBr63Race))->aSnap[3]) ? 9 : 1;

    col = g_BrVisCells[0].col;
    if (col != 0xFF) {
        pd = &g_BrVisCells[0].dist;
        do {
        d = *pd;
        if (d > 8 && g_BrVisFirstNear == -1)
            g_BrVisFirstNear = g_BrVisSpanCount;
        if (d > thr && g_BrVisFirstMid == -1)
            g_BrVisFirstMid = g_BrVisSpanCount;
        if (d > thr2 && g_BrVisFirstFar == -1)
            g_BrVisFirstFar = g_BrVisSpanCount;

        row = ((unsigned char *)pd)[-1];
        if (g_brRaceBeginAirplane != 0 && g_BrSpanPending[g_brRaceBeginAirplane] != 0
            && (pPos = &BR_PTR32(BrSpanRec *, g_brTrkHdr.aInstances)[g_brRaceBeginAirplane].pos,
                BrSpanTestPoint(pPos->x, pPos->y))) {
            g_BrVisSpans[g_BrVisSpanCount] = (unsigned short)g_brRaceBeginAirplane;
            g_BrVisSpanCount++;
            g_BrSpanPending[g_brRaceBeginAirplane] = 0;
        }

        pair    = BrGrid16Pair(col, row);
        q.head  = (unsigned short)pair;
        q.count = (unsigned short)(pair >> 16);
        if (pair != 0) {
            s = BrU16QueuePop(&q);
            while (s != 0) {
                if (g_BrSpanPending[s] != 0 && s != g_brRaceBeginAirplane) {
                    g_BrVisSpans[g_BrVisSpanCount] = s;
                    g_BrSpanPending[s] = 0;
                    g_BrVisSpanCount++;
                }
                s = BrU16QueuePop(&q);
            }
        }
        pd += 2;
        col = ((unsigned char *)pd)[-2];
        } while (col != 0xFF);
    }

    /* Cars, ranked by how far ahead of the camera they are (top four). */
    g_BrVisCarCount = 0;
    pCar = (BrDriverCar *)pCars;
    for (i = 0; i < g_BrCarCount; i++, pCar += 1) {
        if (pCar->pfnControl != 0 && g_BrCarVisOpaque[i] != 0) {
            BrVec3Sub(&g_BrVisCarDelta, &pCar->pos, &g_BrCamera->pos);
            dot = BrVec3Dot(&g_BrCamera->dir, &g_BrVisCarDelta);
            if (dot >= g_BrVisAheadMin) {
                for (j = g_BrVisCarCount - 1; j >= 0; j--) {
                    if (dot >= g_BrVisCarDot[j])
                        break;
                    g_BrVisCarDot[j + 1] = g_BrVisCarDot[j];
                    g_BrVisCarIdx[j + 1] = g_BrVisCarIdx[j];
                }
                g_BrVisCarIdx[j + 1] = i;
                g_BrVisCarDot[j + 1] = dot;
                g_BrVisCarCount++;
            }
        }
    }
    if (g_BrVisCarCount > 4)
        g_BrVisCarCount = 4;
    for (k = 0; k < g_BrVisCarCount; k++) {
        pCar = (BrDriverCar *)pCars + g_BrVisCarIdx[k];
        for (i = 0; i < pCar->farCount; i++)
            g_BrSpanPending[(&pCar->aFarIds[0])[i]] |= 1 << g_BrVisCarIdx[k];
    }

    /* Environment flags of the flagged spans. */
    (*(unsigned short *)&DAT_10396eb4) = 0;
    for (k = 0; k < g_BrEnvFlagCount; k++)
        (*(unsigned short *)&DAT_10396eb4) |= BR_PTR32(BrSpanRec *, g_brTrkHdr.aInstances)[g_BrEnvFlagIndices[k]].envFlags;

    /* The frame's light. */
    fx = g_BrCamera->dir.x;
    fy = g_BrCamera->dir.y;
    if (fx == g_BrZeroD && fy == g_BrZeroD)
        fx = 0.0001f;
    g_BrVisLights = BrPool32Alloc();
    BrLightDirsFromLookAt(&(*(int *)&g_BrDrawCombined), g_BrVisLights,
                          fx * g_BrVisEyeScale, fy * g_BrVisEyeScale, 0.0f,
                          0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    if ((*(int *)((char *)&g_aBrEntRecs + 0x7C)) != 0) {
        g_BrVisLightDir.x = (*(BrPlayerCar * *)&g_pBr63Race)->pos.x - (*(BrPlayerCar * *)&g_pBr63Race)->vel.x * g_BrVisVelScale;
        g_BrVisLightDir.y = (*(BrPlayerCar * *)&g_pBr63Race)->pos.y - (*(BrPlayerCar * *)&g_pBr63Race)->vel.y * g_BrVisVelScale;
        g_BrVisLightDir.z = (*(BrPlayerCar * *)&g_pBr63Race)->pos.z - (*(BrPlayerCar * *)&g_pBr63Race)->vel.z * g_BrVisVelScale;
    } else {
        g_BrVisLightDir = g_BrVisLightDefault;
    }
    br_dl_normalise(&g_BrVisLightDir);
    BrVec3ScaleBy(&g_BrVisLightDir, 120.0f);

    g_BrVisLightHistIdx = (g_BrVisLightHistIdx + 1) % 4;
    g_BrVisLightHist[g_BrVisLightHistIdx]    = g_BrVisLightTemplate;
    g_BrVisLightHist[g_BrVisLightHistIdx].bx = (char)(int)g_BrVisLightDir.x;
    g_BrVisLightHist[g_BrVisLightHistIdx].by = (char)(int)g_BrVisLightDir.y;
    g_BrVisLightHist[g_BrVisLightHistIdx].bz = (char)(int)g_BrVisLightDir.z;

    /* Clamp every driver's projected box to the view rectangle. */
    for (i = 0; i < g_brRaceNDriver; i++) {
        pDrv = ((BrDriver *)pRace)[i].pCar;              /* driver i, +0x60 */
        if (pDrv != 0) {
            pt.h = pDrv->pos.z - pDrv->fHitDist;
            pt.x = *(int *)&pDrv->pos.x;
            pt.y = *(int *)&pDrv->pos.y;
            pMax = &pDrv->f29A0;
            pMin = &pDrv->f299C;
            FUN_1000c9e0(pView, &pt, 8, pMin, pMax);
            if (*pMin < pView[(*(int *)&g_BrEnvSection)].x)
                *pMin = (short)pView[(*(int *)&g_BrEnvSection)].x;
            if (*pMax < pView[(*(int *)&g_BrEnvSection)].x)
                *pMax = (short)pView[(*(int *)&g_BrEnvSection)].x;
            if (pDrv->f299E < pView[(*(int *)&g_BrEnvSection)].y)
                pDrv->f299E = (short)pView[(*(int *)&g_BrEnvSection)].y;
            if (pDrv->f29A2 < pView[(*(int *)&g_BrEnvSection)].y)
                pDrv->f29A2 = (short)pView[(*(int *)&g_BrEnvSection)].y;
            if (*pMin > pView[(*(int *)&g_BrEnvSection)].w + pView[(*(int *)&g_BrEnvSection)].x)
                *pMin = (short)(pView[(*(int *)&g_BrEnvSection)].w + pView[(*(int *)&g_BrEnvSection)].x);
            if (pDrv->f29A0 > pView[(*(int *)&g_BrEnvSection)].w + pView[(*(int *)&g_BrEnvSection)].x)
                pDrv->f29A0 = (short)(pView[(*(int *)&g_BrEnvSection)].w + pView[(*(int *)&g_BrEnvSection)].x);
            if (pDrv->f299E > pView[(*(int *)&g_BrEnvSection)].y + pView[(*(int *)&g_BrEnvSection)].h)
                pDrv->f299E = (short)(pView[(*(int *)&g_BrEnvSection)].y + pView[(*(int *)&g_BrEnvSection)].h);
            if (pDrv->f29A2 > pView[(*(int *)&g_BrEnvSection)].y + pView[(*(int *)&g_BrEnvSection)].h)
                pDrv->f29A2 = (short)(pView[(*(int *)&g_BrEnvSection)].y + pView[(*(int *)&g_BrEnvSection)].h);
        }
    }
}

