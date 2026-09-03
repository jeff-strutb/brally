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
#ifdef BR_MATCHING_BUILD
/* Header is cdecl (nav, ctl). Original is thiscall with this = ctl. */
#define BrUiNavCtlHit_10047A60 BrUiNavCtlHit_10047A60_hdr
#endif
#include "br_uinav.h"
#ifdef BR_MATCHING_BUILD
#undef BrUiNavCtlHit_10047A60
#endif
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

/* @implements 0x10047A60 d3d BrUiNavCtlHit_10047A60 */
#ifdef BR_MATCHING_BUILD
/* Orig is thiscall (this = pCtl in ecx, `mov esi, ecx`) and reads the
 * cursor / hot-rects / ordinal / activity flags as standalone globals,
 * not through a BrUiNav *. BrIsAnyActive is 0-arg cdecl; page-select is
 * thiscall on pOwner->pCur. */
typedef struct BrHitRect { int32_t l, t, r, b; } BrHitRect;
typedef struct BrObj2C { int32_t _[11]; int32_t f2C; int32_t f30; } BrObj2C;
extern int32_t   *g_navCursor;     /* 0x10AC5DD8 */
extern uint16_t   g_wAA286C;       /* 0x10AC5BC4 */
extern uint16_t   g_wAA2870;       /* 0x10AC5BC8 */
extern uint16_t   g_w0AB3DC;       /* 0x100AAB7C */
extern int32_t    g_nAA284C;       /* 0x10AC5BA4 */
extern BrHitRect  g_hot0;          /* 0x100AABE8 */
extern BrHitRect  g_hot1;          /* 0x100AABB8 */
extern BrHitRect  g_hot2;          /* 0x100AABC8 */
extern int32_t    g_act0;          /* 0x10AC6730 */
extern int32_t    g_act1;
extern int32_t    g_act2;
extern int32_t    g_act3;
extern int32_t    g_actOverride;   /* 0x10AC5BB4 */
extern int32_t    g_act5;          /* 0x10AC5E50 */
extern int32_t    g_act6;          /* 0x10AC6050 */
extern BrObj2C   *g_pAA2E80;       /* 0x10AC61E0 */
int BrIsAnyActiveGlide(void);
int BR_THISCALL1 BrUiNavPageSelectGlide(BrUiPage_ *pPage);

int BR_THISCALL1 BrUiNavCtlHit_10047A60(BrUiCtl_ *pCtl)
{
    int fZero = 0;
    int32_t f = pCtl->flags1C;
    int32_t *pCurXY;
    int fHot;
    int fCurrent;
    int32_t a;

    if (f & 8)
        return 0;

    if (f & 0x10) {
        if (g_wAA286C == g_wAA2870) {
            g_wAA286C = (uint16_t)(g_wAA286C + g_w0AB3DC);
            BrUiNavPageSelectGlide(pCtl->pOwner->pCur);
        }
        ++g_wAA2870;
        return 0;
    }

    if (f & 0x80000) {
        if (BrIsAnyActiveGlide() == 0) {
            pCtl->flags1C &= 0xFFF7FFFD;
            goto hit_test;
        }
    }
    if (pCtl->flags1C & 0x80000) {
        if (BrIsAnyActiveGlide() != 0) {
            a = pCtl->flags1C;
            *(unsigned char *)&a |= 0x22;
            pCtl->flags1C = a;
            return 1;
        }
    }

hit_test:
    pCurXY = g_navCursor;

    if (g_hot0.l > pCurXY[0])
        goto miss0;
    if (g_hot0.r < pCurXY[0])
        goto miss0;
    if (g_hot0.t > pCurXY[1])
        goto miss0;
    if (g_hot0.b < pCurXY[1])
        goto miss0;
    fHot = 1;
    fCurrent = 0;
    goto after_hot;
miss0:
    if (g_hot1.l > pCurXY[0])
        goto miss1;
    if (g_hot1.r < pCurXY[0])
        goto miss1;
    if (g_hot1.t > pCurXY[1])
        goto miss1;
    if (g_hot1.b < pCurXY[1])
        goto miss1;
    fHot = 1;
    fCurrent = 0;
    goto after_hot;
miss1:
    if (g_hot2.l > pCurXY[0])
        goto miss2;
    if (g_hot2.r < pCurXY[0])
        goto miss2;
    if (g_hot2.t > pCurXY[1])
        goto miss2;
    if (g_hot2.b < pCurXY[1])
        goto miss2;
    fHot = 1;
    fCurrent = 0;
    goto after_hot;
miss2:
    {
        int32_t ord = (int32_t)(int16_t)g_wAA2870;
        int32_t sel = (int32_t)(int16_t)g_wAA286C;
        fHot = 0;
        fCurrent = (sel * 19 == ord * 19) ? 1 : fZero;
    }
after_hot:
    ++g_wAA2870;
    g_nAA284C = fHot;

    if (pCtl->rcLeft > pCurXY[0])
        goto not_inside;
    if (pCtl->rcRight < pCurXY[0])
        goto not_inside;
    if (pCtl->rcTop > pCurXY[1])
        goto not_inside;
    if (pCtl->rcBottom < pCurXY[1])
        goto not_inside;
    goto inside;
not_inside:
    if (fCurrent == 0) {
        a = pCtl->flags1C;
        *(unsigned char *)&a &= 0xdd;
        pCtl->flags1C = a;
        return 0;
    }
inside:
    if (fCurrent) {
        if (g_act0 != 0 || g_act1 != 0 || g_act2 != 0 || g_act3 != 0)
            return 1;
    }

    a = pCtl->flags1C;
    if (a & 0x40000) {
        if (g_pAA2E80->f2C != 0 || g_pAA2E80->f30 != 0)
            a |= 0x80002;
        else
            *(unsigned char *)&a &= 0xfd;
    } else {
        if (g_actOverride != 0)
            goto do_pred;
        if (g_act5 != 0)
            goto set_bit;
        if (g_act6 != 0)
            goto set_bit;
    do_pred:
        if (BrIsAnyActiveGlide() == 0)
            goto clear_bit;
    set_bit:
        a = pCtl->flags1C;
        *(unsigned char *)&a |= 2;
        goto store_f;
    clear_bit:
        a = pCtl->flags1C;
        *(unsigned char *)&a &= 0xfd;
    }
store_f:
    pCtl->flags1C = a;
    *(unsigned char *)&a |= 0x20;
    pCtl->flags1C = a;
    return 1;
}
#else
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
#endif /* BR_MATCHING_BUILD */

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
 * 0x100489A0 -- phase vtable +0x0C, one frame of one phase.
 *
 * The top of the chain. slice3_32.c has this body over BrPhaseFull; see the
 * DUPLICATE OWNERSHIP table in br_uinav.h and the banner beside
 * BrPhaseRun_100489A0 in slice3_32.c. Every global it touches is reached
 * through pNav->pG or through the pAA2904 / pAA2908 members whose single-
 * population rule br_uinav.h states, so no storage is duplicated.
 * ========================================================================== */

/* The two-call teardown both failure exits run. slice3_32.c's is
 * BrScrPhaseBail; the two differ only in the type of `this`.
 *
 * The vtable is passed in rather than re-read because the SECOND exit uses the
 * pointer loaded before the tick call, which is the original's `edi`. */
static void BrNavPhaseBail(BrUiNav *pNav, BrPhase_ *pThis,
                           const BrPhaseVtbl_ *pV)
{
    BrSub1003E310();
    BrSub1006A4A0(pNav->pG->pB4DF30, pNav->pG->pB4FBE8);
    pThis->iPage = 0;
    pV->f18(pThis, NULL);
}

int BrUiNavPhaseRun_100489A0(BrUiNav *pNav, BrPhase_ *pThis)
{
    const BrPhaseVtbl_ *pV;
    int32_t i;

    if (pThis->f68 == 0) {
        BrNavPhaseBail(pNav, pThis, pThis->pVtbl);
        return 0;
    }

    (void)pThis->pVtbl->f04(pThis);

    /* 0x10060260, with the current phase temporarily pointed at the root.
     * The swap is the original's and is restored immediately; the callee is
     * host-injected, for the reason br_uinav.h's BrUiNavPollFn banner gives. */
    {
        BrPhase_ *pPrev = pNav->pAA2904;
        pNav->pAA2904 = pNav->pAA2908;
        if (pNav->pfnPoll != NULL)
            pNav->pfnPoll(pNav);
        pNav->pAA2904 = pPrev;
    }

    /* 0x1005FFB0 -- OUTSIDE the swap, which is why it is a separate call and
     * not folded into the seam above. */
    BrDikPollAndEdge();

    pNav->pG->nAA2868 = (pNav->pAA2904 == pNav->pAA2908) ? 1 : 0;

    pThis->iPage = 0;
    for (i = 0; i < (int32_t)pThis->nPages; ++i) {
        BrUiPage_ *pPg = pThis->aPages[i];

        /* GOTCHA: pCur is written BEFORE the NULL test. */
        pThis->pCur = pPg;
        if (pPg == NULL)
            return 0;
        pThis->iPage = (uint16_t)(uint32_t)i;
        if (pThis->aFlags[i] != 0) {
            BrUiPage_ *pCur = pThis->pCur;   /* the original re-reads +0x64 */
            if (pCur->pVtbl->f04(pCur) == 0)
                return 0;
        }
    }

    pV = pThis->pVtbl;
    (void)pV->f08(pThis);
    if (pThis->f68 != 0)
        return 1;
    BrNavPhaseBail(pNav, pThis, pV);
    return 0;
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

/* port-only body; Glide match is src/core/cpp/0x100400E0.cpp */
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
#ifdef BR_MATCHING_BUILD
    return BrUiNavCtlHit_10047A60(p);
#else
    return BrUiNavCtlHit_10047A60(g_pBrUiNav, p);
#endif
}
static int32_t   NavV_f3C(BrUiCtl_ *p)
{
    return BrUiNavCtlOther_10048060(g_pBrUiNav, p);
}
static int32_t   NavV_page04(BrUiPage_ *p)
{
    return BrUiNavPageFrame_10048530(g_pBrUiNav, p);
}
static int32_t   NavV_phase0C(BrPhase_ *p)
{
    return (int32_t)BrUiNavPhaseRun_100489A0(g_pBrUiNav, p);
}
static void      NavV_phase1C(BrPhase_ *p)
{
    BrUiNavPhaseRelease_10048AA0(g_pBrUiNav, p);
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

void BrUiNavInstallPhaseVtbl(BrPhaseVtbl_ *pVtbl)
{
    pVtbl->f0C = NavV_phase0C;
    pVtbl->f1C = NavV_phase1C;
    /* +0x00, +0x04, +0x08, +0x10, +0x14, +0x18 and +0x20 are NOT touched --
     * see br_uinav.h. Three of them are dispatched through by the frame. */
}

/* 0x100603A0's two edges, and only those two. The step and the cursor are
 * written together because that function writes them together.
 *
 * NOT TAGGED. This is a FRAGMENT -- 48 bytes of a 939-byte function -- and
 * config/shared.csv maps d3d 0x100603A0 to Glide 0x10059410, which is a true
 * twin (939 bytes in both binaries) whose real transcription is BrGlNavPoll
 * below. Tagged @implements until 2026-09-03, which put two names on one
 * address and scored a 48-byte fragment against the whole function. Fragments
 * and thunks must not carry @implements -- docs/VC5-IDIOMS.md. */
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

/* ====================================================================== *
 * 0x10059410 -- the GLIDE mouse poller (939 B).  thiscall on the nav
 * record (this in ecx, one never-read stack arg, callee-pops), reached
 * through the fastcall shim.  DirectInput device at this+0x50: Acquire /
 * GetDeviceState(0x10) with the DIERR_INPUTLOST (0x8007001E) reacquire
 * loop, the device pointer re-read from the record at every use.  Then
 * axis accumulate+clamp, button bits & 0x80, the timeout/page-flip
 * bookkeeping, per-button edge machines, and the common tail hook
 * (g_..5DD8 = this; 0x10059060).
 * ====================================================================== */
#ifdef BR_MATCHING_BUILD

typedef struct BrDInVtbl_ {
    int (__stdcall *f00[7])(void *);
    int (__stdcall *pfnAcquire)(void *);                      /* +0x1C */
    int (__stdcall *f20)(void *);
    int (__stdcall *pfnGetDeviceState)(void *, int, void *);  /* +0x24 */
} BrDInVtbl_;
typedef struct BrDInDev_ { BrDInVtbl_ *pVtbl; } BrDInDev_;

typedef struct BrGlNavRec {
    int32_t   x, y, z;            /* +0x00 +0x04 +0x08 */
    int32_t   xPrev, yPrev, zPrev;/* +0x0C +0x10 +0x14 */
    uint8_t   pad18[0xC];         /* +0x18 */
    uint8_t   ab[4];              /* +0x24 button bits (&0x80) */
    uint8_t   abPrev[4];          /* +0x28 */
    int32_t   aHeld[4];           /* +0x2C */
    int32_t   aClick[4];          /* +0x3C */
    int32_t   iIdle4C;            /* +0x4C */
    BrDInDev_ *pDev;              /* +0x50 */
} BrGlNavRec;

extern int32_t  BrGlNavOff5B9C;     /* 0x10AC5B9C  poll disable          */
extern int32_t  BrGlNavTick670C;    /* 0x10AC670C  last poll time        */
extern int32_t  BrGlNavAccum6710;   /* 0x10AC6710  idle accumulator      */
extern int32_t  BrGlNavT6708;       /* 0x10AC6708  timeout latch         */
extern int32_t  BrGlNavMaxX;        /* 0x10AC6718                        */
extern int32_t  BrGlNavMaxY;        /* 0x10AC6714                        */
extern int32_t  BrGlNavKey5F3C;     /* 0x10AC5F3C                        */
extern int32_t  BrGlNavKey5F40;     /* 0x10AC5F40                        */
extern uint8_t  BrGlNavKey66B0;     /* 0x10AC66B0                        */
extern uint8_t  BrGlNavKey66B8;     /* 0x10AC66B8                        */
extern int32_t  BrGlNavF66F8;       /* 0x10AC66F8                        */
extern int32_t  BrGlNavF66FC;       /* 0x10AC66FC                        */
extern int32_t  BrGlNavF6700;       /* 0x10AC6700                        */
extern int32_t  BrGlNavF6704;       /* 0x10AC6704                        */
extern int32_t  BrGlNavK610C;       /* 0x10AC610C                        */
extern int32_t  BrGlNavK6114;       /* 0x10AC6114                        */
extern uint16_t BrGlNavStepAB7C;    /* 0x100AAB7C                        */
extern uint16_t BrGlNavCur5BC4;     /* 0x10AC5BC4                        */
extern int32_t  BrGlNavLast6748;    /* 0x10AC6748  last-activity time    */
extern int32_t  BrGlNavEdge6720;    /* 0x10AC6720                        */
extern int32_t  BrGlNavEdge6724;    /* 0x10AC6724                        */
extern int32_t  BrGlNavEdge6728;    /* 0x10AC6728                        */
extern int32_t  BrGlNavEdge672C;    /* 0x10AC672C                        */
extern void    *BrGlNavThis5DD8;    /* 0x10AC5DD8                        */

int32_t BrGlNavTimeNow(void);       /* 0x1006E280 */
void    BrGlNavKeyLeft(void);       /* 0x10002C70 */
void    BrGlNavKeyRight(void);      /* 0x10002CB0 */
void    BrGlNavTail(void);          /* 0x10059060 */

/* TAG LIVE WITH DIFFS (drawcar convention): 3 regions / ~10 insns of
 * coupled coloring remain -- the -1 materialization (`or edx,-1` vs
 * folded 0xffff; a shared int local still folds), the eax/ecx flag-home
 * swap, the cl/al 0x80 swap, and one fresh `xor eax,eax`.  Everything
 * structural is byte-shape exact: rotated-while reacquire loop with
 * duplicated condition, dword+byte button loads with batch masks, edge
 * machines, tail hook.  Permuter bait (first_live/recompute class). */
/* @implements 0x10059410 glide BrGlNavPoll */
void __fastcall BrGlNavPoll(BrGlNavRec *pNav, int _edx_unused, int _unused)
{
    uint8_t aState[0x10];
    int32_t t;
    BrDInDev_ *pDev;

    (void)_edx_unused;
    (void)_unused;

    if (BrGlNavOff5B9C != 0)
        return;

    t = BrGlNavTimeNow();
    BrGlNavAccum6710 += t - BrGlNavTick670C;
    BrGlNavTick670C = t;
    if (BrGlNavAccum6710 > 0x78)
        BrGlNavT6708 = 1;

    if (pNav->pDev == 0)
        return;

    pDev = pNav->pDev;
    pDev->pVtbl->pfnAcquire(pDev);
    pDev = pNav->pDev;
    while (pDev->pVtbl->pfnGetDeviceState(pDev, 0x10, aState) ==
           (int32_t)0x8007001E) {
        pDev = pNav->pDev;
        if (pDev->pVtbl->pfnAcquire(pDev) < 0)
            break;
        pDev = pNav->pDev;
        pDev->pVtbl->pfnAcquire(pDev);
        pDev = pNav->pDev;
    }

    pNav->x += *(int32_t *)&aState[0];
    pNav->y += *(int32_t *)&aState[4];
    pNav->z += *(int32_t *)&aState[8];
    if (pNav->x < 0)
        pNav->x = 0;
    else if (pNav->x >= BrGlNavMaxX)
        pNav->x = BrGlNavMaxX;
    if (pNav->y < 0)
        pNav->y = 0;
    else if (pNav->y >= BrGlNavMaxY)
        pNav->y = BrGlNavMaxY;

    {
        /* Buttons 0/1 come out of ONE dword read (cl/ch extraction);
         * 2/3 are plain byte loads; the any-pressed test reads the same
         * masked locals the stores used. */
        uint32_t bw = *(uint32_t *)&aState[0xC];
        uint8_t  b2 = aState[0xE];
        uint8_t  b3 = aState[0xF];
        uint8_t  b0 = (uint8_t)bw;
        uint8_t  b1 = ((uint8_t *)&bw)[1];
        b0 &= 0x80;
        b1 &= 0x80;
        b2 &= 0x80;
        b3 &= 0x80;
        pNav->ab[0] = b0;
        pNav->ab[1] = b1;
        pNav->ab[2] = b2;
        pNav->ab[3] = b3;
        if (b0 != 0 || b1 != 0 || b2 != 0 || b3 != 0)
            pNav->iIdle4C = 1;
    }

    if (BrGlNavKey5F3C != 0)
        BrGlNavKeyLeft();
    if (BrGlNavKey5F40 != 0)
        BrGlNavKeyRight();

    {
    int32_t step = -1;   /* one -1, shared by both backward-step stores */
    if (BrGlNavT6708 != 0) {
        if (BrGlNavKey66B0 & 0x80) {
            BrGlNavF66F8 = 1;
            BrGlNavStepAB7C = (uint16_t)step;
            BrGlNavAccum6710 = 0;
        }
        if (BrGlNavKey66B8 & 0x80) {
            BrGlNavF66FC = 1;
            BrGlNavStepAB7C = 1;
            BrGlNavAccum6710 = 0;
        }
    }
    {
        int32_t f;
        if (BrGlNavK610C != 0) {
            f = 1;
            BrGlNavStepAB7C = (uint16_t)step;
            BrGlNavAccum6710 = 0;
        } else {
            f = BrGlNavF6700;
        }
        if (BrGlNavK6114 != 0) {
            BrGlNavF6704 = 1;
            BrGlNavStepAB7C = 1;
            BrGlNavAccum6710 = 0;
        }
        if (f != 0) {
            pNav->ab[0] = 1;
            pNav->iIdle4C = 0;
            BrGlNavT6708 = 0;
        }
        if (BrGlNavF6704 != 0) {
            pNav->ab[1] = 1;
            pNav->iIdle4C = 0;
            BrGlNavT6708 = 0;
        }
    }
    }
    if (BrGlNavT6708 != 0) {
        if (BrGlNavF66F8 != 0) {
            --BrGlNavCur5BC4;
            BrGlNavT6708 = 0;
        }
        if (BrGlNavF66FC != 0) {
            ++BrGlNavCur5BC4;
            BrGlNavT6708 = 0;
        }
    }
    BrGlNavF6704 = 0;
    BrGlNavF6700 = 0;
    BrGlNavF66FC = 0;
    BrGlNavF66F8 = 0;

    if (pNav->x != pNav->xPrev || pNav->y != pNav->yPrev ||
        pNav->ab[0] != pNav->abPrev[0] || pNav->ab[1] != pNav->abPrev[1] ||
        pNav->ab[2] != pNav->abPrev[2] || pNav->ab[3] != pNav->abPrev[3])
        BrGlNavLast6748 = BrGlNavTimeNow();

    pNav->xPrev = pNav->x;
    pNav->yPrev = pNav->y;
    pNav->abPrev[0] = pNav->ab[0];
    pNav->abPrev[1] = pNav->ab[1];
    pNav->abPrev[2] = pNav->ab[2];
    pNav->abPrev[3] = pNav->ab[3];

    BrGlNavEdge6720 = 0;
    BrGlNavEdge6724 = 0;
    BrGlNavEdge6728 = 0;
    BrGlNavEdge672C = 0;

    if (pNav->ab[0] != 0 && pNav->aHeld[0] == 0) {
        pNav->aHeld[0] = 1;
    } else if (pNav->ab[0] == 0 && pNav->aHeld[0] != 0) {
        pNav->aHeld[0] = 0;
        pNav->aClick[0] = 1;
        pNav->iIdle4C = 0;
        BrGlNavEdge6720 = 1;
    } else {
        pNav->aClick[0] = 0;
    }
    if (pNav->ab[1] != 0 && pNav->aHeld[1] == 0) {
        pNav->aHeld[1] = 1;
    } else if (pNav->ab[1] == 0 && pNav->aHeld[1] != 0) {
        pNav->aHeld[1] = 0;
        pNav->aClick[1] = 1;
        pNav->iIdle4C = 0;
        BrGlNavEdge6724 = 1;
    } else {
        pNav->aClick[1] = 0;
    }
    if (pNav->ab[2] != 0 && pNav->aHeld[2] == 0) {
        pNav->aHeld[2] = 1;
    } else if (pNav->ab[2] == 0 && pNav->aHeld[2] != 0) {
        pNav->aHeld[2] = 0;
        pNav->aClick[2] = 1;
        pNav->iIdle4C = 0;
        BrGlNavEdge6728 = 1;
    } else {
        pNav->aClick[2] = 0;
    }
    if (pNav->ab[3] != 0 && pNav->aHeld[3] == 0) {
        pNav->aHeld[3] = 1;
    } else if (pNav->ab[3] == 0 && pNav->aHeld[3] != 0) {
        pNav->aHeld[3] = 0;
        pNav->aClick[3] = 1;
        pNav->iIdle4C = 0;
        BrGlNavEdge672C = 1;
    } else {
        pNav->aClick[3] = 0;
    }

    BrGlNavThis5DD8 = pNav;
    BrGlNavTail();
}

#endif /* BR_MATCHING_BUILD */
