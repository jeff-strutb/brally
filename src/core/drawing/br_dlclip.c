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
#ifdef BR_MATCHING_BUILD

#include "slice1_03.h"

/* 0x105CDA00 -- head of the spare-vertex free list, shared with
 * BrClipLerpVert (0x1001F200) in slice1_03.c. */
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
