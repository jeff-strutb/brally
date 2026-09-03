/* slice2_11.c -- Boss Rally (BRD3D.dll) work packet 11, 0x100011F0-0x10005130.
 *
 * See slice2_11.h for the per-function contracts, the x87 comparison
 * modelling note and the list of gotchas.  Read that first.
 */
#include <stdio.h>
#include <string.h>

#include "slice2_11.h"
#include "slice1_06.h"   /* BrTriContainsPoint -- 0x1003B940 */

/* The 0x44-byte camera record is load-bearing: 0x100019D0 copies it with
 * `rep movsd` counts of 0x11.  Fail the build if the host disagrees. */
typedef char BrCamFrameSizeCheck[(sizeof(BrCamFrame) == 0x44) ? 1 : -1];

/* BrNetCarStateSend walks BrCarState as a flat float array, exactly as the
 * original does (and as slice1_02.c already does for the same struct). */
typedef char BrSlice2CarStateSizeCheck[
    (sizeof(BrCarState) == BR_CARSTATE_FLOATS * sizeof(float)) ? 1 : -1];

/* ------------------------------------------------------------------ */
/* Float constants, by the address they live at in the DLL             */
/* ------------------------------------------------------------------ */
static const float kBr08F000 =  0.0f;
static const float kBr08F004 = -0.10000000149011612f;   /* -(0.1f) */
static const float kBr08F014 =  2.4000000953674316f;
static const float kBr08F018 = 11.0f;
static const float kBr08F01C = 19.799999237060547f;
static const float kBr08F020 = -2.4000000953674316f;
static const float kBr08F024 = -0.6600000262260437f;
static const float kBr08F028 =  3.5f;
static const float kBr08F02C =  2.0f;
static const float kBr08F030 =  7.0f;
static const float kBr08F034 =  0.5714285969734192f;    /* ~= 4/7 */
static const float kBr08F038 =  4.0f;
static const float kBr08F03C =  0.10000000149011612f;
static const float kBr08F0AC =  4188888.0f;             /* sentinel */

/* ================================================================== */
/* 0x100011F0 -- camera collision sweep                               */
/* ================================================================== */

/* The body of one sweep pass.  The original has TWO textually identical
 * copies of this, one per pass; they are folded into one function here.
 * DEVIATION: representation only.  Nothing else differs -- see the header
 * for the two things that genuinely vary between the passes.
 *
 * The grid is walked in ELEMENTS rather than by the original's byte
 * arithmetic (`cell * 4800 + 0x11750338`, count `<< 5`), because a
 * BrCollPlane is 32 bytes only on a 32-bit host.  The element counts are
 * the original's: BR_COLL_CELL_PLANES per cell.
 * DEVIATION: representation only. */
static const BrCollPlane *BrCamSweepPass(const int *pCells, int nCells,
                                         const BrVec3 *pOrigin,
                                         const BrVec3 *pDir,
                                         float *pTBest, BrVec3 *pHitOut)
{
    const BrCollPlane *pBest = NULL;
    int c;

    for (c = 0; c < nCells; ++c) {
        int cell = pCells[c];
        const BrCollPlane *pPlane = g_pBrCollGrid
                                  + (size_t)cell * BR_COLL_CELL_PLANES;
        const BrCollPlane *pEnd   = pPlane + g_pBrCollGridCount[cell];

        for (; pPlane != pEnd; ++pPlane) {
            const BrVec3 *pN = (const BrVec3 *)(const void *)pPlane;
            BrVec3 toV0, hit;
            float denom, t;

            /* fcomp 0.0 / test ah,1 -- C0, so unordered also passes. */
            denom = BrVec3Dot(pDir, pN);
            if (denom >= kBr08F000)
                continue;

            BrVec3Sub(&toV0, pPlane->pV0, pOrigin);
            t = BrVec3Dot(&toV0, pN) / denom;

            /* fcomp 0.0 / test ah,0x41 -- C0|C3, so unordered is rejected. */
            if (!(t > kBr08F000))
                continue;
            /* fcomp tBest / test ah,1 -- C0, unordered accepted. */
            if (t >= *pTBest)
                continue;

            BrVec3MulAdd(&hit, pOrigin, pDir, t);
            if (!BrTriContainsPoint(&hit, pPlane->pV0, pPlane->pV1,
                                    pPlane->pV2, pN))
                continue;

            *pTBest  = t;
            *pHitOut = hit;
            pBest    = pPlane;
        }
    }
    return pBest;
}

void BrCamCollideSweep(void *pCar, BrCamFrame *pCam, const BrVec3 *pPrevPos)
{
    unsigned char *p        = (unsigned char *)pCar;
    BrVec3        *pAnchor  = (BrVec3 *)(void *)(p + BR_CAR_OFF_ANCHOR);
    const BrCollPlane *pBest;
    BrVec3 dir, hit;
    int    cells[2];
    int    nCells;
    float  len, tBest, dist;

    g_brCamCollided = 0;

    /* Both cell lookups happen up front; pass 2 reuses them. */
    cells[0] = BrCollGridCellAcquire(pAnchor->x, pAnchor->y);
    cells[1] = BrCollGridCellAcquire(pCam->f30.x, pCam->f30.y);
    nCells   = (cells[0] != cells[1]) ? 2 : 1;

    /* Pass 1: anchor -> camera, with the ray allowed to run 0.1f long. */
    BrVec3Sub(&dir, &pCam->f30, pAnchor);
    len = BrVec3Length(&dir);
    /* fcom 0.0 / test ah,0x40 -- C3, so unordered takes the 1.0f side. */
    if (!(len < kBr08F000) && !(len > kBr08F000))
        tBest = 1.0f;
    else
        tBest = (len - kBr08F004) / len;

    if (BrCamSweepPass(cells, nCells, pAnchor, &dir, &tBest, &hit) == NULL)
        return;

    /* Pass 2: previous camera position -> camera, t capped at exactly 1. */
    BrVec3Sub(&dir, &pCam->f30, pPrevPos);
    tBest = 1.0f;
    pBest = BrCamSweepPass(cells, nCells, pPrevPos, &dir, &tBest, &hit);
    if (pBest == NULL)
        return;

    /* Distance is measured BEFORE the camera is moved. */
    dist = BrVec3Dist(&pCam->f30, pAnchor);
    BrVec3MulAdd(&pCam->f30, &hit,
                 (const BrVec3 *)(const void *)pBest, 0.3f);

    BrVec3Sub(&dir, &pCam->f30, pAnchor);
    len = BrVec3Length(&dir);
    /* test ah,1 -- C0, unordered accepted; then C3 against 0.0f. */
    if (!(dist >= len) && (len < kBr08F000 || len > kBr08F000)) {
        BrVec3ScaleBy(&dir, dist / len);
        BrVec3Add(&pCam->f30, pAnchor, &dir);
    }
    g_brCamCollided = 1;
}

/* ================================================================== */
/* 0x100015D0 -- place the chase camera                               */
/* ================================================================== */

void BrCamPlaceChase(void *pCar, BrCamFrame *pCam, float bias)
{
    unsigned char *p     = (unsigned char *)pCar;
    BrCamFrame    *pF    = (BrCamFrame *)(void *)(p + BR_CAR_OFF_FRAME);
    const int     *pFlag = (const int *)(const void *)(p + BR_CAR_OFF_CAMFLAG);
    BrVec3 ideal;
    float  len;

    if (*pFlag != 0) {
        /* `bias` is unused on this path. */
        float back = (g_brMode0AA8B4 == 1) ? -11.0f : -19.799999237060547f;
        BrVec3MulAdd(&pCam->f30, &pF->f30, &pF->f20, kBr08F014);
        BrVec3MulAdd(&pCam->f30, &pCam->f30, &pF->f00, back);
        return;
    }

    /* Height is taken out for the duration and put back at the end. */
    pCam->f30.z = pCam->f30.z - kBr08F014;

    BrVec3Sub(&ideal, &pCam->f30, &pF->f30);
    len = BrVec3Length(&ideal);
    if (len < kBr08F000 || len > kBr08F000) {
        float want = (g_brMode0AA8B4 == 1) ? kBr08F018 : kBr08F01C;
        BrVec3ScaleBy(&ideal, want / len);
    }

    if (g_brFlag6909E0 != 0) {
        BrVec3Scale(&pCam->f30, &pF->f00, 11.0f);
    } else if (g_brMode0AA010 == 5) {
        BrVec3Scale(&pCam->f30, &pF->f10, -11.0f);
        BrVec3MulAddTo(&pCam->f30, &pF->f00, -13.0f);
    } else {
        BrVec3Scale(&pCam->f30, &pF->f00, -11.0f);
    }

    len = BrVec3Length(&pCam->f30);
    if (len < kBr08F000 || len > kBr08F000)
        BrVec3ScaleBy(&pCam->f30, kBr08F018 / len);

    /* BrVec3Lerp: t == 0 yields pB, t == 1 yields pA. */
    BrVec3Lerp(&pCam->f30, &pCam->f30, &ideal, bias);
    BrVec3AddTo(&pCam->f30, &pF->f30);

    pCam->f30.z = pCam->f30.z - kBr08F020;
}

/* ================================================================== */
/* 0x10001760 -- update the look-at anchor                            */
/* ================================================================== */

void BrCamAnchorUpdate(void *pCar)
{
    unsigned char *p       = (unsigned char *)pCar;
    BrCamFrame    *pF      = (BrCamFrame *)(void *)(p + BR_CAR_OFF_FRAME);
    BrVec3        *pAnchor = (BrVec3 *)(void *)(p + BR_CAR_OFF_ANCHOR);
    float         *pSlew   = (float *)(void *)(p + BR_CAR_OFF_SLEW);
    const BrVec3  *pV204   = (const BrVec3 *)(const void *)
                             (p + BR_CAR_OFF_V204);
    const int     *pFlag   = (const int *)(const void *)
                             (p + BR_CAR_OFF_CAMFLAG);
    double target, step;
    float  speed, cur;

    if (*pFlag != 0) {
        BrVec3MulAdd(pAnchor, &pF->f30, &pF->f20, 1.1f);
        return;
    }

    pAnchor->x = pF->f30.x;
    pAnchor->y = pF->f30.y;
    pAnchor->z = pF->f30.z - kBr08F024;

    speed = BrVec3Length(pV204);
    if (g_brMode0AA010 == 5)
        return;

    /* Both tests are `fcomp` + `test ah,1` (C0), so a NaN speed lands on
     * the first branch and yields 2.0f. */
    if (!(speed >= kBr08F028))
        target = (double)kBr08F02C;
    else if (!(speed >= kBr08F030))
        target = (double)kBr08F038 - (double)speed * (double)kBr08F034;
    else
        target = (double)kBr08F000;

    /* The slew step is compared against the target BEFORE being rounded to
     * float32, which is why it is kept in double here. */
    cur = *pSlew;
    if (target > (double)cur) {
        step   = (double)cur - (double)kBr08F004;
        *pSlew = (float)step;
        if (step > target)                 /* test ah,0x41 -- C0|C3 */
            *pSlew = (float)target;
    } else if (!(target >= (double)cur)) { /* test ah,1 -- C0 */
        step   = (double)cur - (double)kBr08F03C;
        *pSlew = (float)step;
        if (!(step >= target))             /* test ah,1 -- C0 */
            *pSlew = (float)target;
    }
    /* target == cur leaves *pSlew completely untouched. */

    BrVec3MulAddTo(pAnchor, &pF->f00, *pSlew);
}

/* ================================================================== */
/* 0x10001890 -- orient a camera frame                                */
/* ================================================================== */

void BrCamOrient(void *pCar, BrCamFrame *pCam)
{
    unsigned char *p       = (unsigned char *)pCar;
    BrCamFrame    *pF      = (BrCamFrame *)(void *)(p + BR_CAR_OFF_FRAME);
    const BrVec3  *pAnchor = (const BrVec3 *)(const void *)
                             (p + BR_CAR_OFF_ANCHOR);
    const int     *pFlag   = (const int *)(const void *)
                             (p + BR_CAR_OFF_CAMFLAG);
    BrVec3 d;
    float  len;

    BrVec3Sub(&d, pAnchor, &pCam->f30);
    len = BrVec3Length(&d);
    if (len < kBr08F000 || len > kBr08F000) {
        BrVec3Div(&pCam->f00, &d, len);
    } else {
        /* Camera sits exactly on the anchor: keep the previous forward
         * vector unless it too is degenerate. */
        float own = BrVec3Length(&pCam->f00);
        if (!(own < kBr08F000) && !(own > kBr08F000))
            pCam->f00 = pF->f00;
    }

    if (*pFlag != 0) {
        BrVec3Cross(&pCam->f10, &pF->f20, &pCam->f00);
    } else {
        d.x = 0.0f;
        d.y = 0.0f;
        d.z = 1.0f;
        BrVec3Cross(&pCam->f10, &d, &pCam->f00);
    }
    BrVec3Cross(&pCam->f20, &pCam->f00, &pCam->f10);
}

/* ================================================================== */
/* 0x10001970 / 0x10001FF0 -- camera frame setup                      */
/* ================================================================== */

/* See the DEVIATION note in slice2_11.h: the two slots hold the selected
 * frame's byte offset within the record, not its address. */
BrCamFrame *BrCarActiveCam(void *pCar)
{
    unsigned char *p = (unsigned char *)pCar;
    return (BrCamFrame *)(void *)
           (p + *(const uint32_t *)(const void *)(p + BR_CAR_OFF_ACTIVECAM));
}

BrCamFrame *BrCarActiveCam2(void *pCar)
{
    unsigned char *p = (unsigned char *)pCar;
    return (BrCamFrame *)(void *)
           (p + *(const uint32_t *)(const void *)(p + BR_CAR_OFF_ACTIVECAM2));
}

void BrCamFrameInitD(void *pCar)
{
    unsigned char *p        = (unsigned char *)pCar;
    BrCamFrame    *pF       = (BrCamFrame *)(void *)(p + BR_CAR_OFF_FRAME);
    BrCamFrame    *pD       = (BrCamFrame *)(void *)(p + BR_CAR_OFF_CAM_D);
    uint32_t      *pActive  = (uint32_t *)(void *)
                              (p + BR_CAR_OFF_ACTIVECAM);
    int           *pMode    = (int *)(void *)(p + BR_CAR_OFF_MODE);

    *pActive = (uint32_t)BR_CAR_OFF_CAM_D;
    BrVec3MulAdd(&pD->f30, &pF->f30, &pF->f00, 6.0f);
    BrVec3MulAddTo(&pD->f30, &pF->f10, 2.0f);
    BrVec3AddTo(&pD->f30, &pF->f20);
    *pMode = 2;
}

void BrCamFrameInitB(void *pCar)
{
    unsigned char *p         = (unsigned char *)pCar;
    BrCamFrame    *pF        = (BrCamFrame *)(void *)(p + BR_CAR_OFF_FRAME);
    BrCamFrame    *pB        = (BrCamFrame *)(void *)(p + BR_CAR_OFF_CAM_B);
    BrCamFrame    *pD        = (BrCamFrame *)(void *)(p + BR_CAR_OFF_CAM_D);
    uint32_t      *pActive   = (uint32_t *)(void *)
                               (p + BR_CAR_OFF_ACTIVECAM);
    uint32_t      *pActive2  = (uint32_t *)(void *)
                               (p + BR_CAR_OFF_ACTIVECAM2);
    BrVec3        *pPrev     = (BrVec3 *)(void *)(p + BR_CAR_OFF_PREVPOS);
    BrVec3        *pV2900    = (BrVec3 *)(void *)(p + BR_CAR_OFF_V2900);
    float         *pShake    = (float *)(void *)(p + BR_CAR_OFF_SHAKE);
    float         *pSlew     = (float *)(void *)(p + BR_CAR_OFF_SLEW);
    uint32_t       sel       = (g_brMode0AA010 == 5)
                               ? (uint32_t)BR_CAR_OFF_CAM_B
                               : (uint32_t)BR_CAR_OFF_CAM_A;

    /* Both slots get the same frame; the frame that is SEATED below is
     * always B regardless of which one was selected. */
    *pActive  = sel;
    *pActive2 = sel;

    BrVec3MulAdd(&pB->f30, &pF->f30, &pF->f20, 4.0f);
    if (g_brFlag6909E0 != 0)
        BrVec3MulAddTo(&pB->f30, &pF->f00, 10.0f);
    BrVec3SubFrom(&pB->f30, &pF->f00);

    pD->f30 = pB->f30;
    *pPrev  = pB->f30;
    *pShake = 0.0f;
    *pV2900 = pB->f30;
    *pSlew  = 2.0f;
}

/* ================================================================== */
/* 0x100020D0 -- "%d:%02d.%02d"                                       */
/* ================================================================== */

void BrTimeFormat(char *pDst, size_t cap, float seconds)
{
    /* fmul 100.0f then __ftol: truncation toward zero. */
    int total = (int)((double)seconds * 100.0);
    int secs  = total / 100;
    int hund  = total - secs * 100;
    int mins  = secs / 60;

    secs = secs - mins * 60;

    /* DEVIATION: snprintf with an explicit capacity; the original calls
     * sprintf at 0x1007C830 into a buffer of unstated size. */
    (void)snprintf(pDst, cap, "%d:%02d.%02d", mins, secs, hund);
}

/* ================================================================== */
/* 0x10002E90 -- 64x64 u16 grid fetch                                 */
/* ================================================================== */

uint32_t BrGrid64Fetch(int i, int j)
{
    uint32_t idx, a, b;

    if (i < 0 || i >= 64)
        return 0;
    if (j < 0 || j >= 64)
        return 0;

    idx = ((uint32_t)j << 6) + (uint32_t)i;
    a   = g_pBrGrid64[idx & 0xFFFFu];
    b   = g_pBrGrid64[(idx + 1u) & 0xFFFFu];

    /* The `a << 16` term is shifted out of the register by the second
     * shift and contributes nothing.  Kept as written in the original. */
    return ((b + (a << 16) - a) << 16) | a;
}

/* ================================================================== */
/* 0x10002F40 -- pop from a u16 ring                                  */
/* ================================================================== */

/* @n64 0x8021EA90 located */
uint16_t BrU16QueuePop(void *pQ)
{
    uint16_t *q = (uint16_t *)pQ;
    uint32_t  hi, lo, packed;

    hi = q[1];
    if (hi == 0)
        return 0;
    lo = q[0];

    /* Both halves are updated through one 32-bit register, so a carry out
     * of (lo + 1) lands in the new count.  Bug preserved. */
    packed = ((hi + 0xFFFFu) << 16) | (lo + 1u);
    q[0] = (uint16_t)packed;
    q[1] = (uint16_t)(packed >> 16);

    return g_pBrU16QueueTable[lo];
}

/* ================================================================== */
/* 0x10002930 / 0x10002970 / 0x100029B0 -- CD track stepping          */
/* ================================================================== */

int BrCdTrackPrev(void)
{
    if (g_brCdEnabled != 0 && g_brCdPlaying != 0) {
        int track = BrCdTrackGet() - 1;
        g_brCdTrackCur = track;
        if (track < g_brCdTrackFirst) {
            track = g_brCdTrackFirst;
            g_brCdTrackCur = track;
        }
        BrCdTrackPlay(track);
    }
    return 1;
}

int BrCdTrackNext(void)
{
    if (g_brCdEnabled != 0 && g_brCdPlaying != 0) {
        int track = BrCdTrackGet() + 1;
        g_brCdTrackCur = track;
        if (track > g_brCdTrackLast) {   /* CLAMP */
            track = g_brCdTrackLast;
            g_brCdTrackCur = track;
        }
        BrCdTrackPlay(track);
    }
    return 1;
}

int BrCdTrackNextWrap(void)
{
    if (g_brCdEnabled != 0 && g_brCdPlaying != 0) {
        int track = BrCdTrackGet() + 1;
        g_brCdTrackCur = track;
        if (track > g_brCdTrackLast) {   /* WRAP -- the one difference */
            track = g_brCdTrackFirst;
            g_brCdTrackCur = track;
        }
        BrCdTrackPlay(track);
    }
    return 1;
}

/* ================================================================== */
/* 0x10005130 -- car-state send throttle                              */
/* ================================================================== */

#define BR_NET_PEAK_FIRST 32   /* BrCarState::f80 is float #32          */
#define BR_NET_PEAK_COUNT 7    /* f80 .. f98                            */
#define BR_NET_STAMP      30   /* BrCarState::f78 is float #30          */

int BrNetCarStateSend(BrCarState *pState)
{
    float *pAll = (float *)(void *)pState;
    int i, n;

    /* f78 crossing the sentinel upward while the reference has not: send
     * a full packet at once. */
    if (pAll[BR_NET_STAMP] >= kBr08F0AC && !(g_brNet220D68 >= kBr08F0AC)) {
        g_brNetLastFull = *pState;
        return BrNetSendFull(pState);
    }

    n = g_brNetTickCount + 1;
    g_brNetTickCount = n;
    if (n < 3) {
        /* `fcomp` + `test ah,1` -- C0, so unordered assigns. */
        for (i = 0; i < BR_NET_PEAK_COUNT; ++i)
            if (!(g_abrNetPeak[i] >= pAll[BR_NET_PEAK_FIRST + i]))
                g_abrNetPeak[i] = pAll[BR_NET_PEAK_FIRST + i];
        return 1;
    }

    for (i = 0; i < BR_NET_PEAK_COUNT; ++i)
        if (!(pAll[BR_NET_PEAK_FIRST + i] >= g_abrNetPeak[i]))
            pAll[BR_NET_PEAK_FIRST + i] = g_abrNetPeak[i];
    for (i = 0; i < BR_NET_PEAK_COUNT; ++i)
        g_abrNetPeak[i] = 0.0f;
    g_brNetTickCount = 0;

    n = g_brNetSendCount + 1;
    g_brNetSendCount = n;

    /* cdq/xor/sub, and 3, xor/sub -- signed remainder, truncating. */
    if (n % 4 == 0) {
        g_brNetLastFull = *pState;
        return BrNetSendFull(pState);
    } else {
        int rc = BrNetSendDelta(pState, &g_brNetLastFull);
        BrNetSendFlush();
        BrNetKeepAliveTick();
        return rc;
    }
}
