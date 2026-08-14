/* slice2_19.c -- decompiled from BRD3D.dll, range 0x10033CB1 .. 0x10036C00.
 *
 * See slice2_19.h for the recovered layouts, the DEVIATION list, the skipped
 * functions and the gotchas. Everything here was traced from
 * work/slice2/agent19.asm.
 *
 * x87 note: every fcomp/fnstsw pair in this range was decoded through the
 * flag mapping C0 = ah bit 0, C2 = ah bit 2, C3 = ah bit 6, so
 *      test ah,0x01 / jne  -> ST0 <  mem, or unordered
 *      test ah,0x41 / jne  -> ST0 <= mem, or unordered
 * and the C below uses the negated-comparison forms that reproduce the
 * unordered case as well, not just the ordered one.
 */
#include "slice2_19.h"

#include <string.h>

/* ================================================================== */
/* Globals the original reaches by absolute address                    */
/* ================================================================== */

float g_BrK08F514 = 2.0f;          /* DERIVED   */
float g_BrK08F518 = 1.0f;          /* ASSUMED   */
float g_BrK08F51C = 1.0f;          /* ASSUMED   */
float g_BrK08F520 = 2.5f;          /* ASSUMED   */
float g_BrK08F524 = 5.0f;          /* ASSUMED   */
float g_BrK08F52C = 4096.0f;       /* DERIVED   */
float g_BrK08F530 = 1.0f / 128.0f; /* ASSUMED   */
float g_BrK08F534 = 0.5f;          /* ASSUMED   */
float g_BrK08F548 = 1.0f / 80.0f;  /* ASSUMED   */
float g_BrK08F54C =  1.0f;         /* ASSUMED   */
float g_BrK08F550 = -1.0f;         /* ASSUMED   */

BrVec3 g_BrCamEye;
BrVec3 g_BrCamCentre;
BrVec3 g_BrCamExtentR;
BrVec3 g_BrCamExtentU;
BrVec3 g_BrCamCentreCopy;
BrVec3 g_BrCamCorner0;
BrVec3 g_BrCamCorner1;
BrVec3 g_BrCamCorner2;
BrVec3 g_BrCamCorner3;
float  g_BrCamDist;
float  g_BrCamFovIn;
/* 0x100AA8B4, 0x100AC300, 0x106C661C, 0x106C6624 and 0x106C2CFC are defined
 * ONCE, in port/src/br_data.c -- see the ALIAS RESOLVED notes in slice2_19.h.
 * Three of them carry non-zero initialisers this module never had. */

BrMat4    g_BrViewMat;
BrMat4    g_BrProjMat;
BrMat4    g_BrProjMatFixed;
BrMat4    g_BrCurMat;
uint16_t  g_BrPerspNorm;
float     g_BrCamFar;
float     g_BrCamNear;
void     *g_BrMtxSlot;
uint32_t *g_BrGfxPtr;
BrPool   *g_BrPool;

int32_t     g_Br0B380C;
int32_t     g_Br6C666C;
const void *g_BrDlTableA;

int32_t g_BrCarCount;
void  (*g_BrGfxSubmit)(uint32_t dl);
void  (*g_BrGfxSubmitB)(uint32_t p);


const unsigned char *g_BrPadModeBytes;
int32_t              g_Br6909B4;
const void          *g_BrPadHookFn;

void  (*g_BrModelFixup)(uint32_t *pSlot);
void *(*g_BrModelDeref)(uint32_t slot);
BrSegMap *g_BrSegMap;

void *g_BrLogArg;

/* ================================================================== */
/* 1. Camera / matrix set-up                                          */
/* ================================================================== */

/* 0x10035C70  DESTINATION FIRST -- see the header. */
void BrVec3Copy(BrVec3 *pDst, const BrVec3 *pSrc)
{
    pDst->x = pSrc->x;
    pDst->y = pSrc->y;
    pDst->z = pSrc->z;
}

/* Both display-list emitters below inline this in the original: take the
 * write cursor, advance it by 8 bytes, and fill the two words. */
static uint32_t *BrGfxTake2(void)
{
    uint32_t *p = g_BrGfxPtr;
    g_BrGfxPtr += 2;
    return p;
}

/* 0x10033CB1 */
void BrCamFrustumBuild(const BrCamBasis *pCam, float a2, float a3,
                       float a4, float a5)
{
    float a, b;

    a = BrSub10002240(a2) * a3;
    b = a * a5 / a4;
    if (g_BrCamMode == 2)
        b = b / g_BrK08F514;

    BrVec3Copy(&g_BrCamEye, &pCam->eye);

    /* out = a + b*s, so centre = eye + fwd*a3. */
    BrVec3MulAdd(&g_BrCamCentre, &g_BrCamEye, &pCam->fwd, a3);

    BrVec3Scale(&g_BrCamExtentR, &pCam->right, a);
    BrVec3Scale(&g_BrCamExtentU, &pCam->up,    b);

    BrVec3Copy(&g_BrCamCentreCopy, &g_BrCamCentre);

    BrVec3Add   (&g_BrCamCorner0, &g_BrCamCentre, &g_BrCamExtentR);
    BrVec3AddTo (&g_BrCamCorner0, &g_BrCamExtentU);   /* C + R + U */

    BrVec3Add     (&g_BrCamCorner3, &g_BrCamCentre, &g_BrCamExtentR);
    BrVec3SubFrom (&g_BrCamCorner3, &g_BrCamExtentU); /* C + R - U */

    BrVec3Sub   (&g_BrCamCorner1, &g_BrCamCentre, &g_BrCamExtentR);
    BrVec3AddTo (&g_BrCamCorner1, &g_BrCamExtentU);   /* C - R + U */

    BrVec3Sub     (&g_BrCamCorner2, &g_BrCamCentre, &g_BrCamExtentR);
    BrVec3SubFrom (&g_BrCamCorner2, &g_BrCamExtentU); /* C - R - U */

    /* (c - eye)*0.75 + eye, in the original's order. */
    BrVec3Lerp(&g_BrCamCorner1, &g_BrCamCorner1, &g_BrCamEye, 0.75f);
    BrVec3Lerp(&g_BrCamCorner2, &g_BrCamCorner2, &g_BrCamEye, 0.75f);
    BrVec3Lerp(&g_BrCamCorner0, &g_BrCamCorner0, &g_BrCamEye, 0.75f);
    BrVec3Lerp(&g_BrCamCorner3, &g_BrCamCorner3, &g_BrCamEye, 0.75f);

    g_BrCamDist  = a3;
    g_BrCamFovIn = a2;
}

/* 0x10033E83 */
void BrCamMatrixSetup(const BrCamBasis *pCam, float a2, float a3,
                      float a4, float a5)
{
    float fovy;

    BrMat4LookAt(&g_BrViewMat,
                 pCam->eye.x, pCam->eye.y, pCam->eye.z,
                 pCam->eye.x + pCam->fwd.x,
                 pCam->eye.y + pCam->fwd.y,
                 pCam->eye.z + pCam->fwd.z,
                 pCam->up.x, pCam->up.y, pCam->up.z);

    g_BrCamFar  = a3;
    g_BrCamNear = 0.8f;   /* the literal 0x3F4CCCCD, stored to 0x106C3360 */

    /* ((a2 * K518) * (a5 / a4)) * K51C -- note a5/a4 here but a4/a5 as the
     * aspect two lines down. Both are in the original. */
    fovy = a2 * g_BrK08F518;
    fovy = fovy * (a5 / a4);
    fovy = fovy * g_BrK08F51C;

    BrMat4Perspective7(&g_BrProjMat, &g_BrPerspNorm,
                       fovy, a4 / a5, g_BrCamNear, g_BrCamFar, 1.0f);

    BrMat4Mul(&g_BrViewMat, &g_BrProjMat, &g_BrCurMat);

    g_BrMtxSlot = BrPoolAlloc(g_BrPool);
    BrMat4Copy(&g_BrCurMat, (BrMat4 *)g_BrMtxSlot);   /* source first */
}

/* 0x10033F7E  Both parameters are dead; see the header. */
void BrCamMatrixSetupFixed(float a1, float a2)
{
    uint32_t *pCmd;

    (void)a1;
    (void)a2;

    BrMat4LookAt(&g_BrViewMat,
                 512.0f, 384.0f, 1000.0f,
                 512.0f, 384.0f,    0.0f,
                   0.0f,   1.0f,    0.0f);

    BrMat4Perspective7(&g_BrProjMatFixed, &g_BrPerspNorm,
                       45.0f, 1.3333334f, 10.0f, 2000.0f, 1.0f);

    BrMat4Mul(&g_BrViewMat, &g_BrProjMatFixed, &g_BrCurMat);

    pCmd = BrGfxTake2();
    pCmd[0] = 0xBC00000Eu;
    pCmd[1] = g_BrPerspNorm;      /* zero-extended from the u16 */

    g_BrMtxSlot = BrPoolAlloc(g_BrPool);
    BrMat4Copy(&g_BrCurMat, (BrMat4 *)g_BrMtxSlot);

    pCmd = BrGfxTake2();
    pCmd[0] = 0x01030040u;
    pCmd[1] = (uint32_t)(uintptr_t)g_BrMtxSlot;
}

/* 0x1003407D */
void BrCamMatrixSetupOrtho(float w, float h)
{
    uint32_t *pCmd;

    g_BrCurMat.m[0][0] = g_BrK08F514 / w;
    g_BrCurMat.m[0][1] = 0.0f;
    g_BrCurMat.m[0][2] = 0.0f;
    g_BrCurMat.m[0][3] = 0.0f;
    g_BrCurMat.m[1][0] = 0.0f;
    g_BrCurMat.m[1][1] = g_BrK08F514 / h;
    g_BrCurMat.m[1][2] = 0.0f;
    g_BrCurMat.m[1][3] = 0.0f;
    g_BrCurMat.m[2][0] = 0.0f;
    g_BrCurMat.m[2][1] = 0.0f;
    g_BrCurMat.m[2][2] = 0.0f;   /* explicit; z is discarded, not passed on */
    g_BrCurMat.m[2][3] = 0.0f;
    g_BrCurMat.m[3][0] = -1.0f;
    g_BrCurMat.m[3][1] = -1.0f;
    g_BrCurMat.m[3][2] = 0.0f;
    g_BrCurMat.m[3][3] = 1.0f;

    pCmd = BrGfxTake2();
    pCmd[0] = 0xBC00000Eu;
    pCmd[1] = g_BrPerspNorm;

    g_BrMtxSlot = BrPoolAlloc(g_BrPool);
    BrMat4Copy(&g_BrCurMat, (BrMat4 *)g_BrMtxSlot);

    pCmd = BrGfxTake2();
    pCmd[0] = 0x01030040u;
    pCmd[1] = (uint32_t)(uintptr_t)g_BrMtxSlot;
}

/* ================================================================== */
/* 2. Display-list segment fixup                                      */
/* ================================================================== */

/* 0x10035060 */
void BrDlRebaseWord(uint32_t *pWord, uint32_t lo, uint32_t hi, uint32_t base)
{
    if (*pWord >= lo && *pWord < hi)
        *pWord = *pWord - lo + base;
}

/* 0x10035089 */
void BrDlRebase(uint32_t *pDl, uint32_t lo, uint32_t hi, uint32_t base)
{
    if (pDl == NULL)
        return;

    for (;;) {
        uint32_t op = (pDl[0] >> 24) & 0xFFu;

        if (op == 0x04u || op == 0xFDu)          /* G_VTX, G_SETTIMG */
            BrDlRebaseWord(&pDl[1], lo, hi, base);
        else if (op == 0xB8u)                    /* G_ENDDL          */
            return;

        pDl += 2;
    }
}

/* 0x1003445A */
void BrDlOwnerFixup(BrDlOwner *pOwner)
{
    int32_t want;

    g_Br6C666C = 0;

    if (g_Br0B380C == 2 || g_Br0B380C == 8)
        want = 0;
    else
        want = 1;

    if ((pOwner->flags & 4u) == 0)
        g_Br6C666C = want;

    if (BrSub100341B3(pOwner->pDl, g_BrDlTableA))
        pOwner->flags = (uint16_t)(pOwner->flags | 8u);
}

/* ================================================================== */
/* 3. Per-car RDP mode words                                          */
/* ================================================================== */

/* The original writes both halfwords byte-swapped (big-endian, for the RDP).
 * Transcribed as the same shift/mask pair it uses, not as a memory swap. */
static uint16_t BrSwapHalf(uint16_t v)
{
    return (uint16_t)(((uint32_t)v << 8 & 0xFF00u) | ((uint32_t)v >> 8 & 0xFFu));
}

/* 0x10035CA0  __thiscall, ret 0xC. Only the low byte of each argument. */
void BrRgbSinkSet(BrRgbSink *pSink, int r, int g, int b)
{
    pSink->r = (unsigned char)r;
    pSink->g = (unsigned char)g;
    pSink->b = (unsigned char)b;
}

/* 0x100350EE */
void BrCarGfxSetColour(BrCarGfx *pCar, int r, int g, int b)
{
    int32_t   i;
    BrGfxSlot *pSlot;
    uint16_t  *pw;

    if (g_BrCarCount == 0)
        return;

    for (i = 0; i < 12; i++) {
        uint16_t v;

        pSlot = &pCar->pSlots[pCar->aSlotIdx[i]];
        pw    = pSlot->pWords;
        if (pw == NULL)
            continue;
        if (((pSlot->f20 >> 24) & 0xFu) != 1u)
            continue;

        /* GOTCHA: the alpha bit is taken from pw[i], the results land in
         * pw[0] and pw[1]. Faithful. */
        v = (uint16_t)((pw[i] & 1u)
                       | ((uint32_t)r << 11)
                       | ((uint32_t)g << 6)
                       | ((uint32_t)b << 1));
        pw[0] = BrSwapHalf(v);

        v = (uint16_t)((pw[i] & 1u)
                       | (((uint32_t)r & 0x1Eu) << 10)
                       | (((uint32_t)g & 0x1Eu) << 5)
                       | ((uint32_t)b & 0x1Eu));
        pw[1] = BrSwapHalf(v);
    }

    for (i = 0; i < pCar->cDl; i++)
        g_BrGfxSubmit(pCar->aDl[i]);

    pw = pCar->pSlots[pCar->aSlotIdx[11]].pWords;
    if (pw == NULL)
        return;
    if (g_Br0AC300 != 0)
        return;

    if (pCar->aDlExtra[0] != 0) {
        if (g_Br6C661C != 0 || g_Br6C6624 != 0) {
            pw[15] = 0x0070u;   /* +0x1E */
            pw[10] = 0x8290u;   /* +0x14 */
        } else {
            pw[15] = 0x0190u;
            pw[10] = 0x01A0u;
        }
        pw[14] = 0x0190u;       /* +0x1C */
        pw[13] = pw[15];        /* +0x1A <- +0x1E */
        pw[9]  = 0x01A0u;       /* +0x12 */
        pw[8]  = pw[10];        /* +0x10 <- +0x14 */
        pw[12] = 0x8179u;       /* +0x18 */
        pw[7]  = 0x4192u;       /* +0x0E */
        pw[11] = 0x6BADu;       /* +0x16 */
        pw[6]  = 0x31C6u;       /* +0x0C */
        g_BrGfxSubmit(pCar->aDlExtra[0]);
    }

    if (pCar->aDlExtra[1] != 0) {
        pw[14] = 0x00C0u;
        pw[13] = pw[14];        /* +0x1A <- +0x1C, unlike block 1 */
        pw[9]  = 0x04F9u;
        pw[8]  = pw[9];         /* +0x10 <- +0x12, unlike block 1 */
        pw[11] = 0x6BADu;
        pw[6]  = 0x31C6u;
        g_BrGfxSubmit(pCar->aDlExtra[1]);
    }

    if (pCar->aDlExtra[2] != 0) {
        pw[14] = 0x0190u;
        pw[13] = pw[15];        /* +0x1A <- +0x1E */
        pw[9]  = 0x01A0u;
        pw[8]  = pw[10];        /* +0x10 <- +0x14 */
        pw[11] = 0x38E7u;
        pw[6]  = 0xFEFFu;
        g_BrGfxSubmit(pCar->aDlExtra[2]);
    }

    if (pCar->aDlExtra[3] != 0) {
        pw[14] = 0x00C0u;
        pw[13] = pw[14];
        pw[9]  = 0x04F9u;
        pw[8]  = pw[9];
        pw[11] = 0x38E7u;
        pw[6]  = 0xFEFFu;
        g_BrGfxSubmit(pCar->aDlExtra[3]);
    }
}

/* 0x10035452 */
void BrCarGfxReadColour(BrRgbSink *pSink, const BrCarGfx *pCar)
{
    const BrGfxSlot *pSlot = &pCar->pSlots[pCar->aSlotIdx[2]];
    const uint16_t  *pw    = pSlot->pWords;
    int c, r, g, b;

    if (pw == NULL)
        return;
    if (((pSlot->f20 >> 24) & 0xFu) != 1u)
        return;

    /* Read natively -- see the GOTCHA in the header. */
    c = (int)pw[0];
    r = ((c >> 8) & 0xF8) | ((c >> 13) & 7);
    g = ((c >> 3) & 0xF8) | ((c >>  8) & 7);
    b = ((c << 2) & 0xF8) | ((c >>  3) & 7);

    BrRgbSinkSet(pSink, r, g, b);
}

/* ================================================================== */
/* 4. Keyframe vertex animation                                       */
/* ================================================================== */

/* 0x10035585 */
void BrAnimFlagsApply(BrAnimSet *pSet, uint16_t orBits, uint32_t clearBits)
{
    int32_t i, n;

    clearBits = ~clearBits;          /* the original's `not eax`, 32-bit */

    if (pSet->pList == NULL)
        return;

    n = pSet->pList->n;
    for (i = 0; i < n; i++) {
        BrAnimTrack *pT = pSet->pList->a[i];
        pT->flags = (uint16_t)(pT->flags | orBits);
        pT->flags = (uint16_t)(pT->flags & (uint16_t)clearBits);
    }
}

void BrAnimSetOnce(BrAnimSet *pSet)     { BrAnimFlagsApply(pSet, 0, 3); }
void BrAnimSetLoop(BrAnimSet *pSet)     { BrAnimFlagsApply(pSet, 1, 2); }
void BrAnimSetPingPong(BrAnimSet *pSet) { BrAnimFlagsApply(pSet, 3, 0); }

/* 0x1007C8A0 __ftol -- truncate toward zero, low dword before any clamp.
 *
 * DEVIATION: C's (int) cast is undefined for values outside int range and
 * for NaN, and BrAnimUpdate's three documented divide-by-zero paths do
 * produce those. The original's x87 FISTP stores the integer indefinite
 * 0x80000000 there, so the port does the same explicitly. In every one of
 * those paths the two brackets are the SAME keyframe, so the resulting
 * garbage frac is multiplied by a zero delta and never reaches the output. */
static int BrFtol(float f)
{
    if (!(f > -2147483649.0f && f < 2147483648.0f))
        return (int)0x80000000L;
    return (int)f;
}

/* lo + (((hi - lo) * frac) >> 12), truncated back to the source width. The
 * truncation is a `movsx ax` / `movsx al` in the original and does wrap. */
static int BrAnimLerp16(int lo, int hi, int frac)
{
    return (int)(int16_t)((((hi - lo) * frac) >> 12) + lo);
}

static int BrAnimLerp8(int lo, int hi, int frac)
{
    return (int)(int8_t)((((hi - lo) * frac) >> 12) + lo);
}

/* 0x1003563A */
void BrAnimUpdate(BrAnimSet *pSet)
{
    BrAnimList *pList;
    int32_t i, n;

    if (pSet->pList == NULL)
        return;

    pList = pSet->pList;
    n = pList->n;

    for (i = 0; i < n; i++) {
        BrAnimTrack     *pT = pList->a[i];
        const BrAnimKey *pLo;
        const BrAnimKey *pHi;
        const int16_t   *pS16;
        const int16_t   *pE16;
        const int8_t    *pS8;
        const int8_t    *pE8;
        BrAnimVtx       *pOut;
        float t, u, span, lim;
        int32_t k, m, cVerts;
        int frac;

        if ((pT->flags & 4u) != 0) {
            /* ---- playing in reverse (0x1003569A) ---- */
            pT->t -= g_BrAnimDt;
            t = pT->t;

            if (!(t >= pT->tLo)) {
                /* 0x1003595E -- reflect off the low end, or stop */
                if ((pT->flags & 1u) == 0)
                    continue;
                t = g_BrK08F514 * pT->tLo - t;
                pT->t    = t;
                pT->flags = (uint16_t)(pT->flags & 0xFFFBu);
                pT->iKey  = 0;
                goto search;
            }
            if (t >= pT->tHi)
                continue;
            goto search;
        }

        /* ---- playing forward (0x100359AE) ---- */
        pT->t += g_BrAnimDt;
        t = pT->t;

        if (!(t >= pT->tLo)) {
            /* GOTCHA: both brackets become aKeys[0], so the interpolation
             * below divides by zero. Original behaviour. */
            pHi = pT->aKeys[0];
            pLo = pT->aKeys[0];
            t = 0.0f;
            goto interp;
        }

        if (!(t >= pT->tHi)) {
            if (!(t >= pT->tLo))
                continue;
            goto search;
        }

        /* 0x10035A21 -- past the end */
        if ((pT->flags & 1u) == 0) {
            /* GOTCHA: same degenerate bracket as above. The original indexes
             * +0x1C + cKeys*4, i.e. the LAST key; with cKeys == 0 it would
             * read the `t` field as a pointer. */
            pHi = pT->aKeys[pT->cKeys - 1];
            pLo = pT->aKeys[pT->cKeys - 1];
            t = 0.0f;
            goto interp;
        }

        span = pT->tHi - pT->tLo;
        lim  = pT->tHi + span;

        if ((pT->flags & 2u) != 0) {
            span = (pT->tHi - pT->tLo) * g_BrK08F514;
            lim  = pT->tHi + span;
            while (t > lim)
                t -= span;
            span = span * g_BrK08F534;
            lim  = lim - span;
            /* GOTCHA: this falls into the PLAIN wrap loop, which then also
             * runs the plain tail -- the reverse bit is never set. */
            if (t > lim)
                goto wrap_plain;
            t = g_BrK08F514 * pT->tHi - t;
            pT->t = t;
            pT->flags = (uint16_t)(pT->flags | 4u);
            goto reset_key;
        }

        span = pT->tHi - pT->tLo;
        lim  = pT->tHi + span;

    wrap_plain:
        while (t > lim)
            t -= span;
        t = t - (pT->tHi - pT->tLo);
        pT->t = t;

    reset_key:
        pT->iKey = 0;

    search:
        /* GOTCHA: k is not re-tested against cKeys before the load, so a
         * track whose last key time is <= t reads aKeys[cKeys]. */
        k = pT->iKey;
        while (k < pT->cKeys) {
            if (pT->aKeys[k]->t > t)
                break;
            k++;
        }
        pHi = pT->aKeys[k];
        k--;
        pLo = pT->aKeys[k];

    interp:
        u    = (t - pLo->t) / (pHi->t - pLo->t);
        frac = BrFtol(u * g_BrK08F52C);     /* 0x1007C8A0 */

        cVerts = (int32_t)pT->cVerts;
        pS16 = (const int16_t *)((const char *)pLo + 4);
        pE16 = (const int16_t *)((const char *)pHi + 4);
        pS8  = (const int8_t  *)(pS16 + (size_t)cVerts * 3);
        pE8  = (const int8_t  *)(pE16 + (size_t)cVerts * 3);
        pOut = pT->pOut;

        for (m = 0; m < cVerts; m++) {
            pOut[m].x = (float)BrAnimLerp16(pS16[0], pE16[0], frac);
            pOut[m].y = (float)BrAnimLerp16(pS16[1], pE16[1], frac);
            pOut[m].z = (float)BrAnimLerp16(pS16[2], pE16[2], frac);

            pOut[m].nx = (float)BrAnimLerp8(pS8[0], pE8[0], frac) * g_BrK08F530;
            pOut[m].ny = (float)BrAnimLerp8(pS8[1], pE8[1], frac) * g_BrK08F530;
            pOut[m].nz = (float)BrAnimLerp8(pS8[2], pE8[2], frac) * g_BrK08F530;

            pS16 += 3;
            pE16 += 3;
            pS8  += 3;
            pE8  += 3;
        }
    }
}

/* ================================================================== */
/* 5. Controller translation                                          */
/* ================================================================== */

/* One of the two identical ramp steps at 0x10035E9C. */
static void BrPadRamp(const int32_t *pEnable, int32_t *pCur, const int32_t *pLim)
{
    if (*pEnable == 0)
        return;
    if (*pCur < *pLim && g_Br6909B4 == 0)
        *pCur = *pCur + 2;
}

/* Reproduces `if (v > hi) v = 1; else if (v < lo) v = -1;` including the
 * unordered case, which the original routes to the LOW assignment. */
static float BrPadClamp(float v)
{
    if (v > g_BrK08F54C)
        return 1.0f;
    if (!(v >= g_BrK08F550))
        return -1.0f;
    return v;
}

/* 0x10035CE0  __thiscall */
void BrPadTranslate(BrPad *pPad)
{
    BrPadRaw *pRaw = pPad->pRaw;
    unsigned lo, hi;
    uint32_t bt;

    if (pRaw->status != 0) {
        pPad->f28 = (pRaw->status == 8) ? 1 : 0;
        pRaw->stickX = 0;
        pRaw->stickY = 0;
        pRaw->b0 = 0;
        pRaw->b1 = 0;
    } else {
        pPad->f28 = 0;
    }

    /* `mov ax, word ptr [edx]`: al is byte 0, ah is byte 1. */
    lo = pRaw->b0;
    hi = pRaw->b1;

    bt = 0;
    if (hi & 0x08u) bt  = BR_PAD_DUP;      /* CONT_UP    0x0800 */
    if (hi & 0x04u) bt |= BR_PAD_DDOWN;    /* CONT_DOWN  0x0400 */
    if (hi & 0x02u) bt |= BR_PAD_DLEFT;    /* CONT_LEFT  0x0200 */
    if (hi & 0x01u) bt |= BR_PAD_DRIGHT;   /* CONT_RIGHT 0x0100 */
    if (hi & 0x80u) bt |= BR_PAD_A;        /* CONT_A     0x8000 */
    if (hi & 0x40u) bt |= BR_PAD_B;        /* CONT_B     0x4000 */
    if (lo & 0x20u) bt |= BR_PAD_L;        /* CONT_L     0x0020 */
    if (lo & 0x10u) bt |= BR_PAD_R;        /* CONT_R     0x0010 */
    if (hi & 0x20u) bt |= BR_PAD_Z;        /* CONT_G     0x2000 */
    if (hi & 0x10u) bt |= BR_PAD_START;    /* CONT_START 0x1000 */
    if (lo & 0x08u) bt |= BR_PAD_CUP;      /* CONT_E     0x0008 */
    if (lo & 0x01u) bt |= BR_PAD_CRIGHT;   /* CONT_F     0x0001 */
    if (lo & 0x04u) bt |= BR_PAD_CDOWN;    /* CONT_D     0x0004 */
    if (lo & 0x02u) bt |= BR_PAD_CLEFT;    /* CONT_C     0x0002 */
    pPad->buttons = bt;

    if (BrHookIsCurrent(g_BrPadHookFn)) {
        if (pPad->buttons & BR_PAD_L) pPad->buttons |= BR_PAD_L_ALT;
        if (pPad->buttons & BR_PAD_R) pPad->buttons |= BR_PAD_R_ALT;

        if ((g_BrPadModeBytes[1] & 0x80u) == 0 &&
            (g_BrPadModeBytes[7] & 0x80u) == 0) {
            uint32_t a = pPad->buttons;
            if (a & BR_PAD_DLEFT)
                pPad->steer = (a & BR_PAD_DRIGHT) ? (int8_t)0 : (int8_t)0xB0;
            else
                pPad->steer = (a & BR_PAD_DRIGHT) ? (int8_t)0x50 : (int8_t)0;
        } else {
            pPad->steer = pRaw->stickX;
        }

        if (pPad->buttons & BR_PAD_A) {
            if (pRaw->stickY < (int8_t)0xC0)          /* signed, -64 */
                pPad->buttons |= BR_PAD_A_BACK;
            pPad->buttons |= BR_PAD_A_D;
        }
        if (pPad->buttons & BR_PAD_B) {
            if (pPad->buttons & BR_PAD_A_D)
                pPad->buttons |= BR_PAD_B_A;
            else
                pPad->buttons |= BR_PAD_B_ALT;
        }
        if (pPad->buttons & BR_PAD_CUP)   pPad->buttons |= BR_PAD_CUP2;
        if (pPad->buttons & BR_PAD_CDOWN) pPad->buttons |= BR_PAD_CDOWN2;
        if (pPad->buttons & BR_PAD_CLEFT) pPad->buttons |= BR_PAD_CLEFT2;
    }

    if (pPad->f2C == 0 && pPad->f30 == 0) {
        /* The original loads f44 into eax here and never uses it. */
    } else {
        BrPadRamp(&pPad->f2C, &pPad->f34, &pPad->f3C);
        BrPadRamp(&pPad->f30, &pPad->f38, &pPad->f40);
    }

    pPad->axisX     = (float)(int)pRaw->stickX * g_BrK08F548;
    pPad->axisY     = (float)(int)pRaw->stickY * g_BrK08F548;
    pPad->axisSteer = (float)(int)pPad->steer  * g_BrK08F548;

    pPad->axisX     = BrPadClamp(pPad->axisX);
    pPad->axisY     = BrPadClamp(pPad->axisY);
    /* DEVIATION: the original compares the steering axis while it is still
     * in an x87 register (80-bit), unlike the other two which are reloaded
     * from their 32-bit slots. The port compares the 32-bit value. */
    pPad->axisSteer = BrPadClamp(pPad->axisSteer);
}

/* 0x10035FC0  __thiscall */
void BrBitEdgeSplit(BrBitPair *pPair)
{
    uint32_t a = pPair->a;
    uint32_t b = pPair->b;

    pPair->a = a & ~b;
    pPair->b = a &  b;
}

/* ================================================================== */
/* 6. Big-endian model fixup                                          */
/* ================================================================== */

/* `swap byte n with byte n+3, byte n+1 with byte n+2` -- what the original
 * spells out for every 32-bit slot it is about to hand to the fixup. */
static void BrRev4(void *pv)
{
    unsigned char *p = (unsigned char *)pv;
    unsigned char t;

    t = p[0]; p[0] = p[3]; p[3] = t;
    t = p[1]; p[1] = p[2]; p[2] = t;
}

static void BrRev2(void *pv)
{
    unsigned char *p = (unsigned char *)pv;
    unsigned char t = p[0];

    p[0] = p[1];
    p[1] = t;
}

/* The other form the original uses for 32-bit fields: compose the value
 * byte-wise MSB-first and store it natively. Identical to BrRev4 on the
 * little-endian host the original ran on; kept distinct because the two are
 * genuinely different instruction sequences. */
static void BrRdBe32(void *pv)
{
    const unsigned char *p = (const unsigned char *)pv;
    uint32_t v = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
               | ((uint32_t)p[2] <<  8) | (uint32_t)p[3];

    memcpy(pv, &v, 4);
}

static uint32_t BrLd32(const void *pv)
{
    uint32_t v;

    memcpy(&v, pv, 4);
    return v;
}

static uint16_t BrLd16(const void *pv)
{
    uint16_t v;

    memcpy(&v, pv, 2);
    return v;
}

/* 0x10036C00 */
void BrModelSwap(void *pImage)
{
    unsigned char *pHdr = (unsigned char *)pImage;
    unsigned char *pRec;
    uint32_t iRec;

    /* Header +0x00 and +0x02: two independent big-endian halfwords. */
    BrRev2(pHdr + 0);
    BrRev2(pHdr + 2);

    /* GOTCHA: tested BEFORE the byte reversal. Only works because zero is a
     * palindrome. */
    if (BrLd32(pHdr + 4) != 0) {
        unsigned char *pBlock;
        int32_t iItem;

        BrRev4(pHdr + 4);
        g_BrModelFixup((uint32_t *)(pHdr + 4));
        pBlock = (unsigned char *)g_BrModelDeref(BrLd32(pHdr + 4));

        BrRdBe32(pBlock);                  /* block->n */

        /* The original re-reads the count from the block on every pass. */
        for (iItem = 0; iItem < (int32_t)BrLd32(pBlock); iItem++) {
            unsigned char *pSlot = pBlock + 4 + 4 * (size_t)iItem;
            unsigned char *pItem;
            int32_t iLeaf;
            size_t off;

            BrRev4(pSlot);
            g_BrModelFixup((uint32_t *)pSlot);
            pItem = (unsigned char *)g_BrModelDeref(BrLd32(pSlot));

            BrRdBe32(pItem + 0x00);        /* item->m */
            BrRev4  (pItem + 0x04);
            g_BrModelFixup((uint32_t *)(pItem + 0x04));

            /* The vertex-cache resolve is handed the SLOT, not the value. */
            BrModelVtxResolve((uint32_t *)(pItem + 0x04),
                              (int)BrLd32(pItem + 0x00));

            BrRev4(pItem + 0x08);
            g_BrModelFixup((uint32_t *)(pItem + 0x08));

            BrRdBe32(pItem + 0x0C);        /* item->k */
            BrRev2  (pItem + 0x10);
            BrRev2  (pItem + 0x12);
            BrRdBe32(pItem + 0x14);
            BrRdBe32(pItem + 0x18);
            BrRdBe32(pItem + 0x1C);

            /* The leaf count is likewise re-read from the item every pass;
             * the original's `if (k <= 0) skip` guard is the same test. */
            off = 0x20;
            for (iLeaf = 0;
                 iLeaf < (int32_t)BrLd32(pItem + 0x0C);
                 iLeaf++, off += 4) {
                unsigned char *pLeaf;
                int32_t nHalf, j;

                BrRev4(pItem + off);
                g_BrModelFixup((uint32_t *)(pItem + off));
                pLeaf = (unsigned char *)g_BrModelDeref(BrLd32(pItem + off));

                BrRev4(pLeaf + 0);

                /* GOTCHA: the halfword count comes from the ITEM's first
                 * dword, not the leaf's. */
                nHalf = 3 * (int32_t)BrLd32(pItem + 0x00);
                for (j = 0; j < nHalf; j++)
                    BrRev2(pLeaf + 4 + 2 * (size_t)j);
            }
        }
    }

    /* ---- the record array at +0x08, stride 0x14 ----
     * The count is re-read from the header on every pass, and the compare
     * is unsigned. */
    pRec = pHdr + 8;

    for (iRec = 0; iRec < (uint32_t)BrLd16(pHdr + 2); iRec++, pRec += 0x14) {
        uint32_t v;

        if (BrLd32(pRec) == 0)
            continue;

        BrRev4(pRec + 0x00);
        g_BrModelFixup((uint32_t *)(pRec + 0x00));
        BrRev2(pRec + 0x04);
        BrRev2(pRec + 0x06);
        BrRev4(pRec + 0x08);
        BrRev4(pRec + 0x0C);
        BrRev4(pRec + 0x10);

        v = BrLd32(pRec);
        BrSub1002BF80(v);
        BrSub10074DC0(8);
        g_BrGfxSubmitB(BrLd32(pRec));
    }
}

/* 0x10036BD0 */
void *BrModelLoad(void *pMgr, void *a1, void *a2)
{
    void *p;

    /* GOTCHA: a2 is pushed last, so it is the callee's FIRST argument. */
    p = BrSub100088B0(pMgr, a2, a1);

    BrSegSetBases(g_BrSegMap, 0, (uint32_t)(uintptr_t)p);
    BrModelSwap(p);
    return p;
}

/* ================================================================== */
/* 7. Odds and ends                                                   */
/* ================================================================== */

/* 0x100347BA */
void BrAccumAddClamp(float *aTable, int i, float amt)
{
    if (amt > g_BrK08F520)
        amt = 2.5f;

    aTable[i] = aTable[i] + amt;

    if (aTable[i] > g_BrK08F524)
        aTable[i] = 5.0f;
}

/* 0x10035041 */
void BrPairSlotReset(BrPairSlot *p, uint32_t v)
{
    p->f04 = 0;
    p->f08 = v;
}

/* 0x10035059 */
int BrRet0_10035059(void) { return 0; }
/* 0x1003557B */
int BrRet1_1003557B(void) { return 1; }
/* 0x10035B87 */
int BrRet1_10035B87(void) { return 1; }

/* 0x10035520 */
void BrCarSlotLoad(unsigned char *aCars, void **aCarPtr, int i,
                   void *pArg, int flag)
{
    unsigned char *pCar = aCars + (size_t)i * 0x15F88u;

    /* GOTCHA: flag != 0 means "do not load", not "load". */
    if (flag == 0)
        BrSub10037740(pCar, pArg);
    else
        BrLogPrint("LoadCar()");

    BrSub1003551B(pCar);
    aCarPtr[i] = pArg;
}

/* 0x10035BA7  The parameter is never read. */
void BrLogEmit(void *ignored)
{
    (void)ignored;
    BrLogPrint(g_BrLogArg);
}

/* 0x10035BBA */
void BrLogSet(void *p)
{
    g_BrLogArg = p;
    BrLogEmit(NULL);
}

/* ==================================================================
 * NOTES ON THE SKIPPED FUNCTIONS -- recorded so the analysis is not lost.
 * ==================================================================
 *
 * 0x100341B3 (packet starts at 0x100341E2, 47 bytes in)
 *   Walks an 8-byte-command display list until a null command pointer,
 *   dispatching on (w0 >> 24) - 0xB8 through a 0x45-entry byte index at
 *   0x10034415 into a jump table at 0x100343FD. Four handled cases:
 *     * match w0/w1 against six 32-byte records at arg2 and, on a hit,
 *       replace the command with the pair at (record + [ebp-4]*8); a hit at
 *       record index >= 3 sets the return value to 1;
 *     * with [ebp-0xC] != 0, match against one 16-byte record at 0x100AA8B8
 *       and substitute from +0x08/+0x0C;
 *     * scan two 8-byte entries at 0x100AA8C8 and set a local flag;
 *     * two near-identical tails that force w1 to 0x60789000 or 0x8C9CA800
 *       when that flag and g_6C6620 are both set.
 *   The prologue would tell us how [ebp-0x18] (which selects the +1 or +2
 *   column via `sete`) and the return slot [ebp-0x14] are initialised.
 *   Without it the function cannot be written down honestly.
 *
 * 0x10034F37 (packet starts mid-function)
 *   A plane-interleaved RLE decoder. For each of arg3 planes it reads a
 *   4-byte little-endian chunk length through 0x1007ED60 (memcpy), then
 *   consumes control bytes:
 *     c  < 0 : copy -c literal bytes, each written stride-arg3 apart;
 *     c >= 0 : repeat the next byte (c + BIAS) times, same stride.
 *   The destination pointer advances by ONE between planes, which is what
 *   makes the output interleaved. It returns the final destination offset.
 *   BIAS lives in [ebp-8] and is only ever written by the missing prologue,
 *   and it changes every decoded length, so the function is unusable
 *   without it.
 */
