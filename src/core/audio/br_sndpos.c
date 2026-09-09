/* br_sndpos.c -- audio.
 *
 * Positional audio: the Doppler shift and stereo-pan/volume maths, and the
 * nearest-sound-source tracker they feed.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 *
 * Every x87 sequence in here was traced through its fxch chain; where the
 * original reads a status word twice and looks at different bits each time,
 * the C is written to reproduce that exactly (including what happens to a
 * NaN), not to look tidy.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
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
    /* orig: `fst [len]; fcomp 0.0f; fnstsw ax; test ah,0x40; jne skip`.
     * VC5's `!= 0.0f` is that single C3 test (NaN also sets C3, so a NaN
     * length skips too). A two-sided `> || <` emits a second fcomp. */
    if (len != 0.0f)
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
/* @implements 0x10060C30 glide BrSndPan */
void BrSndPan(const BrVec3 *pSrcPos, const BrMat4 *pListener,
              float *pGainA, float *pGainB, int32_t *pVol, int32_t fNarrow)
{
    BrVec3 d;
    float  proj, p, q, dist;

    /* Row 3 of the listener's matrix is its position, row 1 is the axis the
     * pan is measured along. */
    BrVec3Sub(&d, pSrcPos, (const BrVec3 *)(const void *)&pListener->m[3][0]);

    /* The original's first fmul is m[1][1]*d.y (`fld m10; fld m11; fmul dy`),
     * and NAMING THAT ONE PRODUCT is the only thing that produces it.  VC5
     * canonicalises a flat three-term sum of products, so every permutation
     * and every grouping of the three terms compiles to the same bytes -- with
     * the x term's fmul first.  Pulling the y term out into a temp takes it
     * out of the flat sum, so it is evaluated on its own and lands first; the
     * remaining two terms canonicalise as before.  See the sum-of-products
     * entries in docs/VC5-IDIOMS.md. */
    {
        float ty = pListener->m[1][1] * d.y;

        proj = ty + pListener->m[1][0] * d.x + pListener->m[1][2] * d.z;
    }

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
/* @n64 0x8022B404 located */
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
/* @n64 0x8022B428 located */
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
/* @n64 0x8022BA10 located */
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

/* 0x100611F0 (D3D 0x10068180) -- the track-keyed sibling of the one above. */
extern int32_t g_brCfgChosenTrack;    /* 0x100B3014, br_appstart.h */

/* 0x46ABE000, the doubled base frequency the two loud tracks use. */
#define BR_SND_DOUBLE_HZ   22000.0f

/* WHAT IT DOES: offers a stock ambient sound keyed on which track is being
 * driven: two tracks get it pitched an octave up, two get it at the base
 * pitch, and every other track gets nothing at all.  Unlike the mode-keyed
 * sibling it uses set index 0 and the full 0xFF volume scale. */
/* @implements 0x100611F0 glide BrSndNearestOfferTrack */
void BrSndNearestOfferTrack(int32_t f8C, const BrVec3 *pPos,
                            const BrMat4 *pListener)
{
    int32_t f84 = -1;
    float   f98 = BR_SND_DEFAULT_HZ;
    int32_t f9C = 0x80;

    switch (g_brCfgChosenTrack) {
    case 2:
    case 8:
        f84 = 0;
        f98 = BR_SND_DOUBLE_HZ;
        f9C = 0xFF;
        break;
    case 4:
    case 10:
        f84 = 0;
        f98 = BR_SND_DEFAULT_HZ;
        f9C = 0xFF;
        break;
    }

    if (f84 != -1)
        BrSndNearestOffer(f8C, f84, f9C, f98, pPos, pListener);
}

/* =====================================================================
 * 5.  The per-frame commit: 0x10060F40 (D3D 0x10067ED0, shared body)
 * ===================================================================== */

#ifdef BR_MATCHING_BUILD
#define BR_K_00779E4   9.09090886125341e-05f   /* 1/11000, the Hz->ratio scale */
#define BR_K_00779E8   4294967296.0            /* 2^32, double: the 32.32 shift */

extern int32_t   DAT_100b2f04;                 /* g_brRaceNCar               */
extern uint8_t  *DAT_10af2108;                 /* the race's owner object    */
extern int32_t   DAT_10af2180;                 /* car[0]'s "gone" word, stride 0x2B68 */
extern int32_t   DAT_106e86c8;                 /* view 0's car index         */
extern int32_t   DAT_106e8720;                 /* view 1's car index         */
extern int32_t   DAT_100aa044;                 /* g_brCViews                 */
extern int64_t   DAT_118eef90;                 /* the nearest voice's 32.32 ratio */
extern int32_t   DAT_118eef98;
extern uint32_t  DAT_118eef9c;                 /* packed L/R gains, 16.16    */
/* The set table, 24-byte rows: the original addresses each field from its
 * OWN symbol (`[eax+0x100B32C0]` etc.), so three externs, not one base. */
extern int32_t   DAT_100b32b0[];
extern int32_t   DAT_100b32bc[];
extern int32_t   DAT_100b32c0[];
extern int BrSfxSrcPlaySilent(int, int, int, int);   /* 0x1006E560 */

/* WHAT IT DOES: once a frame, act on whoever won the nearest-sound contest.
 * Nothing happens with no cars, with the race owner flagged, or while a
 * previous voice is still ramping.  A withdrawn winner clears the voice; a
 * winner that changed only fades the gains by half; the same winner as last
 * frame is re-voiced: Doppler against the listener (the sign of the base
 * frequency picks which pair of positions is compared), pan and volume from
 * the listener matrix, the set's silent-start the first time round, and --
 * unless the viewed cars are gone -- the 32.32 pitch ratio and the packed
 * stereo gains, halved on the first frame.  The commit then rolls the
 * candidate into the committed slots. */
/* PARKED T2 2026-09-09 at 687/683 B, 174/174 insns, register-blind 0+0.
 * The whole residue is ONE colouring choice: the original keeps its zero
 * in esi and the low gain's truncation in edi, and adds the shifted high
 * gain INTO its own register (`add eax,edi`, then `mov ecx,[0x118EEF9C]`
 * is loaded before the shift); ours swaps the two callee-saved registers
 * and adds into the low gain's (`add esi,eax`).  The extra 4 bytes are the
 * lost `mov eax,[abs]` short encodings.  Three levers DID land here and
 * are the reusable part: the pan call is written INSIDE each Doppler arm
 * with the flag as a literal; the set table is three per-field externs
 * (DAT_100b32b0/bc/c0) indexed by f84*6, not one base symbol; and the
 * listener row copy is three scalar assignments.
 * Dead probes (fn.py, all inert): shifted term left/right of the add;
 * named lo/hi temps in either order; `* 0x10000` instead of `<< 16` in
 * either order; the halving as a ternary; the halving as two stores. */
/* @implements 0x10060F40 glide BrSndNearestCommit */
void BrSndNearestCommit(void)
{
    int32_t vol;
    float   ratio;
    float   gainB;
    float   gainA;
    int32_t packed;

    if (DAT_100b2f04 == 0)
        return;
    if (DAT_10af2108[0x68] & 1)
        return;

    if (g_BrSndAA3470 != -1) {
        if (DAT_118eef90 != 0)
            return;
        g_BrSndAA3470     = -1;
        g_BrSndNearest.f90 = -1;
        g_BrSndNearest.fA0 = 0;
    }

    if (g_BrSndNearest.f8C == -1) {
        DAT_118eef9c = 0;
        DAT_118eef98 = 0;
        DAT_118eef90 = 0;
    } else if (g_BrSndNearest.f8C == g_BrSndNearest.f90 &&
               g_BrSndNearest.pObj == g_BrSndNearest.pObjPrev) {
        /* `fcomp 0.0f` + `test ah,1`: C0, so a NaN base frequency takes the
         * negative arm as well. */
        if (!(g_BrSndNearest.f98 >= 0.0f)) {
            ratio = -(BrSndDoppler(&g_BrSndNearest.pos, &g_BrSndNearest.pos,
                                   (const BrVec3 *)(const void *)&g_BrSndNearest.pObj->m[3][0],
                                   &g_BrSndNearest.objPosPrev)
                      * g_BrSndNearest.f98);
            BrSndPan(&g_BrSndNearest.pos, g_BrSndNearest.pObj,
                     &gainA, &gainB, &vol, 1);
        } else {
            ratio = BrSndDoppler(&g_BrSndNearest.pos, &g_BrSndNearest.posPrev,
                                 (const BrVec3 *)(const void *)&g_BrSndNearest.pObj->m[3][0],
                                 &g_BrSndNearest.objPosPrev)
                    * g_BrSndNearest.f98;
            BrSndPan(&g_BrSndNearest.pos, g_BrSndNearest.pObj,
                     &gainA, &gainB, &vol, 0);
        }
        vol = (vol * g_BrSndNearest.f9C) >> 8;

        if (g_BrSndNearest.fA0 == 0) {
            g_BrSndNearest.fA0 = 1;
            BrSfxSrcPlaySilent(3, DAT_100b32b0[g_BrSndNearest.f84 * 6],
                               DAT_100b32bc[g_BrSndNearest.f84 * 6],
                               DAT_100b32c0[g_BrSndNearest.f84 * 6]);
        }

        if (*(int32_t *)((char *)&DAT_10af2180 + DAT_106e86c8 * 0x2B68) == 0 &&
            (DAT_100aa044 == 1 ||
             *(int32_t *)((char *)&DAT_10af2180 + DAT_106e8720 * 0x2B68) == 0)) {
            DAT_118eef90 = (int64_t)(ratio * BR_K_00779E4 * BR_K_00779E8);
            packed = ((int32_t)(gainA * (float)vol) << 16)
                   + (int32_t)(gainB * (float)vol);
            if (DAT_118eef9c == 0)
                packed = (packed >> 1) & 0x7FFF7FFF;
            DAT_118eef9c = packed;
        }

        /* pos -> posPrev is a struct copy (three loads, then three stores);
         * the listener row is copied SCALAR BY SCALAR (load/store pairs off
         * `[ecx+0x30]`, `+0x34`, `+0x38` -- a struct copy through a pointer
         * costs an extra `lea`). */
        g_BrSndNearest.posPrev      = g_BrSndNearest.pos;
        g_BrSndNearest.objPosPrev.x = g_BrSndNearest.pObj->m[3][0];
        g_BrSndNearest.objPosPrev.y = g_BrSndNearest.pObj->m[3][1];
        g_BrSndNearest.objPosPrev.z = g_BrSndNearest.pObj->m[3][2];
    } else {
        g_BrSndNearest.fA0 = 0;
        DAT_118eef9c = (DAT_118eef9c >> 1) & 0x7FFF7FFFu;
    }

    g_BrSndNearest.f88      = g_BrSndNearest.f84;
    g_BrSndNearest.pObjPrev = g_BrSndNearest.pObj;
    g_BrSndNearest.f90      = g_BrSndNearest.f8C;
}
#endif /* BR_MATCHING_BUILD */
