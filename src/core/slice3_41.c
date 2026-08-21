/* slice3_41.c -- Boss Rally (BRD3D.dll) slice 3, a later pass.
 *
 * See slice3_41.h for the packet inventory, the offsets that were recovered,
 * and the list of functions that were deliberately left out.
 *
 * Every x87 sequence in here was traced through its fxch chain; where the
 * original reads a status word twice and looks at different bits each time,
 * the C is written to reproduce that exactly (including what happens to a
 * NaN), not to look tidy.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "slice3_41.h"

/* ---------------------------------------------------------------------
 * Constants read out of BRD3D.dll .rdata with tools/pe.py.  Do not
 * "simplify" these -- the decimal forms are the exact float32 values.
 * ------------------------------------------------------------------- */
#define BR_K_0008F8D0   10.0f                        /* pan clamp, upper   */
#define BR_K_0008F904   0.5f                         /* the pan snap value */
#define BR_K_0008F930   1.0f
#define BR_K_0008F944   0.4f                         /* narrow-pan scale   */
#define BR_K_0008F9A0   (-10.0f)                     /* pan clamp, lower   */
#define BR_K_0008F9EC   (-0.0029154520016163588f)    /* -1/343             */
#define BR_K_0008F9F0   0.0029154520016163588f       /* +1/343             */
#define BR_K_0008F9F4   0.05f                        /* 1/20               */
#define BR_K_0008F9F8   0.49f                        /* snap window, low   */
#define BR_K_0008F9FC   0.51f                        /* snap window, high  */
#define BR_K_0008FA00   1.6f
#define BR_K_0008FA04   (-0.6f)
#define BR_K_0008FA08   32.0                         /* double! see below  */
#define BR_K_0008FA10   32.0f                        /* min distance       */
#define BR_K_0008FA14   1024.0f                      /* volume numerator   */

/* 0x462BE000, the literal 0x10068210 pushes as the base frequency. */
#define BR_SND_DEFAULT_HZ  11000.0f

/* =====================================================================
 * 1.  Driver records and the race-position sort
 * ===================================================================== */

/* The 8-byte element the original sorts.  `key` MUST come first: the
 * comparator dereferences the element pointer as a float directly. */
typedef struct BrRankPair {
    float   key;
    int32_t idx;
} BrRankPair;

/* 0x10066620 */
/* WHAT IT DOES: the "who is ahead" test used when the game sorts the field
 * into race order -- it compares two drivers' progress figures and says which
 * comes first. If either figure is not a real number the answer it gives is
 * "less than" rather than "equal", which is the original's behaviour and not
 * a tidy-up opportunity. */
/* @implements 0x10066620 d3d BrRankCmpKey */
int BrRankCmpKey(const void *pA, const void *pB)
{
    const float a = *(const float *)pA;
    const float b = *(const float *)pB;

    /* First fcomp + `test ah,0x41` tests C0|C3, i.e. "a < b or a == b or
     * unordered".  Taking the fall-through means strictly a > b, which is
     * false for a NaN -- so `a > b` is the exact translation. */
    if (a > b)
        return 1;

    /* Second fcomp + `test ah,1` tests C0 alone, i.e. "a < b or unordered".
     * !(a >= b) is exactly that, NaN included.  So an unordered pair reports
     * -1 rather than 0; that is the original's behaviour, not a slip. */
    if (!(a >= b))
        return -1;

    return 0;
}

/* The g_22AF18 == 0 half of 0x10066510. */
void BrRankAssign(BrDriver *pSlots, int32_t n)
{
    BrRankPair a[BR_RANK_MAX];
    int32_t    i, j, m = 0;

    for (i = 0; i < n; i++) {
        if ((pSlots[i].f68 & BR_DRIVER_SKIP) != 0)
            continue;

        /* DEVIATION: the original's pair buffer is a bare 0xA0-byte stack
         * array with no bound check, so a 21st participating slot smashes
         * the saved registers behind it.  Extra slots are dropped here
         * instead.  Everything at or below 20 participants is bit-identical. */
        if (m >= BR_RANK_MAX)
            break;

        a[m].idx = i;
        a[m].key = (pSlots[i].pCar != NULL) ? pSlots[i].pCar->fFF4
                                            : pSlots[i].f50;
        m++;
    }

    if (m != 0)
        qsort(a, (size_t)m, sizeof a[0], BrRankCmpKey);

    for (j = 0; j < m; j++) {
        BrDriver *pS = &pSlots[a[j].idx];

        /* Note `n`, not `m`: see the GOTCHA in the header. */
        if (pS->pCar != NULL)
            pS->pCar->fFF8 = n - j - 1;
        else
            pS->f54 = n - j - 1;
    }
}

/* =====================================================================
 * 2.  Variable-block save / restore
 * ===================================================================== */

/* 0x10067880 */
/* WHAT IT DOES: gathers a list of scattered game variables into one
 * contiguous block of memory -- the snapshot the replay and save-state code
 * works from. If the block turns out not to have been big enough it stops the
 * game with an error, but only after the overrun has already happened. */
/* @implements 0x10067880 d3d BrVarSave */
void BrVarSave(const BrVarBlock *pTable, void *pDst, int32_t cbAvail)
{
    uint8_t *pOut = (uint8_t *)pDst;
    int32_t  cbUsed;

    while (pTable->pData != NULL) {
        memcpy(pOut, pTable->pData, (size_t)pTable->cb);
        pOut += pTable->cb;
        pTable++;
    }

    cbUsed = (int32_t)(pOut - (uint8_t *)pDst);
    if (cbUsed > cbAvail) {
        /* DEVIATION: the original sprintf()s into an 80-byte stack buffer
         * (0x1007C830) before calling BrFatal (0x10035BBA).  snprintf into
         * the same-sized buffer; the text is byte-for-byte the original's. */
        char szMsg[0x50];
        snprintf(szMsg, sizeof szMsg,
                 "VAR SAVE OVERFLOW (%d avail, %d used)",
                 (int)cbAvail, (int)cbUsed);
        BrFatal(szMsg);
    }
}

/* 0x10067900 */
/* WHAT IT DOES: puts a previously gathered snapshot back where it came from,
 * restoring every variable in the list. It trusts the buffer completely --
 * there is no length given and no check made. */
/* @implements 0x10067900 d3d BrVarLoad */
void BrVarLoad(const BrVarBlock *pTable, const void *pSrc)
{
    const uint8_t *pIn = (const uint8_t *)pSrc;

    while (pTable->pData != NULL) {
        memcpy(pTable->pData, pIn, (size_t)pTable->cb);
        pIn += pTable->cb;
        pTable++;
    }
}

/* =====================================================================
 * 3.  Positional audio maths
 * ===================================================================== */

/* 0x10067AE0 */
/* WHAT IT DOES: works out how much to raise or lower the pitch of a sound
 * because the thing making it and the thing hearing it are moving relative to
 * one another -- the rising-then-falling note of a car going past. It uses the
 * real speed of sound, and it has no protection against a source approaching
 * faster than sound, which sends the answer negative. */
/* @implements 0x10067AE0 d3d BrSndDoppler */
float BrSndDoppler(const BrVec3 *pSrcPos, const BrVec3 *pSrcPrev,
                   const BrVec3 *pLisPos, const BrVec3 *pLisPrev)
{
    BrVec3 u, vLis, vSrc;
    float  len, a, b;

    BrVec3Sub(&u,    pSrcPos, pLisPos);     /* listener -> source          */
    BrVec3Sub(&vLis, pLisPos, pLisPrev);    /* listener travel this frame  */
    BrVec3Sub(&vSrc, pSrcPos, pSrcPrev);    /* source travel this frame    */

    len = BrVec3Length(&u);
    /* `fcomp 0.0f` then `test ah,0x40` (C3, "equal") and jne to skip.  An
     * unordered compare also sets C3, so a NaN length skips normalising
     * too -- hence the explicit two-sided test rather than `len != 0`. */
    if (len > 0.0f || len < 0.0f)
        BrVec3DivBy(&u, len);

    /* Argument order preserved: the original passes the velocity first and
     * the direction second to both dot products. */
    a = -(BrVec3Dot(&vSrc, &u) / g_BrAnimDt);
    b =   BrVec3Dot(&vLis, &u) / g_BrAnimDt;

    /* fsubr against 1.0 in both halves; the two 1/343 constants carry
     * opposite signs, which is where the numerator's + comes from. */
    return (BR_K_0008F930 - b * BR_K_0008F9EC)
         / (BR_K_0008F930 - a * BR_K_0008F9F0);
}

/* 0x10067BC0 */
/* WHAT IT DOES: places a sound in the stereo image and decides how loud it
 * should be, from where it is relative to the listener: how far off to one
 * side gives the balance between the two speakers, how far away gives the
 * volume. Sounds very nearly centred are snapped to dead centre so they do
 * not wander, and anything closer than a fixed minimum distance is treated as
 * being at that distance so it cannot become infinitely loud. A "narrow" mode
 * squeezes the whole stereo spread towards the middle. Which of the two gains
 * is the left speaker and which the right could not be established. */
/* @implements 0x10067BC0 d3d BrSndPan */
void BrSndPan(const BrVec3 *pSrcPos, const BrMat4 *pListener,
              float *pGainA, float *pGainB, int32_t *pVol, int32_t fNarrow)
{
    BrVec3 d;
    float  proj, p, q, dist;

    /* Row 3 of the listener's matrix is its position, row 1 is the axis the
     * pan is measured along. */
    BrVec3Sub(&d, pSrcPos, (const BrVec3 *)(const void *)&pListener->m[3][0]);

    /* Summation order is (m1x*dx + m1y*dy) + m1z*dz, as the fxch chain
     * builds it. */
    proj = pListener->m[1][0] * d.x + pListener->m[1][1] * d.y
         + pListener->m[1][2] * d.z;

    /* First branch falls through only on a strict >, second only on C0
     * (less-than OR unordered) -- so a NaN projection ends up at -10. */
    if (proj > BR_K_0008F8D0)
        proj = BR_K_0008F8D0;
    else if (!(proj >= BR_K_0008F9A0))
        proj = BR_K_0008F9A0;

    if (fNarrow != 0)
        proj *= BR_K_0008F944;

    proj -= BR_K_0008F9A0;              /* i.e. += 10 */

    p = proj * BR_K_0008F9F4;           /* [0, 1] wide, [0.3, 0.7] narrow */
    q = BR_K_0008F930 - p;

    /* Both windows are checked even though the second is implied by the
     * first; preserved. */
    if (BR_K_0008F9F8 <= p && p <= BR_K_0008F9FC &&
        BR_K_0008F9F8 <= q && q <= BR_K_0008F9FC) {
        p = BR_K_0008F904;
        q = BR_K_0008F904;
    }

    /* The larger channel is pushed toward 1, the smaller is scaled by 1.6.
     * Dead centre lands on 0.8 from both sides, so the law is continuous. */
    if (p < q) {
        float pOut = p * BR_K_0008FA00;
        q = q - (BR_K_0008F930 - q) * BR_K_0008FA04;
        p = pOut;
    } else {
        float pOut = p - (BR_K_0008F930 - p) * BR_K_0008FA04;
        q = q * BR_K_0008FA00;
        p = pOut;
    }

    *pGainA = p;
    *pGainB = q;

    dist = BrVec3Length(&d);
    /* `fcom qword 32.0` (a DOUBLE) but the replacement loaded on the taken
     * branch is the FLOAT 32.0 at 0x1008FA10.  Same value, different
     * constants in the image.  C0 also covers unordered, so a NaN distance
     * clamps to 32 as well. */
    if (!((double)dist >= BR_K_0008FA08))
        dist = BR_K_0008FA10;

    /* __ftol (0x1007C8A0) truncates toward zero.  dist >= 32 bounds the
     * quotient at 32, so there is nothing to overflow. */
    *pVol = (int32_t)(BR_K_0008FA14 / dist);
}

/* =====================================================================
 * 4.  Nearest-source tracker
 * ===================================================================== */

BrSndNearest g_BrSndNearest;
int32_t      g_BrSndAA3470 = -1;

/* 0x10067DA0 */
/* WHAT IT DOES: clears the "closest sound this frame" contest so a new round
 * of candidates can be offered, while deliberately keeping the record of
 * whatever won last time -- that is how the game later notices a sound that
 * has stopped being offered at all. */
/* @implements 0x10067DA0 d3d BrSndNearestInvalidate */
void BrSndNearestInvalidate(void)
{
    g_BrSndNearest.metric = BR_SND_NEAREST_FAR;
    g_BrSndNearest.f84    = -1;
    g_BrSndNearest.f8C    = -1;
    /* f88 and f90 are deliberately untouched -- see the header. */
}

/* 0x10067DC0 */
/* WHAT IT DOES: wipes the closest-sound tracker completely, including the
 * memory of what won on previous frames -- the full reset done when the
 * game changes scene, as opposed to the light per-frame clear above. One
 * field, the base pitch, is left as it was. */
/* @implements 0x10067DC0 d3d BrSndNearestReset */
void BrSndNearestReset(void)
{
    g_BrSndAA3470 = -1;

    g_BrSndNearest.pos.x = 0.0f;
    g_BrSndNearest.pos.y = 0.0f;
    g_BrSndNearest.pos.z = 0.0f;
    g_BrSndNearest.posPrev.x = 0.0f;
    g_BrSndNearest.posPrev.y = 0.0f;
    g_BrSndNearest.posPrev.z = 0.0f;
    g_BrSndNearest.pObj     = NULL;
    g_BrSndNearest.pObjPrev = NULL;
    g_BrSndNearest.objPosPrev.x = 0.0f;
    g_BrSndNearest.objPosPrev.y = 0.0f;
    g_BrSndNearest.objPosPrev.z = 0.0f;

    g_BrSndNearest.f84 = -1;
    g_BrSndNearest.f88 = -1;
    g_BrSndNearest.f8C = -1;
    g_BrSndNearest.f90 = -1;

    g_BrSndNearest.metric = BR_SND_NEAREST_FAR;
    g_BrSndNearest.f9C    = 0;
    g_BrSndNearest.fA0    = 0;
    /* f98 is NOT cleared by the original. */
}

/* 0x10067E50 */
/* WHAT IT DOES: puts one sound source forward as a candidate for the single
 * slot the game reserves for the nearest sound, and it takes that slot only
 * if it is closer to the listener than anything offered so far this frame. */
/* @implements 0x10067E50 d3d BrSndNearestOffer */
void BrSndNearestOffer(int32_t f8C, int32_t f84, int32_t f9C, float f98,
                       const BrVec3 *pPos, const BrMat4 *pListener)
{
    float d = BrVec3Dist(pPos,
                         (const BrVec3 *)(const void *)&pListener->m[3][0]);

    /* `test ah,1` on the fcom against the running best: strictly nearer
     * only, unordered included in the reject path via !(d < metric). */
    if (!(d < g_BrSndNearest.metric))
        return;

    g_BrSndNearest.pos    = *pPos;
    g_BrSndNearest.metric = d;
    g_BrSndNearest.f84    = f84;
    g_BrSndNearest.pObj   = pListener;
    g_BrSndNearest.f8C    = f8C;
    g_BrSndNearest.f98    = f98;
    g_BrSndNearest.f9C    = f9C;
}

/* 0x10068210 */
/* WHAT IT DOES: offers a sound for a thing that has no sound of its own,
 * picking a stock one -- but only in two particular game modes; in every
 * other mode it offers nothing and returns having done nothing at all. */
/* @implements 0x10068210 d3d BrSndNearestOfferDefault */
void BrSndNearestOfferDefault(int32_t f8C, const BrVec3 *pPos,
                              const BrMat4 *pListener)
{
    /* Mode is read first so it occupies ecx; the volume scale then lands in edx. */
    int32_t mode = g_Br0B380C;
    int32_t f84 = -1;
    int32_t f9C = 0x80;

    if (mode == 4 || mode == 10) {
        f84 = 0x0F;
        f9C = 0x180;
    }

    /* The -1 default is a "do nothing" marker, so the 0x80 volume scale the
     * function starts with is dead in every path. */
    if (f84 != -1)
        BrSndNearestOffer(f8C, f84, f9C, BR_SND_DEFAULT_HZ, pPos, pListener);
}

/* =====================================================================
 * 5.  Per-frame slot banks
 * ===================================================================== */

BrFrameBank g_BrPool16 = { NULL, 16, 20, 21, 0, 0 };    /* 0x100694E0 */
BrFrameBank g_BrPool32 = { NULL, 32, 20, 21, 0, 0 };    /* 0x10069530 */

BrPool *g_pBrPool64 = NULL;

void *BrFrameBankAlloc(BrFrameBank *pBank)
{
    int32_t  slot;
    uint8_t *p;

    /* Signed compare, as in the original (`jge`). */
    if (pBank->count < pBank->nUsable)
        slot = pBank->nBank * pBank->frame + pBank->count;
    else
        slot = pBank->nBank * pBank->frame + pBank->nUsable;

    p = pBank->pBase + (ptrdiff_t)slot * pBank->cbSlot;

    /* Incremented on BOTH paths, so the counter runs past the limit and
     * every late request in the frame aliases the same overflow slot. */
    pBank->count++;
    return p;
}

/* 0x100694E0 */
/* WHAT IT DOES: hands out one small scratch block that only has to last the
 * rest of the frame, from a pool that is thrown away wholesale at the end of
 * it. Once the pool is full every further request gets the same last block
 * back, so late callers quietly share one. */
/* @implements 0x100694E0 d3d BrPool16Alloc */
void *BrPool16Alloc(void)
{
    return BrFrameBankAlloc(&g_BrPool16);
}

/* Glide 0x100625A0 == D3D 0x10069530 (shared.csv pair).  Tagged on the Glide
 * side -- the reference build, and the address 0x1000A110's specular pass
 * calls -- so claimcheck audits the body against it. */
/* @implements 0x100625A0 glide BrPool32Alloc */
void *BrPool32Alloc(void)
{
    return BrFrameBankAlloc(&g_BrPool32);
}

/* 0x10069580.  Order preserved: 64-byte counter, then 16, then 32. */
/* WHAT IT DOES: throws away everything handed out of the three frame-scratch
 * pools, which is how they are emptied -- nothing is freed individually, the
 * counts simply go back to zero and the space is reused. */
/* @implements 0x10069580 d3d BrGfx69580 */
void BrGfx69580(void)
{
    /* DEVIATION: the original writes 0x10B01C40 directly.  That counter is
     * br_pool.h's BrPool::count, and br_pool.h exposes no global instance,
     * so it is reached through a integration-supplied pointer.  A NULL hook
     * simply skips it. */
    if (g_pBrPool64 != NULL)
        g_pBrPool64->count = 0;

    g_BrPool16.count = 0;
    g_BrPool32.count = 0;
    /* The frame index is untouched -- something else advances 0x106C65EC. */
}
