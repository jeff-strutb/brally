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
#include <stdint.h>

#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <stdlib.h>

/* ---- grid cells ------------------------------------------------------- */

/* One coarse grid cell: column, row and the squared cell distance from the
 * camera's cell.  0x1035F7E8, 192 entries plus the 0xFF/0xFF terminator. */
typedef struct BrVisCell {
    unsigned char col;
    unsigned char row;
    short         dist;
} BrVisCell;

#define BR_VIS_CELL_MAX 192

extern BrVisCell g_BrVisCells[BR_VIS_CELL_MAX + 1];  /* 0x1035F7E8 */
extern int       g_BrVisCellOverflow;                /* 0x102E16B8 */

extern int g_BrVisRowLo;                             /* 0x10AC2C5C */
extern int g_BrVisRowHi;                             /* 0x10AC2C54 */
extern int g_BrVisColLo[64];                         /* 0x10AC2C60 */
extern int g_BrVisColHi[64];                         /* 0x10AC2D60 */

extern float g_BrVisCellRecip;                       /* 0x10077214 */
extern float g_BrVisRangeRecip;                      /* 0x10077218 */
extern float g_BrCamDist;                            /* 0x106E72A0 */

/* ---- spans ------------------------------------------------------------ */

extern unsigned char  g_BrSpanPending[];             /* 0x10386CA8 one byte per span */
extern int            g_BrSpanCount;                 /* 0x106EED3C */
extern unsigned short g_BrVisSpans[];                /* 0x1035E710 */
extern int            g_BrVisSpanCount;              /* 0x1035FB8C */
extern int            g_BrVisFirstNear;              /* 0x1035F7D4 first span past 8 cells */
extern int            g_BrVisFirstMid;               /* 0x102E16A8 first span past thr  */
extern int            g_BrVisFirstFar;               /* 0x102E170C first span past thr2 */
extern int32_t        g_brRaceBeginAirplane;         /* 0x105BC7C0 span forced visible */
extern int32_t        g_brRaceReplay;                /* 0x105CCB88 */

typedef struct BrVec3 { float x, y, z; } BrVec3;

/* 84-byte track span records (br_drawcar.h). */
typedef struct BrSpanRec {
    unsigned char  pad0[0x30];
    BrVec3         pos;           /* +0x30 */
    unsigned char  pad1[0x0E];
    unsigned short envFlags;      /* +0x4A */
    unsigned char  pad2[0x08];
} BrSpanRec;
extern BrSpanRec *g_BrDrawTrackFlags;                /* 0x106EED38 */

unsigned int BrGrid16Pair(int a, int b);             /* 0x100031D0 */
uint16_t     BrU16QueuePop(void *pQ);                /* 0x10003280 */
int          BrQsortCmpS2(const void *, const void *);   /* 0x1000E2F0 */
int          BrSpanTestPoint(float x, float y);      /* 0x10033FD0 */
void         FUN_100597f0(void *dst, unsigned count, int c);   /* 0x100597F0 */

/* The two u16 halves of a BrGrid16Pair result, read back by BrU16QueuePop. */
typedef struct BrU16Queue {
    unsigned short head;
    unsigned short count;
} BrU16Queue;

/* ---- camera / cars ---------------------------------------------------- */

typedef struct BrCamera {
    BrVec3        dir;            /* +0x00 */
    unsigned char pad[0x24];
    BrVec3        pos;            /* +0x30 */
} BrCamera;
extern BrCamera *g_BrCamera;                         /* 0x106ED520 */

typedef struct BrPlayerCar {
    BrVec3        vel;            /* +0x00 */
    unsigned char pad[0x14];
    BrVec3        pos;            /* +0x20 */
} BrPlayerCar;
extern BrPlayerCar *g_BrPlayerCar;                   /* 0x106E9D88 */

extern int32_t g_BrCarVisOpaque[];                   /* 0x10273648 */
extern int32_t g_BrCarCount;                         /* 0x100B2F04 */
extern int32_t g_brRaceNDriver;                      /* 0x100B2F00 */

extern BrVec3 g_BrVisCarDelta;                       /* 0x102E0C90 */
extern float  g_BrVisAheadMin;                       /* 0x1007721C */
extern int    g_BrVisCarCount;                       /* 0x102E16C0 */
extern int    g_BrVisCarIdx[];                       /* 0x102E16C8 */
extern float  g_BrVisCarDot[];                       /* 0x1035F710 */

extern unsigned short g_BrVisEnvFlags;               /* 0x10396EB4 */
extern int32_t        g_BrEnvFlagCount;              /* 0x106E8A18 */
extern uint16_t       g_BrEnvFlagIndices[];          /* 0x106ED528 */

float BrVec3Dot(const BrVec3 *pA, const BrVec3 *pB);
void  BrVec3Sub(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB);
void  BrVec3ScaleBy(BrVec3 *pV, float s);
void  br_dl_normalise(BrVec3 *pV);

/* ---- light ------------------------------------------------------------ */

extern double g_BrZeroD;                             /* 0x10077220 */
extern float  g_BrVisEyeScale;                       /* 0x1007722C */
extern float  g_BrVisVelScale;                       /* 0x10077230 */
extern int    g_BrDrawCombined;                      /* 0x106E78F0 */
extern void  *g_BrVisLights;                         /* 0x1035FBA0 */
extern int    g_BrVisLightFromCar;                   /* 0x106ED6AC */
extern BrVec3 g_BrVisLightDir;                       /* 0x1035FB90 */
extern BrVec3 g_BrVisLightDefault;                   /* 0x106E7700 */

typedef struct BrLightHist {
    unsigned char body[0x10];
    char          bx, by, bz;     /* +0x10 */
    unsigned char pad[5];
} BrLightHist;
extern BrLightHist g_BrVisLightHist[4];              /* 0x100A5CA8 */
extern BrLightHist g_BrVisLightTemplate;             /* 0x100A9FF0 */
extern int         g_BrVisLightHistIdx;              /* 0x1035FB70 */

void *BrPool32Alloc(void);
void  BrLightDirsFromLookAt(void *pM, void *pLights,
                            float xEye, float yEye, float zEye,
                            float xAt,  float yAt,  float zAt,
                            float xUp,  float yUp,  float zUp);

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
extern int g_brIView;                                /* 0x106EC798 */

void FUN_1000c9e0(BrViewRect *pView, BrVisPt *pPt, int n, short *pMin, short *pMax);

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
/* T2 residue (2026-09-13): 1992/1992 B, 543/543 insns, 63 differing bytes in
 * two regions, both compiler decisions the idiom dictionary lists as not
 * source-reachable:
 *  - the driver loop's pt.x/pt.y loads: the original hoists both above the
 *    pt.h store and sinks the two stores below the pushes; here each load
 *    follows the previous store (pt is address-taken, so VC5 keeps the
 *    order).  Dead: x/y/h temps in every order (the temps re-colour
 *    pMax/pMin and break the compares), an 8-byte struct copy, an int[3]
 *    or float[3] pt, a typed driver record, a const source pointer.
 *  - the four right/bottom edge sums: the original accumulates into the x
 *    and h fields, this build into w and y (`mov ax,[ecx]; add ax,[ecx+8]`
 *    vs `mov ax,[ecx+8]; add ax,[ecx]` and the same for the 32-bit compare
 *    operand).  Same-struct integer adds canonicalise absolutely: operand
 *    order, pointer local, int-array view, union view, unsigned fields,
 *    `- -`, 16-bit casts, a named accumulator all give the same bytes.
 * Everything else was a source shape: the cell walk is a do-while over a
 * short pointer to the distance word (the +2 induction base; an index or a
 * struct pointer bases at +1), the car walk is an explicit pointer in the
 * for-increment (IV update order), the flagged-car index is read from the
 * array at both uses (the ×7 lea), the airplane record's position is a
 * BrVec3 pointer (the CSE'd +0x30 lea), thr is a ternary (the slot order of
 * the whole frame follows from it), the pop loop stores the span before it
 * clears the pending byte.
 * @t4-pass 0x1000E320 63 2026-09-13 probes 60 bytes 0 insns 0 regions 2 rows 10+10 census no
 * T3 verdict (2026-09-15): NOT certifiable, and it is A2 (raw distance), not
 * A3, that blocks it.  Region 2 (the view-rect edge sums x+w and y+h) is a
 * PROVEN commutative 16-bit-integer-add exact identity: `mov D,[a]; add D,[b]`
 * == `mov D,[b]; add D,[a]` -- same value, same flags (add carry/overflow are
 * symmetric), same two reads, no rounding.  A guarded quad-cancel fold of it
 * (integer analog of the x87 memory-memory fold) takes A3 16 unpaired -> 0 and
 * revalidates all 125 certified tags UNCHANGED (zero demotions), but promotes
 * nothing else <=400 B, so it was not landed (session plan: do not edit
 * t3.py).  A2 still fails 10+10 = 20 vs limit 13.6: the 20 raw rows are real
 * byte diffs and A2 caps distance regardless of classification.  To pass A2
 * would need ~7 fewer raw rows, i.e. region 2 byte-exact (impossible: source-
 * inert canonicalisation) and/or region 1's address-taken pt.x/pt.y schedule
 * (dossier-dead).  Not the x87 schedule-state class, so co-filing does not
 * apply (region 2 is front-end integer canonicalisation; region 1 is the
 * address-taken store/load order).  Parks as T2.  Next lever, if ever: land
 * the integer-commutative-add exact-identity fold in a deliberate t3.py
 * migration AND find an un-spill of region 1 -- both are needed together.
 */
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
    unsigned char *pCar;
    unsigned char *pDrv;
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

    FUN_100597f0(g_BrSpanPending, g_BrSpanCount, -1);
    g_BrVisSpanCount = 0;
    g_BrVisFirstNear = -1;
    g_BrVisFirstFar  = -1;
    g_BrVisFirstMid  = -1;
    r    = (int)(g_BrCamDist * g_BrVisRangeRecip);
    thr2 = (-3 - r) * (-3 - r);
    thr = (g_brRaceReplay != 0 && g_BrCamera == (BrCamera *)((unsigned char *)g_BrPlayerCar + 0x2808)) ? 9 : 1;

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
            && (pPos = &g_BrDrawTrackFlags[g_brRaceBeginAirplane].pos,
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
    pCar = pCars;
    for (i = 0; i < g_BrCarCount; i++, pCar += 0x2B68) {
        if (*(int *)(pCar + 0xF08) != 0 && g_BrCarVisOpaque[i] != 0) {
            BrVec3Sub(&g_BrVisCarDelta, (BrVec3 *)(pCar + 0x30), &g_BrCamera->pos);
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
        pCar = pCars + g_BrVisCarIdx[k] * 0x2B68;
        for (i = 0; i < *(int *)(pCar + 0x2990); i++)
            g_BrSpanPending[((unsigned short *)(pCar + 0x2950))[i]] |= 1 << g_BrVisCarIdx[k];
    }

    /* Environment flags of the flagged spans. */
    g_BrVisEnvFlags = 0;
    for (k = 0; k < g_BrEnvFlagCount; k++)
        g_BrVisEnvFlags |= g_BrDrawTrackFlags[g_BrEnvFlagIndices[k]].envFlags;

    /* The frame's light. */
    fx = g_BrCamera->dir.x;
    fy = g_BrCamera->dir.y;
    if (fx == g_BrZeroD && fy == g_BrZeroD)
        fx = 0.0001f;
    g_BrVisLights = BrPool32Alloc();
    BrLightDirsFromLookAt(&g_BrDrawCombined, g_BrVisLights,
                          fx * g_BrVisEyeScale, fy * g_BrVisEyeScale, 0.0f,
                          0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    if (g_BrVisLightFromCar != 0) {
        g_BrVisLightDir.x = g_BrPlayerCar->pos.x - g_BrPlayerCar->vel.x * g_BrVisVelScale;
        g_BrVisLightDir.y = g_BrPlayerCar->pos.y - g_BrPlayerCar->vel.y * g_BrVisVelScale;
        g_BrVisLightDir.z = g_BrPlayerCar->pos.z - g_BrPlayerCar->vel.z * g_BrVisVelScale;
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
        pDrv = *(unsigned char **)(pRace + 0x60 + i * 0x80);
        if (pDrv != 0) {
            pt.h = *(float *)(pDrv + 0x38) - *(float *)(pDrv + 0x2994);
            pt.x = *(int *)(pDrv + 0x30);
            pt.y = *(int *)(pDrv + 0x34);
            pMax = (short *)(pDrv + 0x29A0);
            pMin = (short *)(pDrv + 0x299C);
            FUN_1000c9e0(pView, &pt, 8, pMin, pMax);
            if (*pMin < pView[g_brIView].x)
                *pMin = (short)pView[g_brIView].x;
            if (*pMax < pView[g_brIView].x)
                *pMax = (short)pView[g_brIView].x;
            if (*(short *)(pDrv + 0x299E) < pView[g_brIView].y)
                *(short *)(pDrv + 0x299E) = (short)pView[g_brIView].y;
            if (*(short *)(pDrv + 0x29A2) < pView[g_brIView].y)
                *(short *)(pDrv + 0x29A2) = (short)pView[g_brIView].y;
            if (*pMin > pView[g_brIView].w + pView[g_brIView].x)
                *pMin = (short)(pView[g_brIView].w + pView[g_brIView].x);
            if (*(short *)(pDrv + 0x29A0) > pView[g_brIView].w + pView[g_brIView].x)
                *(short *)(pDrv + 0x29A0) = (short)(pView[g_brIView].w + pView[g_brIView].x);
            if (*(short *)(pDrv + 0x299E) > pView[g_brIView].y + pView[g_brIView].h)
                *(short *)(pDrv + 0x299E) = (short)(pView[g_brIView].y + pView[g_brIView].h);
            if (*(short *)(pDrv + 0x29A2) > pView[g_brIView].y + pView[g_brIView].h)
                *(short *)(pDrv + 0x29A2) = (short)(pView[g_brIView].y + pView[g_brIView].h);
        }
    }
}

#endif /* BR_MATCHING_BUILD */
