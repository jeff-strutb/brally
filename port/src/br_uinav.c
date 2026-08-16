/* br_uinav.c -- the menu navigation chain over br_ui.h's struct model.
 *
 * See br_uinav.h for what this module is, which addresses it duplicates, and
 * why a second transcription exists at all. The short version: seven of these
 * eight bodies also exist in port/src/slice3_32.c over that module's
 * byte-image objects, and a byte-offset body cannot address a struct whose
 * fields moved under LP64.
 *
 * Where a body here and slice3_32.c's disagree, slice3_32.c is wrong or this
 * one is -- they were transcribed from the same listings and every GOTCHA
 * comment slice3_32.c carries is repeated here so a future diff of the two is
 * a diff of behaviour and not of documentation.
 */
#include "br_uinav.h"
#include <stddef.h>

BrUiNav *g_pBrUiNav;

/* ==========================================================================
 * 0x100484F0 -- clamp the selection cursor against one page
 * ========================================================================== */

int BrUiNavPageSelect_100484F0(BrUiNav *pNav, BrUiPage_ *pPage)
{
    BrScrGlobals *pG   = pNav->pG;
    uint32_t      nMod = (uint32_t)pPage->cSel;         /* zero-extended     */
    int32_t       nCur = (int32_t)(int16_t)pG->wAA286C; /* SIGN-extended     */
    uint16_t      res;

    if (nCur >= (int32_t)nMod) {
        res = 0;
        pG->wAA286C = res;
    } else if (nCur >= 0) {
        res = (uint16_t)nCur;
        /* GOTCHA: the global is deliberately NOT written on this arm. */
    } else {
        /* `lea eax,[edx-1]` on the FULL edx, of which only AX is stored --
         * the high half of edx is indeterminate at this point in the
         * original and cannot reach the result. */
        res = (uint16_t)(nMod - 1u);
        pG->wAA286C = res;
    }
    pPage->iSel = res;
    return 1;
}

/* ==========================================================================
 * 0x100480A0 -- control vtable +0x04, the step timer
 * ========================================================================== */

int32_t BrUiNavCtlTick_100480A0(BrUiCtl_ *pCtl)
{
    int32_t nNow, nDelta;

    if (pCtl->f2968 == 0)
        return 1;

    nNow   = BrSub10075020();
    nDelta = (int32_t)((uint32_t)nNow - (uint32_t)pCtl->f2970);
    pCtl->f2970 = nNow;
    pCtl->f2974 = (int32_t)((uint32_t)pCtl->f2974 + (uint32_t)nDelta);

    if (pCtl->f296C != 0) {
        int32_t i = pCtl->wStep;

        /* `jle` -- a step of length 0 can never elapse. */
        if (pCtl->f2974 <= pCtl->aStepMs[i])
            return 1;

        pCtl->f2974   = 0;
        pCtl->flags1C = (int32_t)((uint32_t)pCtl->flags1C | BR_SCR_BIT100);
        pCtl->list.f18 |= BR_SCR_BIT100;
        ++i;
        pCtl->wStep = (int16_t)i;
        i = pCtl->wStep;                 /* movsx of the store just made */
        if (pCtl->aStepMs[i] > 0)
            return 1;
        pCtl->wStep = 0;
        return 1;
    }

    /* `jle 0x3C` -- STRICTLY more than 60 ms. */
    if (pCtl->f2974 > 0x3C) {
        pCtl->f2974   = 0;
        pCtl->flags1C = (int32_t)((uint32_t)pCtl->flags1C | BR_SCR_BIT100);
        pCtl->list.f18 |= BR_SCR_BIT100;
    }
    return 1;
}

/* ==========================================================================
 * 0x10047A10 -- control vtable +0x10
 * ========================================================================== */

int32_t BrUiNavCtlStepCode_10047A10(BrUiCtl_ *pCtl)
{
    if (pCtl->f296C == 0) {
        pCtl->pVtbl->f1C(pCtl);
        return 1;
    }
    {
        int32_t        i = pCtl->wStep;
        uint16_t       w = pCtl->aStepId[i];
        unsigned char *pRec = (unsigned char *)pCtl->p1E210;

        pCtl->w1E20C = w;
        /* The stride-0x10 array at +0x1E210; the original adds i*16. */
        pCtl->pVtbl->f18(pCtl, pRec + (size_t)((uint32_t)i * 16u));
    }
    return 1;
}

/* ==========================================================================
 * 0x10048010 -- control vtable +0x08
 * ========================================================================== */

int32_t BrUiNavCtlEnter_10048010(BrUiNav *pNav, BrUiCtl_ *pCtl)
{
    uint32_t f;

    (void)pNav;
    if (((uint32_t)pCtl->flags28 & 1u) == 0)
        return 1;

    f = (uint32_t)pCtl->flags1C;
    if (f & BR_SCR_F1C_100000) {
        /* The original tests `&this->aText[0] != NULL` here -- the address of
         * a member, so the branch is dead. Kept as this comment only. */
        pCtl->aText[0].pVtbl->pfn10(&pCtl->aText[0]);
        return 1;
    }
    if (f & BR_SCR_F1C_200000)
        return 1;

    if (pCtl->pVtbl->f10(pCtl) != 0)
        return 1;
    return 0;
}

/* ==========================================================================
 * 0x10048060 -- control vtable +0x3C
 * ========================================================================== */

int32_t BrUiNavCtlOther_10048060(BrUiNav *pNav, const BrUiCtl_ *pCtl)
{
    BrUiCtl_ *pOther = pNav->pAA29C0;
    BrPhase_ *pPhase;

    if (pOther == NULL) {
        pNav->pG->nAA2858 = 0;
        return 0;
    }
    pPhase = pOther->pOwner;
    if (pPhase->aFlags[1] != 1) {
        pNav->pG->nAA2858 = 0;
        return 0;
    }
    if (pCtl == pOther)
        return 0;              /* nAA2858 left ALONE on this path */
    pNav->pG->nAA2858 = 1;
    return 1;
}

/* ==========================================================================
 * 0x10047A60 -- control vtable +0x20.
 *
 * The one function in this module that is a FIRST transcription. It is also
 * the one that decides everything: which control carries the CURRENT bit
 * (+0x20), which carries the ACTIVATE bit (+0x02), and -- for every ordinary
 * menu item -- when the selection cursor advances.
 * ========================================================================== */

/* The hit test the original writes out three times over, once per rect, and
 * again on the control's own rectangle. Written once here; the four copies
 * are instruction-identical apart from their operands.
 *
 * NOTE the asymmetry, which is the original's: the LEFT and TOP comparisons
 * exclude equality on the far side (`jg` -> outside when edge > p), the RIGHT
 * comparison excludes when edge < p, and the BOTTOM one INCLUDES equality
 * (`jge` -> inside). So the box is closed on all four edges. */
static int BrNavPtIn(int32_t l, int32_t t, int32_t r, int32_t b,
                     int32_t x, int32_t y)
{
    if (l > x) return 0;
    if (r < x) return 0;
    if (t > y) return 0;
    if (b < y) return 0;
    return 1;
}

static int BrNavPtInStyle(const BrTextStyle *pRc, int32_t x, int32_t y)
{
    /* Port-only guard: the original always has all three rects. */
    if (pRc == NULL)
        return 0;
    return BrNavPtIn(pRc->left, pRc->top, pRc->right, pRc->bottom, x, y);
}

int BrUiNavCtlHit_10047A60(BrUiNav *pNav, BrUiCtl_ *pCtl)
{
    BrScrGlobals  *pG = pNav->pG;
    BrActiveFlags *pA = pNav->pActive;
    uint32_t f;
    int32_t  x, y;
    int      fCurrent;      /* edx -- "this control is the selected one"     */
    int      fHot;          /* ecx -- "cursor is inside one of the 3 rects"  */
    int      fInside;

    f = (uint32_t)pCtl->flags1C;

    /* `test al,8` -- inert. Returns 0 with nothing written. */
    if (f & 0x00000008u)
        return 0;

    /* `test al,0x10` -- the page frame's own ordinal arm handles this
     * control; here it only advances the cursor and counts. */
    if (f & BR_SCR_F1C_0010) {
        if (pG->wAA286C == pG->wAA2870) {
            pG->wAA286C = (uint16_t)(pG->wAA286C + pG->w0AB3DC);
            (void)BrUiNavPageSelect_100484F0(pNav, pCtl->pOwner->pCur);
        }
        ++pG->wAA2870;
        return 0;
    }

    /* The 0x80000 latch. The original calls 0x1003E080 TWICE here and
     * re-tests the same flag between the calls; the predicate reads only
     * globals and nothing between the two calls writes them, so the second
     * answer is the first. Both calls are kept so the call graph matches. */
    if (f & 0x00080000u) {
        if (BrIsAnyActive(pA) != 0) {
            if (BrIsAnyActive(pA) != 0) {
                pCtl->flags1C = (int32_t)((uint32_t)pCtl->flags1C | 0x22u);
                return 1;
            }
        } else {
            /* `and dword [esi+0x1c], 0xfff7fffd` -- clears 0x80000 AND 0x2. */
            pCtl->flags1C =
                (int32_t)((uint32_t)pCtl->flags1C & 0xFFF7FFFDu);
        }
    }

    x = pNav->pCursor[0];
    y = pNav->pCursor[1];

    if (BrNavPtInStyle(pNav->apHot[0], x, y)
     || BrNavPtInStyle(pNav->apHot[1], x, y)
     || BrNavPtInStyle(pNav->apHot[2], x, y)) {
        fHot     = 1;
        fCurrent = 0;
    } else {
        fHot = 0;
        /* The original computes 19*cursor and 19*ordinal (two `lea` pairs,
         * both biased by whatever was left in edx) and compares them. It is
         * an equality test on the two words, and the bias cancels. */
        fCurrent = ((int32_t)(int16_t)pG->wAA286C
                 == (int32_t)(int16_t)pG->wAA2870) ? 1 : 0;
    }

    ++pG->wAA2870;
    pNav->nAA284C = fHot;

    fInside = BrNavPtIn(pCtl->rcLeft, pCtl->rcTop, pCtl->rcRight,
                        pCtl->rcBottom, x, y);

    if (!fInside && !fCurrent) {
        /* `and al,0xdd` -- clears CURRENT (0x20) and ACTIVATE (0x02). */
        pCtl->flags1C = (int32_t)((uint32_t)pCtl->flags1C & ~0x22u);
        return 0;
    }

    if (fCurrent) {
        /* The first four of 0x1003E080's nine globals, inlined. Any of them
         * set means "a modal thing is running": leave the flags alone. */
        if (pA->a0 != 0 || pA->a1 != 0 || pA->a2 != 0 || pA->a3 != 0)
            return 1;
    }

    f = (uint32_t)pCtl->flags1C;
    if (f & 0x00040000u) {
        const BrObjAA2E80 *p = pG->pAA2E80;
        if (p->f2C != 0 || p->f30 != 0)
            f |= 0x00080002u;
        else
            f &= ~0x00000002u;
    } else {
        int fFire;

        /* The original's arms, kept in its order: the override global short-
         * circuits straight to the full predicate (which it forces to 0),
         * while a5 or a6 set the bit without calling anything. */
        if (pA->override != 0)
            fFire = (BrIsAnyActive(pA) != 0);
        else if (pA->a5 != 0 || pA->a6 != 0)
            fFire = 1;
        else
            fFire = (BrIsAnyActive(pA) != 0);

        if (fFire)
            f |= 0x00000002u;
        else
            f &= ~0x00000002u;
    }
    /* The original stores TWICE: once without the CURRENT bit and once with
     * it. Reproduced, because a hook reached from another thread would see
     * the intermediate value. */
    pCtl->flags1C = (int32_t)f;
    f |= 0x00000020u;
    pCtl->flags1C = (int32_t)f;
    return 1;
}

/* ==========================================================================
 * 0x10048180 -- control vtable +0x0C, one frame of one control.
 *
 * This is where a control's HOOKS are finally called: +0x04 unconditionally,
 * then +0x08 when the ACTIVATE bit is set (and the bit is cleared afterwards),
 * or +0x0C when it is not, and +0x18 near the end.
 * ========================================================================== */

/* The child lookup the original performs six times over: the control's phase,
 * that phase's current page, then the page control named by the int16 at
 * +0x2AB6 + 2*i. */
static BrUiCtl_ *BrNavChild(BrUiCtl_ *pCtl, int32_t i)
{
    BrPhase_ *pPhase = pCtl->pOwner;
    int32_t   k = pCtl->aChild[i];
    return pPhase->pCur->apCtl[k];
}

int BrUiNavCtlFrame_10048180(BrUiNav *pNav, BrUiCtl_ *pCtl)
{
    const BrUiCtlVtbl_ *pV;
    int16_t   wSaved = pCtl->wStep;
    uint32_t  f;
    BrUiCtlHookFn_ pfn;

    if ((uint32_t)pCtl->flags1C & BR_SCR_F1C_0010) {
        pCtl->pVtbl->f08(pCtl);
        return 1;
    }
    pV = pCtl->pVtbl;
    if (pV->f3C(pCtl) != 0) {
        pV->f08(pCtl);
        return 1;
    }
    if (pCtl->twActive != 0)
        pV->f30(pCtl);
    pV->f04(pCtl);

    pfn = pCtl->pfn04;
    if (pfn != NULL) {
        int32_t r = pfn(pCtl);
        if (r == -2) {
            /* GOTCHA: -2 skips the whole body but still reports success --
             * and, unlike every other exit, does NOT call vtable +0x08. */
            return 1;
        }
        if (r == -1)
            return 0;
    }

    if (pV->f20(pCtl) != 0 && pNav->pG->nAA28D8 == 0) {
        f = (uint32_t)pCtl->flags1C;

        if (f & BR_SCR_F1C_400000) {
            const BrObjAA2E80 *p = pNav->pG->pAA2E80;
            if (p->f2C != 0 || p->f30 != 0)
                pCtl->w1E20C = pCtl->aStepId[1];   /* the old `f2A42` */
        }

        if (f & BR_SCR_F1C_0002) {
            BrUiCtlHookFn_ p8 = pCtl->pfn08;
            if (p8 != NULL) {
                int32_t r;
                if ((void *)p8 == pNav->pG->pfn10043760) {
                    BrSub10072AF0(2, 0x200020);
                    pNav->pG->nAA2854 = 2;
                } else if ((void *)p8 != pNav->pG->pfn10042CF0) {
                    BrSub10072AF0(1, 0x200020);
                    pNav->pG->nAA2854 = 1;
                }
                /* the field is RE-READ for the call */
                r = pCtl->pfn08(pCtl);
                if (r == 0)
                    return 0;
                if ((void *)pCtl->pfn08 == pNav->pG->pfn10042CF0) {
                    BrSub10072AF0(1, 0x200020);
                    pNav->pG->nAA2854 = 1;
                }
                pNav->pG->nAA33E4 = 0;
            }
            pCtl->flags1C =
                (int32_t)((uint32_t)pCtl->flags1C & ~BR_SCR_F1C_0002);
        } else {
            if (pCtl->pfn0C != NULL)
                (void)pCtl->pfn0C(pCtl);
        }

        if (((uint32_t)pCtl->flags1C & BR_SCR_F1C_10000) != 0
            && (int16_t)pCtl->cChild > 0) {
            int32_t i = 0;
            do {
                /* The original repeats this lookup five times before the
                 * dispatch; with no intervening call the results are
                 * identical, so it is done once. It is repeated after the
                 * dispatch, which the original also does. */
                BrUiCtl_ *pKid = BrNavChild(pCtl, i);

                pKid->flags1C =
                    (int32_t)((uint32_t)pKid->flags1C | BR_SCR_F1C_20000);
                pKid->wStep  = wSaved;
                pKid->f2974  = 0;
                pKid->f2970  = 0;
                pCtl->rcRight += (int32_t)pKid->w48;
                pKid->pVtbl->f0C(pKid);

                pKid = BrNavChild(pCtl, i);
                pKid->flags1C =
                    (int32_t)((uint32_t)pKid->flags1C & ~BR_SCR_F1C_20000);
                ++i;
            } while (i < (int32_t)(int16_t)pCtl->cChild);

            pV->f08(pCtl);
            return 1;
        }
        pV->f08(pCtl);
        return 1;
    }

    /* The "vtable +0x20 said no" tail. */
    f = (uint32_t)pCtl->flags1C;
    if (f & BR_SCR_F1C_400000)
        pCtl->w1E20C = pCtl->aStepId[0];

    if ((f & 0x00000004u) == 0 && (f & BR_SCR_F1C_20000) == 0) {
        pCtl->wStep = 0;
        if ((f & BR_SCR_F1C_100000) != 0
            && (f & BR_SCR_F1C_0010) == 0
            && pCtl->pfn0C != NULL) {
            pCtl->w1E20C = 3;
            pCtl->aText[0].f08 = 1;
            pV->f08(pCtl);
            return 1;
        }
    } else {
        if (pCtl->pfn0C != NULL)
            (void)pCtl->pfn0C(pCtl);
    }
    pV->f08(pCtl);
    return 1;
}

/* ==========================================================================
 * 0x10048530 -- page vtable +0x04, one frame of one page
 * ========================================================================== */

int BrUiNavPageFrame_10048530(BrUiNav *pNav, BrUiPage_ *pPage)
{
    BrScrGlobals *pG = pNav->pG;
    int32_t i;

    if (pPage->pfn04 != NULL)
        pPage->pfn04();
    if (pPage->pfn0C != NULL)
        pPage->pfn0C();

    pG->wAA2870 = 0;
    (void)BrUiNavPageSelect_100484F0(pNav, pPage);

    for (i = 0; i < (int32_t)pPage->cCtl; ++i) {
        BrUiCtl_ *pCtl = pPage->apCtl[i];
        uint32_t  f;

        if (pCtl == NULL)
            return 0;

        if (pCtl->pfn14 != NULL && pCtl->pfn14(pCtl) == 0)
            return 0;

        f = (uint32_t)pCtl->flags1C;
        if (f & BR_SCR_F1C_1000) {
            pCtl->pVtbl->f04(pCtl);
            if (pCtl->pfn04 != NULL)
                (void)pCtl->pfn04(pCtl);

            if ((uint32_t)pCtl->flags1C & BR_SCR_F1C_0010) {
                if (pG->wAA286C == pG->wAA2870) {
                    pG->wAA286C = (uint16_t)(pG->wAA286C + pG->w0AB3DC);
                    (void)BrUiNavPageSelect_100484F0(pNav, pPage);
                }
                ++pG->wAA2870;
            }
            f = (uint32_t)pCtl->flags1C;      /* RE-READ */
            if ((f & BR_SCR_F1C_0010) == 0)
                continue;
        }
        if (f & BR_SCR_F1C_0800)
            continue;

        if (pCtl->pVtbl->f0C(pCtl) == 0) {
            pG->bAA28A8 = 0;
            return 0;
        }

        f = (uint32_t)pCtl->flags1C;
        if (f & (BR_SCR_F1C_2000 | BR_SCR_F1C_4000)) {
            uint32_t nSel = (uint32_t)pPage->pOwner->fBC;
            if (nSel == (uint32_t)i || (f & BR_SCR_F1C_4000)) {
                int16_t nKids = (int16_t)pCtl->cChild;
                int32_t k = 0;
                while (k < nKids) {
                    BrUiCtl_ *pKid = pPage->apCtl[pCtl->aChild[k]];
                    pKid->pVtbl->f0C(pKid);
                    nKids = (int16_t)pCtl->cChild;     /* re-read */
                    ++k;
                }
            }
        }

        if (pCtl->pfn18 != NULL && pCtl->pfn18(pCtl) == 0)
            return 0;

        f = (uint32_t)pCtl->flags1C;
        if ((f & BR_SCR_F1C_0020) != 0
            && pG->nAA28D8 == 0
            && (f & BR_SCR_F1C_2000) != 0) {
            BrPhase_ *pOwner = pPage->pOwner;
            if ((uint32_t)pOwner->fBC != (uint32_t)i) {
                int k;
                pOwner->fBC = (uint16_t)(uint32_t)i;
                for (k = 0; k < BR_PHASE_PAGES; ++k)
                    pPage->pOwner->aFlags[k] = 0;
                pPage->pOwner->aFlags[0] = 1;
            }
        }
    }

    if (pPage->pfn08 != NULL)
        pPage->pfn08();
    return 1;
}

/* ==========================================================================
 * 0x10048AA0 -- phase vtable +0x1C
 * ========================================================================== */

void BrUiNavPhaseRelease_10048AA0(BrUiNav *pNav, BrPhase_ *pPhase)
{
    int32_t i;

    for (i = 0; i < (int32_t)pPhase->nPages; ++i) {
        BrUiPage_ *pPg = pPhase->aPages[i];

        /* DEVIATION: see br_uinav.h. The original null-checks the page only
         * AFTER walking its 200 slots. */
        if (pPg != NULL) {
            int k;
            for (k = 0; k < BR_UI_PAGE_CTL_MAX; ++k) {
                BrUiCtl_ *pCtl = pPg->apCtl[k];
                if (pCtl != NULL) {
                    /* br_ui.h leaves control vtable +0x00 as `void *` on the
                     * stated grounds that nothing had been seen to call it.
                     * This calls it: 0x100478A0 is the MSVC scalar deleting
                     * destructor, same shape as the page's and the phase's
                     * (`this`, flags -> this). The cast is here rather than in
                     * the header because ONE observed call site is thin
                     * evidence for changing a published slot type. */
                    void *(*pfnDel)(BrUiCtl_ *, int32_t) =
                        (void *(*)(BrUiCtl_ *, int32_t))pCtl->pVtbl->f00;
                    if (pfnDel != NULL)
                        (void)pfnDel(pCtl, 1);
                }
                pPg->apCtl[k] = NULL;
            }
            (void)pPg->pVtbl->f00(pPg, 1);
        }
    }
    pNav->pG->wAA286C = 0;
}

/* ==========================================================================
 * The two control hooks that move between screens
 * ========================================================================== */

/* XSLICE 0x1007DFE0 -- operator new; does NOT zero. */
extern void *BrOperatorNew(uint32_t cb);
/* XSLICE 0x1004F700 -- the enter hook 0x10045AF0 installs. A ported builder. */
extern void BrExt_1004F700(BrPhase_ *pSelf);

int32_t BrUiNavHook_10045AF0(BrUiCtl_ *pCtl)
{
    BrUiNav  *pNav = g_pBrUiNav;
    BrPhase_ *p;

    (void)pCtl;   /* the original takes the control and never reads it */

    if (pNav->pAA2924 != NULL) {
        /* Already built: republish and return WITHOUT running the builder. */
        pNav->pAA2904 = pNav->pAA2924;
        return 1;
    }

    /* HARDENING (port): the original pushes 0xC8. See br_uinav.h. */
    p = (BrPhase_ *)BrOperatorNew(BR_PHASE_ALLOC_SIZE);
    p = (p != NULL) ? BrOptObjCtor(p) : NULL;

    /* Both globals are written even when the allocation failed. */
    pNav->pAA2924 = p;
    pNav->pAA2904 = p;
    if (p == NULL)
        return 1;      /* `mov eax,1` is on this path too */

    p->pfnEnter = BrExt_1004F700;

    /* The original re-reads 0x10AA2924 and calls through THAT, not through
     * the register holding the new object. */
    p = pNav->pAA2924;
    p->pfnEnter(p);

    /* ...and re-reads 0x10AA2904 once per store, so an enter hook that
     * re-pointed the current phase lands these two flags on ITS phase.
     * DEVIATION: guarded against NULL, which the original would fault on. */
    if (pNav->pAA2904 != NULL)
        pNav->pAA2904->f0C = 1;
    if (pNav->pAA2904 != NULL)
        pNav->pAA2904->f68 = 1;
    return 1;
}

int32_t BrUiNavHook_10046C90(BrUiCtl_ *pCtl)
{
    BrUiNav  *pNav = g_pBrUiNav;
    BrPhase_ *pOwner = pCtl->pOwner;

    pOwner->pVtbl->f1C(pOwner);

    /* The CURRENT phase is re-read here, after +0x1C has run. */
    if (pNav->pAA2904 != NULL)
        (void)pNav->pAA2904->pVtbl->f00(pNav->pAA2904, 1);

    /* GOTCHA, and it is the original's order: 0x10AA2908 is loaded BEFORE
     * 0x10AA291C is cleared, and only then stored into 0x10AA2904. */
    {
        BrPhase_ *pRoot = pNav->pAA2908;
        pNav->nAA291C = 0;
        pNav->pAA2904 = pRoot;
    }
    /* `xor eax,eax` -- 0 from a +0x08 hook stops 0x10048180 and, through it,
     * the page walk in 0x10048530. */
    return 0;
}

/* ==========================================================================
 * Vtable adapters and the input seam
 * ========================================================================== */

static void      NavV_f04(BrUiCtl_ *p) { (void)BrUiNavCtlTick_100480A0(p); }
static void      NavV_f08(BrUiCtl_ *p)
{
    (void)BrUiNavCtlEnter_10048010(g_pBrUiNav, p);
}
static int32_t   NavV_f0C(BrUiCtl_ *p)
{
    return BrUiNavCtlFrame_10048180(g_pBrUiNav, p);
}
static int32_t   NavV_f10(BrUiCtl_ *p)
{
    return BrUiNavCtlStepCode_10047A10(p);
}
static int32_t   NavV_f20(BrUiCtl_ *p)
{
    return BrUiNavCtlHit_10047A60(g_pBrUiNav, p);
}
static int32_t   NavV_f3C(BrUiCtl_ *p)
{
    return BrUiNavCtlOther_10048060(g_pBrUiNav, p);
}
static int32_t   NavV_page04(BrUiPage_ *p)
{
    return BrUiNavPageFrame_10048530(g_pBrUiNav, p);
}

void BrUiNavInstallCtlVtbl(BrUiCtlVtbl_ *pVtbl)
{
    pVtbl->f04 = NavV_f04;
    pVtbl->f08 = NavV_f08;
    pVtbl->f0C = NavV_f0C;
    pVtbl->f10 = NavV_f10;
    pVtbl->f20 = NavV_f20;
    pVtbl->f3C = NavV_f3C;
    /* +0x14, +0x18, +0x1C, +0x24, +0x28, +0x2C, +0x30 are NOT touched: they
     * are the draw and tween slots, which this module does not port. Leaving
     * them as the caller had them keeps an unported method a fault. */
}

void BrUiNavInstallPageVtbl(BrUiPageVtbl_ *pVtbl)
{
    pVtbl->f04 = NavV_page04;
}

/* 0x100603A0's two edges, and only those two. The step and the cursor are
 * written together because that function writes them together. */
void BrUiNavMove(BrUiNav *pNav, int dir)
{
    if (dir < 0) {
        pNav->pG->w0AB3DC = (uint16_t)(int16_t)-1;
        --pNav->pG->wAA286C;            /* 0 -> 0xFFFF is the wrap, not a bug */
    } else if (dir > 0) {
        pNav->pG->w0AB3DC = 1;
        ++pNav->pG->wAA286C;
    }
}

void BrUiNavSetStep(BrUiNav *pNav, int step)
{
    pNav->pG->w0AB3DC = (uint16_t)(int16_t)step;
}

void BrUiNavSetActivate(BrUiNav *pNav, int fDown)
{
    pNav->pActive->a5 = fDown ? 1 : 0;
}

int BrUiNavSelection(const BrUiNav *pNav)
{
    return (int)(int16_t)pNav->pG->wAA286C;
}
