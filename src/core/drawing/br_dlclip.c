/* br_dlclip.c -- drawing: the seven homogeneous clip planes.
 *
 * WHY THIS FILE EXISTS.  slice1_03.c carries the PORT of this cluster, where
 * the seven planes are one shared routine taking a distance function pointer.
 * The original is not written that way: it is SEVEN separate functions, six
 * of them 311 bytes and byte-for-byte identical apart from the two x87 pairs
 * that compute the plane distance, plus a 303-byte seventh (the plain-w
 * plane, which needs no second operand).  A function pointer would emit a
 * `call [reg]` the original does not have, so the matching build spells the
 * body ONCE as a macro and instantiates it seven times -- the same
 * hand-inlining recipe docs/VC5-IDIOMS.md records under "Inlining a helper by
 * hand: use a MACRO".
 *
 * THE SEVEN, and what pins each one.  The whole family was proved to be one
 * body by diffing the extracted originals against each other: 0x1001F2B0 vs
 * its five 311-byte siblings differ in 6 or 8 bytes, and every one of those
 * bytes is either a field displacement in the `fld`/`fadd`/`fsub` pair or a
 * byte of the two `call rel32` displacements (which must differ, since the
 * call sites are at different addresses).  Nothing else in 311 bytes moves.
 *
 *     0x1001F0D0   d = w                303 B   the W plane
 *     0x1001F7B0   d = z + w            311 B   NEAR
 *     0x1001F2B0   d = w + x            311 B   LEFT
 *     0x1001F3F0   d = w - x            311 B   RIGHT
 *     0x1001F670   d = w - y            311 B   TOP
 *     0x1001F8F0   d = w - z            311 B   FAR
 *     0x1001F530   d = y + w            311 B   BOTTOM
 *
 * ‼ RESIDUE.  SIX of the seven are byte-exact.  Only LEFT (0x1001F2B0) is
 * still out, by 2 bytes, and both are the field displacement in ONE
 * `fld`/`fadd` pair -- the dPrev site:
 *
 *     LEFT   orig  fld [w] ; fadd [x]      ours  fld [x] ; fadd [w]
 *                                          (the dCur site now matches)
 *
 * ‼ THE DISTANCE EXPRESSION IS PER-SITE, NOT PER-PLANE.  This is what the
 * earlier pass got wrong, and it cost NEAR several sessions.  The macro body
 * evaluates the plane distance at TWO sites (dCur and dPrev), and the
 * original's two sites do NOT agree with each other: NEAR leads with f0C at
 * the dCur site and with f18 at the dPrev site.  With a single DIST
 * parameter that is unrepresentable -- one expansion yields one spelling --
 * so the old note concluded "the choice is made per site, by the scheduler"
 * and parked it.  That inference was WRONG: the constraint was in OUR macro,
 * not in the compiler.  BR_CLIP_PLANE now takes DIST_CUR and DIST_PREV, and
 * NEAR went byte-exact immediately.  Generalise the habit, not the fix: when
 * a hand-inlined macro body uses its parameter at more than one site, the
 * sites are independent evidence and must be independently spellable before
 * anything about them can be called unreachable.
 *
 * The lever that picks the leading operand is the redundant paren round the
 * FIRST operand (`((v)->f18) + (v)->f04`; docs/VC5-IDIOMS.md, "((a) + b) + c
 * picks the fld operand").  It has a COST: at the dPrev site it also sinks
 * that site's `fadd` past four unrelated instructions.  So it is usable at
 * the dCur site and not at the dPrev site -- which is exactly why NEAR (needs
 * the flip at dCur only) fell and LEFT (needs it at BOTH) did not.
 *
 * DEAD, do not re-run (all /O2 /Op, one-file sweep; diff-byte counts are
 * LEFT's):
 *   - swapping the operands of the `+` in either direction, on each of the
 *     three PLUS planes: byte-identical output.  The "VC5 canonicalises
 *     commutative float addition" entry holds for a plain two-term add of
 *     two struct fields.
 *   - paren at the dPrev site only: 33.  Paren at BOTH sites: 31 (the old
 *     note's "53" predates the macro split; the number moved, the verdict
 *     did not).  Paren at the dCur site only: 2 -- best, and current.
 *   - SWAPPING THE TWO DISTANCE STATEMENTS (dPrev computed before dCur) is
 *     INERT: still 2, and NEAR stays byte-exact.  Statement order does not
 *     pick the operand here; only the paren does.
 *   - naming the leading operand instead of parenthesising it
 *     (`(dLead = (v)->f18), dLead + (v)->f04`): 31, AND the extra float
 *     local perturbs the shared frame enough to un-match two other planes
 *     (6/7 -> 4/7).  The "name the product" lever needs a SUM OF PRODUCTS;
 *     on a plain two-term add it does nothing and here it is destructive.
 *
 * WHAT WOULD MOVE LEFT: something that pins the schedule at the dPrev site,
 * so the paren's flip can be taken there without the `fadd` sinking.  Do not
 * re-probe the paren or the operand order on their own -- both are mapped
 * above.
 *
 * Field meanings (established from the caller, 0x1001EE70; see slice1_03.h):
 *     f04 = x   f08 = y   f0C = z   f10 = s   f14 = t   f18 = w
 *
 * POLARITY.  Each routine does `fcomp` against the 0.0f at 0x10077410 and
 * branches on C0, which is also set for an unordered compare, so a NaN
 * distance counts as OUTSIDE.  Writing the INSIDE test as `d >= 0.0f` rather
 * than `d < 0.0f` is what reproduces that.
 *
 * DEVIATIONS THE PORT ADDED AND THIS FILE MUST NOT HAVE: the original never
 * null-checks pList, never null-checks the head before dereferencing it, and
 * stores through BrClipLerpVert's result unconditionally -- an exhausted node
 * pool is a null dereference in the original.  The pool bounds are the
 * literal addresses 0x105CCFF0 .. 0x105CD9F0 (0xA00 = 64 records of 0x28),
 * compared unsigned, and there is no "is the pool installed" test either.
 */
#include "slice1_03.h"

#ifdef BR_MATCHING_BUILD

/* 0x105CDA00 -- head of the spare-vertex free list, as the seven planes
 * address it.  BrClipLerpVert (0x1001F200), further down this file, reaches
 * the same list through its own static. */
extern BrClipVert *DAT_105cda00;

/* The node pool's bounds, as the original compares them: bare addresses in
 * `cmp reg, imm32`, not a symbol plus a length. */
#define BR_CLIP_POOL_LO  0x105CCFF0uL
#define BR_CLIP_POOL_HI  0x105CD9F0uL

/* ---------------------------------------------------------------------
 * The body, once.
 *
 * Register correspondence with the original, for anyone checking:
 *   ebx  pList          esi  pCur           ecx  pPrev
 *   edi  pOutPrev       ebp  pDead          [esp+0x14] i
 *   [esp+0x10] dCur     [esp+0x18] pNext    dPrev lands in the dead
 *                                           incoming-argument slot
 * ------------------------------------------------------------------ */
#define BR_CLIP_PLANE(NAME, DIST_CUR, DIST_PREV)                              \
void NAME(BrClipList *pList)                                                  \
{                                                                             \
    BrClipVert *pPrev;                                                        \
    BrClipVert *pOutPrev;                                                     \
    BrClipVert *pCur;                                                         \
    BrClipVert *pNext;                                                        \
    BrClipVert *pDead;                                                        \
    BrClipVert *pTmp;                                                         \
    BrClipVert *pNew;                                                         \
    float       dCur, dPrev, t;                                               \
    int         i;                                                            \
                                                                              \
    pPrev    = pList->pHead;                                                  \
    pCur     = pPrev->pNext;                                                  \
    pOutPrev = pPrev;                                                         \
    i        = pList->cVerts;                                                 \
    pDead    = NULL;                                                          \
                                                                              \
    if (i > 0) {                                                              \
        for (;;) {                                                            \
            dCur  = DIST_CUR(pCur);                                           \
            dPrev = DIST_PREV(pPrev);                                         \
            pNext = pCur->pNext;                                              \
                                                                              \
            if (dCur >= 0.0f) {                                               \
                if (dPrev >= 0.0f) {                                          \
                    pOutPrev = pCur;                                          \
                } else {                                                      \
                    t = dPrev / (dPrev - dCur);                               \
                    pNew = BrClipLerpVert(pPrev, pCur, t);                    \
                    pNew->pNext = pOutPrev->pNext;                            \
                    pOutPrev->pNext = pNew;                                   \
                    pOutPrev = pCur;                                          \
                    pList->cVerts = pList->cVerts + 1;                        \
                }                                                             \
            } else if (dPrev >= 0.0f) {                                       \
                pTmp = pOutPrev->pNext;                                       \
                if (pTmp != NULL)                                             \
                    pOutPrev->pNext = pTmp->pNext;                            \
                t = dCur / (dCur - dPrev);                                    \
                pCur->pNext = pDead;                                          \
                pDead = pCur;                                                 \
                pNew = BrClipLerpVert(pCur, pPrev, t);                        \
                pNew->pNext = pOutPrev->pNext;                                \
                pOutPrev->pNext = pNew;                                       \
                pOutPrev = pNew;                                              \
            } else {                                                          \
                pTmp = pOutPrev->pNext;                                       \
                if (pTmp != NULL)                                             \
                    pOutPrev->pNext = pTmp->pNext;                            \
                pCur->pNext = pDead;                                          \
                pDead = pCur;                                                 \
                pList->cVerts = pList->cVerts - 1;                            \
            }                                                                 \
                                                                              \
            pPrev = pCur;                                                     \
            pCur  = pNext;                                                    \
                                                                              \
            if (pList->cVerts < 2)                                            \
                break;                                                        \
            if (--i <= 0)                                                     \
                break;                                                        \
        }                                                                     \
    }                                                                         \
                                                                              \
    pList->pHead = pCur;                                                      \
                                                                              \
    pTmp = pDead;                                                             \
    if (pDead != NULL)                                                        \
        pDead = pDead->pNext;                                                 \
    while (pTmp != NULL) {                                                    \
        if ((unsigned long)pTmp >= BR_CLIP_POOL_LO &&                         \
            (unsigned long)pTmp <  BR_CLIP_POOL_HI) {                         \
            pTmp->pNext = DAT_105cda00;                                       \
            DAT_105cda00 = pTmp;                                              \
        }                                                                     \
        pNew = pDead;                                                         \
        if (pDead != NULL)                                                    \
            pDead = pDead->pNext;                                             \
        pTmp = pNew;                                                          \
    }                                                                         \
}

/* The distance expressions, spelled in the original's own operand order. */
#define BRCLIP_W(v)          ((v)->f18)
#define BRCLIP_W_PLUS_X(v)   ((v)->f18 + (v)->f04)
#define BRCLIP_W_MINUS_X(v)  ((v)->f18 - (v)->f04)
#define BRCLIP_Y_PLUS_W(v)   ((v)->f08 + (v)->f18)
#define BRCLIP_W_MINUS_Y(v)  ((v)->f18 - (v)->f08)
#define BRCLIP_Z_PLUS_W(v)   ((v)->f0C + (v)->f18)
#define BRCLIP_W_MINUS_Z(v)  ((v)->f18 - (v)->f0C)

/* Same value, but the redundant paren round the FIRST operand makes VC5 lead
 * the pair with it (see docs/VC5-IDIOMS.md, "((a) + b) + c picks the fld
 * operand").  Used at ONE site only, where the original's order differs. */
#define BRCLIP_Z_PLUS_W_LEAD(v)   (((v)->f0C) + (v)->f18)
#define BRCLIP_W_PLUS_X_LEAD(v)   (((v)->f18) + (v)->f04)

/* WHAT IT DOES: cuts a polygon against the plane where w reaches zero --
 * the one behind the camera's eye -- dropping the corners on the wrong side
 * and adding new ones exactly on the plane. */
/* @implements 0x1001F0D0 glide BrClipPlaneW */
BR_CLIP_PLANE(BrClipPlaneW, BRCLIP_W, BRCLIP_W)

/* WHAT IT DOES: cuts a polygon against the left edge of the screen. */
/* @implements 0x1001F2B0 glide BrClipPlaneWPlusF04 */
BR_CLIP_PLANE(BrClipPlaneWPlusF04, BRCLIP_W_PLUS_X_LEAD, BRCLIP_W_PLUS_X)

/* WHAT IT DOES: cuts a polygon against the right edge of the screen. */
/* @implements 0x1001F3F0 glide BrClipPlaneWMinusF04 */
BR_CLIP_PLANE(BrClipPlaneWMinusF04, BRCLIP_W_MINUS_X, BRCLIP_W_MINUS_X)

/* WHAT IT DOES: cuts a polygon against the bottom edge of the screen. */
/* @implements 0x1001F530 glide BrClipPlaneWPlusF08 */
BR_CLIP_PLANE(BrClipPlaneWPlusF08, BRCLIP_Y_PLUS_W, BRCLIP_Y_PLUS_W)

/* WHAT IT DOES: cuts a polygon against the top edge of the screen. */
/* @implements 0x1001F670 glide BrClipPlaneWMinusF08 */
BR_CLIP_PLANE(BrClipPlaneWMinusF08, BRCLIP_W_MINUS_Y, BRCLIP_W_MINUS_Y)

/* WHAT IT DOES: cuts a polygon against the near plane -- the closest
 * distance the camera will draw anything at. */
/* @implements 0x1001F7B0 glide BrClipPlaneWPlusF0C */
BR_CLIP_PLANE(BrClipPlaneWPlusF0C, BRCLIP_Z_PLUS_W_LEAD, BRCLIP_Z_PLUS_W)

/* WHAT IT DOES: cuts a polygon against the far plane -- the horizon beyond
 * which nothing is drawn. */
/* @implements 0x1001F8F0 glide BrClipPlaneWMinusF0C */
BR_CLIP_PLANE(BrClipPlaneWMinusF0C, BRCLIP_W_MINUS_Z, BRCLIP_W_MINUS_Z)

#endif /* BR_MATCHING_BUILD */

/* =====================================================================
 * The node pool and the vertex the planes splice in.  Moved here out of
 * slice1_03.c (an address batch, not a module) unchanged; the port's
 * shared-body clip routine, which slice1_03.h documents, comes with it
 * because it is the only other user of the pool statics.
 *
 * Original storage:
 *   0x104C01A8 .. 0x104C0BA8   node pool, 0xA00 bytes = 64 * 0x28
 *   0x104C0BBC                 free-list head
 *   0x1008F3C8                 the compared-against constant -- it is 0.0f,
 *                              read out of the DLL's .rdata.
 * ===================================================================== */

static BrClipVert *g_pClipFree;    /* 0x104C0BBC */
static BrClipVert *g_aClipPool;    /* 0x104C01A8 */
static int         g_cClipPool;    /* (0x104C0BA8 - 0x104C01A8) / 0x28 = 64 */

void BrClipPoolInit(BrClipVert *aNodes, int cNodes)
{
    int i;

    g_aClipPool = aNodes;
    g_cClipPool = (aNodes != NULL && cNodes > 0) ? cNodes : 0;
    g_pClipFree = NULL;

    /* Thread lowest-index-first so the free list pops in ascending order,
     * which is what a fresh static pool would do. */
    for (i = g_cClipPool - 1; i >= 0; i--) {
        aNodes[i].pNext = g_pClipFree;
        g_pClipFree = &aNodes[i];
    }
}

int BrClipPoolCount(void)
{
    const BrClipVert *p;
    int n = 0;

    for (p = g_pClipFree; p != NULL; p = p->pNext)
        n++;
    return n;
}

void BrClipPoolFree(BrClipVert *pNode)
{
    if (pNode == NULL || g_aClipPool == NULL)
        return;
    if (pNode < g_aClipPool || pNode >= g_aClipPool + g_cClipPool)
        return;                    /* not ours -- silently dropped */
    pNode->pNext = g_pClipFree;
    g_pClipFree = pNode;
}

/* 0x1001D940 */
/* WHAT IT DOES: makes a new vertex sitting part way between two existing
 * ones, blending every one of its nine properties -- position, texture
 * coordinates and colour. This is what produces the new corner where a
 * triangle is cut by the edge of the screen. It takes the vertex off a small
 * fixed pool; the original would crash if that pool ran dry, whereas this
 * reports failure and the callers skip the insertion. */
/* @implements 0x1001D940 d3d BrClipLerpVert */
BrClipVert *BrClipLerpVert(const BrClipVert *pA, const BrClipVert *pB,
                           float t)
{
    BrClipVert *pOut = g_pClipFree;

    if (pOut != NULL)
        g_pClipFree = pOut->pNext;

    /* DEVIATION (port only): the original does the pop guarded
     * (`test eax,eax / je`) but then stores through eax unconditionally, so
     * an exhausted free list is a NULL dereference. The port returns NULL
     * instead; every caller in this file checks and skips the insertion. */
#ifndef BR_MATCHING_BUILD
    if (pOut == NULL)
        return NULL;
#endif

    /* +0x00 is deliberately not written -- it still holds the free-list
     * link, and each caller overwrites it right after this returns. */
    pOut->f04 = (pB->f04 - pA->f04) * t + pA->f04;
    pOut->f08 = (pB->f08 - pA->f08) * t + pA->f08;
    pOut->f0C = (pB->f0C - pA->f0C) * t + pA->f0C;
    pOut->f10 = (pB->f10 - pA->f10) * t + pA->f10;
    pOut->f14 = (pB->f14 - pA->f14) * t + pA->f14;
    pOut->f18 = (pB->f18 - pA->f18) * t + pA->f18;
    pOut->f1C = (pB->f1C - pA->f1C) * t + pA->f1C;
    pOut->f20 = (pB->f20 - pA->f20) * t + pA->f20;
    pOut->f24 = (pB->f24 - pA->f24) * t + pA->f24;
    return pOut;
}

#ifndef BR_MATCHING_BUILD
/* The matching build gets the original's SEVEN separate 311-byte functions
 * from the macro above (one macro, seven instantiations); the
 * shared-body-plus-function-pointer form below is the port only, because the
 * indirect call it emits is not in the original. */
typedef float (*BrClipDistFn)(const BrClipVert *pV);

/* The body shared by 0x1001D810 / 0x1001D9F0 / 0x1001DB30 / 0x1001DC70.
 * Those four are byte-for-byte identical apart from the two `fld/fadd/fsub`
 * pairs that produce the plane distance, so they are one function here with
 * the distance passed in.
 *
 * Register correspondence, for anyone checking against the disassembly:
 *   ecx  pPrev     previous vertex of the SOURCE polygon
 *   edi  pOutPrev  previous node of the OUTPUT list (they diverge)
 *   esi  pCur
 *   ebp  pDead     chain of unlinked vertices, recycled at the end
 *   ebx  pList
 *   [esp+0x14] i,  [esp+0x18] pNext, [esp+0x10] dCur, [esp+0x20] dPrev
 */
static void BrClipPlane(BrClipList *pList, BrClipDistFn pfnDist)
{
    BrClipVert *pPrev;
    BrClipVert *pOutPrev;
    BrClipVert *pCur;
    BrClipVert *pNext;
    BrClipVert *pDead = NULL;
    BrClipVert *pTmp;
    BrClipVert *pNew;
    float dCur, dPrev, t;
    int i;

    /* DEVIATION: the original loads pList->pHead and dereferences it before
     * testing the count, so an empty list with a NULL head faults. */
    if (pList == NULL || pList->pHead == NULL)
        return;

    pPrev    = pList->pHead;
    i        = pList->cVerts;
    pCur     = pPrev->pNext;
    pOutPrev = pPrev;

    if (i > 0) {
        for (;;) {
            dCur  = pfnDist(pCur);
            dPrev = pfnDist(pPrev);
            pNext = pCur->pNext;    /* saved before anything is relinked */

            /* Written as `>= 0.0f` rather than `< 0.0f` on purpose: the
             * original branches on the x87 C0 bit, which is also set for an
             * unordered compare, so a NaN distance counts as OUTSIDE. */
            if (dCur >= 0.0f) {
                if (dPrev >= 0.0f) {
                    /* both inside -- nothing to do but advance */
                    pOutPrev = pCur;
                } else {
                    /* entering: splice an on-plane vertex in before pCur */
                    t = dPrev / (dPrev - dCur);
                    pNew = BrClipLerpVert(pPrev, pCur, t);
                    if (pNew != NULL) {
                        pNew->pNext = pOutPrev->pNext;
                        pOutPrev->pNext = pNew;
                        pList->cVerts = pList->cVerts + 1;
                    }
                    pOutPrev = pCur;
                }
            } else if (dPrev >= 0.0f) {
                /* leaving: drop pCur, splice an on-plane vertex in its
                 * place. The count is deliberately left alone -- one out,
                 * one in. */
                pTmp = pOutPrev->pNext;
                if (pTmp != NULL)
                    pOutPrev->pNext = pTmp->pNext;

                t = dCur / (dCur - dPrev);
                pCur->pNext = pDead;
                pDead = pCur;

                pNew = BrClipLerpVert(pCur, pPrev, t);
                if (pNew != NULL) {
                    pNew->pNext = pOutPrev->pNext;
                    pOutPrev->pNext = pNew;
                    pOutPrev = pNew;
                }
            } else {
                /* both outside: drop pCur */
                pTmp = pOutPrev->pNext;
                if (pTmp != NULL)
                    pOutPrev->pNext = pTmp->pNext;

                pCur->pNext = pDead;
                pDead = pCur;
                pList->cVerts = pList->cVerts - 1;
            }

            pPrev = pCur;
            pCur  = pNext;

            /* Two independent exits, in this order. The count test comes
             * first, so a polygon that collapses below 2 vertices stops
             * immediately and leaves the remaining vertices unvisited. */
            if (pList->cVerts < 2)
                break;
            if (--i <= 0)
                break;
        }
    }

    /* The head is set to pCur unconditionally -- which, after a full pass
     * over a circular list, is the node one position past the old head. So
     * every clip call ROTATES the polygon by one vertex. On the early exits
     * it can even be a vertex that was just discarded. */
    pList->pHead = pCur;

    /* Return the discarded chain to the free list, but only those nodes
     * that live inside the pool; anything else is dropped on the floor. */
    pTmp = pDead;
    if (pDead != NULL)
        pDead = pDead->pNext;

    while (pTmp != NULL) {
        if (g_aClipPool != NULL &&
            pTmp >= g_aClipPool && pTmp < g_aClipPool + g_cClipPool) {
            pTmp->pNext = g_pClipFree;
            g_pClipFree = pTmp;
        }
        pNew = pDead;
        if (pDead != NULL)
            pDead = pDead->pNext;
        pTmp = pNew;
    }
}

static float BrClipDistW(const BrClipVert *pV)          { return pV->f18; }
static float BrClipDistWPlusF04(const BrClipVert *pV)   { return pV->f18 + pV->f04; }
static float BrClipDistWMinusF04(const BrClipVert *pV)  { return pV->f18 - pV->f04; }
/* 0x1001DC70 loads f08 first and adds f18, unlike its three siblings which
 * lead with f18. Same value, but noted because it is the one asymmetry. */
static float BrClipDistWPlusF08(const BrClipVert *pV)   { return pV->f08 + pV->f18; }

/* The three slice1_04.h left out, read off BRGlide's copies (0x1001F670,
 * 0x1001F7B0, 0x1001F8F0) rather than BRD3D's: same 311-byte body, same two
 * x87 loads, and the operand order below is theirs -- 0x1001F7B0 leads with
 * f0C exactly as 0x1001DC70 leads with f08. */
static float BrClipDistWMinusF08(const BrClipVert *pV) { return pV->f18 - pV->f08; }
static float BrClipDistWPlusF0C(const BrClipVert *pV)  { return pV->f0C + pV->f18; }
static float BrClipDistWMinusF0C(const BrClipVert *pV) { return pV->f18 - pV->f0C; }

void BrClipPlaneW(BrClipList *pList)          { BrClipPlane(pList, BrClipDistW); }
void BrClipPlaneWPlusF04(BrClipList *pList)   { BrClipPlane(pList, BrClipDistWPlusF04); }
void BrClipPlaneWMinusF04(BrClipList *pList)  { BrClipPlane(pList, BrClipDistWMinusF04); }
void BrClipPlaneWPlusF08(BrClipList *pList)   { BrClipPlane(pList, BrClipDistWPlusF08); }
void BrClipPlaneWMinusF08(BrClipList *pList)  { BrClipPlane(pList, BrClipDistWMinusF08); }
void BrClipPlaneWPlusF0C(BrClipList *pList)   { BrClipPlane(pList, BrClipDistWPlusF0C); }
void BrClipPlaneWMinusF0C(BrClipList *pList)  { BrClipPlane(pList, BrClipDistWMinusF0C); }
#endif /* !BR_MATCHING_BUILD */
