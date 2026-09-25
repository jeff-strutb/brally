/* br_animupdate.c -- scene: keyframe vertex animation.
 *
 * BrAnimUpdate advances every track of an animation set by one frame and
 * rebuilds each track's vertex block by blending positions and normals between
 * the two bracketing key poses (loop, ping-pong or stop at the ends).
 *
 * Filed out of the address batch slice2_19.c; the preamble is slice2_19.c's,
 * carried whole. The x87 note below is that file's.
 */
/*
 * x87 note: every fcomp/fnstsw pair in this range was decoded through the
 * flag mapping C0 = ah bit 0, C2 = ah bit 2, C3 = ah bit 6, so
 *      test ah,0x01 / jne  -> ST0 <  mem, or unordered
 *      test ah,0x41 / jne  -> ST0 <= mem, or unordered
 * and the C below uses the negated-comparison forms that reproduce the
 * unordered case as well, not just the ordered one.
 */
#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl (this, r, g, b).  Original is thiscall with
 * ret 0xC; hide that prototype so the definition can take the struct-arg
 * __fastcall shape that reproduces it. */
#define BrRgbSinkSet BrRgbSinkSet_hdr
#endif
#ifdef BR_MATCHING_BUILD
/* slice2_19.h / br_seg.h declare these cdecl with a leading state pointer the
 * originals do not have.  Hide those prototypes so BrModelLoad can call them
 * with the shapes the bytes show. */
#define BrSub100088B0 BrSub100088B0_cdecl
#define BrSegSetBases BrSegSetBases_cdecl
#endif
#include "slice2_19.h"
#ifdef BR_MATCHING_BUILD
#undef BrSub100088B0
#undef BrSegSetBases
typedef struct { void *p; } BrModelLoadArg;
extern int g_brModelMgr;                        /* 0x10AC0810 */
void * __fastcall BrSub100088B0(void *pThis, BrModelLoadArg a,
                                BrModelLoadArg b);
void BrSegSetBases(uint32_t n64Base, uint32_t hostBase);
#endif
#ifdef BR_MATCHING_BUILD
#undef BrRgbSinkSet
#endif

#include <string.h>

/* 0x10035585 */
/* 0x1007C8A0 __ftol -- truncate toward zero, low dword before any clamp.
 *
 * DEVIATION (port only): C's (int) cast is undefined for values outside int
 * range and for NaN, and BrAnimUpdate's three documented divide-by-zero paths
 * do produce those. The original's x87 FISTP stores the integer indefinite
 * 0x80000000 there, so the port does the same explicitly. In every one of
 * those paths the two brackets are the SAME keyframe, so the resulting
 * garbage frac is multiplied by a zero delta and never reaches the output.
 * The matching build spells the cast itself: the original is a plain
 * `call __ftol`, and at /Od a static helper would be a real call. */
#ifdef BR_MATCHING_BUILD
#define BrFtol(f) ((int)(f))
#else
static int BrFtol(float f)
{
    if (!(f > -2147483649.0f && f < 2147483648.0f))
        return (int)0x80000000L;
    return (int)f;
}
#endif

/* lo + (((hi - lo) * frac) >> 12), truncated back to the source width. The
 * truncation is a `movsx ax` / `movsx al` in the original and does wrap.
 * Macros, not helpers: the original open-codes all six per vertex, and at
 * /Od a static helper is a real call (docs/VC5-IDIOMS.md, /Od source facts). */
#define BrAnimLerp16(lo, hi, frac) \
    ((int)(int16_t)(((((hi) - (lo)) * (frac)) >> 12) + (lo)))
#define BrAnimLerp8(lo, hi, frac) \
    ((int)(int8_t)(((((hi) - (lo)) * (frac)) >> 12) + (lo)))

/* 0x1003563A */
/* WHAT IT DOES: advances every animation in a set by one frame's worth of
 * time and works out the shape of the model in between its stored key poses,
 * blending each corner point and its surface direction between the pose
 * before and the pose after. Animations that have run off the end either stop,
 * jump back to the start or turn round and play backwards, according to how
 * they were set up. Several of the stopping cases end up interpolating
 * between a pose and itself, which divides by zero -- harmless because the
 * result is then multiplied by no difference at all, and preserved. */
/* MATCHING STATE (2026-09-04): /Od /Op, 1357 B; every instruction and every
 * stack slot now agrees with the original -- 7 bytes remain, all one cause.
 * (Sweep: 763 diffs on the O2y variant -> 183 on Odp, the residue being a
 * 3-byte shift at +0x48A and everything after it moved.)
 *
 * WHAT WAS WRONG: written in the /O2 idiom.  Fixed: (1) the shared
 * search/interp/vertex block lives INSIDE the reverse arm's `if (t < tHi)`,
 * where the original emits it, and the forward arm reaches it by backward
 * gotos; (2) `weight` is a plain `(int)` cast (a real `call __ftol`), the
 * lerps are open-coded macros -- at /Od a static helper is a CALL; (3) 16
 * named locals, not 18: `t` is reused for the 0..1 blend (no `u`), one
 * counter is both the key index and the vertex index, one count is both
 * cKeys and cVerts, and pOut is assigned TWICE (once before the pointer
 * set-up, once after -- the bytes say so); (4) the flag updates are compound
 * assignments (`&= 0xFFFB` is a WORD `and`, `|= 4` a byte `or`); (5) slot
 * homing by name, see the declaration block.
 *
 * SOLVED (2026-09-21) -- BYTE-EXACT.  The forward `goto wrap_plain` is routed
 * through an explicit trailer trampoline (`goto wrap_tramp` at the site, then
 * `wrap_tramp: goto wrap_plain;` parked at the function tail, reached only by
 * that goto).  MSVC 5.0 /Od does NOT thread jump-to-jump, so it emits
 * `jmp near wrap_tramp` + `wrap_tramp: jmp short wrap_plain`, reproducing the
 * original 0x1002F170 -> 0x1002F232 -> 0x1002F1BE exactly; the preceding
 * `return;` reconstructs the shared return thunk at 0x1002F230.  Result: 0
 * reloc-masked byte diffs, recomp 1357 / orig 1357 -- a MATCH.  The earlier T3
 * "branch-layout wall" verdict is superseded; the lever it missed was writing
 * the trampoline in source rather than fighting the /Od branch shortener. */
/* @implements 0x1003563A d3d BrAnimUpdate */
void BrAnimUpdate(BrAnimSet *pSet)
{
    /* ALL SIXTEEN locals at function scope, and in THIS order.  /Od homes a
     * local by a hash of its NAME (docs/VC5-IDIOMS.md): the slot order is
     * hash-bucket ascending, and inside one bucket the LATER declaration
     * takes the EARLIER slot.  Names were chosen against a measured bucket
     * table so the frame reads exactly -4 pDst8, -8 ii, -0xC t, -0x10 pLow,
     * -0x14 pHigh, -0x18 pTrk, -0x1C pVa, -0x20 pVb, -0x24 nk, -0x28 weight,
     * -0x2C nCount, -0x30 pOut, -0x34 k, -0x38 pSrc8, -0x3C bound, -0x40 dur;
     * the nine slots below that are the compiler's own int->float temps.
     * Block-scoping any of them, or reordering two that share a bucket
     * (ii/pDst8, pHigh/pLow/t, pVa/pTrk, weight/nk, pOut/nCount,
     * bound/pSrc8), moves displacements all over the function. */
    int32_t          ii;        /* track index                              */
    const int8_t    *pDst8;     /* hi key's normals (int8 x3 per vertex)    */
    const BrAnimKey *pHigh;     /* key after t                              */
    const BrAnimKey *pLow;      /* key before t                             */
    float            t;         /* track time, then the 0..1 blend          */
    const int16_t   *pVa;       /* lo key's positions (int16 x3 per vertex) */
    BrAnimTrack     *pTrk;
    const int16_t   *pVb;       /* hi key's positions                       */
    int              weight;    /* blend as 12-bit fixed point              */
    int32_t          nk;        /* key count for the search, then the vertex count */
    BrAnimVtx       *pOut;
    int32_t          nCount;    /* tracks in the set                        */
    int32_t          k;         /* key index, then vertex index             */
    float            bound;     /* wrap threshold                           */
    const int8_t    *pSrc8;     /* lo key's normals                         */
    float            dur;       /* wrap period                              */

    /* Wrapped, not an early return: the original's guard is a single near
     * `je` to the epilogue (0x1002ECF8 -> 0x1002F230), where `return` emits a
     * short branch over a jump. Same lever as BrCarGfxSetColour.
     *
     * NO pList local: the original re-derefs pSet->pList at every use, which
     * is what /Od does with a member expression. Caching it costs a slot and
     * shifts every displacement. */
    if (pSet->pList != NULL) {

    nCount = pSet->pList->n;

    for (ii = 0; ii < nCount; ii++) {
        pTrk = pSet->pList->a[ii];

        if ((pTrk->flags & 4u) != 0) {
            /* ---- playing in reverse (0x1002ED4B) ---- */
            pTrk->t -= g_BrAnimDt;
            t = pTrk->t;

            if (t >= pTrk->tLo) {
                if (t < pTrk->tHi) {
                search:
                    /* GOTCHA: k is not re-tested against cKeys before the
                     * load, so a track whose last key time is <= t reads
                     * aKeys[cKeys]. */
                    nk = pTrk->cKeys;
                    for (k = pTrk->iKey; k < nk; k++) {
                        if (pTrk->aKeys[k]->t > t)
                            break;
                    }
                    pHigh = pTrk->aKeys[k];
                    k--;
                    pLow = pTrk->aKeys[k];

                    t = (t - pLow->t) / (pHigh->t - pLow->t);
                interp:
                    weight = BrFtol(t * g_BrK08F52C);     /* 0x1007C8A0 */

                    nk  = (int32_t)pTrk->cVerts;
                    pOut = pTrk->pOut;
                    pVa = (const int16_t *)((const char *)pLow + 4);
                    pVb = (const int16_t *)((const char *)pHigh + 4);
                    pSrc8  = (const int8_t  *)(pVa + nk * 3);
                    pDst8  = (const int8_t  *)(pVb + nk * 3);
                    pOut = pTrk->pOut;

                    for (k = 0; k < nk; k++) {
                        pOut[k].x = (float)BrAnimLerp16(pVa[0], pVb[0], weight);
                        pOut[k].y = (float)BrAnimLerp16(pVa[1], pVb[1], weight);
                        pOut[k].z = (float)BrAnimLerp16(pVa[2], pVb[2], weight);

                        pOut[k].nx = (float)BrAnimLerp8(pSrc8[0], pDst8[0], weight) * g_BrK08F530;
                        pOut[k].ny = (float)BrAnimLerp8(pSrc8[1], pDst8[1], weight) * g_BrK08F530;
                        pOut[k].nz = (float)BrAnimLerp8(pSrc8[2], pDst8[2], weight) * g_BrK08F530;

                        pVa += 3;
                        pVb += 3;
                        pSrc8  += 3;
                        pDst8  += 3;
                    }
                }
            } else {
                /* 0x1002F00F -- reflect off the low end, or stop */
                if ((pTrk->flags & 1u) != 0) {
                    t = g_BrK08F514 * pTrk->tLo - t;
                    pTrk->t = t;
                    pTrk->flags &= 0xFFFBu;
                    pTrk->iKey  = 0;
                    goto search;
                }
            }
        } else {
            /* ---- playing forward (0x1002F05F) ---- */
            pTrk->t += g_BrAnimDt;
            t = pTrk->t;

            if (t < pTrk->tLo) {
                /* GOTCHA: both brackets become aKeys[0], so the interpolation
                 * above divides by zero. Original behaviour. */
                pHigh = pTrk->aKeys[0];
                pLow = pTrk->aKeys[0];
                t = 0.0f;
                goto interp;
            }

            if (t < pTrk->tHi) {
                if (t >= pTrk->tLo)
                    goto search;
            } else {
                /* 0x1002F0D2 -- past the end */
                if ((pTrk->flags & 1u) != 0) {
                    dur = pTrk->tHi - pTrk->tLo;
                    bound  = pTrk->tHi + dur;

                    if ((pTrk->flags & 2u) != 0) {
                        dur = (pTrk->tHi - pTrk->tLo) * g_BrK08F514;
                        bound  = pTrk->tHi + dur;
                        while (t > bound)
                            t -= dur;
                        dur = dur * g_BrK08F534;
                        bound  = bound - dur;
                        /* GOTCHA: this falls into the PLAIN wrap loop, which
                         * then also runs the plain tail -- the reverse bit is
                         * never set. */
                        if (t > bound)
                            goto wrap_tramp;
                        t = g_BrK08F514 * pTrk->tHi - t;
                        pTrk->t = t;
                        pTrk->flags |= 4u;
                    } else {
                        dur = pTrk->tHi - pTrk->tLo;
                        bound  = pTrk->tHi + dur;
                    wrap_plain:
                        while (t > bound)
                            t -= dur;
                        t = t - (pTrk->tHi - pTrk->tLo);
                        pTrk->t = t;
                    }
                    pTrk->iKey = 0;
                    goto search;
                } else {
                    /* GOTCHA: same degenerate bracket as above. The original
                     * indexes +0x1C + cKeys*4, ii.e. the LAST key; with
                     * cKeys == 0 it would read the `t` field as a pointer. */
                    pHigh = pTrk->aKeys[pTrk->cKeys - 1];
                    pLow = pTrk->aKeys[pTrk->cKeys - 1];
                    t = 0.0f;
                    goto interp;
                }
            }
        }
    }
    }
    /* The forward exit to wrap_plain is spelled as a near jump to a trailer
     * trampoline: MSVC 5.0 /Od does NOT thread jump-to-jump, so routing the
     * goto through a label parked here (reached only by that goto) makes the
     * compiler emit `jmp near wrap_tramp` + `wrap_tramp: jmp short wrap_plain`,
     * exactly as the original (0x1002F170 -> 0x1002F232 -> 0x1002F1BE, 78 B
     * away yet near, not shortened).  The `return;` reconstructs the shared
     * return thunk (0x1002F230, `jmp` over the trampoline into the epilogue). */
    return;
wrap_tramp:
    goto wrap_plain;
}

/* BrAnimUpdate lever note (2026-09-21) -- how the `goto wrap_plain` wall fell.
 * The sole residue was one construct: the original routes that forward goto
 * through a near `e9` to a 2-byte trailer trampoline (`jmp short wrap_plain`)
 * that the loop exit jumps over; naive source emits a direct short `eb`, 3
 * bytes shorter, cascading displacements through the tail.  A large probe
 * census -- flags /J /Gf /Zp1 /Gd /Ob1 /Oi /Gs /G3 /G4 /Gy /Gz /Za, and source
 * shapes (shared-tail, two-level do/while, flags-first, for(;;)+break, done-skip
 * label, bool flag, nested block, dup-both-arms, continue) -- all failed,
 * because they tried to stop the compiler shortening a direct jump.  The lever
 * that works is the opposite: MSVC 5.0 /Od does not thread jump-to-jump, so
 * WRITE the trampoline in source (`goto wrap_tramp; wrap_tramp: goto wrap_plain;`
 * parked at the function tail).  See the head-of-function note; byte-exact,
 * 1357/1357.  Reusable for any function whose only residue is this pattern. */
